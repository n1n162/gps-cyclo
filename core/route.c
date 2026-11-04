#include "route.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define EARTH_R 6371008.8
#define DEG (3.14159265358979323846 / 180.0)

static double hav(double la1, double lo1, double la2, double lo2)
{
    double p1 = la1 * DEG, p2 = la2 * DEG;
    double s1 = sin((p2 - p1) / 2.0), s2 = sin((lo2 - lo1) * DEG / 2.0);
    double h = s1 * s1 + cos(p1) * cos(p2) * s2 * s2;
    return 2.0 * EARTH_R * asin(sqrt(h));
}

void route_init(route_t *r, float *x, float *y, float *ele, int max_n, float step_m)
{
    memset(r, 0, sizeof(*r));
    r->x = x; r->y = y; r->ele = ele;
    r->max_n = max_n;
    r->step_m = step_m;
}

void route_project(const route_t *r, double lat, double lon, float *x, float *y)
{
    *x = (float)((lon - r->lon0) * DEG * EARTH_R * r->cos0);
    *y = (float)((lat - r->lat0) * DEG * EARTH_R);
}

void route_unproject(const route_t *r, float x, float y, double *lat, double *lon)
{
    *lat = r->lat0 + (double)y / (DEG * EARTH_R);
    *lon = r->lon0 + (double)x / (DEG * EARTH_R * r->cos0);
}

void route_add_point(route_t *r, double lat, double lon, float ele, int has_ele)
{
    r->raw_points++;
    if (has_ele) { r->has_ele = 1; r->last_ele = ele; } else { ele = r->last_ele; }

    if (!r->started) {
        r->started = 1;
        r->lat0 = lat; r->lon0 = lon; r->cos0 = cos(lat * DEG);
        float x, y;
        route_project(r, lat, lon, &x, &y);
        if (r->max_n > 0) { r->x[0] = x; r->y[0] = y; r->ele[0] = ele; r->n = 1; }
        r->p_lat = lat; r->p_lon = lon; r->p_dist = 0.0;
        r->p_x = x; r->p_y = y; r->p_ele = ele;
        r->next_d = r->step_m;
        return;
    }

    float x, y;
    route_project(r, lat, lon, &x, &y);
    double seg = hav(r->p_lat, r->p_lon, lat, lon);
    double d_new = r->p_dist + seg;

    while (r->next_d <= d_new) {
        if (r->n >= r->max_n) { r->truncated = 1; break; }
        float t = (seg > 0.0) ? (float)((r->next_d - r->p_dist) / seg) : 0.0f;
        r->x[r->n] = r->p_x + t * (x - r->p_x);
        r->y[r->n] = r->p_y + t * (y - r->p_y);
        r->ele[r->n] = r->p_ele + t * (ele - r->p_ele);
        r->n++;
        r->next_d += r->step_m;
    }
    r->p_lat = lat; r->p_lon = lon; r->p_dist = d_new;
    r->p_x = x; r->p_y = y; r->p_ele = ele;
    r->total_m = (float)d_new;
}

/* ---------- lecture GPX en flux ---------- */

typedef struct {
    route_t *r;
    char   tag[320];
    int    tag_len, in_tag;
    int    collecting_ele;
    char   txt[32];
    int    txt_len;
    int    point_open;
    double lat, lon;
    float  ele;
    int    has_ele;
} gpx_state_t;

static int name_end(char c) { return c == '\0' || c == ' ' || c == '/' || c == '\t' || c == '\n' || c == '\r'; }

static int is_point_name(const char *s)
{
    return (strncmp(s, "trkpt", 5) == 0 || strncmp(s, "rtept", 5) == 0) && name_end(s[5]);
}

/* Cherche name="valeur" dans le texte d'une balise. */
static int attr_value(const char *tag, const char *name, double *out)
{
    size_t nl = strlen(name);
    for (const char *p = tag + 1; *p; p++) {
        if (strncmp(p, name, nl) != 0) continue;
        char prev = p[-1];
        if (prev != ' ' && prev != '\t' && prev != '\n' && prev != '\r') continue;
        const char *q = p + nl;
        while (*q == ' ') q++;
        if (*q != '=') continue;
        q++;
        while (*q == ' ') q++;
        if (*q != '"' && *q != '\'') continue;
        q++;
        char *end;
        double v = strtod(q, &end);
        if (end == q) continue;
        *out = v;
        return 1;
    }
    return 0;
}

static void emit_point(gpx_state_t *g)
{
    route_add_point(g->r, g->lat, g->lon, g->ele, g->has_ele);
    g->point_open = 0;
}

static void handle_tag(gpx_state_t *g)
{
    const char *t = g->tag;
    int closing = (t[0] == '/');
    const char *name = closing ? t + 1 : t;
    int selfclose = (g->tag_len > 0 && t[g->tag_len - 1] == '/');

    if (!closing && is_point_name(name)) {
        double la, lo;
        if (attr_value(t, "lat", &la) && attr_value(t, "lon", &lo)) {
            g->lat = la; g->lon = lo; g->ele = 0.0f; g->has_ele = 0; g->point_open = 1;
            if (selfclose) emit_point(g);
        }
    } else if (closing && is_point_name(name)) {
        if (g->point_open) emit_point(g);
    } else if (!closing && g->point_open && strcmp(name, "ele") == 0) {
        g->collecting_ele = 1;
        g->txt_len = 0;
    }
}

int gpx_load(gpx_read_fn rd, void *ctx, route_t *r)
{
    gpx_state_t g;
    memset(&g, 0, sizeof(g));
    g.r = r;
    char buf[512];
    size_t k;
    while ((k = rd(ctx, buf, sizeof(buf))) > 0) {
        for (size_t i = 0; i < k; i++) {
            char c = buf[i];
            if (g.in_tag) {
                if (c == '>') {
                    g.tag[g.tag_len] = '\0';
                    g.in_tag = 0;
                    handle_tag(&g);
                } else if (g.tag_len < (int)sizeof(g.tag) - 1) {
                    g.tag[g.tag_len++] = c;
                }
            } else {
                if (c == '<') {
                    if (g.collecting_ele) {
                        g.txt[g.txt_len] = '\0';
                        char *end;
                        double v = strtod(g.txt, &end);
                        if (end != g.txt) { g.ele = (float)v; g.has_ele = 1; }
                        g.collecting_ele = 0;
                    }
                    g.in_tag = 1;
                    g.tag_len = 0;
                } else if (g.collecting_ele && g.txt_len < (int)sizeof(g.txt) - 1) {
                    g.txt[g.txt_len++] = c;
                }
            }
        }
    }
    return (r->n >= 2) ? r->n : -1;
}
