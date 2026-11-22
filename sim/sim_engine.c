#include "sim_engine.h"
#include "matcher.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EARTH_R 6371008.8
#define DEG (3.14159265358979323846 / 180.0)

static double hav(double la1, double lo1, double la2, double lo2)
{
    double p1 = la1 * DEG, p2 = la2 * DEG;
    double s1 = sin((p2 - p1) / 2.0), s2 = sin((lo2 - lo1) * DEG / 2.0);
    return 2.0 * EARTH_R * asin(sqrt(s1 * s1 + cos(p1) * cos(p2) * s2 * s2));
}

static double bearing(double la1, double lo1, double la2, double lo2)
{
    double p1 = la1 * DEG, p2 = la2 * DEG, dl = (lo2 - lo1) * DEG;
    double y = sin(dl) * cos(p2), x = cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl);
    double b = atan2(y, x) / DEG;
    return b < 0 ? b + 360.0 : b;
}

static long days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}

static int parse_iso(const char *s, uint32_t *out)
{
    int y, mo, d, h, mi, se;
    if (sscanf(s, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) != 6) return 0;
    *out = (uint32_t)(days_from_civil(y, mo, d) * 86400L + h * 3600L + mi * 60L + se);
    return 1;
}

/* Valeur numerique d'une balise <nom>, avec ou sans prefixe d'espace de noms, dans [blk, end). */
static int tag_num(const char *blk, const char *end, const char *name, double *out)
{
    size_t n = strlen(name);
    for (const char *p = blk; (p = strstr(p, name)) != NULL && p < end; p += n) {
        if (p[n] != '>') continue;
        if (p[-1] != '<' && p[-1] != ':') continue;
        *out = atof(p + n + 1);
        return 1;
    }
    return 0;
}

static int tag_text(const char *blk, const char *end, const char *name, char *out, size_t cap)
{
    size_t n = strlen(name);
    for (const char *p = blk; (p = strstr(p, name)) != NULL && p < end; p += n) {
        if (p[n] != '>' || p[-1] != '<') continue;
        const char *q = p + n + 1;
        const char *e = strchr(q, '<');
        if (!e || (size_t)(e - q) >= cap) return 0;
        memcpy(out, q, (size_t)(e - q));
        out[e - q] = '\0';
        return 1;
    }
    return 0;
}

static int load_rows(sim_t *s, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return -1; }
    fclose(f);
    buf[sz] = '\0';
    int cap = 4096, n = 0;
    sim_row_t *rows = (sim_row_t *)malloc((size_t)cap * sizeof(*rows));
    for (char *p = buf; (p = strstr(p, "<trkpt")) != NULL;) {
        char *end = strstr(p, "</trkpt>");
        if (!end) break;
        char *la = strstr(p, "lat=\""), *lo = strstr(p, "lon=\"");
        char tbuf[40];
        double ele = 0, v;
        if (la && lo && la < end && lo < end && tag_text(p, end, "time", tbuf, sizeof tbuf)) {
            if (n == cap) { cap *= 2; rows = (sim_row_t *)realloc(rows, (size_t)cap * sizeof(*rows)); }
            sim_row_t *r = &rows[n];
            memset(r, 0, sizeof(*r));
            r->lat = atof(la + 5); r->lon = atof(lo + 5);
            if (!parse_iso(tbuf, &r->t)) { p = end + 8; continue; }
            tag_num(p, end, "ele", &ele);
            r->ele = (float)ele;
            r->hr = tag_num(p, end, "hr", &v) ? (int)v : -1;
            r->cad = tag_num(p, end, "cad", &v) ? (int)v : (tag_num(p, end, "cadence", &v) ? (int)v : -1);
            r->power = tag_num(p, end, "power", &v) ? (int)v : (tag_num(p, end, "PowerInWatts", &v) ? (int)v : (tag_num(p, end, "watts", &v) ? (int)v : -1));
            n++;
        }
        p = end + 8;
    }
    free(buf);
    if (n < 2) { free(rows); return -2; }
    rows[0].dist_m = 0.0f; rows[0].speed_mps = 0.0f;
    for (int i = 1; i < n; i++) {
        double d = hav(rows[i - 1].lat, rows[i - 1].lon, rows[i].lat, rows[i].lon);
        rows[i].dist_m = rows[i - 1].dist_m + (float)d;
        double dt = (double)rows[i].t - (double)rows[i - 1].t;
        double sp = dt > 0 ? d / dt : 0.0;
        rows[i].speed_mps = (float)(sp > 25.0 ? 25.0 : sp);
    }
    s->rows = rows; s->n = n;
    return n;
}

int sim_load(sim_t *s, const char *gpx_path, const route_t *route, const climb_t *climbs, int n_climbs)
{
    memset(s, 0, sizeof(*s));
    int n = load_rows(s, gpx_path);
    if (n < 0) return n;
    s->route = route; s->climbs = climbs; s->n_climbs = n_climbs;
    s->pos_m = (float *)calloc((size_t)n, sizeof(float));
    s->off_m = (float *)calloc((size_t)n, sizeof(float));
    s->ascent = (float *)calloc((size_t)n, sizeof(float));
    s->on_route = (uint8_t *)calloc((size_t)n, 1);
    matcher_t m;
    matcher_init(&m, route);
    float asc = 0.0f, ref = s->rows[0].ele;
    for (int i = 0; i < n; i++) {
        const sim_row_t *r = &s->rows[i];
        int j = i + 1 < n ? i + 1 : i - 1;
        double seg = hav(r->lat, r->lon, s->rows[j].lat, s->rows[j].lon);
        float hd = -1.0f;
        if (seg >= 1.0) hd = (float)(j > i ? bearing(r->lat, r->lon, s->rows[j].lat, s->rows[j].lon) : bearing(s->rows[j].lat, s->rows[j].lon, r->lat, r->lon));
        match_t mt;
        matcher_update(&m, r->lat, r->lon, r->speed_mps, hd, &mt);
        s->pos_m[i] = mt.dist_m;
        s->on_route[i] = (uint8_t)mt.on_route;
        s->off_m[i] = mt.off_m;
        if (r->ele - ref >= 1.0f) { asc += r->ele - ref; ref = r->ele; }
        else if (ref - r->ele >= 1.0f) ref = r->ele;
        s->ascent[i] = asc;
    }
    s->speed = 1.0; s->playing = 1; s->batt = 100.0f;
    return n;
}

void sim_free(sim_t *s)
{
    free(s->rows); free(s->pos_m); free(s->off_m); free(s->ascent); free(s->on_route);
    memset(s, 0, sizeof(*s));
}

double sim_duration(const sim_t *s) { return (double)(s->rows[s->n - 1].t - s->rows[0].t); }

void sim_seek_row(sim_t *s, int row)
{
    if (row < 0) row = 0;
    if (row > s->n - 1) row = s->n - 1;
    s->idx = row;
    s->t = (double)(s->rows[row].t - s->rows[0].t);
}

void sim_advance(sim_t *s, double dt)
{
    if (!s->playing) return;
    double step = dt * s->speed;
    s->t += step;
    s->batt -= (float)(step / SIM_BATT_S_PER_PCT);
    if (s->batt < 0.0f) s->batt = 0.0f;
    uint32_t t0 = s->rows[0].t;
    while (s->idx + 1 < s->n && (double)(s->rows[s->idx + 1].t - t0) <= s->t) s->idx++;
    if (s->idx >= s->n - 1) { s->playing = 0; s->t = sim_duration(s); }
}

void sim_seek_pos(sim_t *s, float pos_m)
{
    for (int i = 0; i < s->n; i++) if (s->pos_m[i] >= pos_m) { sim_seek_row(s, i); return; }
    sim_seek_row(s, s->n - 1);
}

void sim_next_climb(sim_t *s, int dir)
{
    float p = s->pos_m[s->idx];
    /* sim_seek_pos se place sur le premier point a la distance demandee, donc jusqu'a un pas plus loin.
     * Pour 'precedente', on ignore cet ecart (sinon on retombe sur la meme montee et on reste bloque). */
    float slack = s->idx > 0 ? p - s->pos_m[s->idx - 1] : 0.0f;
    if (slack < 1.0f) slack = 1.0f;
    if (slack > 50.0f) slack = 50.0f;
    if (dir > 0) {
        for (int i = 0; i < s->n_climbs; i++)
            if (s->climbs[i].start_m - 500.0f > p + 1.0f) { sim_seek_pos(s, s->climbs[i].start_m - 500.0f); return; }
    } else {
        for (int i = s->n_climbs - 1; i >= 0; i--)
            if (s->climbs[i].start_m - 500.0f < p - slack) { sim_seek_pos(s, s->climbs[i].start_m - 500.0f); return; }
        sim_seek_row(s, 0);
    }
}

void sim_get(const sim_t *s, sim_sample_t *o)
{
    const sim_row_t *r = &s->rows[s->idx];
    memset(o, 0, sizeof(*o));
    o->time_s = r->t - s->rows[0].t;
    time_t tt = (time_t)r->t;
    struct tm lt;
    localtime_r(&tt, &lt);
    o->hour = lt.tm_hour; o->minute = lt.tm_min;
    o->pos_m = s->pos_m[s->idx];
    o->on_route = s->f_off ? 0 : s->on_route[s->idx];
    o->off_m = s->f_off ? 150.0f : s->off_m[s->idx];
    o->speed_kmh = r->speed_mps * 3.6f;
    o->dist_km = r->dist_m / 1000.0f;
    o->avg_kmh = o->time_s > 0 ? o->dist_km / ((float)o->time_s / 3600.0f) : 0.0f;
    o->ascent_m = s->ascent[s->idx];
    o->hr = s->f_hr ? -1 : (r->hr >= 0 ? r->hr + (s->hr_boost ? 40 : 0) : -1);
    o->cad = r->cad; o->power = r->power;
    o->gps_ok = !s->f_gps;
    o->battery_pct = (int)(s->batt + 0.5f);
    o->paused = s->rec_paused;
}
