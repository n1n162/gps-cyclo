#include "osm_guidance.h"
#include <assert.h>
#include <string.h>

static int memory_read(void *ctx, uint32_t offset, void *dst, uint32_t len)
{
    const uint8_t *src = (const uint8_t *)ctx;
    memcpy(dst, src + offset, len);
    return 0;
}

int main(void)
{
    uint8_t buf[128] = {0};
    memcpy(buf, "GCR1", 4);
    buf[4] = 1;
    buf[8] = OSM_MANEUVER_RIGHT;
    buf[9] = 100;

    osm_maneuver_t items[4];
    osm_guidance_t g;
    assert(osm_guidance_load(memory_read, buf, items, 4, &g) == 0);
    assert(g.count == 1);

    osm_maneuver_t next;
    assert(!osm_guidance_next(&g, 120.0f, 700.0f, &next));
    assert(osm_guidance_next(&g, 0.0f, 700.0f, &next));
    assert(next.type == OSM_MANEUVER_RIGHT);
    assert(next.distance_from_start_m == 100);
    return 0;
}
