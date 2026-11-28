/* Test du lecteur GVT1 : en-tete, index, decodage de toutes les tuiles, recherche d'un point connu,
 * tuile absente et tuiles corrompues.
 * Usage : test_gvt carte.gvt lat lon   (le point doit etre un sommet d'une route de classe 1) */
#include "gvt.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  ECHEC : "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static int read_file(void *ctx, uint32_t off, void *dst, uint32_t len)
{
    FILE *f = ctx;
    if (fseek(f, (long)off, SEEK_SET) != 0) return -1;
    return fread(dst, 1, len, f) == len ? 0 : -1;
}

typedef struct { const uint8_t *data; uint32_t length; } header_ctx_t;
static int read_header(void *ctx, uint32_t off, void *dst, uint32_t len)
{
    header_ctx_t *m = ctx;
    if ((uint64_t)off + len > m->length) return -1;
    memcpy(dst, m->data + off, len);
    return 0;
}

static void test_open_bounds(FILE *f, uint32_t actual_size)
{
    uint8_t h[GVT_HEADER_SIZE];
    header_ctx_t ctx = { h, sizeof h };
    gvt_t g;
    uint32_t index_offset, n_tiles;
    CHECK(fseek(f, 0, SEEK_SET) == 0 && fread(h, 1, sizeof h, f) == sizeof h,
          "lecture de l'en-tete pour tests de limites");
    if (fails) return;
    index_offset = (uint32_t)h[16] | ((uint32_t)h[17] << 8) | ((uint32_t)h[18] << 16) | ((uint32_t)h[19] << 24);
    n_tiles = (uint32_t)h[12] | ((uint32_t)h[13] << 8) | ((uint32_t)h[14] << 16) | ((uint32_t)h[15] << 24);
    CHECK(gvt_open(&g, read_header, &ctx, actual_size) == 0, "ouverture avec taille de fichier exacte");
    CHECK(gvt_open(&g, read_header, &ctx, GVT_HEADER_SIZE - 1) != 0, "refus d'un fichier plus court que l'en-tete");
    CHECK(gvt_open(&g, read_header, &ctx, index_offset + n_tiles * GVT_ENTRY_SIZE - 1u) != 0,
          "refus d'un index tronque d'un octet");
    {
        uint8_t saved[4];
        memcpy(saved, h + 16, 4);
        memset(h + 16, 0, 4);
        CHECK(gvt_open(&g, read_header, &ctx, actual_size) != 0, "refus d'un index place dans l'en-tete");
        memcpy(h + 16, saved, 4);
    }
    {
        uint8_t saved[4];
        memcpy(saved, h + 12, 4);
        memset(h + 12, 0xFF, 4);
        CHECK(gvt_open(&g, read_header, &ctx, actual_size) != 0, "refus d'un index annonce au-dela du fichier");
        memcpy(h + 12, saved, 4);
    }
}

static double seg_dist(double px, double py, double ax, double ay, double bx, double by)
{
    double dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
    double t = l2 == 0 ? 0 : ((px - ax) * dx + (py - ay) * dy) / l2;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return hypot(px - ax - t * dx, py - ay - t * dy);
}

int main(int argc, char **argv)
{
    static uint8_t blob[1 << 20];
    static int16_t xy[2 * GVT_MAX_POINTS];
    static int16_t axy[2 * GVT_MAX_AREA_POINTS];
    uint16_t ring_len[GVT_MAX_RINGS];
    unsigned long areas = 0;
    gvt_t g;
    gvt_entry_t *index;
    FILE *f;
    long file_size;
    double lat, lon;
    unsigned long lines = 0, points = 0, class_count[GVT_CLASS_COUNT] = {0};

    if (argc != 4) { fprintf(stderr, "usage : %s carte.gvt lat lon\n", argv[0]); return 2; }
    lat = atof(argv[2]);
    lon = atof(argv[3]);
    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    fseek(f, 0, SEEK_END);
    file_size = ftell(f);

    CHECK(gvt_open(&g, read_file, f, (uint32_t)file_size) == 0, "en-tete invalide");
    if (fails) return 1;
    CHECK(g.zmin == 11 && g.zmax == 15 && g.extent == GVT_EXTENT, "zooms %u-%u, etendue %u", g.zmin, g.zmax, g.extent);
    CHECK(g.version == GVT_VERSION, "version %u attendue %u", g.version, GVT_VERSION);
    test_open_bounds(f, (uint32_t)file_size);
    CHECK(g.n_tiles > 0, "aucune tuile");
    index = malloc(g.n_tiles * sizeof *index);
    CHECK(gvt_load_index(&g, index, g.n_tiles - 1) != 0, "un tampon d'index trop petit doit etre refuse");
    CHECK(gvt_load_index(&g, index, g.n_tiles) == 0, "index invalide");
    if (fails) return 1;

    /* Compare explicitement les deux modes de recherche : index RAM et index lu dans le fichier. */
    {
        const uint32_t samples[] = { 0, g.n_tiles / 2, g.n_tiles - 1 };
        gvt_entry_t expected, actual;
        for (unsigned s = 0; s < sizeof samples / sizeof samples[0]; s++) {
            const gvt_entry_t *ram = &index[samples[s]];
            int found = gvt_find_entry(&g, (int)(ram->key >> 32),
                                       (uint32_t)((ram->key >> 16) & 0xFFFFu),
                                       (uint32_t)(ram->key & 0xFFFFu), &expected);
            CHECK(found == 1, "recherche explicite avec index RAM (entree %u)", samples[s]);
            if (found == 1)
                CHECK(expected.key == ram->key && expected.offset == ram->offset && expected.size == ram->size,
                      "resultat RAM different de l'index (entree %u)", samples[s]);
        }

        /* Desactive l'index RAM : les memes recherches doivent lire l'index sur le fichier. */
        g.index = NULL;
        for (unsigned s = 0; s < sizeof samples / sizeof samples[0]; s++) {
            const gvt_entry_t *ram = &index[samples[s]];
            int found = gvt_find_entry(&g, (int)(ram->key >> 32),
                                       (uint32_t)((ram->key >> 16) & 0xFFFFu),
                                       (uint32_t)(ram->key & 0xFFFFu), &actual);
            CHECK(found == 1, "recherche fichier : entree %u introuvable (code %d)", samples[s], found);
            if (found == 1)
                CHECK(actual.key == ram->key && actual.offset == ram->offset && actual.size == ram->size,
                      "resultat fichier different de l'index RAM (entree %u)", samples[s]);
        }
        CHECK(gvt_find_entry(&g, g.zmin, 65535u, 65535u, &actual) == 0,
              "une tuile absente doit etre signalee en mode fichier");
        CHECK(gvt_find_entry(&g, 3, 4, 4, &actual) == 0,
              "un zoom hors plage doit etre absent en mode fichier");

        /* Une seconde recherche dans le meme bloc doit reutiliser les 512 octets deja lus. */
        {
            uint32_t hits_before;
            const gvt_entry_t *ram = &index[g.n_tiles / 2];
            int z = (int)(ram->key >> 32);
            uint32_t x = (uint32_t)((ram->key >> 16) & 0xFFFFu);
            uint32_t y = (uint32_t)(ram->key & 0xFFFFu);
            CHECK(gvt_find_entry(&g, z, x, y, &actual) == 1,
                  "premiere recherche repetee en mode cache");
            hits_before = g.index_cache_hits;
            CHECK(gvt_find_entry(&g, z, x, y, &actual) == 1,
                  "seconde recherche repetee en mode cache");
            CHECK(g.index_cache_hits > hits_before,
                  "le cache d'index n'a enregistre aucun hit");
        }
        CHECK(g.index_cache_loads > 0, "aucun bloc d'index n'a ete charge");
        g.index = index;
    }

    for (uint32_t i = 0; i < g.n_tiles; i++) {
        const gvt_entry_t *e = &index[i];
        gvt_tile_it_t it;
        uint8_t cls;
        uint16_t n;
        int r, size;
        CHECK((uint64_t)e->offset + e->size <= (uint64_t)file_size, "tuile %u hors du fichier", i);
        size = gvt_read_tile(&g, e, blob, sizeof blob);
        CHECK(size == (int)e->size, "lecture tuile %u", i);
        if (gvt_tile_begin(&it, blob, e->size) != 0) { CHECK(0, "debut tuile %u", i); continue; }
        while ((r = gvt_tile_next(&it, &cls, &n, xy, GVT_MAX_POINTS)) == 1) {
            lines++;
            points += n;
            class_count[cls]++;
            for (int k = 0; k < n; k++)
                CHECK(xy[2 * k] >= -GVT_BUFFER - 1 && xy[2 * k] <= GVT_EXTENT + GVT_BUFFER + 1 &&
                      xy[2 * k + 1] >= -GVT_BUFFER - 1 && xy[2 * k + 1] <= GVT_EXTENT + GVT_BUFFER + 1,
                      "point hors de la tuile %u", i);
        }
        CHECK(r == 0, "tuile %u corrompue (code %d)", i, r);
        {   /* les surfaces : memes controles, avec les anneaux */
            gvt_tile_it_t ia;
            uint16_t nr;
            gvt_tile_begin(&ia, blob, e->size);
            while ((r = gvt_tile_next_area(&ia, &cls, &nr, ring_len, axy, GVT_MAX_AREA_POINTS)) == 1) {
                unsigned total = 0;
                areas++;
                class_count[cls]++;
                CHECK(cls >= GVT_AREA_URBAN && nr >= 1 && nr <= GVT_MAX_RINGS, "surface : classe %u, %u anneaux", cls, nr);
                for (unsigned k = 0; k < nr; k++) { CHECK(ring_len[k] >= 3, "anneau de %u points", ring_len[k]); total += ring_len[k]; }
                CHECK(total <= GVT_MAX_AREA_POINTS, "surface de %u points", total);
                for (unsigned k = 0; k < total; k++)
                    CHECK(axy[2 * k] >= -GVT_BUFFER - 1 && axy[2 * k] <= GVT_EXTENT + GVT_BUFFER + 1 &&
                          axy[2 * k + 1] >= -GVT_BUFFER - 1 && axy[2 * k + 1] <= GVT_EXTENT + GVT_BUFFER + 1,
                          "point de surface hors de la tuile %u", i);
            }
            CHECK(r == 0, "surfaces de la tuile %u corrompues (code %d)", i, r);
        }
    }
    printf("  %u tuiles, %lu lignes, %lu points, %lu surfaces\n", g.n_tiles, lines, points, areas);
    for (int c = 1; c < GVT_CLASS_COUNT; c++) CHECK(class_count[c] > 0, "classe %d absente", c);

    /* le sommet connu doit se retrouver a moins de 10 unites d'une ligne de classe 1, a chaque zoom */
    for (int z = g.zmin; z <= g.zmax; z++) {
        uint32_t tx, ty;
        float fx, fy;
        const gvt_entry_t *e;
        gvt_tile_it_t it;
        uint8_t cls;
        uint16_t n;
        double best = 1e9;
        gvt_tile_of(z, lat, lon, &tx, &ty, &fx, &fy);
        e = gvt_find(&g, z, tx, ty);
        CHECK(e != NULL, "zoom %d : tuile (%u,%u) introuvable", z, tx, ty);
        if (!e) continue;
        gvt_read_tile(&g, e, blob, sizeof blob);
        gvt_tile_begin(&it, blob, e->size);
        while (gvt_tile_next(&it, &cls, &n, xy, GVT_MAX_POINTS) == 1) {
            if (cls != GVT_ROAD_MAJOR) continue;
            for (int k = 0; k + 1 < n; k++) {
                double d = seg_dist(fx, fy, xy[2 * k], xy[2 * k + 1], xy[2 * k + 2], xy[2 * k + 3]);
                if (d < best) best = d;
            }
        }
        CHECK(best <= 10.0, "zoom %d : distance %.1f unites (max 10)", z, best);
    }

    CHECK(gvt_find(&g, 15, 0, 0) == NULL, "une tuile absente doit retourner NULL");
    CHECK(gvt_find(&g, 3, 4, 4) == NULL, "un zoom hors plage doit retourner NULL");

    /* une tuile tronquee a chaque longueur ne doit jamais faire planter le decodeur */
    {
        const gvt_entry_t *e = &index[g.n_tiles / 2];
        for (uint32_t i = 0; i < g.n_tiles; i++) {          /* de preference une tuile qui a des lignes de plus de 2 points et des surfaces */
            gvt_tile_it_t a, b;
            uint8_t c1;
            uint16_t n1, nr1;
            int has_line = 0, has_area = 0;
            if (gvt_read_tile(&g, &index[i], blob, sizeof blob) < 0) continue;
            gvt_tile_begin(&a, blob, index[i].size);
            has_line = gvt_tile_next(&a, &c1, &n1, xy, GVT_MAX_POINTS) == 1 && n1 >= 3;
            gvt_tile_begin(&b, blob, index[i].size);
            has_area = gvt_tile_next_area(&b, &c1, &nr1, ring_len, axy, GVT_MAX_AREA_POINTS) == 1;
            if (has_line && has_area) { e = &index[i]; break; }
        }
        int size = gvt_read_tile(&g, e, blob, sizeof blob), bad = 0;
        for (int cut = 0; cut < size; cut++) {
            gvt_tile_it_t it;
            uint8_t cls;
            uint16_t n;
            int r = gvt_tile_begin(&it, blob, (uint32_t)cut);
            while (r == 0 && (r = gvt_tile_next(&it, &cls, &n, xy, GVT_MAX_POINTS)) == 1) r = 0;
            if (r == 0 && cut < size) bad++;   /* fin normale avant la vraie fin : acceptable seulement si tout est lu */
            {   /* idem pour la lecture des surfaces */
                gvt_tile_it_t ia;
                uint16_t nr;
                int ra = gvt_tile_begin(&ia, blob, (uint32_t)cut);
                while (ra == 0 && (ra = gvt_tile_next_area(&ia, &cls, &nr, ring_len, axy, GVT_MAX_AREA_POINTS)) == 1) ra = 0;
                if (ra == 0 && cut < size) bad++;
            }
        }
        /* seule la troncature qui tombe exactement sur une frontiere de ligne peut donner une fin "propre" ;
           le compteur de lignes en tete de tuile la rend impossible */
        CHECK(bad == 0, "%d troncatures ont ete prises pour une tuile valide", bad);
        /* un tampon de points trop petit doit etre signale, sans consommer la ligne */
        {
            gvt_tile_it_t it;
            uint8_t cls;
            uint16_t n;
            int16_t tiny[4];
            gvt_tile_begin(&it, blob, (uint32_t)size);
            CHECK(gvt_tile_next(&it, &cls, &n, tiny, 2) == -2, "tampon trop petit non signale");
            CHECK(gvt_tile_next(&it, &cls, &n, xy, GVT_MAX_POINTS) == 1, "ligne perdue apres -2");
        }
    }

    free(index);
    fclose(f);
    if (fails) { printf("%d ECHEC(S)\n", fails); return 1; }
    printf("OK carte GVT1 : en-tete, index, %lu lignes decodees, recherche par position, troncatures\n", lines);
    return 0;
}
