#include "climb.h"

void climb_cfg_default(climb_cfg_t *c)
{
    c->step_m = 10.0f;
    c->smooth_half = 2;
    c->slope_half = 15;
    c->lp_alpha = 0.15f;
    c->start_pct = 3.0f;
    c->end_pct = 1.0f;
    c->gap_max_m = 500.0f;
    c->drop_max_m = 15.0f;
    c->backoff_max_m = 600.0f;
    c->backoff_tol_m = 2.0f;
    c->min_len_m = 500.0f;
    c->min_grade_pct = 3.0f;
    c->display_window_m = 100.0f;
    c->section_m = 100.0f;
}

int route_resample(const float *dist, const float *ele, int n, float step, float *out, int max_out)
{
    if (n < 2 || step <= 0.0f) return 0;
    int m = (int)(dist[n - 1] / step) + 1;
    if (m > max_out) m = max_out;
    int j = 0;
    for (int k = 0; k < m; k++) {
        float d = (float)k * step;
        while (j < n - 2 && dist[j + 1] < d) j++;
        float d0 = dist[j], d1 = dist[j + 1];
        float t = (d1 == d0) ? 0.0f : (d - d0) / (d1 - d0);
        out[k] = ele[j] + t * (ele[j + 1] - ele[j]);
    }
    return m;
}

void profile_smooth(float *ele, int n, int half, float *work)
{
    for (int k = 0; k < n; k++) {
        int a = k - half; if (a < 0) a = 0;
        int b = k + half + 1; if (b > n) b = n;
        float s = 0.0f;
        for (int i = a; i < b; i++) s += ele[i];
        work[k] = s / (float)(b - a);
    }
    for (int k = 0; k < n; k++) ele[k] = work[k];
}

/* Debut = point le plus bas, fin = point le plus haut apres lui (premier extremum rencontre). */
static void trim(const float *e, int s, int t, int *s2, int *t2)
{
    int bs = s;
    for (int k = s + 1; k <= t; k++) if (e[k] < e[bs] - 0.01f) bs = k;
    int bt = bs;
    for (int k = bs + 1; k <= t; k++) if (e[k] > e[bt] + 0.01f) bt = k;
    *s2 = bs; *t2 = bt;
}

/* Recule le depart vers le point bas precedent (route qui monte doucement avant la vraie montee).
 * On s'arrete quand l'altitude remonte de plus de 'tol' au-dessus du point bas, ou apres 'max_back' points. */
static int extend_start(const float *e, int s2, int lower_bound, int max_back, float tol)
{
    int low = s2;
    int lim = s2 - max_back;
    if (lim < lower_bound) lim = lower_bound;
    for (int k = s2 - 1; k >= lim; k--) {
        if (e[k] < e[low] - 0.01f) low = k;
        else if (e[k] - e[low] > tol) break;
    }
    return low;
}

/* Ajoute un segment brut, en le fusionnant avec le precedent s'il est assez proche. */
static void add_raw(const float *e, climb_t *out, int *count, int max_out,
                    int s, int t, const climb_cfg_t *cfg)
{
    if (*count > 0) {
        climb_t *p = &out[*count - 1];
        int ps, ta, tb, ts;
        trim(e, p->start_idx, p->end_idx, &ps, &ta);
        trim(e, s, t, &tb, &ts);
        float gap = (float)(tb - ta) * cfg->step_m;
        float lowest = e[ta];
        for (int k = ta; k <= tb; k++) if (e[k] < lowest) lowest = e[k];
        float drop = e[ta] - lowest;
        if (gap <= cfg->gap_max_m && drop <= cfg->drop_max_m) {
            p->end_idx = t;
            return;
        }
    }
    if (*count < max_out) {
        out[*count].start_idx = s;
        out[*count].end_idx = t;
        (*count)++;
    }
}

int climb_detect(const float *e, int n, float *work, const climb_cfg_t *cfg,
                 climb_t *out, int max_out)
{
    if (n < 3 || max_out <= 0) return 0;
    float *f = work;
    int W = cfg->slope_half;
    float step = cfg->step_m;

    for (int i = 0; i < n; i++) {
        int a = i - W; if (a < 0) a = 0;
        int b = i + W; if (b > n - 1) b = n - 1;
        f[i] = (b > a) ? 100.0f * (e[b] - e[a]) / ((float)(b - a) * step) : 0.0f;
    }
    for (int i = 1; i < n; i++) f[i] = f[i - 1] + cfg->lp_alpha * (f[i] - f[i - 1]);
    for (int i = n - 2; i >= 0; i--) f[i] = f[i + 1] + cfg->lp_alpha * (f[i] - f[i + 1]);

    int count = 0, inside = 0, s = 0;
    for (int i = 0; i < n; i++) {
        if (!inside && f[i] >= cfg->start_pct) {
            inside = 1; s = i;
        } else if (inside && f[i] <= cfg->end_pct) {
            inside = 0;
            add_raw(e, out, &count, max_out, s, i, cfg);
        }
    }
    if (inside) add_raw(e, out, &count, max_out, s, n - 1, cfg);

    /* Finalisation : bornes exactes, recul du depart, mesures, filtre longueur et pente. */
    int max_back = (int)(cfg->backoff_max_m / step);
    int kept = 0;
    for (int j = 0; j < count; j++) {
        int s2, t2;
        trim(e, out[j].start_idx, out[j].end_idx, &s2, &t2);
        int lb = (kept > 0) ? out[kept - 1].end_idx + 1 : 0;
        s2 = extend_start(e, s2, lb, max_back, cfg->backoff_tol_m);
        float L = (float)(t2 - s2) * step;
        if (L <= 0.0f) continue;
        float up = e[t2] - e[s2];
        float g = 100.0f * up / L;
        if (L < cfg->min_len_m || g < cfg->min_grade_pct) continue;
        climb_t c;
        c.start_idx = s2; c.end_idx = t2;
        c.start_m = (float)s2 * step; c.end_m = (float)t2 * step;
        c.length_m = L; c.avg_grade_pct = g; c.ascent_m = up; c.score = L * g;
        c.name[0] = '\0';
        out[kept++] = c;
    }
    return kept;
}

int climb_find(const climb_t *list, int count, float pos_m)
{
    for (int i = 0; i < count; i++)
        if (pos_m <= list[i].end_m) return i;
    return -1;
}

static float ele_at(const float *e, int n, float step, float pos)
{
    float x = pos / step;
    if (x <= 0.0f) return e[0];
    if (x >= (float)(n - 1)) return e[n - 1];
    int i = (int)x;
    float fr = x - (float)i;
    return e[i] + fr * (e[i + 1] - e[i]);
}

int climb_progress(const climb_t *c, const float *e, int n, float step,
                   float pos_m, climb_progress_t *out)
{
    if (pos_m < c->start_m || pos_m > c->end_m) return 0;
    out->dist_done_m = pos_m - c->start_m;
    out->dist_remaining_m = c->end_m - pos_m;
    float asc = e[c->end_idx] - ele_at(e, n, step, pos_m);
    if (asc < 0.0f) asc = 0.0f;
    out->ascent_remaining_m = asc;
    out->avg_grade_remaining_pct = (out->dist_remaining_m > 1.0f)
        ? 100.0f * asc / out->dist_remaining_m : 0.0f;
    return 1;
}

float climb_grade_at(const float *e, int n, const climb_cfg_t *cfg, float pos_m)
{
    int half = (int)(cfg->display_window_m / cfg->step_m / 2.0f + 0.5f);
    if (half < 1) half = 1;
    int i = (int)(pos_m / cfg->step_m + 0.5f);
    int a = i - half; if (a < 0) a = 0;
    int b = i + half; if (b > n - 1) b = n - 1;
    if (b <= a) return 0.0f;
    return 100.0f * (e[b] - e[a]) / ((float)(b - a) * cfg->step_m);
}

static uint8_t level_of(float g)
{
    if (g < 1.0f) return 0;
    if (g < 3.0f) return 1;
    if (g < 6.0f) return 2;
    if (g < 9.0f) return 3;
    if (g < 12.0f) return 4;
    return 5;
}

int climb_sections(const climb_t *c, const float *e, int n, const climb_cfg_t *cfg,
                   climb_section_t *out, int max_out)
{
    (void)n;
    int len = (int)(cfg->section_m / cfg->step_m + 0.5f);
    if (len < 1) len = 1;
    int count = 0;
    for (int a = c->start_idx; a < c->end_idx && count < max_out; a += len) {
        int b = a + len; if (b > c->end_idx) b = c->end_idx;
        if (b <= a) break;
        float g = 100.0f * (e[b] - e[a]) / ((float)(b - a) * cfg->step_m);
        out[count].start_m = (float)a * cfg->step_m;
        out[count].end_m = (float)b * cfg->step_m;
        out[count].grade_pct = g;
        out[count].level = level_of(g);
        count++;
    }
    return count;
}

int climb_category(float score)
{
    if (score > 80000.0f) return 0;
    if (score > 64000.0f) return 1;
    if (score > 32000.0f) return 2;
    if (score > 16000.0f) return 3;
    if (score > 8000.0f) return 4;
    return 5;
}

float climb_grade_ahead(const float *e, int n, float step, float pos_m, float dist_m)
{
    float end = pos_m + dist_m;
    float max_m = (float)(n - 1) * step;
    if (end > max_m) end = max_m;
    if (end - pos_m < 1.0f) return 0.0f;
    return 100.0f * (ele_at(e, n, step, end) - ele_at(e, n, step, pos_m)) / (end - pos_m);
}

int profile_sections(const float *e, int n, float step, float from_m, float to_m,
                     float section_m, climb_section_t *out, int max_out)
{
    int a0 = (int)(from_m / step + 0.5f);
    int bend = (int)(to_m / step + 0.5f);
    if (a0 < 0) a0 = 0;
    if (bend > n - 1) bend = n - 1;
    int len = (int)(section_m / step + 0.5f);
    if (len < 1) len = 1;
    int count = 0;
    for (int a = a0; a < bend && count < max_out; a += len) {
        int b = a + len;
        if (b > bend) b = bend;
        if (b <= a) break;
        float g = 100.0f * (e[b] - e[a]) / ((float)(b - a) * step);
        out[count].start_m = (float)a * step;
        out[count].end_m = (float)b * step;
        out[count].grade_pct = g;
        out[count].level = level_of(g);
        count++;
    }
    return count;
}
