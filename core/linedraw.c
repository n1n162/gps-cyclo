#include "linedraw.h"
#include <math.h>
#include <stddef.h>

typedef struct { float x0, y0, x1, y1, tx, ty, nx, ny, len; } seg_t;

/* Intervalle [lo, hi] des abscisses de centres de pixels de la ligne yc qui sont a moins de r du segment (stade convexe :
 * deux disques et un rectangle). Retourne 0 s'il est vide. */
static int row_interval(const seg_t *s, float yc, float r, float *lo_out, float *hi_out)
{
    float lo = 1e30f, hi = -1e30f, l = -1e30f, h = 1e30f, a, b, p, q;
    int body = 1;
    float dy = yc - s->y0, dx;
    if (fabsf(dy) <= r) { dx = sqrtf(r * r - dy * dy); lo = s->x0 - dx; hi = s->x0 + dx; }
    dy = yc - s->y1;
    if (fabsf(dy) <= r) {
        dx = sqrtf(r * r - dy * dy);
        if (s->x1 - dx < lo) lo = s->x1 - dx;
        if (s->x1 + dx > hi) hi = s->x1 + dx;
    }
    a = s->nx;                                          /* |nx (x - x0) + ny (yc - y0)| <= r */
    b = s->ny * (yc - s->y0) - s->nx * s->x0;
    if (fabsf(a) < 1e-7f) { if (fabsf(b) > r) body = 0; }
    else { p = (-r - b) / a; q = (r - b) / a; if (p > q) { float t = p; p = q; q = t; } if (p > l) l = p; if (q < h) h = q; }
    a = s->tx;                                          /* 0 <= tx (x - x0) + ty (yc - y0) <= len */
    b = s->ty * (yc - s->y0) - s->tx * s->x0;
    if (fabsf(a) < 1e-7f) { if (b < 0.0f || b > s->len) body = 0; }
    else { p = -b / a; q = (s->len - b) / a; if (p > q) { float t = p; p = q; q = t; } if (p > l) l = p; if (q < h) h = q; }
    if (body && l <= h) { if (l < lo) lo = l; if (h > hi) hi = h; }
    if (lo > hi) return 0;
    *lo_out = lo;
    *hi_out = hi;
    return 1;
}

static float seg_distance(const seg_t *s, float px, float py)
{
    float t = (px - s->x0) * s->tx + (py - s->y0) * s->ty;
    float qx, qy;
    if (t < 0.0f) t = 0.0f;
    if (t > s->len) t = s->len;
    qx = s->x0 + t * s->tx - px;
    qy = s->y0 + t * s->ty - py;
    return sqrtf(qx * qx + qy * qy);
}

/* Melange de color sur *p avec alpha de 0 a 32, par l'astuce des 5-6-5 etales sur 32 bits. */
static inline void blend(uint16_t *p, uint32_t color_spread, int a32)
{
    uint32_t d = *p;
    d = (d | (d << 16)) & 0x07E0F81Fu;
    d = d + ((((color_spread - d) & 0xFFFFFFFFu) * (uint32_t)a32) >> 5);
    d &= 0x07E0F81Fu;
    *p = (uint16_t)((d | (d >> 16)) & 0xFFFFu);
}

void linedraw_rgb565(uint16_t *buf, int w, int h, int stride_px, float x0, float y0, float x1, float y1, float width, uint16_t color)
{
    seg_t s;
    float hw = width * 0.5f, ro = hw + 0.5f, ri = hw - 0.5f, dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
    float ymin = y0 < y1 ? y0 : y1, ymax = y0 < y1 ? y1 : y0;
    uint32_t cs = color;
    int ya, yb;
    if (!(width > 0.0f) || !isfinite(x0) || !isfinite(y0) || !isfinite(x1) || !isfinite(y1)) return;
    cs = (cs | (cs << 16)) & 0x07E0F81Fu;
    s.x0 = x0; s.y0 = y0; s.x1 = x1; s.y1 = y1; s.len = len;
    if (len > 1e-6f) { s.tx = dx / len; s.ty = dy / len; } else { s.tx = 1.0f; s.ty = 0.0f; s.len = 0.0f; }
    s.nx = -s.ty;
    s.ny = s.tx;
    ya = (int)floorf(ymin - ro - 1.0f);
    yb = (int)ceilf(ymax + ro + 1.0f);
    if (ya < 0) ya = 0;
    if (yb > h - 1) yb = h - 1;
    for (int y = ya; y <= yb; y++) {
        float yc = (float)y + 0.5f, lo, hi, ilo = 1.0f, ihi = 0.0f;
        int xa, xb, ia = 1, ib = 0;
        uint16_t *row = buf + (size_t)y * (size_t)stride_px;
        if (!row_interval(&s, yc, ro, &lo, &hi)) continue;
        if (hi < -1.0f || lo > (float)w + 1.0f) continue;
        xa = (int)ceilf(lo - 0.5f) - 1;                        /* une marge d'un pixel : la distance exacte tranche */
        xb = (int)floorf(hi - 0.5f) + 1;
        if (xa < 0) xa = 0;
        if (xb > w - 1) xb = w - 1;
        if (ri > 0.0f && row_interval(&s, yc, ri, &ilo, &ihi)) {
            ia = (int)ceilf(ilo - 0.5f);                       /* pixels entierement couverts */
            ib = (int)floorf(ihi - 0.5f);
        }
        for (int x = xa; x <= xb; x++) {
            if (x >= ia && x <= ib) { row[x] = color; continue; }
            {
                float cov = ro - seg_distance(&s, (float)x + 0.5f, yc);
                int a32;
                if (cov <= 0.0f) continue;
                if (cov >= 1.0f) { row[x] = color; continue; }
                a32 = (int)(cov * 32.0f + 0.5f);
                if (a32 >= 32) row[x] = color; else if (a32 > 0) blend(&row[x], cs, a32);
            }
        }
    }
}
