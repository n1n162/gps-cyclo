#ifndef SIM_ENGINE_H
#define SIM_ENGINE_H

#include <stdint.h>
#include "route.h"
#include "climb.h"
#include "osm_guidance.h"

typedef struct {
    uint32_t t;
    double lat, lon;
    float ele;
    int hr, cad, power;
    float dist_m;
    float speed_mps;
} sim_row_t;

typedef struct {
    uint32_t time_s; int hour, minute;
    float pos_m; int on_route; float off_m;
    float speed_kmh, avg_kmh, dist_km, ascent_m;
    int hr, cad, power;
    int gps_ok, battery_pct, paused;
    osm_maneuver_t maneuver;
    int has_maneuver;
} sim_sample_t;

typedef struct {
    sim_row_t *rows; int n;
    float *pos_m, *off_m, *ascent;
    uint8_t *on_route;
    const route_t *route;
    const climb_t *climbs; int n_climbs;
    osm_maneuver_t osm_items[64];
    osm_guidance_t osm_guidance;
    float osm_lookahead_m;
    double t;
    int idx;
    double speed;
    int playing;
    int f_off, f_gps, f_hr, hr_boost;
    float batt;
    int rec_paused, laps;
} sim_t;

#define SIM_BATT_S_PER_PCT 420.0

int  sim_load(sim_t *s, const char *gpx_path, const route_t *route, const climb_t *climbs, int n_climbs);
int  sim_load_osm_guidance(sim_t *s, const char *gcr1_path);
void sim_free(sim_t *s);
void sim_advance(sim_t *s, double real_dt_s);
void sim_get(const sim_t *s, sim_sample_t *out);
void sim_seek_row(sim_t *s, int row);
void sim_seek_pos(sim_t *s, float pos_m);
void sim_next_climb(sim_t *s, int dir);
double sim_duration(const sim_t *s);

#endif
