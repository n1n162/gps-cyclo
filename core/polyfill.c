#include "polyfill.h"
#include <math.h>
#include <stdlib.h>

typedef struct { float y0, y1, x0, dxdy; } edge_t;

static edge_t edges[POLYFILL_MAX_EDGES];
static int16_t active[POLYFILL_MAX_EDGES];
static float xs[POLYFILL_MAX_EDGES];

static int by_y0(const void *a, const void *b)
{
    float ya = ((const edge_t *)a)->y0, yb = ((const edge_t *)b)->y0;
    return (ya > yb) - (ya < yb);
}

int polyfill_rgb565(uint16_t *buf, int w, int h, int stride_px, const float *xy, const uint16_t *ring_len, int n_rings, uint16_t color)
{
    int n = 0, first, last, na = 0, ne = 0;
    float ymin = 1e30f, ymax = -1e30f;
    const float *p = xy;

    for (int r = 0; r < n_rings; r++) {
        int len = ring_len[r];
        if (len >= 3) {
            for (int i = 0; i < len; i++) {
                float xa = p[2 * i], ya = p[2 * i + 1];
                float xb = p[2 * ((i + 1) % len)], yb = p[2 * ((i + 1) % len) + 1];
                if (ya == yb) continue;                            /* horizontale : sans effet */
                if (n >= POLYFILL_MAX_EDGES) return -1;
                if (ya < yb) { edges[n].y0 = ya; edges[n].y1 = yb; edges[n].x0 = xa; edges[n].dxdy = (xb - xa) / (yb - ya); }
                else         { edges[n].y0 = yb; edges[n].y1 = ya; edges[n].x0 = xb; edges[n].dxdy = (xa - xb) / (ya - yb); }
                if (edges[n].y0 < ymin) ymin = edges[n].y0;
                if (edges[n].y1 > ymax) ymax = edges[n].y1;
                n++;
            }
        }
        p += 2 * len;
    }
    if (n == 0) return 0;
    qsort(edges, (size_t)n, sizeof edges[0], by_y0);
    first = (int)ceilf(ymin - 0.5f);
    last = (int)ceilf(ymax - 0.5f) - 1;                            /* derniere ligne dont le centre est avant ymax */
    if (first < 0) first = 0;
    if (last > h - 1) last = h - 1;
    for (int y = first; y <= last; y++) {
        float yc = (float)y + 0.5f;
        int k = 0;
        while (ne < n && edges[ne].y0 <= yc) { active[na++] = (int16_t)ne; ne++; }
        for (int i = 0; i < na; i++)                               /* retire les aretes terminees */
            if (edges[active[i]].y1 > yc) active[k++] = active[i];
        na = k;
        for (int i = 0; i < na; i++) {                             /* abscisses de croisement, triees (insertion) */
            const edge_t *e = &edges[active[i]];
            float x = e->x0 + (yc - e->y0) * e->dxdy;
            int j = i;
            while (j > 0 && xs[j - 1] > x) { xs[j] = xs[j - 1]; j--; }
            xs[j] = x;
        }
        for (int i = 0; i + 1 < na; i += 2) {
            int xa = (int)ceilf(xs[i] - 0.5f), xb = (int)ceilf(xs[i + 1] - 0.5f);   /* pixels [xa, xb) */
            uint16_t *row = buf + (size_t)y * (size_t)stride_px;
            if (xa < 0) xa = 0;
            if (xb > w) xb = w;
            for (int x = xa; x < xb; x++) row[x] = color;
        }
    }
    return 0;
}
