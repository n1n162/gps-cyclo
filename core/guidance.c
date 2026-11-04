#include "guidance.h"
#include <math.h>
#include <string.h>

#define RAD_TO_DEG 57.295779513082320876f

static float angle_at(const route_t *r, int i, int span)
{
    int a = i - span, b = i + span;
    if (a < 0 || b >= r->n) return 0.0f;
    float in_x = r->x[i] - r->x[a];
    float in_y = r->y[i] - r->y[a];
    float out_x = r->x[b] - r->x[i];
    float out_y = r->y[b] - r->y[i];
    if (in_x * in_x + in_y * in_y < 1.0f || out_x * out_x + out_y * out_y < 1.0f) return 0.0f;
    /* Avec x=est et y=nord, un delta positif correspond a un virage a droite. */
    float h1 = atan2f(in_x, in_y);
    float h2 = atan2f(out_x, out_y);
    float d = h2 - h1;
    while (d > (float)M_PI) d -= 2.0f * (float)M_PI;
    while (d < -(float)M_PI) d += 2.0f * (float)M_PI;
    return d * RAD_TO_DEG;
}


/* Detecte une portion circulaire dans la trace : plusieurs changements de cap
   de meme sens, cumules sur environ 55 m. Le GPX seul ne permet pas de connaitre
   de facon fiable le numero de sortie du rond-point. */
static int roundabout_ahead(const route_t *r, int i, int span, int direction)
{
    int end = i + (int)(55.0f / r->step_m + 0.5f);
    if (end >= r->n - span - 1) end = r->n - span - 2;
    float same = 0.0f, opposite = 0.0f;
    int bends = 0;
    for (int k = i; k <= end; k += (span > 1 ? span : 2)) {
        float d = angle_at(r, k, span);
        if (d * direction > 8.0f) { same += d * direction; bends++; }
        else if (d * direction < -10.0f) opposite += -d * direction;
    }
    return bends >= 3 && same >= 210.0f && opposite < 70.0f;
}

int guidance_next_turn(const route_t *r, float pos_m, float lookahead_m,
                       float min_angle_deg, guidance_turn_t *out)
{
    if (out) memset(out, 0, sizeof(*out));
    if (!r || !out || !r->x || !r->y || r->n < 7 || r->step_m <= 0.0f || lookahead_m <= 0.0f) return 0;

    int span = (int)(15.0f / r->step_m + 0.5f);
    if (span < 1) span = 1;
    if (2 * span + 3 >= r->n) return 0;

    /* Le centre du virage doit etre suffisamment devant nous pour que la fenetre
       de calcul (+/- span) ne classe pas un virage deja passe comme prochain. */
    int first = (int)(pos_m / r->step_m) + span + 1;
    if (first < span + 1) first = span + 1;
    int last = (int)((pos_m + lookahead_m) / r->step_m);
    if (last > r->n - span - 2) last = r->n - span - 2;

    for (int i = first; i <= last; i++) {
        float d = angle_at(r, i, span);
        float ad = fabsf(d);
        if (ad < min_angle_deg) continue;
        float before = fabsf(angle_at(r, i - 2, span));
        float after = fabsf(angle_at(r, i + 2, span));
        int is_roundabout = roundabout_ahead(r, i, span, d >= 0.0f ? 1 : -1);
        /* Un maximum local evite les doublons sur un virage simple. Une courbe
           circulaire garde une courbure presque constante : on la laisse passer. */
        if (!is_roundabout && (ad < before || ad <= after)) continue;

        out->found = 1;
        out->maneuver = is_roundabout ? 1 : 0;
        out->direction = d >= 0.0f ? 1 : -1;
        out->angle_deg = (int)(ad + 0.5f);
        out->idx = i;
        out->dist_m = (float)i * r->step_m - pos_m;
        if (out->dist_m < 0.0f) out->dist_m = 0.0f;
        return 1;
    }
    return 0;
}
