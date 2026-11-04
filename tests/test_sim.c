#include <stdio.h>
#include <math.h>
#include <string.h>
#include "sim_engine.h"

typedef struct { FILE *f; } fctx;
static size_t rd(void *c, char *b, size_t m) { return fread(b, 1, m, ((fctx *)c)->f); }
static int fails;
static void check(const char *what, int ok, const char *d) { printf("  %-66s %s %s\n", what, ok ? "OK" : "ECHEC", d); if (!ok) fails++; }

int main(int argc, char **argv)
{
    const char *gpx = argc > 1 ? argv[1] : "sortie.gpx";
    fctx cx = { fopen(gpx, "rb") };
    if (!cx.f) { perror(gpx); return 1; }
    static float X[20000], Y[20000], E[20000], W[20000];
    route_t r; route_init(&r, X, Y, E, 20000, 10.0f);
    gpx_load(rd, &cx, &r); fclose(cx.f);
    climb_cfg_t cfg; climb_cfg_default(&cfg);
    profile_smooth(E, r.n, cfg.smooth_half, W);
    static climb_t cl[64];
    int k = climb_detect(E, r.n, W, &cfg, cl, 64);

    static sim_t s;
    char d[160];
    printf("MOTEUR DU SIMULATEUR\n");
    int n = sim_load(&s, gpx, &r, cl, k);
    snprintf(d, sizeof d, "(%d points, duree %.0f s)", n, n > 0 ? sim_duration(&s) : 0.0);
    check("chargement de la sortie (heures, FC, positions)", n > 1000 && sim_duration(&s) > 3600.0, d);
    check("la frequence cardiaque est lue", s.rows[100].hr > 40 && s.rows[100].hr < 230, "");

    double sum = 0; float mx = 0;
    for (int i = 0; i < s.n; i++) { float e = fabsf(s.pos_m[i] - s.rows[i].dist_m); sum += e; if (e > mx) mx = e; }
    snprintf(d, sizeof d, "(ecart moyen %.1f m, maximal %.1f m)", sum / s.n, (double)mx);
    check("position sur le parcours = distance reelle de la sortie", sum / s.n < 8.0 && mx < 60.0, d);

    sim_advance(&s, 10.0);
    snprintf(d, sizeof d, "(t = %.0f s, point %d)", s.t, s.idx);
    check("lecture a vitesse x1 pendant 10 s", fabs(s.t - 10.0) < 0.01 && s.idx >= 9 && s.idx <= 10, d);
    s.speed = 60.0; sim_advance(&s, 5.0);
    snprintf(d, sizeof d, "(t = %.0f s)", s.t);
    check("puis x60 pendant 5 s reelles = 300 s de sortie", fabs(s.t - 310.0) < 0.01, d);

    s.playing = 0; double t0 = s.t; sim_advance(&s, 100.0);
    check("en pause, le temps ne bouge pas", s.t == t0, "");
    s.playing = 1;

    sim_seek_row(&s, 0);
    sim_next_climb(&s, +1);
    snprintf(d, sizeof d, "(position %.0f m, depart de la montee 1 a %.0f m)", (double)s.pos_m[s.idx], (double)cl[0].start_m);
    check("'montee suivante' : 500 m avant la premiere montee", fabsf(s.pos_m[s.idx] - (cl[0].start_m - 500.0f)) < 15.0f, d);
    sim_next_climb(&s, +1);
    snprintf(d, sizeof d, "(position %.0f m, montee 2 a %.0f m)", (double)s.pos_m[s.idx], (double)cl[1].start_m);
    check("encore : 500 m avant la deuxieme montee", fabsf(s.pos_m[s.idx] - (cl[1].start_m - 500.0f)) < 15.0f, d);
    sim_next_climb(&s, -1);
    snprintf(d, sizeof d, "(position %.0f m)", (double)s.pos_m[s.idx]);
    check("'montee precedente' : retour avant la premiere", fabsf(s.pos_m[s.idx] - (cl[0].start_m - 500.0f)) < 15.0f, d);
    sim_seek_pos(&s, 50000.0f);
    check("aller a 50 km", fabsf(s.pos_m[s.idx] - 50000.0f) < 30.0f, "");
    sim_seek_pos(&s, cl[k - 1].end_m + 5000.0f);
    sim_next_climb(&s, +1);
    snprintf(d, sizeof d, "(position %.0f m)", (double)s.pos_m[s.idx]);
    check("apres la derniere montee, 'suivante' ne bouge pas", s.pos_m[s.idx] > cl[k - 1].end_m, d);

    sim_seek_row(&s, 3000);
    sim_sample_t a;
    sim_get(&s, &a);
    check("echantillon normal : sur le parcours, GPS ok, ceinture ok", a.on_route && a.gps_ok && a.hr > 0 && a.battery_pct <= 100, "");
    snprintf(d, sizeof d, "(%.1f km, %.1f km/h de moyenne, D+ %.0f m, heure %02d:%02d)", (double)a.dist_km, (double)a.avg_kmh, (double)a.ascent_m, a.hour, a.minute);
    check("distance, vitesse moyenne, denivele et heure coherents", a.dist_km > 5.0 && a.avg_kmh > 5.0 && a.avg_kmh < 50.0 && a.ascent_m > 10.0 && a.hour < 24 && a.minute < 60, d);
    int hr0 = a.hr;
    s.f_off = 1; sim_get(&s, &a);
    check("panne 'hors parcours' : 150 m de la trace", !a.on_route && fabsf(a.off_m - 150.0f) < 0.1f, "");
    s.f_off = 0; s.f_gps = 1; sim_get(&s, &a);
    check("panne 'GPS perdu'", !a.gps_ok, "");
    s.f_gps = 0; s.f_hr = 1; sim_get(&s, &a);
    check("panne 'ceinture coupee' : pas de mesure", a.hr == -1, "");
    s.f_hr = 0; s.hr_boost = 1; sim_get(&s, &a);
    snprintf(d, sizeof d, "(%d au lieu de %d)", a.hr, hr0);
    check("'FC +40' ajoute 40 bpm", a.hr == hr0 + 40, d);
    s.hr_boost = 0;

    sim_seek_row(&s, 0); s.batt = 100.0f; s.speed = 1.0;
    sim_advance(&s, 4200.0);
    sim_get(&s, &a);
    snprintf(d, sizeof d, "(%d %% apres 70 minutes)", a.battery_pct);
    check("la batterie baisse de 1 % toutes les 7 minutes", a.battery_pct == 90, d);

    sim_seek_row(&s, 0); s.speed = 300.0; s.playing = 1;
    for (int i = 0; i < 100 && s.playing; i++) sim_advance(&s, 1.0);
    snprintf(d, sizeof d, "(point %d sur %d)", s.idx, s.n);
    check("en fin de sortie la lecture s'arrete", !s.playing && s.idx == s.n - 1, d);
    sim_free(&s);
    printf("%s\n", fails ? "ECHECS" : "TOUS LES TESTS REUSSIS");
    return fails != 0;
}
