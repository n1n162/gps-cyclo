#include <stdio.h>
#include <string.h>
#include "alerts.h"

static alerts_t A;
static alerts_cfg_t cfg;
static uint32_t T;
static int fails;

static alerts_input_t base(void)
{
    alerts_input_t in;
    memset(&in, 0, sizeof in);
    in.gps_ok = 1; in.on_route = 1; in.off_m = 0.0f; in.hr = 140; in.battery_pct = 80; in.next_climb_id = -1;
    return in;
}
static int step(alerts_input_t in) { in.time_s = ++T; return alerts_update(&A, &in); }
static int run(alerts_input_t in, int secs) { int n = 0; for (int i = 0; i < secs; i++) n += step(in); return n; }
static void reset(void) { alerts_cfg_default(&cfg); alerts_init(&A, &cfg); T = 0; }
static void check(const char *what, int ok) { printf("  %-70s %s\n", what, ok ? "OK" : "ECHEC"); if (!ok) fails++; }
static int cur_is(alert_kind_t k) { const alert_t *c = alerts_current(&A); return c && c->kind == k; }
static int none(void) { return alerts_current(&A) == NULL; }

int main(void)
{
    alerts_input_t in;

    printf("HORS PARCOURS\n");
    reset();
    in = base(); in.on_route = 0; in.off_m = 85.0f;
    run(in, 9);
    check("9 s hors parcours : pas d'alerte (delai de 10 s)", none());
    int n = step(in);
    check("10 s : l'alerte se declenche", cur_is(ALERT_OFF_ROUTE) && n == 1);
    check("titre et distance affiches", strcmp(alerts_current(&A)->title, "HORS PARCOURS") == 0 && strstr(alerts_current(&A)->sub, "85 m") != NULL);
    alerts_dismiss(&A);
    check("toucher la banniere l'acquitte", none());
    run(in, 29);
    check("29 s apres : toujours acquittee", none());
    n = step(in);
    check("30 s apres : rappel tant que le probleme dure", cur_is(ALERT_OFF_ROUTE) && n == 1);
    in.on_route = 1; in.off_m = 5.0f;
    run(in, 2);
    check("de retour sur la trace 2 s : l'alerte reste (il faut 3 s)", cur_is(ALERT_OFF_ROUTE));
    step(in);
    check("3 s : l'alerte s'efface", none());
    reset();
    in = base(); in.on_route = 0; in.off_m = 50.0f;
    run(in, 10);
    run(in, 10);
    check("sans toucher, un avertissement disparait seul apres 10 s", none());
    reset();
    in = base(); in.gps_ok = 0; in.on_route = 0; in.off_m = -1.0f;
    run(in, 12);
    check("sans signal GPS, pas de fausse alerte 'hors parcours'", !cur_is(ALERT_OFF_ROUTE));

    printf("BATTERIE\n");
    reset();
    in = base(); in.battery_pct = 21; run(in, 20);
    check("21 % : rien", none());
    in.battery_pct = 20; run(in, 2);
    check("20 % pendant 2 s : rien (delai anti-rebond de 3 s)", none());
    step(in);
    check("20 % pendant 3 s : batterie faible", cur_is(ALERT_BATTERY) && alerts_current(&A)->level == ALERT_WARN && strstr(alerts_current(&A)->sub, "20 %") != NULL);
    alerts_dismiss(&A);
    in.battery_pct = 10; run(in, 3);
    check("10 % : batterie critique (nouvelle alerte)", cur_is(ALERT_BATTERY) && alerts_current(&A)->level == ALERT_CRIT);
    run(in, 60);
    check("une alerte critique ne disparait pas seule", cur_is(ALERT_BATTERY));
    alerts_dismiss(&A);
    in.battery_pct = 9; run(in, 20);
    check("acquittee : pas de nouvelle alerte a 9 %", none());
    in.battery_pct = 30; run(in, 3);
    in.battery_pct = 20; run(in, 3);
    check("charge remontee puis redescendue : l'alerte est de nouveau armee", cur_is(ALERT_BATTERY));
    reset();
    in = base();
    for (int i = 0; i < 20; i++) { in.battery_pct = (i % 2) ? 21 : 20; step(in); }
    check("mesure qui oscille autour de 20 % : pas d'alerte", none());

    printf("FREQUENCE CARDIAQUE\n");
    reset();
    in = base(); in.hr = 190; run(in, 30);
    check("desactivee par defaut : aucune alerte meme a 190 bpm", none());
    cfg.enabled[ALERT_HR_HIGH] = 1; cfg.hr_max_bpm = 170; alerts_set_cfg(&A, &cfg);
    in.hr = 171; run(in, 9);
    check("171 bpm pendant 9 s (limite 170) : rien", none());
    step(in);
    check("10 s : alerte avec la valeur et la limite", cur_is(ALERT_HR_HIGH) && strstr(alerts_current(&A)->sub, "171 bpm, limite 170") != NULL);
    alerts_dismiss(&A);
    in.hr = 168; run(in, 10);
    check("168 bpm (entre limite et limite - 5) : l'alerte n'est pas effacee a tort", A.st[ALERT_HR_HIGH].active);
    in.hr = 160; run(in, 4);
    check("160 bpm pendant 4 s : encore active", A.st[ALERT_HR_HIGH].active);
    step(in);
    check("5 s : effacee", !A.st[ALERT_HR_HIGH].active);

    printf("SIGNAL GPS ET CEINTURE\n");
    reset();
    in = base(); in.gps_ok = 0; run(in, 4);
    check("GPS perdu 4 s : rien", none());
    step(in);
    check("5 s : signal GPS perdu", cur_is(ALERT_GPS_LOST));
    in.gps_ok = 1; run(in, 2); check("signal revenu 2 s : encore affichee ou acquittee, pas effacee", A.st[ALERT_GPS_LOST].active);
    step(in); check("3 s : effacee", !A.st[ALERT_GPS_LOST].active);
    reset();
    in = base(); in.hr = -1; run(in, 40);
    check("ceinture jamais detectee : pas d'alerte", none());
    reset();
    in = base(); in.hr = 140; run(in, 12);
    in.hr = -1; run(in, 9);
    check("ceinture coupee 9 s : rien", none());
    step(in);
    check("10 s : ceinture perdue", cur_is(ALERT_HR_SENSOR));
    in.hr = 140; run(in, 2); check("retour 2 s : encore active", A.st[ALERT_HR_SENSOR].active);
    step(in); check("3 s : effacee", !A.st[ALERT_HR_SENSOR].active);
    reset();
    in = base(); in.hr = 140; run(in, 12);
    in.hr = -1; run(in, 3); in.hr = 140; run(in, 20);
    check("coupure de 3 s : pas d'alerte", none() && !A.st[ALERT_HR_SENSOR].active);

    printf("MONTEE PROCHE\n");
    reset();
    in = base(); in.next_climb_id = 4; in.next_climb_name = "Col du Mont Tournier"; in.next_climb_len_m = 10390.0f; in.next_climb_grade_pct = 5.6f;
    in.next_climb_dist_m = 400.0f; run(in, 3);
    check("montee a 400 m : rien", none());
    in.next_climb_dist_m = 300.0f; n = step(in);
    check("a 300 m : annonce", cur_is(ALERT_CLIMB_SOON) && n == 1 && alerts_current(&A)->level == ALERT_INFO);
    check("titre et description", strcmp(alerts_current(&A)->title, "MONTÉE DANS 300 m") == 0 && strstr(alerts_current(&A)->sub, "Col du Mont Tournier, 10.4 km, 5.6 %") != NULL);
    in.next_climb_dist_m = 250.0f; run(in, 6);
    check("un message d'information disparait seul apres 6 s", none());
    in.next_climb_dist_m = 200.0f; n = step(in);
    check("pas de deuxieme annonce pour la meme montee", none() && n == 0);
    in.in_climb = 1; in.next_climb_dist_m = 0.0f; step(in);
    check("pendant la montee : etat remis a zero", !A.st[ALERT_CLIMB_SOON].active);
    in.in_climb = 0; in.next_climb_id = 5; in.next_climb_dist_m = 280.0f; n = step(in);
    check("montee suivante : nouvelle annonce", cur_is(ALERT_CLIMB_SOON) && n == 1);

    printf("PRIORITES ET REGLAGES\n");
    reset();
    in = base(); in.on_route = 0; in.off_m = 40.0f; in.battery_pct = 8;
    run(in, 10);
    check("hors parcours et batterie critique en meme temps : la critique est affichee", cur_is(ALERT_BATTERY) && alerts_pending(&A) == 1);
    alerts_dismiss(&A);
    step(in);
    check("la critique acquittee, l'autre apparait", cur_is(ALERT_OFF_ROUTE) && alerts_pending(&A) == 0);
    reset();
    cfg.enabled[ALERT_OFF_ROUTE] = 0; alerts_set_cfg(&A, &cfg);
    in = base(); in.on_route = 0; run(in, 60);
    check("alerte desactivee : jamais declenchee", none());
    reset();
    in = base(); in.on_route = 0; in.off_m = 60.0f; run(in, 10);
    cfg.enabled[ALERT_OFF_ROUTE] = 0; alerts_set_cfg(&A, &cfg); step(in);
    check("desactiver une alerte active l'efface", none() && !A.st[ALERT_OFF_ROUTE].active);

    printf("REGLAGES ENREGISTRABLES\n");
    alerts_cfg_default(&cfg);
    char enc[64]; alerts_cfg_encode(&cfg, enc, sizeof enc);
    check("reglages par defaut", strcmp(enc, "a1;110111;180;20;10") == 0);
    alerts_cfg_t t; alerts_cfg_default(&t); t.enabled[ALERT_BATTERY] = 0; t.hr_max_bpm = 172; t.battery_warn_pct = 25; t.battery_crit_pct = 12;
    alerts_cfg_encode(&t, enc, sizeof enc);
    alerts_cfg_t back; alerts_cfg_default(&back);
    check("enregistrer puis relire : memes reglages", alerts_cfg_decode(enc, &back) == 0 && !back.enabled[ALERT_BATTERY] && back.hr_max_bpm == 172 && back.battery_warn_pct == 25 && back.battery_crit_pct == 12);
    const char *bad[] = {"", "a2;110111;180;20;10", "a1;11011;180;20;10", "a1;110111;50;20;10", "a1;110111;180;3;2", "a1;110111;180;20;20", "a1;110112;180;20;10", "a1;110111;abc;20;10", "a1;110111;180;20;10;5"};
    int ok = 1;
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) { alerts_cfg_t x = back; if (alerts_cfg_decode(bad[i], &x) == 0 || memcmp(&x, &back, sizeof x) != 0) ok = 0; }
    check("9 reglages invalides refuses sans rien modifier", ok);

    printf("%s\n", fails ? "ECHECS" : "TOUS LES TESTS REUSSIS");
    return fails != 0;
}
