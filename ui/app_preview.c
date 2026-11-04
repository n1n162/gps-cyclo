/* Simulation d'une sortie sur PC : la page de montee s'ouvre seule, la fenetre de fin s'affiche, le toucher fonctionne.
 * Usage (depuis tests/data) : app_preview [a|b|c]
 *   a : parcours complet sans intervention        b : l'utilisateur quitte la page de montee au doigt
 *   c : l'utilisateur ferme la fenetre de fin en touchant OK    f : champs personnalisables (appui long, selecteur, 4/6/8 champs)
 *   g : alertes (hors parcours, batterie, frequence cardiaque), acquittement, reglages */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "app.h"
#include "cols.h"

typedef struct { FILE *f; } fctx;
static size_t rd(void *c, char *buf, size_t max) { return fread(buf, 1, max, ((fctx *)c)->f); }

static uint16_t fb[UI_H][UI_W];
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    const uint16_t *src = (const uint16_t *)px;
    int w = area->x2 - area->x1 + 1;
    for (int y = area->y1; y <= area->y2; y++)
        for (int x = area->x1; x <= area->x2; x++) fb[y][x] = src[(y - area->y1) * w + (x - area->x1)];
    lv_display_flush_ready(disp);
}

static int ptr_x, ptr_y, ptr_down;
static void read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point.x = ptr_x; data->point.y = ptr_y;
    data->state = ptr_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static void step(int ms) { lv_tick_inc((uint32_t)ms); lv_timer_handler(); }
static void settle(void) { for (int i = 0; i < 14; i++) step(30); }
static void tap(int x, int y) { ptr_x = x; ptr_y = y; ptr_down = 1; step(40); step(40); ptr_down = 0; step(40); step(40); }
static void swipe(int x0, int x1, int y)
{
    ptr_x = x0; ptr_y = y; ptr_down = 1; step(40); step(40);
    for (int i = 1; i <= 6; i++) { ptr_x = x0 + (x1 - x0) * i / 6; step(40); }
    ptr_down = 0; step(40); step(40); settle();
}
static void longpress(int x, int y) { ptr_x = x; ptr_y = y; ptr_down = 1; for (int i = 0; i < 14; i++) step(40); ptr_down = 0; step(40); step(40); settle(); }
static void drag_v(int x, int y0, int y1)
{
    ptr_x = x; ptr_y = y0; ptr_down = 1; step(40); step(40);
    for (int i = 1; i <= 6; i++) { ptr_y = y0 + (y1 - y0) * i / 6; step(40); }
    ptr_down = 0; step(40); step(40); settle();
}
/* Amene une ligne du selecteur dans la zone visible par petits glissements du doigt. Retourne 1 si elle est visible. */
static int bring_into_view(ui_app_t *app, int id)
{
    lv_area_t la, ra;
    for (int k = 0; k < 8; k++) {
        lv_obj_get_coords(app->p_list, &la);
        lv_obj_get_coords(app->p_row[id], &ra);
        if (ra.y1 >= la.y1 && ra.y2 <= la.y2) return 1;
        if (ra.y1 < la.y1) drag_v(150, 200, 270); else drag_v(150, 330, 260);
        for (int q = 0; q < 6; q++) settle();
    }
    lv_obj_get_coords(app->p_list, &la);
    lv_obj_get_coords(app->p_row[id], &ra);
    return ra.y1 >= la.y1 && ra.y2 <= la.y2;
}
static void save(const char *name) { FILE *f = fopen(name, "wb"); fwrite(fb, sizeof(fb), 1, f); fclose(f); }

static int n_alert, n_acfg; static alert_kind_t last_alert_kind; static alerts_cfg_t last_acfg;
static void h_alert(const alert_t *a) { n_alert++; last_alert_kind = a->kind; }
static void h_acfg(const alerts_cfg_t *c) { n_acfg++; last_acfg = *c; }
static int n_cfg; static ui_config_t last_cfg;
static void h_cfg(const ui_config_t *c) { n_cfg++; last_cfg = *c; }

typedef struct { unsigned t; double lat, lon, ele, dist, sp; int hr, cad; } row_t;
static row_t R[20000]; static int NR;
static int fails;
static void check(const char *what, int ok, const char *detail) { printf("  %-62s %s %s\n", what, ok ? "OK" : "ECHEC", detail); if (!ok) fails++; }

static cols_entry_t ent[4000]; static char pool[100000];

int main(int argc, char **argv)
{
    char mode = argc > 1 ? argv[1][0] : 'a';
    fctx cx = { fopen("sortie.gpx", "rb") };
    static float X[20000], Y[20000], E[20000], W[20000];
    route_t r; route_init(&r, X, Y, E, 20000, 10.0f);
    gpx_load(rd, &cx, &r); fclose(cx.f);
    climb_cfg_t cfg; climb_cfg_default(&cfg);
    profile_smooth(E, r.n, cfg.smooth_half, W);
    static climb_t cl[64];
    int k = climb_detect(E, r.n, W, &cfg, cl, 64);
    cols_db_t db; cols_init(&db, ent, 4000, pool, sizeof pool);
    cols_filter_route(&db, &r, 3000.0f);
    { fctx cf = { fopen("gps.csv", "rb") }; if (cf.f) { cols_load(rd, &cf, &db); fclose(cf.f); } }
    { cols_cfg_t cc; cols_cfg_default(&cc); cols_name_climbs(&db, &cc, &r, E, cl, k); }
    FILE *fp = fopen("replay2.csv", "r"); int pw;
    while (NR < 20000 && fscanf(fp, "%u,%lf,%lf,%lf,%lf,%lf,%d,%d,%d", &R[NR].t, &R[NR].lat, &R[NR].lon, &R[NR].ele, &R[NR].dist, &R[NR].sp, &R[NR].hr, &R[NR].cad, &pw) == 9) NR++;
    fclose(fp);

    lv_init();
    lv_display_t *disp = lv_display_create(UI_W, UI_H);
    static uint16_t buf[UI_W * UI_H];
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_indev_t *ind = lv_indev_create();
    lv_indev_set_type(ind, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(ind, read_cb);
    lv_indev_set_display(ind, disp);

    ui_course_t course = { &r, E, r.n, r.step_m, cl, k, &cfg };
    static ui_app_t app;
    ui_app_init(&app, &course);
    int obj0 = ui_app_object_count(&app);

    /* verite terrain pour la premiere montee, lue dans les donnees brutes */
    const climb_t *c0 = &cl[0];
    unsigned t_in = 0, t_out = 0; double hr_sum = 0; int hr_n = 0;
    for (int i = 0; i < NR; i++) {
        if (R[i].dist >= c0->start_m && !t_in) t_in = R[i].t;
        if (R[i].dist >= c0->start_m && R[i].dist <= c0->end_m) { hr_sum += R[i].hr; hr_n++; t_out = R[i].t; }
    }
    int i0 = 0, i1 = 0;
    while (i0 < NR - 1 && R[i0].dist < 21000.0) i0++;
    while (i1 < NR - 1 && R[i1].dist < 36000.0) i1++;
    printf("MODE %c : parcours de %.1f km a %.1f km, montee 1 = %s (km %.2f a %.2f)\n", mode, R[i0].dist / 1000.0, R[i1].dist / 1000.0, c0->name, c0->start_m / 1000.0, c0->end_m / 1000.0);



    if (mode == 'g') {
        ui_hooks.alert = h_alert; ui_hooks.alerts_changed = h_acfg;
        ui_sample_t s; memset(&s, 0, sizeof s);
        int i = i0;
        s.hour = 8; s.minute = 29; s.pos_m = (float)R[i].dist; s.speed_kmh = 28.0f; s.dist_km = (float)(R[i].dist / 1000.0);
        s.avg_kmh = 26.0f; s.ascent_m = 108.0f; s.hr = 140; s.cad = 70; s.power = -1; s.gps_ok = 1; s.battery_pct = 80; s.on_route = 1;
        uint32_t tt = R[i].t - R[0].t;
#define TICK(n) do { for (int q_ = 0; q_ < (n); q_++) { s.time_s = ++tt; ui_app_update(&app, &s); lv_tick_inc(1000); lv_timer_handler(); } } while (0)
#define BANNER_VISIBLE() (!lv_obj_has_flag(app.b_panel, LV_OBJ_FLAG_HIDDEN))
        char d[200];
        lv_area_t ra;
        printf("TEST ALERTES\n");
        TICK(5);
        check("conditions normales : aucune alerte", !BANNER_VISIBLE() && n_alert == 0, "");

        s.on_route = 0; s.off_m = 85.0f; TICK(9);
        check("9 s hors parcours : pas encore de banniere", !BANNER_VISIBLE(), "");
        TICK(1); settle();
        snprintf(d, sizeof d, "(%s | %s)", lv_label_get_text(app.b_title), lv_label_get_text(app.b_sub));
        check("10 s : la banniere HORS PARCOURS apparait avec la distance", BANNER_VISIBLE() && strcmp(lv_label_get_text(app.b_title), "HORS PARCOURS") == 0 && strstr(lv_label_get_text(app.b_sub), "85 m") != NULL, d);
        snprintf(d, sizeof d, "(%d appel)", n_alert);
        check("l'action bip ou vibration est appelee une fois", n_alert == 1 && last_alert_kind == ALERT_OFF_ROUTE, d);
        save("screen_g1_hors_parcours.rgb565");
        tap(160, 65);
        check("toucher la banniere l'acquitte", !BANNER_VISIBLE(), "");
        s.on_route = 1; s.off_m = 0.0f; TICK(5);

        s.battery_pct = 20; TICK(3); settle();
        check("batterie a 20 % : banniere BATTERIE FAIBLE", BANNER_VISIBLE() && strcmp(lv_label_get_text(app.b_title), "BATTERIE FAIBLE") == 0, "");
        tap(160, 65);
        s.battery_pct = 10; TICK(3); settle();
        snprintf(d, sizeof d, "(%s | %s)", lv_label_get_text(app.b_title), lv_label_get_text(app.b_sub));
        check("batterie a 10 % : banniere critique", BANNER_VISIBLE() && strcmp(lv_label_get_text(app.b_title), "BATTERIE CRITIQUE") == 0, d);
        save("screen_g2_batterie_critique.rgb565");
        TICK(30);
        check("une alerte critique reste affichee sans intervention", BANNER_VISIBLE(), "");
        swipe(280, 40, 200);
        snprintf(d, sizeof d, "(page %d)", app.cur);
        check("elle reste visible quand on change de page", BANNER_VISIBLE() && app.cur == 1, d);
        swipe(40, 280, 200);
        tap(160, 65);
        s.battery_pct = 80; TICK(10);
        check("acquittee puis batterie rechargee : plus d'alerte", !BANNER_VISIBLE(), "");

        longpress(100, 14);
        check("appui long sur la barre d'etat : les reglages d'alertes s'ouvrent", !lv_obj_has_flag(app.as_panel, LV_OBJ_FLAG_HIDDEN), "");
        lv_obj_get_coords(app.as_tog[ALERT_HR_HIGH], &ra);
        tap((ra.x1 + ra.x2) / 2, (ra.y1 + ra.y2) / 2);
        tap(274, 350);
        char enc[64]; alerts_cfg_encode(ui_app_get_alerts_cfg(&app), enc, sizeof enc);
        snprintf(d, sizeof d, "(%s, %d changements enregistres)", enc, n_acfg);
        check("activer l'alerte de FC et monter la limite a 185", strcmp(enc, "a1;111111;185;20;10") == 0 && n_acfg == 2 && last_acfg.hr_max_bpm == 185, d);
        save("screen_g3_reglages.rgb565");
        lv_obj_get_coords(app.as_close, &ra);
        tap((ra.x1 + ra.x2) / 2, (ra.y1 + ra.y2) / 2);
        check("fermer les reglages", lv_obj_has_flag(app.as_panel, LV_OBJ_FLAG_HIDDEN), "");

        s.hr = 190; TICK(9);
        check("FC a 190 bpm pendant 9 s : rien", !BANNER_VISIBLE(), "");
        TICK(1); settle();
        snprintf(d, sizeof d, "(%s | %s)", lv_label_get_text(app.b_title), lv_label_get_text(app.b_sub));
        check("10 s : banniere de FC elevee avec la limite choisie", BANNER_VISIBLE() && strstr(lv_label_get_text(app.b_sub), "190 bpm, limite 185") != NULL, d);
        save("screen_g4_fc.rgb565");
        tap(160, 65);
        s.hr = 140; TICK(6);

        longpress(100, 14);
        lv_obj_get_coords(app.as_tog[ALERT_OFF_ROUTE], &ra);
        tap((ra.x1 + ra.x2) / 2, (ra.y1 + ra.y2) / 2);
        lv_obj_get_coords(app.as_close, &ra);
        tap((ra.x1 + ra.x2) / 2, (ra.y1 + ra.y2) / 2);
        s.on_route = 0; s.off_m = 60.0f; TICK(15);
        check("alerte 'hors parcours' desactivee : aucune banniere", !BANNER_VISIBLE(), "");

        int obj1 = ui_app_object_count(&app);
        snprintf(d, sizeof d, "(%d objets au depart, %d a la fin)", obj0, obj1);
        check("aucun objet cree ou perdu", obj0 == obj1, d);
        printf("%s\n", fails ? "ECHECS" : "TOUS LES TESTS REUSSIS");
        return fails != 0;
    }

    if (mode == 'f') {
        ui_hooks.config_changed = h_cfg;
        ui_sample_t s; memset(&s, 0, sizeof s);
        int i = i0;
        s.time_s = R[i].t - R[0].t; s.hour = (int)(((R[i].t + 7200) / 3600) % 24); s.minute = (int)(((R[i].t + 7200) / 60) % 60);
        s.pos_m = (float)R[i].dist; s.speed_kmh = (float)(R[i].sp * 3.6); s.dist_km = (float)(R[i].dist / 1000.0);
        s.avg_kmh = s.dist_km / ((float)s.time_s / 3600.0f); s.ascent_m = 108.0f; s.hr = R[i].hr; s.cad = R[i].cad; s.power = 185;
        s.gps_ok = 1; s.battery_pct = 78; s.on_route = 1;
        ui_app_update(&app, &s); settle();
        save("screen_f1_defaut.rgb565");
        char d[200], enc[64];
        printf("TEST CHAMPS PERSONNALISABLES\n");
        ui_config_encode(ui_app_get_config(&app), enc, sizeof enc);
        check("configuration par defaut", strcmp(enc, "v1;8;0,2,6,8,10,4,11,13") == 0, enc);

        longpress(60, 130);
        snprintf(d, sizeof d, "(titre : %s)", lv_label_get_text(app.p_title));
        check("appui long sur le champ 1 : le selecteur s'ouvre", !lv_obj_has_flag(app.p_panel, LV_OBJ_FLAG_HIDDEN) && strcmp(lv_label_get_text(app.p_title), "CHAMP 1") == 0, d);
        lv_area_t la; lv_obj_get_coords(app.p_list, &la);
        lv_area_t ra; lv_obj_get_coords(app.p_row[14], &ra);
        int visible0 = ra.y1 >= la.y1 && ra.y2 <= la.y2;
        drag_v(150, 330, 190); drag_v(150, 330, 190);
        lv_obj_get_coords(app.p_row[14], &ra);
        int visible1 = ra.y1 >= la.y1 && ra.y2 <= la.y2;
        save("screen_f2_selecteur.rgb565");
        snprintf(d, sizeof d, "(avant le glissement : %s, apres : %s)", visible0 ? "visible" : "hors de vue", visible1 ? "visible" : "hors de vue");
        check("la liste defile au doigt jusqu'a ARRIVEE ESTIMEE", !visible0 && visible1, d);
        tap((ra.x1 + ra.x2) / 2, (ra.y1 + ra.y2) / 2);
        ui_app_update(&app, &s); settle();
        int eta = (s.hour * 60 + s.minute + (int)(((r.total_m - s.pos_m) / 1000.0f) / s.avg_kmh * 60.0f + 0.5f)) % 1440;
        char want[16]; snprintf(want, sizeof want, "%02d:%02d", eta / 60, eta % 60);
        snprintf(d, sizeof d, "(%s : %s, attendu %s)", lv_label_get_text(app.d_title[0]), lv_label_get_text(app.d_val[0]), want);
        check("le champ 1 devient ARRIVEE ESTIMEE avec la bonne heure", app.config.cell[0] == 14 && strcmp(lv_label_get_text(app.d_val[0]), want) == 0 && lv_obj_has_flag(app.p_panel, LV_OBJ_FLAG_HIDDEN), d);
        snprintf(d, sizeof d, "(%d appel, derniere : cases %d)", n_cfg, last_cfg.n_cells);
        check("l'action d'enregistrement de la configuration est appelee", n_cfg == 1 && last_cfg.cell[0] == 14, d);

        longpress(200, 185);
        int vis9 = bring_into_view(&app, 9);
        lv_obj_get_coords(app.p_row[9], &ra);
        tap((ra.x1 + ra.x2) / 2, (ra.y1 + ra.y2) / 2);
        ui_app_update(&app, &s); settle();
        snprintf(d, sizeof d, "(%s : %s)", lv_label_get_text(app.d_title[3]), lv_label_get_text(app.d_val[3]));
        check("le champ 4 devient PUISSANCE : 185 W", vis9 && app.config.cell[3] == 9 && strcmp(lv_label_get_text(app.d_val[3]), "185 W") == 0, d);
        save("screen_f3_personnalise.rgb565");

        int h8 = app.d_strip.h;
        longpress(60, 185);
        lv_obj_get_coords(app.p_lay[1], &ra);
        tap((ra.x1 + ra.x2) / 2, (ra.y1 + ra.y2) / 2);
        lv_obj_get_coords(app.p_close, &ra);
        tap((ra.x1 + ra.x2) / 2, (ra.y1 + ra.y2) / 2);
        settle();
        snprintf(d, sizeof d, "(%d champs, profil %d px au lieu de %d, champs 7 et 8 %s)", app.config.n_cells, app.d_strip.h, h8, lv_obj_has_flag(app.d_val[7], LV_OBJ_FLAG_HIDDEN) ? "masques" : "visibles");
        check("6 champs : deux cases disparaissent et le profil grandit", app.config.n_cells == 6 && app.d_strip.h == 112 && lv_obj_has_flag(app.d_val[6], LV_OBJ_FLAG_HIDDEN) && lv_obj_has_flag(app.d_val[7], LV_OBJ_FLAG_HIDDEN) && lv_obj_has_flag(app.p_panel, LV_OBJ_FLAG_HIDDEN), d);
        save("screen_f4_six_champs.rgb565");

        longpress(60, 130);
        lv_obj_get_coords(app.p_lay[0], &ra);
        tap((ra.x1 + ra.x2) / 2, (ra.y1 + ra.y2) / 2);
        lv_obj_get_coords(app.p_close, &ra);
        tap((ra.x1 + ra.x2) / 2, (ra.y1 + ra.y2) / 2);
        settle();
        snprintf(d, sizeof d, "(%d champs, profil %d px)", app.config.n_cells, app.d_strip.h);
        check("4 champs : le profil occupe tout le bas de la page", app.config.n_cells == 4 && app.d_strip.h == 166, d);
        save("screen_f5_quatre_champs.rgb565");

        ui_config_encode(ui_app_get_config(&app), enc, sizeof enc);
        ui_config_t back; int rc = ui_config_decode(enc, &back);
        snprintf(d, sizeof d, "(%s)", enc);
        check("enregistrer puis relire la configuration donne la meme", rc == 0 && memcmp(&back, ui_app_get_config(&app), sizeof back) == 0, d);
        const char *bad[] = {"", "v2;8;0,1,2,3,4,5,6,7", "v1;5;0,1,2,3,4,5,6,7", "v1;8;0,1,2,3,4,5,6", "v1;8;0,1,2,3,4,5,6,99", "v1;8;0,1,2,3,4,5,6,7,8", "v1;8;a,b"};
        int all_bad = 1; for (unsigned q = 0; q < sizeof bad / sizeof bad[0]; q++) { ui_config_t t = back; if (ui_config_decode(bad[q], &t) == 0 || memcmp(&t, &back, sizeof t) != 0) all_bad = 0; }
        check("7 configurations invalides refusees sans rien modifier", all_bad, "");
        ui_config_t good; ui_config_default(&good); good.n_cells = 6; good.cell[0] = 5;
        ui_app_set_config(&app, &good); settle();
        snprintf(d, sizeof d, "(%d champs, champ 1 : %s)", app.config.n_cells, lv_label_get_text(app.d_title[0]));
        check("appliquer une configuration chargee depuis la memoire", app.config.n_cells == 6 && strcmp(lv_label_get_text(app.d_title[0]), "VITESSE MAX") == 0, d);
        swipe(280, 40, 200);
        snprintf(d, sizeof d, "(page %d)", app.cur);
        check("le glissement entre pages fonctionne toujours", app.cur == 1, d);
        int obj1 = ui_app_object_count(&app);
        snprintf(d, sizeof d, "(%d objets au depart, %d a la fin)", obj0, obj1);
        check("aucun objet cree ou perdu pendant la personnalisation", obj0 == obj1, d);
        printf("%s\n", fails ? "ECHECS" : "TOUS LES TESTS REUSSIS");
        return fails != 0;
    }

    float asc = 0, ref = (float)R[0].ele;
    for (int i = 0; i < i0; i++) { float e = (float)R[i].ele; if (e - ref >= 1.0f) { asc += e - ref; ref = e; } else if (ref - e >= 1.0f) ref = e; }
    int prev_cur = app.cur, prev_sum = 0, auto_seen = 0, sum_seen = 0, back_seen = 0, swiped = 0, mid_saved = 0, closed_by_tap = 0;
    double auto_km = 0, sum_km = 0, back_km = 0; unsigned sum_t = 0;
    int nupd = 0; double cpu = 0;
    const int STRIDE = 3;
    ui_hooks.alert = h_alert;
    int soon_seen = 0; double soon_km = 0; char soon_title[48] = "", soon_sub[80] = "";
    for (int i = i0; i <= i1; i += STRIDE) {
        for (int j = i - STRIDE; j < i && j >= i0; j++) { float e = (float)R[j].ele; if (e - ref >= 1.0f) { asc += e - ref; ref = e; } else if (ref - e >= 1.0f) ref = e; }
        ui_sample_t s; memset(&s, 0, sizeof s);
        s.time_s = R[i].t - R[0].t;
        s.hour = (int)(((R[i].t + 7200) / 3600) % 24); s.minute = (int)(((R[i].t + 7200) / 60) % 60);
        s.pos_m = (float)R[i].dist; s.speed_kmh = (float)(R[i].sp * 3.6); s.dist_km = (float)(R[i].dist / 1000.0);
        s.avg_kmh = s.dist_km / ((float)s.time_s / 3600.0f); s.ascent_m = asc;
        s.hr = R[i].hr; s.cad = R[i].cad; s.power = -1; s.gps_ok = 1; s.battery_pct = 78; s.on_route = 1;
        clock_t t0 = clock();
        ui_app_update(&app, &s);
        lv_tick_inc(STRIDE * 1000); lv_timer_handler(); lv_tick_inc(300); lv_timer_handler();
        cpu += (double)(clock() - t0) / CLOCKS_PER_SEC; nupd++;

        if (i == i0) {
            save("screen_a1_avant.rgb565");
            static const char *pn[3] = {"trace", "altitude", "liste"};
            for (int q = 1; q <= 3; q++) {
                lv_screen_load(app.page[q]); app.cur = q; settle();
                char fn[64]; snprintf(fn, sizeof fn, "screen_p%d_%s.rgb565", q, pn[q - 1]); save(fn);
            }
            lv_screen_load(app.page[0]); app.cur = 0; settle(); prev_cur = 0;
        }
        if (app.cur == 4 && prev_cur != 4 && !auto_seen) {
            auto_seen = 1; auto_km = R[i].dist / 1000.0; save("screen_a2_auto_montee.rgb565");
            if (mode == 'b') { for (int q = 0; q < 4; q++) swipe(40, 280, 200); swiped = 1; printf("  l'utilisateur glisse vers la page %d\n", app.cur); }
        }
        if (BANNER_VISIBLE() && !soon_seen) { soon_seen = 1; soon_km = R[i].dist / 1000.0; snprintf(soon_title, sizeof soon_title, "%s", lv_label_get_text(app.b_title)); snprintf(soon_sub, sizeof soon_sub, "%s", lv_label_get_text(app.b_sub)); save("screen_a6_montee_proche.rgb565"); }
        if (!mid_saved && R[i].dist >= 29800.0) { mid_saved = 1; if (mode != 'b') save("screen_a3_milieu.rgb565"); }
        if (app.summary_visible && !prev_sum && !sum_seen) {
            sum_seen = 1; sum_km = R[i].dist / 1000.0; sum_t = s.time_s; save("screen_a4_fin.rgb565");
            if (mode == 'c') { tap(160, 375); closed_by_tap = 1; }
        }
        if (sum_seen && !app.summary_visible && !back_seen && (app.cur == 0 || mode == 'b')) { back_seen = 1; back_km = R[i].dist / 1000.0; if (mode == 'a') save("screen_a5_retour.rgb565"); }
        prev_cur = app.cur; prev_sum = app.summary_visible;
    }

    printf("RESULTATS\n");
    char d[160];
    snprintf(d, sizeof d, "(km %.2f, debut de montee a km %.2f)", auto_km, c0->start_m / 1000.0);
    check("la page de montee s'ouvre seule au debut de la montee", auto_seen && auto_km >= c0->start_m / 1000.0 && auto_km < c0->start_m / 1000.0 + 0.1, d);
    snprintf(d, sizeof d, "(km %.2f, sommet a km %.2f)", sum_km, c0->end_m / 1000.0);
    check("la fenetre de fin s'affiche au sommet", sum_seen && sum_km >= c0->end_m / 1000.0 && sum_km < c0->end_m / 1000.0 + 0.1, d);
    const ui_summary_t *m = &app.summary;
    double dur_truth = (double)(t_out - t_in);
    snprintf(d, sizeof d, "(%.0f s contre %.0f s)", (double)m->duration_s, dur_truth);
    check("duree de la montee (a 3 s pres)", fabs(m->duration_s - dur_truth) <= 3.5, d);
    snprintf(d, sizeof d, "(%d bpm contre %.0f)", m->avg_hr, hr_sum / hr_n);
    check("frequence cardiaque moyenne (a 2 bpm pres)", fabs(m->avg_hr - hr_sum / hr_n) <= 2.5, d);
    snprintf(d, sizeof d, "(%.1f km, %.1f %%, +%.0f m, %.1f km/h)", (double)(m->dist_m / 1000.0f), (double)m->avg_grade_pct, (double)m->ascent_m, (double)m->avg_speed_kmh);
    check("distance, pente et denivele du resume = ceux de la montee", fabs(m->dist_m - c0->length_m) < 1 && fabs(m->ascent_m - c0->ascent_m) < 1, d);
    if (mode == 'a') { snprintf(d, sizeof d, "(retour km %.2f, fenetre affichee a t=%us)", back_km, sum_t); check("la fenetre disparait seule et la page d'origine revient", back_seen && app.cur == 0 && !app.summary_visible, d); }
    if (mode == 'b') { snprintf(d, sizeof d, "(page %d)", app.cur); check("l'utilisateur n'est pas ramene de force sur la page de montee", swiped && app.cur == 0, d); }
    if (mode == 'c') { snprintf(d, sizeof d, "(page %d)", app.cur); check("toucher OK ferme la fenetre et revient a la page d'origine", closed_by_tap && !app.summary_visible && app.cur == 0, d); }
    snprintf(d, sizeof d, "(km %.2f : %s | %s)", soon_km, soon_title, soon_sub);
    check("l'annonce 'montee dans 300 m' apparait avant la montee", soon_seen && strncmp(soon_title, "MONTÉE DANS", 12) == 0 && soon_km < c0->start_m / 1000.0 && soon_km > c0->start_m / 1000.0 - 0.4, d);
    snprintf(d, sizeof d, "(%d alerte pendant toute la sortie)", n_alert);
    check("aucune autre alerte pendant la sortie", n_alert == 1 && last_alert_kind == ALERT_CLIMB_SOON, d);
    int obj1 = ui_app_object_count(&app);
    snprintf(d, sizeof d, "(%d objets au depart, %d a la fin, %d mises a jour)", obj0, obj1, nupd);
    check("aucun objet cree ou perdu pendant la sortie", obj0 == obj1, d);
    printf("  duree moyenne d'une mise a jour avec affichage : %.1f ms (PC, a titre indicatif)\n", cpu / nupd * 1000.0);
    printf("%s\n", fails ? "ECHECS" : "TOUS LES TESTS REUSSIS");
    return fails != 0;
}
