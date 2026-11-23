#ifndef OSM_GUIDANCE_H
#define OSM_GUIDANCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OSM_GUIDANCE_MAX_NAME 64

enum {
    OSM_MANEUVER_STRAIGHT = 0,
    OSM_MANEUVER_SLIGHT_LEFT,
    OSM_MANEUVER_LEFT,
    OSM_MANEUVER_SLIGHT_RIGHT,
    OSM_MANEUVER_RIGHT,
    OSM_MANEUVER_U_TURN,
    OSM_MANEUVER_ROUNDABOUT,
    OSM_MANEUVER_INTERSECTION,
    OSM_MANEUVER_ARRIVAL
};

typedef int (*osm_guidance_read_fn)(void *ctx, uint32_t offset, void *dst, uint32_t len);

typedef struct {
    uint8_t type;
    uint32_t distance_from_start_m;
    float lat;
    float lon;
    char from_name[OSM_GUIDANCE_MAX_NAME];
    char to_name[OSM_GUIDANCE_MAX_NAME];
} osm_maneuver_t;

typedef struct {
    osm_maneuver_t *items;
    uint32_t count;
    uint32_t capacity;
} osm_guidance_t;

int osm_guidance_load(osm_guidance_read_fn read, void *ctx,
                      osm_maneuver_t *items, uint32_t capacity,
                      osm_guidance_t *out);

int osm_guidance_next(const osm_guidance_t *g, float route_distance_m,
                      float lookahead_m, osm_maneuver_t *out);

#ifdef __cplusplus
}
#endif
#endif
