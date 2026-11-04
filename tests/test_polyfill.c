/* Test de core/polyfill : comparaison pixel par pixel avec un test point-dans-polygone pair-impair independant,
 * cas exacts (rectangle, trou), debordements du tampon et cas degeneres. */
#include "polyfill.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  ECHEC : "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

#define W 120
#define H 90
#define PAD 16                          /* marge de garde autour du tampon : ne doit jamais etre touchee */
#define STRIDE (W + 2 * PAD)
#define ROWS (H + 2 * PAD)

static uint32_t rng = 12345;
static float frand(float a, float b) { rng = rng * 1664525u + 1013904223u; return a + (b - a) * ((rng >> 8) / 16777216.0f); }

static int inside(const float *xy, const uint16_t *len, int n, float px, float py)
{
    int c = 0;
    const float *p = xy;
    for (int r = 0; r < n; r++) {
        for (int i = 0; i < len[r]; i++) {
            float x1 = p[2 * i], y1 = p[2 * i + 1], x2 = p[2 * ((i + 1) % len[r])], y2 = p[2 * ((i + 1) % len[r]) + 1];
            if ((y1 <= py) != (y2 <= py) && px < x1 + (py - y1) * (x2 - x1) / (y2 - y1)) c = !c;
        }
        p += 2 * len[r];
    }
    return c;
}

/* distance du point au contour (tous anneaux) : sert a tolerer les egalites d'arrondi flottant exactement sur une arete */
static float edge_distance(const float *xy, const uint16_t *len, int n, float px, float py)
{
    float best = 1e30f;
    const float *p = xy;
    for (int r = 0; r < n; r++) {
        for (int i = 0; i < len[r]; i++) {
            float ax = p[2 * i], ay = p[2 * i + 1], bx = p[2 * ((i + 1) % len[r])], by = p[2 * ((i + 1) % len[r]) + 1];
            float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
            float t = l2 == 0 ? 0 : ((px - ax) * dx + (py - ay) * dy) / l2;
            float d;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            d = hypotf(px - ax - t * dx, py - ay - t * dy);
            if (d < best) best = d;
        }
        p += 2 * len[r];
    }
    return best;
}

static uint16_t buf[ROWS * STRIDE];
static void clear(void) { memset(buf, 0, sizeof buf); }
static uint16_t *origin(void) { return buf + PAD * STRIDE + PAD; }
static int guard_clean(void)
{
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < STRIDE; x++)
            if ((y < PAD || y >= PAD + H || x < PAD || x >= PAD + W) && buf[y * STRIDE + x]) return 0;
    return 1;
}
static int count(void)
{
    int c = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) c += origin()[y * STRIDE + x] != 0;
    return c;
}

int main(void)
{
    /* 1. polygones aleatoires (jusqu'a 3 anneaux, sommets parfois hors du tampon, aretes qui se croisent) */
    {
        long mismatches = 0, ties = 0, pixels = 0;
        for (int t = 0; t < 400; t++) {
            static float xy[3 * 40 * 2];
            uint16_t len[3];
            int nr = 1 + (int)frand(0, 3), total = 0;
            for (int r = 0; r < nr; r++) {
                len[r] = (uint16_t)(3 + (int)frand(0, 38));
                for (int i = 0; i < len[r]; i++) { xy[2 * (total + i)] = frand(-40, W + 40); xy[2 * (total + i) + 1] = frand(-40, H + 40); }
                total += len[r];
            }
            clear();
            CHECK(polyfill_rgb565(origin(), W, H, STRIDE, xy, len, nr, 0xABCD) == 0, "retour non nul");
            CHECK(guard_clean(), "ecriture hors du tampon (polygone %d)", t);
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    int want = inside(xy, len, nr, x + 0.5f, y + 0.5f), got = origin()[y * STRIDE + x] == 0xABCD;
                    pixels++;
                    if (want != got) {
                        if (edge_distance(xy, len, nr, x + 0.5f, y + 0.5f) < 0.01f) ties++;     /* centre a moins de 0,01 pixel d'une arete */
                        else mismatches++;
                    }
                }
        }
        CHECK(mismatches == 0, "%ld pixels differents sur %ld", mismatches, pixels);
        CHECK(ties < 20, "trop d'egalites d'arrondi (%ld)", ties);
        printf("  400 polygones aleatoires : %ld pixels compares, %ld differents, %ld egalites d'arrondi a moins de 0,01 pixel d'une arete\n", pixels, mismatches, ties);
    }

    /* 2. rectangle exact : [10,30[ x [20,50[ en pixels */
    {
        float xy[] = {10, 20, 30, 20, 30, 50, 10, 50};
        uint16_t len[] = {4};
        clear();
        polyfill_rgb565(origin(), W, H, STRIDE, xy, len, 1, 0x1234);
        CHECK(count() == 20 * 30, "rectangle : %d pixels au lieu de 600", count());
        CHECK(origin()[20 * STRIDE + 10] == 0x1234 && origin()[49 * STRIDE + 29] == 0x1234 &&
              origin()[19 * STRIDE + 10] == 0 && origin()[20 * STRIDE + 30] == 0, "rectangle : bords");
    }

    /* 3. trou : carre 0..100 avec un carre 30..60 (meme sens, pair-impair) */
    {
        float xy[] = {0, 0, 100, 0, 100, 80, 0, 80, 30, 20, 60, 20, 60, 50, 30, 50};
        uint16_t len[] = {4, 4};
        clear();
        polyfill_rgb565(origin(), W, H, STRIDE, xy, len, 2, 0x4321);
        CHECK(count() == 100 * 80 - 30 * 30, "trou : %d pixels au lieu de %d", count(), 100 * 80 - 900);
        CHECK(origin()[30 * STRIDE + 40] == 0, "trou : le centre doit rester vide");
    }

    /* 4. entierement hors du tampon, ou le recouvrant en entier */
    {
        float out[] = {-50, -50, -10, -50, -10, -10, -50, -10};
        float all[] = {-30, -30, W + 30, -30, W + 30, H + 30, -30, H + 30};
        uint16_t len[] = {4};
        clear();
        polyfill_rgb565(origin(), W, H, STRIDE, out, len, 1, 0x1111);
        CHECK(count() == 0 && guard_clean(), "hors tampon : rien ne doit etre dessine");
        polyfill_rgb565(origin(), W, H, STRIDE, all, len, 1, 0x2222);
        CHECK(count() == W * H && guard_clean(), "recouvrant : tout le tampon, rien autour (%d)", count());
    }

    /* 5. cas degeneres */
    {
        float line[] = {10, 10, 50, 10, 90, 10};            /* trois points alignes sur une horizontale */
        float two[] = {5, 5, 50, 50};
        float tri[] = {10, 10, 10, 10, 10, 10};
        uint16_t l3[] = {3}, l2[] = {2};
        clear();
        CHECK(polyfill_rgb565(origin(), W, H, STRIDE, line, l3, 1, 0x1) == 0 && count() == 0, "degenere : points alignes");
        CHECK(polyfill_rgb565(origin(), W, H, STRIDE, two, l2, 1, 0x1) == 0 && count() == 0, "degenere : deux points");
        CHECK(polyfill_rgb565(origin(), W, H, STRIDE, tri, l3, 1, 0x1) == 0 && count() == 0, "degenere : points confondus");
        CHECK(polyfill_rgb565(origin(), W, H, STRIDE, tri, l3, 0, 0x1) == 0 && count() == 0, "degenere : aucun anneau");
    }

    /* 6. surface a tres nombreuses aretes : disque de 4000 points, puis refus au-dela de la limite */
    {
        static float xy[2 * 4000], big[2 * (POLYFILL_MAX_EDGES + 10)];
        uint16_t len[1] = {4000}, lbig[1] = {POLYFILL_MAX_EDGES + 10};
        clear();
        for (int i = 0; i < 4000; i++) { xy[2 * i] = 60 + 40 * cosf(6.2831853f * i / 4000); xy[2 * i + 1] = 45 + 40 * sinf(6.2831853f * i / 4000); }
        CHECK(polyfill_rgb565(origin(), W, H, STRIDE, xy, len, 1, 0x7777) == 0, "disque de 4000 points");
        CHECK(abs(count() - (int)(3.14159265f * 40 * 40)) < 40, "disque : %d pixels (attendu environ 5027)", count());
        for (int i = 0; i < POLYFILL_MAX_EDGES + 10; i++) { big[2 * i] = 60 + 30 * cosf(6.2831853f * i / (POLYFILL_MAX_EDGES + 10)); big[2 * i + 1] = 45 + 30 * sinf(6.2831853f * i / (POLYFILL_MAX_EDGES + 10)); }
        clear();
        CHECK(polyfill_rgb565(origin(), W, H, STRIDE, big, lbig, 1, 0x7777) == -1 && count() == 0, "trop d'aretes : refuse sans rien dessiner");
    }

    if (fails) { printf("%d ECHEC(S)\n", fails); return 1; }
    printf("OK remplissage : pixels identiques a la reference, cas exacts, debordements, degeneres\n");
    return 0;
}
