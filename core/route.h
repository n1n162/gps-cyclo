#ifndef ROUTE_H
#define ROUTE_H

/* Chargement d'un parcours GPX (lecture en flux, sans allocation) et mise a pas constant.
 * Les points sont reechantillonnes tous les step_m metres : l'indice d'un point donne donc
 * directement la distance depuis le depart (indice x step_m). */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* tableaux fournis par l'appelant (max_n elements chacun) */
    float *x, *y, *ele;
    int    max_n;
    float  step_m;
    /* resultats */
    int    n;
    float  total_m;
    int    truncated;     /* 1 si le parcours depasse max_n points */
    int    has_ele;       /* 1 si le GPX contient des altitudes */
    int    raw_points;    /* points lus dans le GPX */
    double lat0, lon0;    /* origine de la projection locale */
    /* etat interne */
    double cos0;
    int    started;
    double p_lat, p_lon, p_dist, next_d;
    float  p_x, p_y, p_ele, last_ele;
} route_t;

void route_init(route_t *r, float *x, float *y, float *ele, int max_n, float step_m);
void route_add_point(route_t *r, double lat, double lon, float ele, int has_ele);
void route_project(const route_t *r, double lat, double lon, float *x, float *y);
void route_unproject(const route_t *r, float x, float y, double *lat, double *lon);

/* Lecteur de flux : remplit buf avec au plus max octets, retourne le nombre lu (0 = fin). */
typedef size_t (*gpx_read_fn)(void *ctx, char *buf, size_t max);

/* Lit un GPX (points <trkpt> ou <rtept>). Retourne le nombre de points, ou -1 si moins de 2 points. */
int gpx_load(gpx_read_fn rd, void *ctx, route_t *r);

#ifdef __cplusplus
}
#endif
#endif
