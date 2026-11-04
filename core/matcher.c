#include "matcher.h"
#include <math.h>

#define PI_F 3.14159265f

void matcher_init(matcher_t *m, const route_t *r)
{
    m->r = r;
    m->idx = -1;
    m->miss = 0;
    m->on_route_m = 30.0f;
    m->ahead_m = 400.0f;
    m->back_m = 60.0f;
    m->reacquire_after = 5;
    m->min_speed_mps = 1.5f;
}

/* Distance du point au segment i (i -> i+1). Retourne 0 si le segment va a contresens du cap. */
static int seg_dist(const route_t *r, int i, float px, float py, int use_h, float hx, float hy,
                    float *d, float *t)
{
    float ax = r->x[i], ay = r->y[i];
    float dx = r->x[i + 1] - ax, dy = r->y[i + 1] - ay;
    if (use_h && dx * hx + dy * hy < 0.0f) return 0;
    float l2 = dx * dx + dy * dy;
    float u = (l2 > 0.0f) ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0.0f;
    if (u < 0.0f) u = 0.0f;
    if (u > 1.0f) u = 1.0f;
    float qx = ax + u * dx - px, qy = ay + u * dy - py;
    *d = sqrtf(qx * qx + qy * qy);
    *t = u;
    return 1;
}

/* Meilleur segment dans [a, b). Parmi les candidats a moins de 5 m du meilleur, prend le plus proche de 'hint'. */
static int search(const route_t *r, float px, float py, int a, int b, int hint,
                  int use_h, float hx, float hy, float *dbest, float *tbest)
{
    float dmin = 1e30f, d, t;
    for (int i = a; i < b; i++)
        if (seg_dist(r, i, px, py, use_h, hx, hy, &d, &t) && d < dmin) dmin = d;
    if (dmin > 1e29f) return -1;

    int best = -1, bdist = 0x7fffffff;
    for (int i = a; i < b; i++) {
        if (!seg_dist(r, i, px, py, use_h, hx, hy, &d, &t)) continue;
        if (d > dmin + 5.0f) continue;
        int gap = i > hint ? i - hint : hint - i;
        if (gap < bdist) { bdist = gap; best = i; *dbest = d; *tbest = t; }
    }
    return best;
}

int matcher_update(matcher_t *m, double lat, double lon, float speed_mps, float heading_deg,
                   match_t *out)
{
    const route_t *r = m->r;
    float px, py;
    route_project(r, lat, lon, &px, &py);

    int use_h = (speed_mps >= m->min_speed_mps) && (heading_deg >= 0.0f);
    float hx = 0.0f, hy = 0.0f;
    if (use_h) {
        float h = heading_deg * PI_F / 180.0f;
        hx = sinf(h); hy = cosf(h);
    }

    int n = r->n;
    int global = (m->idx < 0) || (m->miss >= m->reacquire_after);
    int a, b, hint;
    if (global) {
        a = 0; b = n - 1; hint = (m->idx < 0) ? 0 : m->idx;
    } else {
        a = m->idx - (int)(m->back_m / r->step_m);   if (a < 0) a = 0;
        b = m->idx + (int)(m->ahead_m / r->step_m);  if (b > n - 1) b = n - 1;
        hint = m->idx;
    }

    float d = 0.0f, t = 0.0f;
    int i = search(r, px, py, a, b, hint, use_h, hx, hy, &d, &t);
    out->global = global;

    if (i >= 0 && d <= m->on_route_m) {
        m->idx = i;
        m->miss = 0;
        out->idx = i;
        out->dist_m = ((float)i + t) * r->step_m;
        out->off_m = d;
        out->on_route = 1;
        return 1;
    }
    m->miss++;
    out->idx = m->idx;
    out->dist_m = (m->idx >= 0) ? (float)m->idx * r->step_m : 0.0f;
    out->off_m = (i >= 0) ? d : -1.0f;
    out->on_route = 0;
    return 0;
}
