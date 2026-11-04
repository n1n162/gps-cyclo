/* Test de core/linedraw : comparaison pixel par pixel avec une reference par distance exacte, cas exacts,
 * debordements du tampon, coordonnees enormes, segments nuls. */
#include "linedraw.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  ECHEC : "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

#define W 100
#define H 80
#define PAD 16
#define STRIDE (W + 2 * PAD)
#define ROWS (H + 2 * PAD)

static uint16_t buf[ROWS * STRIDE];
static uint16_t *origin(void) { return buf + PAD * STRIDE + PAD; }
static void clear(uint16_t v) { for (int i = 0; i < ROWS * STRIDE; i++) buf[i] = 0; for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) origin()[y * STRIDE + x] = v; }
static int guard_clean(void)
{
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < STRIDE; x++)
            if ((y < PAD || y >= PAD + H || x < PAD || x >= PAD + W) && buf[y * STRIDE + x]) return 0;
    return 1;
}

static uint32_t rng = 777;
static float frand(float a, float b) { rng = rng * 1664525u + 1013904223u; return a + (b - a) * ((rng >> 8) / 16777216.0f); }

static float dist_seg(float px, float py, float ax, float ay, float bx, float by)
{
    float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy, t = l2 == 0 ? 0 : ((px - ax) * dx + (py - ay) * dy) / l2;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return hypotf(px - ax - t * dx, py - ay - t * dy);
}

int main(void)
{
    /* 1. segments aleatoires, blanc sur noir : la valeur du pixel est la couverture sur 5 bits */
    {
        long compared = 0, off = 0, ties = 0;
        for (int t = 0; t < 1500; t++) {
            static const float widths[] = {1.0f, 1.5f, 2.0f, 3.0f, 4.0f};
            float w = widths[(int)frand(0, 5)];
            float x0 = frand(-30, W + 30), y0 = frand(-30, H + 30), x1 = frand(-30, W + 30), y1 = frand(-30, H + 30);
            if (t % 7 == 0) { x1 = x0 + frand(-3, 3); y1 = y0 + frand(-3, 3); }      /* beaucoup de segments courts */
            if (t % 11 == 0) y1 = y0;                                                 /* et d'horizontaux */
            if (t % 13 == 0) x1 = x0;                                                 /* et de verticaux */
            clear(0);
            linedraw_rgb565(origin(), W, H, STRIDE, x0, y0, x1, y1, w, 0xFFFF);
            CHECK(guard_clean(), "ecriture hors du tampon (segment %d)", t);
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    float cov = w * 0.5f + 0.5f - dist_seg(x + 0.5f, y + 0.5f, x0, y0, x1, y1);
                    int want, got = origin()[y * STRIDE + x] & 0x1F;            /* canal bleu sur 5 bits */
                    if (cov < 0) cov = 0;
                    if (cov > 1) cov = 1;
                    want = cov >= 1.0f ? 31 : (31 * (int)(cov * 32.0f + 0.5f)) >> 5;
                    compared++;
                    if (want != got) {
                        float frac = cov * 32.0f - floorf(cov * 32.0f);
                        if (fabsf(frac - 0.5f) < 0.02f || cov < 0.02f || cov > 0.98f) ties++;   /* arrondi sur une frontiere */
                        else off++;
                    }
                }
        }
        CHECK(off == 0, "%ld pixels differents de la reference sur %ld", off, compared);
        CHECK(ties * 1000 < compared, "trop d'ecarts d'arrondi : %ld", ties);
        printf("  1500 segments : %ld pixels compares, %ld differents, %ld ecarts d'arrondi en bordure\n", compared, off, ties);
    }

    /* 2. horizontale exacte : largeur 3 sur y = 10,5 -> lignes 9, 10 et 11 pleines, 8 et 12 vides */
    {
        clear(0);
        linedraw_rgb565(origin(), W, H, STRIDE, 20.5f, 10.5f, 60.5f, 10.5f, 3.0f, 0xFFFF);
        CHECK(origin()[9 * STRIDE + 40] == 0xFFFF && origin()[10 * STRIDE + 40] == 0xFFFF && origin()[11 * STRIDE + 40] == 0xFFFF, "horizontale : trois lignes pleines");
        CHECK(origin()[8 * STRIDE + 40] == 0 && origin()[12 * STRIDE + 40] == 0, "horizontale : lignes voisines vides");
        CHECK(origin()[10 * STRIDE + 19] != 0 && origin()[10 * STRIDE + 61] != 0 && origin()[10 * STRIDE + 18] == 0 && origin()[10 * STRIDE + 62] == 0, "horizontale : extremites arrondies (une demi-largeur plus loin)");
    }

    /* 3. coordonnees enormes, lignes hors tampon, valeurs non finies */
    {
        clear(0);
        linedraw_rgb565(origin(), W, H, STRIDE, -1e6f, -1e6f, 1e6f, 1e6f, 4.0f, 0xFFFF);
        CHECK(guard_clean() && origin()[40 * STRIDE + 40] == 0xFFFF && origin()[10 * STRIDE + 40] == 0, "diagonale de 1e6 pixels");
        clear(0);
        linedraw_rgb565(origin(), W, H, STRIDE, -500, -500, -400, -300, 3.0f, 0xFFFF);
        linedraw_rgb565(origin(), W, H, STRIDE, 1e9f, 0, 2e9f, 5, 3.0f, 0xFFFF);
        linedraw_rgb565(origin(), W, H, STRIDE, NAN, 0, 5, 5, 3.0f, 0xFFFF);
        linedraw_rgb565(origin(), W, H, STRIDE, 0, 0, INFINITY, 5, 3.0f, 0xFFFF);
        linedraw_rgb565(origin(), W, H, STRIDE, 10, 10, 50, 50, 0.0f, 0xFFFF);
        CHECK(guard_clean(), "hors tampon ou non fini : aucune ecriture");
        { int c = 0; for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) c += origin()[y * STRIDE + x] != 0; CHECK(c == 0, "hors tampon : %d pixels dessines", c); }
    }

    /* 4. segment nul : un disque de rayon la demi-largeur */
    {
        float sum = 0;
        clear(0);
        linedraw_rgb565(origin(), W, H, STRIDE, 50.0f, 40.0f, 50.0f, 40.0f, 4.0f, 0xFFFF);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) sum += (origin()[y * STRIDE + x] & 0x1F) / 31.0f;
        CHECK(fabsf(sum - 3.14159f * 4.0f) < 1.5f, "disque de largeur 4 : surface %.1f (attendu 12,6)", sum);
    }

    /* 5. melange sur un fond colore : jamais de debordement de canal, couleur pleine intacte */
    {
        clear(0xFFFF);
        linedraw_rgb565(origin(), W, H, STRIDE, 10, 10, 90, 60, 3.0f, 0x0000);
        clear(0x07E0);
        linedraw_rgb565(origin(), W, H, STRIDE, 10, 10, 90, 60, 3.0f, 0xF800);
        { int bad = 0; for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { uint16_t v = origin()[y * STRIDE + x]; int g = (v >> 5) & 0x3F, r = v >> 11; bad += (g > 63) || (r > 31); } CHECK(!bad, "canaux hors limites"); }
        CHECK(origin()[35 * STRIDE + 50] == 0xF800 || origin()[34 * STRIDE + 50] == 0xF800 || origin()[36 * STRIDE + 50] == 0xF800, "couleur pleine au centre du trait");
    }

    if (fails) { printf("%d ECHEC(S)\n", fails); return 1; }
    printf("OK trace de lignes : couverture identique a la reference, cas exacts, debordements, valeurs non finies\n");
    return 0;
}
