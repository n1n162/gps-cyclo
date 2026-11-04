#include "mapview.h"
#include <math.h>
#include <string.h>

#define DEG (3.14159265358979323846 / 180.0)
#define M_PER_DEG (DEG * MAPVIEW_EARTH_R)

void mapview_init(mapview_t *mv, gvt_t *g, uint8_t *mem)
{
    memset(mv, 0, sizeof *mv);
    mv->g = g;
    mv->mem = mem;
}

/* Pixel de la zone pour un point (est, nord) en metres dans la projection de la trace. */
static void to_screen(const mapview_view_t *v, double east, double north, double *sx, double *sy)
{
    double e = east - v->cx_m, n = north - v->cy_m;
    double re = e * v->cs - n * v->sn;
    double rn = e * v->sn + n * v->cs;
    *sx = v->cx + re * v->scale;
    *sy = v->cy - rn * v->scale;
}

/* Position monde (en unites de tuile) d'un point lat/lon au zoom z. */
static void to_world(int z, double lat, double lon, double *wx, double *wy)
{
    double n = (double)(1u << z) * GVT_EXTENT;
    double s;
    if (lat > 85.0511287798) lat = 85.0511287798;
    if (lat < -85.0511287798) lat = -85.0511287798;
    s = sin(lat * DEG);
    *wx = (lon + 180.0) / 360.0 * n;
    *wy = (0.5 - log((1.0 + s) / (1.0 - s)) / (4.0 * M_PI)) * n;
}

static void from_world(int z, double wx, double wy, double *lat, double *lon)
{
    double n = (double)(1u << z) * GVT_EXTENT;
    *lon = wx / n * 360.0 - 180.0;
    *lat = atan(sinh(M_PI * (1.0 - 2.0 * wy / n))) / DEG;
}

int mapview_zoom(const gvt_t *g, const mapview_view_t *v)
{
    double lat = v->lat0 + (double)v->cy_m / M_PER_DEG;
    double circ = 2.0 * M_PI * MAPVIEW_EARTH_R * cos(lat * DEG);
    int z = (int)lround(log2(circ * (double)v->scale / 256.0));
    if (g) {
        if (z < g->zmin) z = g->zmin;
        if (z > g->zmax) z = g->zmax;
    }
    return z < 0 ? 0 : (z > 16 ? 16 : z);
}

int mapview_tiles(const mapview_view_t *v, int z, mapview_tile_t *out, int max)
{
    static const int corner[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    double minx = 1e300, maxx = -1e300, miny = 1e300, maxy = -1e300;
    long last = (1L << z) - 1, x0, x1, y0, y1;
    int n = 0;
    for (int i = 0; i < 4; i++) {
        double sx = corner[i][0] * (double)v->w, sy = corner[i][1] * (double)v->h;
        double rx = (sx - v->cx) / v->scale, ry = (v->cy - sy) / v->scale;     /* repere tourne, en metres */
        double e = rx * v->cs + ry * v->sn + v->cx_m;
        double nn = -rx * v->sn + ry * v->cs + v->cy_m;
        double lat = v->lat0 + nn / M_PER_DEG, lon = v->lon0 + e / (M_PER_DEG * v->cos0), wx, wy;
        to_world(z, lat, lon, &wx, &wy);
        if (wx < minx) minx = wx;
        if (wx > maxx) maxx = wx;
        if (wy < miny) miny = wy;
        if (wy > maxy) maxy = wy;
    }
    x0 = (long)floor(minx / GVT_EXTENT); x1 = (long)floor(maxx / GVT_EXTENT);
    y0 = (long)floor(miny / GVT_EXTENT); y1 = (long)floor(maxy / GVT_EXTENT);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > last) x1 = last;
    if (y1 > last) y1 = last;
    for (long y = y0; y <= y1; y++)
        for (long x = x0; x <= x1; x++) {
            if (n >= max) return n;
            out[n].x = (uint32_t)x;
            out[n].y = (uint32_t)y;
            n++;
        }
    return n;
}

void mapview_xform(const mapview_view_t *v, int z, uint32_t tx, uint32_t ty, float m[6])
{
    double s[3][2];
    static const int corner[3][2] = {{0, 0}, {1, 0}, {0, 1}};
    for (int i = 0; i < 3; i++) {
        double lat, lon;
        from_world(z, ((double)tx + corner[i][0]) * GVT_EXTENT, ((double)ty + corner[i][1]) * GVT_EXTENT, &lat, &lon);
        to_screen(v, (lon - v->lon0) * M_PER_DEG * v->cos0, (lat - v->lat0) * M_PER_DEG, &s[i][0], &s[i][1]);
    }
    m[0] = (float)((s[1][0] - s[0][0]) / GVT_EXTENT);
    m[3] = (float)((s[1][1] - s[0][1]) / GVT_EXTENT);
    m[1] = (float)((s[2][0] - s[0][0]) / GVT_EXTENT);
    m[4] = (float)((s[2][1] - s[0][1]) / GVT_EXTENT);
    m[2] = (float)s[0][0];
    m[5] = (float)s[0][1];
}

const uint8_t *mapview_get(mapview_t *mv, int z, uint32_t x, uint32_t y, uint32_t *size)
{
    const gvt_entry_t *e;
    int victim = 0;
    uint8_t *buf;
    for (int i = 0; i < MAPVIEW_SLOTS; i++) {
        mapview_slot_t *s = &mv->slot[i];
        if (s->used && s->z == z && s->x == x && s->y == y) {
            s->stamp = ++mv->clock;
            mv->hits++;
            *size = s->size;
            return mv->mem + (size_t)i * MAPVIEW_SLOT_BYTES;
        }
    }
    e = gvt_find(mv->g, z, x, y);
    if (!e) return NULL;
    if (e->size > MAPVIEW_SLOT_BYTES) { mv->too_big++; return NULL; }
    for (int i = 0; i < MAPVIEW_SLOTS; i++) {
        if (!mv->slot[i].used) { victim = i; break; }
        if (mv->slot[i].stamp < mv->slot[victim].stamp) victim = i;
    }
    buf = mv->mem + (size_t)victim * MAPVIEW_SLOT_BYTES;
    mv->slot[victim].used = 0;
    if (gvt_read_tile(mv->g, e, buf, MAPVIEW_SLOT_BYTES) < 0) { mv->errors++; return NULL; }
    mv->slot[victim] = (mapview_slot_t){ 1, (uint8_t)z, x, y, e->size, ++mv->clock };
    mv->loads++;
    *size = e->size;
    return buf;
}
