#ifndef GUIDANCE_H
#define GUIDANCE_H

#include "route.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int found;
    int maneuver;       /* 0 = virage, 1 = rond-point detecte sur la trace */
    int direction;       /* -1 = gauche, +1 = droite */
    int angle_deg;       /* angle absolu estime */
    int idx;             /* indice du point de trace correspondant au virage */
    float dist_m;        /* distance depuis la position courante */
} guidance_turn_t;

/* Cherche le prochain virage net sur la trace reechantillonnee. */
int guidance_next_turn(const route_t *route, float pos_m, float lookahead_m,
                       float min_angle_deg, guidance_turn_t *out);

#ifdef __cplusplus
}
#endif
#endif
