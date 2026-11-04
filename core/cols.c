#include "cols.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define M_PER_DEG 111320.0
#define DEG_RAD (3.14159265358979323846 / 180.0)

void cols_cfg_default(cols_cfg_t *c)
{
    c->radius_m = 300.0f;
    c->ele_tol_m = 40.0f;
    c->ele_weight = 3.0f;
    c->place_radius_m = 1500.0f;
    c->place_mode = COLS_PLACE_SUMMIT;
}

void cols_init(cols_db_t *db, cols_entry_t *entries, int max_entries, char *pool, uint32_t pool_size)
{
    memset(db, 0, sizeof(*db));
    db->e = entries; db->max_e = max_entries;
    db->pool = pool; db->pool_size = pool_size;
    db->idx_type = 0; db->idx_name = 1; db->idx_lat = 2; db->idx_lon = 3; db->idx_alt = 4;
}

const char *cols_name(const cols_db_t *db, int i)
{
    if (i < 0 || i >= db->n) return "";
    return db->pool + db->e[i].name_off;
}

void cols_filter_route(cols_db_t *db, const route_t *r, float margin_m)
{
    float xmin = r->x[0], xmax = r->x[0], ymin = r->y[0], ymax = r->y[0];
    for (int i = 1; i < r->n; i++) {
        if (r->x[i] < xmin) xmin = r->x[i];
        if (r->x[i] > xmax) xmax = r->x[i];
        if (r->y[i] < ymin) ymin = r->y[i];
        if (r->y[i] > ymax) ymax = r->y[i];
    }
    double la0, lo0, la1, lo1;
    route_unproject(r, xmin - margin_m, ymin - margin_m, &la0, &lo0);
    route_unproject(r, xmax + margin_m, ymax + margin_m, &la1, &lo1);
    db->lat_min = (float)la0; db->lon_min = (float)lo0;
    db->lat_max = (float)la1; db->lon_max = (float)lo1;
    db->filter = 1;
}

/* ---------- lecture du CSV en flux ---------- */
#define MAX_FIELDS 12

/* Types ecrits par convertir_cols.py : col et lieu servent a nommer les montees, les autres sont reconnus mais pas utilises. */
static int is_type(const char *f)
{
    static const char *const types[] = {"col", "lieu", "eau", "boulangerie", "commerce", "toilette", "velociste"};
    for (unsigned i = 0; i < sizeof types / sizeof types[0]; i++) if (strcmp(f, types[i]) == 0) return 1;
    return 0;
}

/* Vrai si le champ (espaces et guillemets autour ignores, majuscules ignorees) vaut a ou b. */
static int name_is(const char *f, const char *a, const char *b)
{
    while (*f == ' ' || *f == '"') f++;
    size_t m = strlen(f);
    while (m > 0 && (f[m - 1] == ' ' || f[m - 1] == '"' || f[m - 1] == '\r')) m--;
    const char *names[2] = {a, b};
    for (int k = 0; k < 2; k++) {
        size_t n = strlen(names[k]), i = 0;
        if (m != n) continue;
        while (i < n && (f[i] | 0x20) == names[k][i]) i++;
        if (i == n) return 1;
    }
    return 0;
}

/* Une ligne d'en-tete nomme les colonnes : type, nom (ou name), lat (ou latitude), lon (ou longitude, lng), alt (ou ele...). */
static int read_header(cols_db_t *db, char **f, int nf)
{
    int t = -1, nm = -1, la = -1, lo = -1, al = -1;
    for (int i = 0; i < nf; i++) {
        if (name_is(f[i], "type", "type")) t = i;
        else if (name_is(f[i], "nom", "name")) nm = i;
        else if (name_is(f[i], "lat", "latitude")) la = i;
        else if (name_is(f[i], "lon", "longitude") || name_is(f[i], "lng", "lng")) lo = i;
        else if (name_is(f[i], "alt", "altitude") || name_is(f[i], "ele", "elevation")) al = i;
    }
    if (t < 0 || nm < 0 || la < 0 || lo < 0) return 0;
    db->idx_type = t; db->idx_name = nm; db->idx_lat = la; db->idx_lon = lo; db->idx_alt = al; db->has_header = 1;
    return 1;
}

static void add_line(cols_db_t *db, char *line)
{
    if (line[0] == '#' || line[0] == '\0') return;
    char *f[MAX_FIELDS] = {0};
    int nf = 0;
    f[nf++] = line;
    for (char *p = line; *p && nf < MAX_FIELDS; p++)
        if (*p == ';') { *p = '\0'; f[nf++] = p + 1; }
    if (nf < 4) return;
    int it = db->idx_type, in = db->idx_name, ila = db->idx_lat, ilo = db->idx_lon, ial = db->idx_alt;
    if (!db->has_header) {
        int p = -1;                                     /* sans en-tete : le type (col ou lieu) est dans l'une des trois premieres colonnes */
        for (int i = 0; i < nf && i < 3 && p < 0; i++) if (is_type(f[i])) p = i;
        if (p < 0) {
            if (read_header(db, f, nf)) return;         /* en-tete qui nomme les colonnes */
            if (!name_is(f[0], "type", "type") && !name_is(f[1], "type", "type") && !name_is(f[2], "type", "type")) db->rejected++;
            return;
        }
        it = p; in = p + 1; ila = p + 2; ilo = p + 3; ial = p + 4;
    }
    if (it >= nf || in >= nf || ila >= nf || ilo >= nf) { db->rejected++; return; }
    int kind;
    if (strcmp(f[it], "col") == 0) kind = COLS_KIND_COL;
    else if (strcmp(f[it], "lieu") == 0) kind = COLS_KIND_LIEU;
    else if (is_type(f[it])) { db->other++; return; }   /* eau, boulangerie... : reconnu, non utilise */
    else { db->rejected++; return; }                    /* ligne inconnue */
    char *end;
    double lat = strtod(f[ila], &end);  if (end == f[ila]) { db->rejected++; return; }
    double lon = strtod(f[ilo], &end);  if (end == f[ilo]) { db->rejected++; return; }
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) { db->rejected++; return; }     /* colonne decalee : un identifiant pris pour une latitude */
    if (db->filter && (lat < db->lat_min || lat > db->lat_max || lon < db->lon_min || lon > db->lon_max)) { db->skipped++; return; }
    int16_t ele = COLS_NO_ELE;
    if (ial >= 0 && ial < nf && f[ial][0]) { double v = strtod(f[ial], &end); if (end != f[ial] && v > -500.0 && v < 9000.0) ele = (int16_t)lround(v); }
    size_t len = strlen(f[in]);
    if (len == 0) return;
    if (db->n >= db->max_e || db->pool_used + len + 1 > db->pool_size) { db->truncated = 1; return; }
    cols_entry_t *e = &db->e[db->n++];
    e->lat = (float)lat; e->lon = (float)lon; e->ele = ele; e->kind = (uint8_t)kind;
    e->name_off = db->pool_used;
    memcpy(db->pool + db->pool_used, f[in], len + 1);
    db->pool_used += (uint32_t)len + 1;
}

int cols_load(gpx_read_fn rd, void *ctx, cols_db_t *db)
{
    char buf[512], line[200];
    int ll = 0;
    size_t k;
    while ((k = rd(ctx, buf, sizeof(buf))) > 0) {
        for (size_t i = 0; i < k; i++) {
            char c = buf[i];
            if (c == '\n') { line[ll] = '\0'; add_line(db, line); ll = 0; }
            else if (c != '\r' && ll < (int)sizeof(line) - 1) line[ll++] = c;
        }
    }
    if (ll > 0) { line[ll] = '\0'; add_line(db, line); }
    return db->n;
}

/* ---------- correspondance ---------- */
static double dist_m(double lat1, double lon1, double lat2, double lon2)
{
    double dy = (lat2 - lat1) * M_PER_DEG;
    double dx = (lon2 - lon1) * M_PER_DEG * cos(0.5 * (lat1 + lat2) * DEG_RAD);
    return sqrt(dx * dx + dy * dy);
}

int cols_match_summit(const cols_db_t *db, const cols_cfg_t *cfg, double lat, double lon, float ele_m, float *dist_out)
{
    int best = -1;
    double best_score = 1e30;
    double box = (double)cfg->radius_m / M_PER_DEG * 1.5;
    for (int i = 0; i < db->n; i++) {
        const cols_entry_t *e = &db->e[i];
        if (e->kind != COLS_KIND_COL) continue;
        if (fabs(e->lat - lat) > box) continue;
        double d = dist_m(lat, lon, e->lat, e->lon);
        if (d > cfg->radius_m) continue;
        double de = 0.0;
        if (e->ele != COLS_NO_ELE) {
            de = fabs((double)e->ele - (double)ele_m);
            if (de > cfg->ele_tol_m) continue;
        }
        double score = d + (double)cfg->ele_weight * de;
        if (score < best_score) { best_score = score; best = i; if (dist_out) *dist_out = (float)d; }
    }
    return best;
}

int cols_match_place(const cols_db_t *db, const cols_cfg_t *cfg, double lat, double lon, float *dist_out)
{
    int best = -1;
    double best_d = 1e30;
    double box = (double)cfg->place_radius_m / M_PER_DEG * 1.5;
    for (int i = 0; i < db->n; i++) {
        const cols_entry_t *e = &db->e[i];
        if (e->kind != COLS_KIND_LIEU) continue;
        if (fabs(e->lat - lat) > box) continue;
        double d = dist_m(lat, lon, e->lat, e->lon);
        if (d <= cfg->place_radius_m && d < best_d) { best_d = d; best = i; if (dist_out) *dist_out = (float)d; }
    }
    return best;
}

/* ---------- "Montee de ..." ---------- */
static int starts_with(const char *s, const char *p) { return strncmp(s, p, strlen(p)) == 0; }

static int is_vowel_start(const char *s)
{
    unsigned char c = (unsigned char)s[0];
    if (strchr("aeiouyAEIOUY", c) && c != 0) return 1;
    if (c == 0xC3) {                               /* lettres accentuees UTF-8 (Latin-1) */
        unsigned char d = (unsigned char)s[1];
        if ((d >= 0x80 && d <= 0x85) || (d >= 0x88 && d <= 0x8F) || (d >= 0x92 && d <= 0x96) || (d >= 0x99 && d <= 0x9C)) return 1;
        if ((d >= 0xA0 && d <= 0xA5) || (d >= 0xA8 && d <= 0xAF) || (d >= 0xB2 && d <= 0xB6) || (d >= 0xB9 && d <= 0xBC)) return 1;
    }
    return 0;
}

static void copy_trunc(char *out, size_t n, const char *s)
{
    size_t len = strlen(s);
    if (len >= n) {
        len = n - 1;
        while (len > 0 && ((unsigned char)s[len] & 0xC0) == 0x80) len--;      /* ne pas couper un caractere UTF-8 */
    }
    memcpy(out, s, len);
    out[len] = '\0';
}

void cols_montee_de(const char *place, char *out, size_t n)
{
    char tmp[96];
    if (starts_with(place, "Les "))      snprintf(tmp, sizeof tmp, "Mont\xC3\xA9" "e des %s", place + 4);
    else if (starts_with(place, "Le "))  snprintf(tmp, sizeof tmp, "Mont\xC3\xA9" "e du %s", place + 3);
    else if (starts_with(place, "La "))  snprintf(tmp, sizeof tmp, "Mont\xC3\xA9" "e de la %s", place + 3);
    else if (starts_with(place, "L'"))   snprintf(tmp, sizeof tmp, "Mont\xC3\xA9" "e de l'%s", place + 2);
    else if (is_vowel_start(place))      snprintf(tmp, sizeof tmp, "Mont\xC3\xA9" "e d'%s", place);
    else                                 snprintf(tmp, sizeof tmp, "Mont\xC3\xA9" "e de %s", place);
    copy_trunc(out, n, tmp);
}

int cols_name_climbs(const cols_db_t *db, const cols_cfg_t *cfg, const route_t *r, const float *ele,
                     climb_t *climbs, int n)
{
    int named = 0;
    for (int i = 0; i < n; i++) {
        climb_t *c = &climbs[i];
        c->name[0] = '\0';
        double lat, lon;
        route_unproject(r, r->x[c->end_idx], r->y[c->end_idx], &lat, &lon);
        int k = cols_match_summit(db, cfg, lat, lon, ele[c->end_idx], NULL);
        if (k >= 0) { copy_trunc(c->name, sizeof c->name, cols_name(db, k)); named++; continue; }
        int ks = -1, kd = -1;
        if (cfg->place_mode != COLS_PLACE_START) ks = cols_match_place(db, cfg, lat, lon, NULL);      /* lieu au sommet */
        if (cfg->place_mode != COLS_PLACE_SUMMIT) {
            double la0, lo0;
            route_unproject(r, r->x[c->start_idx], r->y[c->start_idx], &la0, &lo0);
            kd = cols_match_place(db, cfg, la0, lo0, NULL);                                            /* lieu au depart */
        }
        if (cfg->place_mode == COLS_PLACE_BOTH && ks >= 0 && kd >= 0 && strcmp(cols_name(db, ks), cols_name(db, kd)) != 0) {
            char tmp[96];
            snprintf(tmp, sizeof tmp, "%s > %s", cols_name(db, kd), cols_name(db, ks));
            copy_trunc(c->name, sizeof c->name, tmp);
            named++;
        } else {
            k = ks >= 0 ? ks : kd;
            if (k >= 0) { cols_montee_de(cols_name(db, k), c->name, sizeof c->name); named++; }
        }
    }
    return named;
}
