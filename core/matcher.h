#ifndef MATCHER_H
#define MATCHER_H

/* Positionnement d'une position GPS sur la trace chargee (distance depuis le depart). */

#include "route.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const route_t *r;
    int   idx;              /* segment de la derniere position valide, -1 si aucune */
    int   miss;             /* echecs consecutifs */
    float on_route_m;       /* distance maximale a la trace pour etre "sur le parcours" (30 m) */
    float ahead_m;          /* fenetre de recherche devant (400 m) */
    float back_m;           /* fenetre de recherche derriere (60 m) */
    int   reacquire_after;  /* echecs avant une recherche sur tout le parcours (5) */
    float min_speed_mps;    /* en dessous de cette vitesse le cap n'est pas utilise (1.5 m/s) */
} matcher_t;

typedef struct {
    float dist_m;           /* distance depuis le depart du parcours */
    float off_m;            /* distance a la trace (-1 si aucun candidat) */
    int   idx;
    int   on_route;
    int   global;           /* 1 si la recherche a porte sur tout le parcours */
} match_t;

void matcher_init(matcher_t *m, const route_t *r);

/* heading_deg : cap GNSS en degres depuis le nord (negatif si inconnu). Retourne on_route. */
int matcher_update(matcher_t *m, double lat, double lon, float speed_mps, float heading_deg, match_t *out);

#ifdef __cplusplus
}
#endif
#endif
