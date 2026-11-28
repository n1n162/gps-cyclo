/* Test de core/mapview : la carte doit tomber exactement sous la trace, les tuiles choisies doivent couvrir
 * l'ecran (rotation comprise), le cache doit se comporter comme prevu.
 * Usage : test_mapview carte.gvt */
#include "mapview.h"
#include "route.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  ECHEC : "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)
#define DEG (3.14159265358979323846 / 180.0)

static int fail_reads;
static int read_file(void *ctx, uint32_t off, void *dst, uint32_t len)
{
    FILE *f = ctx;
    if (fail_reads) return -1;
    if (fseek(f, (long)off, SEEK_SET) != 0) return -1;
    return fread(dst, 1, len, f) == len ? 0 : -1;
}

static mapview_view_t make_view(const route_t *r, float cx_m, float cy_m, float scale, double heading)
{
    mapview_view_t v = { r->lat0, r->lon0, r->cos0, cx_m, cy_m, scale, (float)cos(heading), (float)sin(heading), 160, 190, 320, 380 };
    return v;
}

/* Formule de ui/app.c (nav_project_point) : pixel d'un point (x, y) de la trace. */
static void trace_pixel(const mapview_view_t *v, float x, float y, double *sx, double *sy)
{
    float e = x - v->cx_m, n = y - v->cy_m;
    *sx = v->cx + (e * v->cs - n * v->sn) * v->scale;
    *sy = v->cy - (e * v->sn + n * v->cs) * v->scale;
}

int main(int argc, char **argv)
{
    static float X[8], Y[8], E[8];
    static uint8_t cache[MAPVIEW_SLOTS * MAPVIEW_SLOT_BYTES];
    route_t r;
    double worst = 0.0;
    int n_align = 0;

    if (argc != 2) { fprintf(stderr, "usage : %s carte.gvt\n", argv[0]); return 2; }
    route_init(&r, X, Y, E, 8, 10.0f);
    route_add_point(&r, 45.00, 5.00, 0, 0);          /* origine de la projection locale */

    /* 1. alignement : un point de la trace, converti en position dans une tuile puis en pixels, doit retomber
     *    sur le pixel de la trace. Centre loin de l'origine (cos(lat) different), plusieurs zooms et caps. */
    {
        static const float centres[][2] = {{0, 0}, {60000, 90000}, {-30000, 120000}};
        static const float scales[] = {0.05f, 0.12f, 0.3f, 0.6f};
        static const double caps[] = {0.0, 0.65, 3.5};
        static const float offs[] = {-1500, -400, 0, 700, 1500};
        for (unsigned ci = 0; ci < 3; ci++)
            for (unsigned si = 0; si < 4; si++)
                for (unsigned hi = 0; hi < 3; hi++) {
                    mapview_view_t v = make_view(&r, centres[ci][0], centres[ci][1], scales[si], caps[hi]);
                    int z = mapview_zoom(NULL, &v);
                    for (unsigned a = 0; a < 5; a++)
                        for (unsigned b = 0; b < 5; b++) {
                            float x = v.cx_m + offs[a], y = v.cy_m + offs[b], m[6], fx, fy;
                            uint32_t tx, ty;
                            double lat, lon, ex, ey, ax, ay;
                            route_unproject(&r, x, y, &lat, &lon);
                            trace_pixel(&v, x, y, &ex, &ey);
                            if (fabs(ex - v.cx) > 400 || fabs(ey - v.cy) > 400) continue;   /* loin hors de l'ecran */
                            gvt_tile_of(z, lat, lon, &tx, &ty, &fx, &fy);
                            mapview_xform(&v, z, tx, ty, m);
                            ax = m[0] * fx + m[1] * fy + m[2];
                            ay = m[3] * fx + m[4] * fy + m[5];
                            double d = hypot(ax - ex, ay - ey);
                            if (d > worst) worst = d;
                            n_align++;
                        }
                }
        printf("  alignement carte/trace : %d points, ecart maximal %.3f pixel\n", n_align, worst);
        CHECK(n_align > 200, "trop peu de points testes (%d)", n_align);
        CHECK(worst < 0.3, "ecart carte/trace de %.3f pixel (max 0,3)", worst);
    }

    /* 2. niveau de zoom : un cran de zoom par doublement de l'echelle, bornes du fichier respectees */
    {
        mapview_view_t v = make_view(&r, 0, 0, 0.1f, 0.0);
        int z0 = mapview_zoom(NULL, &v), z1;
        v.scale *= 2.0f;
        z1 = mapview_zoom(NULL, &v);
        CHECK(z1 == z0 + 1, "doubler l'echelle doit ajouter un niveau (%d puis %d)", z0, z1);
        gvt_t g = { .zmin = 12, .zmax = 14 };
        v.scale = 0.001f;
        CHECK(mapview_zoom(&g, &v) == 12, "zoom minimal du fichier non respecte");
        v.scale = 5.0f;
        CHECK(mapview_zoom(&g, &v) == 14, "zoom maximal du fichier non respecte");
    }

    /* 3. couverture : tout pixel de la zone tombe dans une tuile choisie, rotation comprise */
    {
        static const double caps[] = {0.0, 0.65, 2.2, 4.0};
        int worst_n = 0;
        for (unsigned hi = 0; hi < 4; hi++)
            for (float scale = 0.04f; scale < 0.7f; scale *= 1.7f) {
                mapview_view_t v = make_view(&r, 40000, 80000, scale, caps[hi]);
                int z = mapview_zoom(NULL, &v);
                mapview_tile_t t[MAPVIEW_MAX_TILES];
                int n = mapview_tiles(&v, z, t, MAPVIEW_MAX_TILES), missing = 0;
                if (n > worst_n) worst_n = n;
                for (int sx = 0; sx <= v.w; sx += 20)
                    for (int sy = 0; sy <= v.h; sy += 20) {
                        double rx = (sx - v.cx) / v.scale, ry = (v.cy - sy) / v.scale;
                        double e = rx * v.cs + ry * v.sn + v.cx_m, nn = -rx * v.sn + ry * v.cs + v.cy_m;
                        double lat = v.lat0 + nn / (DEG * MAPVIEW_EARTH_R), lon = v.lon0 + e / (DEG * MAPVIEW_EARTH_R * v.cos0);
                        uint32_t tx, ty;
                        int found = 0;
                        gvt_tile_of(z, lat, lon, &tx, &ty, NULL, NULL);
                        for (int k = 0; k < n; k++) if (t[k].x == tx && t[k].y == ty) found = 1;
                        if (!found) missing++;
                    }
                CHECK(missing == 0, "zoom %d, cap %.2f, echelle %.2f : %d pixels hors des %d tuiles choisies", z, caps[hi], scale, missing, n);
                CHECK(n >= 1 && n < MAPVIEW_MAX_TILES, "nombre de tuiles %d hors limites", n);
            }
        printf("  couverture : au plus %d tuiles pour l'ecran\n", worst_n);
    }

    /* 4. cache de tuiles sur la carte synthetique */
    {
        FILE *f = fopen(argv[1], "rb");
        gvt_t g;
        gvt_entry_t *index;
        mapview_t mv;
        uint32_t size = 0, tx, ty, probe_x, probe_y;
        const uint8_t *p;
        gvt_tile_it_t it;
        if (!f) { perror(argv[1]); return 2; }
        CHECK(gvt_open(&g, read_file, f) == 0, "en-tete");
        index = malloc(g.n_tiles * sizeof *index);
        CHECK(gvt_load_index(&g, index, g.n_tiles) == 0, "index de reference pour le test");
        if (fails) return 1;
        /* Le cache doit aussi fonctionner sans index en RAM : recherche directe dans le fichier. */
        g.index = NULL;
        mapview_init(&mv, &g, cache);

        gvt_tile_of(15, 45.20, 5.00, &probe_x, &probe_y, NULL, NULL);
        p = mapview_get(&mv, 15, probe_x, probe_y, &size);
        CHECK(p != NULL && size > 0, "tuile connue introuvable");
        CHECK(p && gvt_tile_begin(&it, p, size) == 0, "tuile lue illisible");
        CHECK(mv.loads == 1 && mv.hits == 0, "premiere lecture : %u chargements, %u succes de cache", mv.loads, mv.hits);
        p = mapview_get(&mv, 15, probe_x, probe_y, &size);
        CHECK(p != NULL && mv.loads == 1 && mv.hits == 1, "seconde lecture doit venir du cache");
        CHECK(mapview_get(&mv, 15, 0, 0, &size) == NULL && mv.errors == 0, "tuile absente : NULL sans erreur");

        /* on lit beaucoup plus de tuiles que d'emplacements : la plus recente reste en cache */
        {
            unsigned loaded = 0;
            for (uint32_t i = 0; i < g.n_tiles && loaded < 3 * MAPVIEW_SLOTS; i++) {
                int z = (int)(index[i].key >> 32);
                tx = (uint32_t)((index[i].key >> 16) & 0xFFFF);
                ty = (uint32_t)(index[i].key & 0xFFFF);
                if (mapview_get(&mv, z, tx, ty, &size)) loaded++;
            }
            CHECK(loaded == 3 * MAPVIEW_SLOTS, "chargements : %u", loaded);
            CHECK(mv.loads >= 2 * MAPVIEW_SLOTS, "evictions : %u chargements", mv.loads);
            {
                uint32_t before = mv.loads;
                mapview_get(&mv, (int)(index[loaded - 1].key >> 32), (uint32_t)((index[loaded - 1].key >> 16) & 0xFFFF),
                            (uint32_t)(index[loaded - 1].key & 0xFFFF), &size);
                CHECK(mv.loads == before, "la derniere tuile lue doit rester en cache");
            }
        }
        /* une erreur de lecture ne corrompt pas le cache et se compte */
        {
            uint32_t before = mv.errors;
            fail_reads = 1;
            CHECK(mapview_get(&mv, 15, probe_x, probe_y, &size) != NULL || mv.errors > before, "erreur de lecture non comptee");
            fail_reads = 0;
            CHECK(mapview_get(&mv, 15, probe_x, probe_y, &size) != NULL, "relecture apres erreur");
        }
        printf("  cache : %u chargements, %u succes, %u erreurs\n", mv.loads, mv.hits, mv.errors);
        free(index);
        fclose(f);
    }

    if (fails) { printf("%d ECHEC(S)\n", fails); return 1; }
    printf("OK vue carte : alignement avec la trace, zoom, couverture des tuiles, cache\n");
    return 0;
}
