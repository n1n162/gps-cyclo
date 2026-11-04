#include "map_data.h"
#include <string.h>

#define GVM_HEADER_SIZE 16u
#define GVM_RECORD_SIZE 20u

static uint16_t u16le(const uint8_t *p) { return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8); }
static uint32_t u32le(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static int32_t i32le(const uint8_t *p) { return (int32_t)u32le(p); }
static int read_exact(FILE *f, void *p, size_t n) { return fread(p, 1, n, f) == n ? 0 : -1; }

int map_reader_open(map_reader_t *r, const char *path)
{
    uint8_t h[GVM_HEADER_SIZE];
    if (!r || !path) return -1;
    memset(r, 0, sizeof(*r));
    r->file = fopen(path, "rb");
    if (!r->file) return -1;
    if (read_exact(r->file, h, sizeof h) || memcmp(h, "GVM1", 4) != 0 ||
        u16le(h + 4) != MAP_GVM_VERSION || u16le(h + 6) != 0) {
        map_reader_close(r); return -1;
    }
    r->feature_count = u32le(h + 8);
    r->index = 0;
    r->valid = 1;
    return 0;
}

void map_reader_close(map_reader_t *r)
{
    if (!r) return;
    if (r->file) fclose(r->file);
    memset(r, 0, sizeof(*r));
}

static int intersects(const map_feature_t *f, const map_bounds_t *b)
{
    if (!b) return 1;
    return f->max_lat_e7 >= b->min_lat_e7 && f->min_lat_e7 <= b->max_lat_e7 &&
           f->max_lon_e7 >= b->min_lon_e7 && f->min_lon_e7 <= b->max_lon_e7;
}

int map_reader_next_visible(map_reader_t *r, const map_bounds_t *bounds,
                            map_feature_t *feature, map_point_t *points, uint16_t capacity)
{
    uint8_t h[GVM_RECORD_SIZE];
    if (!r || !r->valid || !r->file || !feature || !points || !capacity) return -1;
    while (r->index < r->feature_count) {
        map_feature_t f;
        uint32_t bytes;
        if (read_exact(r->file, h, sizeof h)) return -1;
        f.type = h[0]; f.n_points = u16le(h + 2);
        f.min_lat_e7 = i32le(h + 4); f.max_lat_e7 = i32le(h + 8);
        f.min_lon_e7 = i32le(h + 12); f.max_lon_e7 = i32le(h + 16);
        r->index++;
        bytes = (uint32_t)f.n_points * 8u;
        if (!f.n_points) return -1;
        if (!intersects(&f, bounds) || f.n_points > capacity) {
            if (fseek(r->file, (long)bytes, SEEK_CUR) != 0) return -1;
            continue;
        }
        for (uint16_t i = 0; i < f.n_points; i++) {
            uint8_t p[8];
            if (read_exact(r->file, p, sizeof p)) return -1;
            points[i].lat_e7 = i32le(p);
            points[i].lon_e7 = i32le(p + 4);
        }
        *feature = f;
        return 1;
    }
    return 0;
}
