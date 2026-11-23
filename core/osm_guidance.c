#include "osm_guidance.h"
#include <string.h>

static int read_exact(osm_guidance_read_fn read, void *ctx, uint32_t offset,
                      void *dst, uint32_t len)
{
    return read ? read(ctx, offset, dst, len) : -1;
}

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static float le_float(const uint8_t *p)
{
    uint32_t u = le32(p);
    float f;
    memcpy(&f, &u, sizeof(f));
    return f;
}

int osm_guidance_load(osm_guidance_read_fn read, void *ctx,
                      osm_maneuver_t *items, uint32_t capacity,
                      osm_guidance_t *out)
{
    if (!read || !items || !out || capacity == 0) return -1;
    memset(out, 0, sizeof(*out));

    uint8_t header[8];
    if (read_exact(read, ctx, 0, header, sizeof header) != 0) return -1;
    if (memcmp(header, "GCR1", 4) != 0) return -1;
    uint32_t count = le32(header + 4);
    if (count > capacity) return -1;

    uint32_t offset = 8;
    for (uint32_t i = 0; i < count; i++) {
        uint8_t fixed[14];
        if (read_exact(read, ctx, offset, fixed, sizeof fixed) != 0) return -1;
        offset += sizeof fixed;

        osm_maneuver_t *m = &items[i];
        memset(m, 0, sizeof(*m));
        m->type = fixed[0];
        m->distance_from_start_m = le32(fixed + 1);
        m->lat = le_float(fixed + 5);
        m->lon = le_float(fixed + 9);

        uint8_t lens[4];
        if (read_exact(read, ctx, offset, lens, sizeof lens) != 0) return -1;
        offset += sizeof lens;
        uint16_t from_len = le16(lens);
        uint16_t to_len = le16(lens + 2);

        if (from_len >= OSM_GUIDANCE_MAX_NAME) from_len = OSM_GUIDANCE_MAX_NAME - 1;
        if (to_len >= OSM_GUIDANCE_MAX_NAME) to_len = OSM_GUIDANCE_MAX_NAME - 1;
        if (read_exact(read, ctx, offset, m->from_name, from_len) != 0) return -1;
        offset += from_len;
        m->from_name[from_len] = '\0';
        if (read_exact(read, ctx, offset, m->to_name, to_len) != 0) return -1;
        offset += to_len;
        m->to_name[to_len] = '\0';
    }

    out->items = items;
    out->count = count;
    out->capacity = capacity;
    return 0;
}

int osm_guidance_next(const osm_guidance_t *g, float route_distance_m,
                      float lookahead_m, osm_maneuver_t *out)
{
    if (!g || !g->items || !out || lookahead_m <= 0.0f) return 0;
    memset(out, 0, sizeof(*out));
    for (uint32_t i = 0; i < g->count; i++) {
        float distance = (float)g->items[i].distance_from_start_m - route_distance_m;
        if (distance >= 0.0f && distance <= lookahead_m) {
            *out = g->items[i];
            return 1;
        }
    }
    return 0;
}
