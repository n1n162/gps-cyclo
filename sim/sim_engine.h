#ifndef SIM_ENGINE_H
#define SIM_ENGINE_H

/* Moteur de rejeu pour le simulateur : rejoue une sortie enregistree (GPX avec heures) devant le GPS.
 * Il calcule une fois pour toutes la position de chaque point sur le parcours, puis permet de lire, accelerer,
 * sauter a la montee suivante et provoquer des pannes (hors parcours, GPS, ceinture, batterie). Sans affichage. */

#include <stdint.h>
#include "route.h"
#include "climb.h"

typedef struct {
    uint32_t t;                    /* heure (secondes depuis 1970) */
    double lat, lon;
    float ele;
    int hr, cad, power;            /* -1 si absent */
    float dist_m;                  /* distance parcourue depuis le depart de la sortie */
    float speed_mps;
} sim_row_t;

typedef struct {
    uint32_t time_s; int hour, minute;
    float pos_m; int on_route; float off_m;
    float speed_kmh, avg_kmh, dist_km, ascent_m;
    int hr, cad, power;
    int gps_ok, battery_pct, paused;
} sim_sample_t;

typedef struct {
    sim_row_t *rows; int n;
    float *pos_m, *off_m, *ascent;          /* calcules pour chaque point */
    uint8_t *on_route;
    const route_t *route;
    const climb_t *climbs; int n_climbs;
    double t;                               /* temps de lecture depuis le debut de la sortie (s) */
    int idx;
    double speed;                           /* facteur de vitesse : 1, 10, 60, 300 */
    int playing;
    int f_off, f_gps, f_hr, hr_boost;       /* pannes */
    float batt;
    int rec_paused, laps;
} sim_t;

#define SIM_BATT_S_PER_PCT 420.0            /* la batterie perd 1 % toutes les 7 minutes de sortie */

int  sim_load(sim_t *s, const char *gpx_path, const route_t *route, const climb_t *climbs, int n_climbs);
void sim_free(sim_t *s);
void sim_advance(sim_t *s, double real_dt_s);
void sim_get(const sim_t *s, sim_sample_t *out);
void sim_seek_row(sim_t *s, int row);
void sim_seek_pos(sim_t *s, float pos_m);           /* premier point arrive a cette distance sur le parcours */
void sim_next_climb(sim_t *s, int dir);             /* 500 m avant la montee suivante (dir > 0) ou precedente (dir < 0) */
double sim_duration(const sim_t *s);

#endif
