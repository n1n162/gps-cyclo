#include "alerts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void alerts_cfg_default(alerts_cfg_t *c)
{
    memset(c, 0, sizeof(*c));
    for (int i = 0; i < ALERT_KINDS; i++) c->enabled[i] = 1;
    c->enabled[ALERT_HR_HIGH] = 0;              /* demande une limite personnelle : desactivee par defaut */
    c->hr_max_bpm = 180;
    c->battery_warn_pct = 20;
    c->battery_crit_pct = 10;
    c->off_route_s = 10;
    c->gps_lost_s = 5;
    c->hr_high_s = 10;
    c->hr_sensor_s = 10;
    c->repeat_s = 30;
    c->climb_soon_m = 300.0f;
    c->info_timeout_s = 6;
    c->warn_timeout_s = 10;
}

void alerts_init(alerts_t *a, const alerts_cfg_t *cfg)
{
    memset(a, 0, sizeof(*a));
    a->cfg = *cfg;
    a->last_soon_id = -1;
}

void alerts_set_cfg(alerts_t *a, const alerts_cfg_t *cfg) { a->cfg = *cfg; }

static void raise_alert(alerts_t *a, int k, alert_level_t lv, uint32_t t, const char *title, const char *sub)
{
    alert_state_t *s = &a->st[k];
    s->active = 1; s->acked = 0; s->clearing = 0;
    s->level = lv; s->raised_s = t;
    snprintf(s->title, sizeof s->title, "%s", title);
    snprintf(s->sub, sizeof s->sub, "%s", sub);
    a->raised_now++;
}

static void clear_alert(alerts_t *a, int k)
{
    alert_state_t *s = &a->st[k];
    s->active = 0; s->acked = 0; s->clearing = 0; s->cond = 0;
}

/* Vrai quand la condition est vraie sur need_s mesures consecutives (une par seconde). */
static int held(alert_state_t *s, int cond, uint32_t t, int need_s)
{
    if (!cond) { s->cond = 0; return 0; }
    if (!s->cond) { s->cond = 1; s->cond_since = t; }
    return (int)(t - s->cond_since) + 1 >= need_s;
}

/* Efface une alerte active quand la condition de retour dure depuis clear_s secondes. */
static void maybe_clear(alerts_t *a, int k, int ok, uint32_t t, int clear_s)
{
    alert_state_t *s = &a->st[k];
    if (!s->active) return;
    if (!ok) { s->clearing = 0; return; }
    if (!s->clearing) { s->clearing = 1; s->clear_since = t; }
    if ((int)(t - s->clear_since) + 1 >= clear_s) clear_alert(a, k);
}

/* alerte a afficher : la plus grave, puis la plus recente */
static void select_current(alerts_t *a)
{
    int best = -1, n = 0;
    for (int k = 0; k < ALERT_KINDS; k++) {
        const alert_state_t *s = &a->st[k];
        if (!s->active || s->acked) continue;
        n++;
        if (best < 0 || s->level > a->st[best].level || (s->level == a->st[best].level && s->raised_s > a->st[best].raised_s)) best = k;
    }
    a->has_cur = best >= 0;
    a->pending = n > 0 ? n - 1 : 0;
    if (best >= 0) {
        a->cur.kind = (alert_kind_t)best;
        a->cur.level = a->st[best].level;
        a->cur.raised_s = a->st[best].raised_s;
        snprintf(a->cur.title, sizeof a->cur.title, "%s", a->st[best].title);
        snprintf(a->cur.sub, sizeof a->cur.sub, "%s", a->st[best].sub);
    }
}

int alerts_update(alerts_t *a, const alerts_input_t *in)
{
    const alerts_cfg_t *c = &a->cfg;
    uint32_t t = in->time_s;
    char sub[64];
    a->raised_now = 0;

    for (int k = 0; k < ALERT_KINDS; k++) if (!c->enabled[k] && a->st[k].active) clear_alert(a, k);

    /* hors parcours */
    if (c->enabled[ALERT_OFF_ROUTE]) {
        alert_state_t *s = &a->st[ALERT_OFF_ROUTE];
        int off = in->gps_ok && !in->on_route;
        if (!s->active) {
            if (held(s, off, t, c->off_route_s)) {
                if (in->off_m >= 0.0f) snprintf(sub, sizeof sub, "à %.0f m de la trace", (double)in->off_m);
                else snprintf(sub, sizeof sub, "trace non retrouvée");
                raise_alert(a, ALERT_OFF_ROUTE, ALERT_WARN, t, "HORS PARCOURS", sub);
            }
        } else {
            if (in->off_m >= 0.0f) snprintf(s->sub, sizeof s->sub, "à %.0f m de la trace", (double)in->off_m);
            maybe_clear(a, ALERT_OFF_ROUTE, in->on_route, t, 3);
        }
    }

    /* batterie : avertissement puis critique, une seule fois chacun, ré-armé quand la charge remonte */
    if (c->enabled[ALERT_BATTERY]) {
        alert_state_t *s = &a->st[ALERT_BATTERY];
        int low = held(s, in->battery_pct <= c->battery_warn_pct, t, 3);
        if (in->battery_pct >= c->battery_warn_pct + 5) { a->batt_level = 0; if (s->active) clear_alert(a, ALERT_BATTERY); }
        else if (low) {
            snprintf(sub, sizeof sub, "%d %%", in->battery_pct);
            if (in->battery_pct <= c->battery_crit_pct && a->batt_level < 2) { a->batt_level = 2; raise_alert(a, ALERT_BATTERY, ALERT_CRIT, t, "BATTERIE CRITIQUE", sub); }
            else if (a->batt_level < 1) { a->batt_level = 1; raise_alert(a, ALERT_BATTERY, ALERT_WARN, t, "BATTERIE FAIBLE", sub); }
        }
        if (s->active) snprintf(s->sub, sizeof s->sub, "%d %%", in->battery_pct);
    }

    /* frequence cardiaque trop elevee */
    if (c->enabled[ALERT_HR_HIGH]) {
        alert_state_t *s = &a->st[ALERT_HR_HIGH];
        int high = in->hr >= 0 && in->hr > c->hr_max_bpm;
        if (!s->active) {
            if (held(s, high, t, c->hr_high_s)) {
                snprintf(sub, sizeof sub, "%d bpm, limite %d", in->hr, c->hr_max_bpm);
                raise_alert(a, ALERT_HR_HIGH, ALERT_WARN, t, "FRÉQ. CARDIAQUE ÉLEVÉE", sub);
            }
        } else {
            if (in->hr >= 0) snprintf(s->sub, sizeof s->sub, "%d bpm, limite %d", in->hr, c->hr_max_bpm);
            maybe_clear(a, ALERT_HR_HIGH, in->hr < 0 || in->hr <= c->hr_max_bpm - 5, t, 5);
        }
    }

    /* signal GPS perdu */
    if (c->enabled[ALERT_GPS_LOST]) {
        alert_state_t *s = &a->st[ALERT_GPS_LOST];
        if (!s->active) { if (held(s, !in->gps_ok, t, c->gps_lost_s)) raise_alert(a, ALERT_GPS_LOST, ALERT_WARN, t, "SIGNAL GPS PERDU", "position indisponible"); }
        else maybe_clear(a, ALERT_GPS_LOST, in->gps_ok, t, 3);
    }

    /* ceinture cardio perdue : seulement si elle avait fonctionne au moins 10 s */
    if (c->enabled[ALERT_HR_SENSOR]) {
        alert_state_t *s = &a->st[ALERT_HR_SENSOR];
        if (in->hr >= 0) {
            if (!a->hr_ok_run) { a->hr_ok_run = 1; a->hr_ok_since = t; }
            if ((int)(t - a->hr_ok_since) + 1 >= 10) a->hr_seen = 1;
        } else a->hr_ok_run = 0;
        if (!s->active) { if (held(s, a->hr_seen && in->hr < 0, t, c->hr_sensor_s)) raise_alert(a, ALERT_HR_SENSOR, ALERT_WARN, t, "CEINTURE CARDIO PERDUE", "vérifie la ceinture"); }
        else maybe_clear(a, ALERT_HR_SENSOR, in->hr >= 0, t, 3);
    }

    /* montee proche : une seule annonce par montee */
    if (c->enabled[ALERT_CLIMB_SOON]) {
        alert_state_t *s = &a->st[ALERT_CLIMB_SOON];
        int near = !in->in_climb && in->next_climb_id >= 0 && in->next_climb_dist_m > 0.0f && in->next_climb_dist_m <= c->climb_soon_m;
        if (near && in->next_climb_id != a->last_soon_id) {
            char title[40];
            snprintf(title, sizeof title, "MONTÉE DANS %.0f m", (double)in->next_climb_dist_m);
            if (in->next_climb_name && in->next_climb_name[0])
                snprintf(sub, sizeof sub, "%s, %.1f km, %.1f %%", in->next_climb_name, (double)(in->next_climb_len_m / 1000.0f), (double)in->next_climb_grade_pct);
            else snprintf(sub, sizeof sub, "%.1f km, %.1f %%", (double)(in->next_climb_len_m / 1000.0f), (double)in->next_climb_grade_pct);
            a->last_soon_id = in->next_climb_id;
            raise_alert(a, ALERT_CLIMB_SOON, ALERT_INFO, t, title, sub);
        } else if (s->active && (in->in_climb || in->next_climb_id != a->last_soon_id || in->next_climb_dist_m > c->climb_soon_m + 50.0f)) clear_alert(a, ALERT_CLIMB_SOON);
    }

    /* disparition automatique et rappels */
    for (int k = 0; k < ALERT_KINDS; k++) {
        alert_state_t *s = &a->st[k];
        if (!s->active) continue;
        if (!s->acked) {
            if (s->level == ALERT_INFO && (int)(t - s->raised_s) >= c->info_timeout_s) s->acked = 1;
            else if (s->level == ALERT_WARN && (int)(t - s->raised_s) >= c->warn_timeout_s) s->acked = 1;
        } else if (c->repeat_s > 0 && (k == ALERT_OFF_ROUTE || k == ALERT_HR_HIGH) && (int)(t - s->raised_s) >= c->repeat_s) {
            s->acked = 0; s->raised_s = t; a->raised_now++;
        }
    }

    select_current(a);
    return a->raised_now;
}

const alert_t *alerts_current(const alerts_t *a) { return a->has_cur ? &a->cur : NULL; }
int alerts_pending(const alerts_t *a) { return a->pending; }

void alerts_dismiss(alerts_t *a)
{
    if (!a->has_cur) return;
    a->st[a->cur.kind].acked = 1;
    select_current(a);                     /* l'alerte suivante, s'il y en a une, apparait tout de suite */
}

int alerts_cfg_encode(const alerts_cfg_t *c, char *out, size_t n)
{
    char bits[ALERT_KINDS + 1];
    for (int i = 0; i < ALERT_KINDS; i++) bits[i] = c->enabled[i] ? '1' : '0';
    bits[ALERT_KINDS] = '\0';
    int w = snprintf(out, n, "a1;%s;%d;%d;%d", bits, c->hr_max_bpm, c->battery_warn_pct, c->battery_crit_pct);
    return (w < 0 || (size_t)w >= n) ? -1 : 0;
}

int alerts_cfg_decode(const char *s, alerts_cfg_t *c)
{
    alerts_cfg_t t = *c;
    char *e;
    if (strncmp(s, "a1;", 3) != 0) return -1;
    s += 3;
    for (int i = 0; i < ALERT_KINDS; i++) {
        if (s[i] != '0' && s[i] != '1') return -2;
        t.enabled[i] = (uint8_t)(s[i] - '0');
    }
    s += ALERT_KINDS;
    if (*s++ != ';') return -3;
    long hr = strtol(s, &e, 10);
    if (e == s || *e != ';' || hr < 80 || hr > 230) return -4;
    s = e + 1;
    long w = strtol(s, &e, 10);
    if (e == s || *e != ';' || w < 5 || w > 50) return -5;
    s = e + 1;
    long cr = strtol(s, &e, 10);
    if (e == s || *e != '\0' || cr < 1 || cr >= w) return -6;
    t.hr_max_bpm = (int)hr; t.battery_warn_pct = (int)w; t.battery_crit_pct = (int)cr;
    *c = t;
    return 0;
}
