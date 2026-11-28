/* Simulateur du GPS velo sur ordinateur : l'appareil a gauche (la souris joue le doigt), un tableau de commandes a droite.
 * Usage : gps_sim parcours.gpx [sortie.gpx] [cols.csv] [--carte carte.gvt] [--zoom 2] [--selftest]
 *   parcours.gpx : le parcours prevu. sortie.gpx : la sortie rejouee (par defaut le meme fichier). cols.csv : base de cols.
 *   carte.gvt : fond de carte vectoriel sous la trace (docs/carte.md ; tools/osm_pbf_to_gvt.py le produit). */
#include <SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "cols.h"
#include "mapview.h"
#include "sim_engine.h"
#include "splash.h"

#define PANEL_X 328
#define PANEL_W 232

typedef struct { FILE *f; } fctx;
static size_t rd(void *c, char *b, size_t m) { return fread(b, 1, m, ((fctx *)c)->f); }

static float X[20000], Y[20000], E[20000], W[20000];
static route_t route;
static climb_cfg_t cfg;
static climb_t climbs[64];
static int n_climbs;
static cols_entry_t ent[20000];
static char pool[400000];
static cols_db_t db;
static sim_t sim;
static ui_app_t app;
static ui_splash_t splash;
static int zoom = 1;

enum { CMD_PLAY, CMD_RESTART, CMD_X1, CMD_X10, CMD_X60, CMD_X300, CMD_PREV, CMD_NEXT, CMD_M5, CMD_P5,
       CMD_F_OFF, CMD_F_GPS, CMD_F_HR, CMD_F_HRBOOST, CMD_BATT_M10, CMD_BATT_100, CMD_COUNT };
static lv_obj_t *g_btn[CMD_COUNT], *g_btn_lbl[CMD_COUNT], *g_info[3], *g_alert1, *g_alert2;
static int g_force;
static char g_alert_title[48], g_alert_sub[64];

/* ---------- actions de l'appareil ---------- */
static void h_pause(void) { sim.rec_paused = !sim.rec_paused; g_force = 1; }
static void h_lap(void) { sim.laps++; g_force = 1; }
static void h_alert(const alert_t *a)
{
    snprintf(g_alert_title, sizeof g_alert_title, "%s", a->title);
    snprintf(g_alert_sub, sizeof g_alert_sub, "%s", a->sub);
    printf("ALERTE  %s | %s\n", a->title, a->sub);
}
static void h_cfg(const ui_config_t *c) { char b[64]; ui_config_encode(c, b, sizeof b); printf("CONFIG CHAMPS  %s\n", b); }
static void h_acfg(const alerts_cfg_t *c) { char b[64]; alerts_cfg_encode(c, b, sizeof b); printf("CONFIG ALERTES  %s\n", b); }
static void h_mcfg(const ui_map_cfg_t *c) { char b[16]; ui_map_cfg_encode(c, b, sizeof b); printf("CONFIG CARTE  %s\n", b); }

/* ---------- mesure envoyee a l'appareil ---------- */
static void push_sample(void)
{
    sim_sample_t s;
    sim_get(&sim, &s);
    ui_sample_t u;
    memset(&u, 0, sizeof u);
    u.time_s = s.time_s; u.hour = s.hour; u.minute = s.minute; u.pos_m = s.pos_m;
    u.speed_kmh = s.speed_kmh; u.avg_kmh = s.avg_kmh; u.dist_km = s.dist_km; u.ascent_m = s.ascent_m;
    u.hr = s.hr; u.cad = s.cad; u.power = s.power; u.gps_ok = s.gps_ok; u.battery_pct = s.battery_pct;
    u.paused = s.paused; u.on_route = s.on_route; u.off_m = s.off_m;
    ui_app_update(&app, &u);
}

/* ---------- tableau de commandes ---------- */
static const lv_color_t C_BG = { 0x1B, 0x15, 0x10 };           /* ordre b, g, r : 0x10151B */
static lv_color_t col(uint32_t h) { return lv_color_hex(h); }

static void on_cmd(lv_event_t *e)
{
    int c = (int)(intptr_t)lv_event_get_user_data(e);
    int jump = 0;
    switch (c) {
    case CMD_PLAY: if (sim.idx >= sim.n - 1) { sim_seek_row(&sim, 0); sim.batt = 100.0f; jump = 1; } sim.playing = !sim.playing; break;
    case CMD_RESTART: sim_seek_row(&sim, 0); sim.batt = 100.0f; sim.playing = 1; jump = 1; break;
    case CMD_X1: sim.speed = 1.0; break;
    case CMD_X10: sim.speed = 10.0; break;
    case CMD_X60: sim.speed = 60.0; break;
    case CMD_X300: sim.speed = 300.0; break;
    case CMD_PREV: sim_next_climb(&sim, -1); jump = 1; break;
    case CMD_NEXT: sim_next_climb(&sim, +1); jump = 1; break;
    case CMD_M5: sim_seek_pos(&sim, sim.pos_m[sim.idx] - 5000.0f); jump = 1; break;
    case CMD_P5: sim_seek_pos(&sim, sim.pos_m[sim.idx] + 5000.0f); jump = 1; break;
    case CMD_F_OFF: sim.f_off = !sim.f_off; break;
    case CMD_F_GPS: sim.f_gps = !sim.f_gps; break;
    case CMD_F_HR: sim.f_hr = !sim.f_hr; break;
    case CMD_F_HRBOOST: sim.hr_boost = !sim.hr_boost; break;
    case CMD_BATT_M10: sim.batt = sim.batt > 10.0f ? sim.batt - 10.0f : 0.0f; break;
    case CMD_BATT_100: sim.batt = 100.0f; break;
    default: break;
    }
    if (jump) ui_app_jump(&app);
    g_force = 1;
}

static void add_btn(lv_obj_t *p, int cmd, int x, int y, int w, int h, const char *txt)
{
    lv_obj_t *b = lv_obj_create(p);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(b, w, h);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_set_style_bg_color(b, col(0x1F2833), 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, on_cmd, LV_EVENT_CLICKED, (void *)(intptr_t)cmd);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &ui_font_14, 0);
    lv_obj_set_style_text_color(l, col(0xE6EAF0), 0);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(l);
    g_btn[cmd] = b;
    g_btn_lbl[cmd] = l;
}

static lv_obj_t *add_lbl(lv_obj_t *p, int x, int y, const lv_font_t *f, uint32_t c, const char *t)
{
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, col(c), 0);
    lv_obj_set_pos(l, x, y);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

static void build_panel(void)
{
    (void)C_BG;
    lv_obj_t *top = lv_layer_top();
    lv_obj_t *p = lv_obj_create(top);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(p, PANEL_W, UI_H);
    lv_obj_set_pos(p, PANEL_X, 0);
    lv_obj_set_style_radius(p, 0, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_set_style_bg_color(p, col(0x10151B), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_t *sep = lv_obj_create(top);
    lv_obj_set_size(sep, 4, UI_H);
    lv_obj_set_pos(sep, PANEL_X - 4, 0);
    lv_obj_set_style_bg_color(sep, col(0x2A333F), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_remove_flag(sep, LV_OBJ_FLAG_CLICKABLE);

    add_lbl(p, 10, 6, &ui_font_20, 0xF2F5F8, "SIMULATEUR");
    for (int i = 0; i < 3; i++) g_info[i] = add_lbl(p, 10, 34 + i * 17, &ui_font_14, 0x8A94A3, "");
    add_btn(p, CMD_PLAY, 8, 92, 108, 32, "Pause");
    add_btn(p, CMD_RESTART, 124, 92, 100, 32, "Début");
    add_lbl(p, 10, 130, &ui_font_14, 0x8A94A3, "VITESSE DE LECTURE");
    static const char *sp[4] = {"x1", "x10", "x60", "x300"};
    for (int i = 0; i < 4; i++) add_btn(p, CMD_X1 + i, 8 + i * 57, 148, 53, 30, sp[i]);
    add_lbl(p, 10, 186, &ui_font_14, 0x8A94A3, "ALLER À");
    add_btn(p, CMD_PREV, 8, 204, 108, 30, "< Montée");
    add_btn(p, CMD_NEXT, 124, 204, 100, 30, "Montée >");
    add_btn(p, CMD_M5, 8, 240, 108, 30, "-5 km");
    add_btn(p, CMD_P5, 124, 240, 100, 30, "+5 km");
    add_lbl(p, 10, 278, &ui_font_14, 0x8A94A3, "PANNES");
    add_btn(p, CMD_F_OFF, 8, 296, 108, 30, "Hors parcours");
    add_btn(p, CMD_F_GPS, 124, 296, 100, 30, "Perte GPS");
    add_btn(p, CMD_F_HR, 8, 332, 108, 30, "Sans ceinture");
    add_btn(p, CMD_F_HRBOOST, 124, 332, 100, 30, "FC +40");
    add_btn(p, CMD_BATT_M10, 8, 368, 108, 30, "Batterie -10 %");
    add_btn(p, CMD_BATT_100, 124, 368, 100, 30, "Batterie 100 %");
    add_lbl(p, 10, 410, &ui_font_14, 0x8A94A3, "DERNIÈRE ALERTE");
    g_alert1 = add_lbl(p, 10, 428, &ui_font_14, 0xF0883E, "aucune");
    g_alert2 = add_lbl(p, 10, 446, &ui_font_14, 0xC5CBD3, "");
    lv_label_set_long_mode(g_alert2, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g_alert2, 212);
}

static void style_btn(int cmd, int on, uint32_t on_color)
{
    lv_obj_set_style_bg_color(g_btn[cmd], on ? col(on_color) : col(0x1F2833), 0);
    lv_obj_set_style_text_color(g_btn_lbl[cmd], on ? col(0x0E1217) : col(0xE6EAF0), 0);
}

static void update_panel(void)
{
    char b[96];
    unsigned t = (unsigned)sim.t, tot = (unsigned)sim_duration(&sim);
    snprintf(b, sizeof b, "Sortie  %u:%02u:%02u / %u:%02u:%02u", t / 3600, (t / 60) % 60, t % 60, tot / 3600, (tot / 60) % 60, tot % 60);
    lv_label_set_text(g_info[0], b);
    snprintf(b, sizeof b, "Position  %.1f / %.1f km", (double)(sim.pos_m[sim.idx] / 1000.0f), (double)(route.total_m / 1000.0f));
    lv_label_set_text(g_info[1], b);
    snprintf(b, sizeof b, "Lecture x%g   Tours %d%s", sim.speed, sim.laps, sim.rec_paused ? "  (en pause)" : "");
    lv_label_set_text(g_info[2], b);
    lv_label_set_text(g_btn_lbl[CMD_PLAY], sim.playing ? "Pause" : "Lecture");
    style_btn(CMD_X1, sim.speed == 1.0, 0x4AA3FF);
    style_btn(CMD_X10, sim.speed == 10.0, 0x4AA3FF);
    style_btn(CMD_X60, sim.speed == 60.0, 0x4AA3FF);
    style_btn(CMD_X300, sim.speed == 300.0, 0x4AA3FF);
    style_btn(CMD_F_OFF, sim.f_off, 0xF0883E);
    style_btn(CMD_F_GPS, sim.f_gps, 0xF0883E);
    style_btn(CMD_F_HR, sim.f_hr, 0xF0883E);
    style_btn(CMD_F_HRBOOST, sim.hr_boost, 0xF0883E);
    lv_label_set_text(g_alert1, g_alert_title[0] ? g_alert_title : "aucune");
    lv_label_set_text(g_alert2, g_alert_sub);
}

/* ---------- boucle ---------- */
static uint32_t last_ms, last_upd_ms;
static int last_idx = -1;

static void frame(double forced_dt)
{
    uint32_t now = SDL_GetTicks();
    double dt = (now - last_ms) / 1000.0;
    last_ms = now;
    if (forced_dt >= 0.0) dt = forced_dt;
    if (dt > 0.25) dt = 0.25;
    if (!ui_splash_done(&splash)) dt = 0.0;   /* la sortie commence apres l'ecran de demarrage */
    sim_advance(&sim, dt);
    if (ui_splash_done(&splash) && (g_force || (sim.idx != last_idx && now - last_upd_ms >= 200))) {
        push_sample();
        last_idx = sim.idx; last_upd_ms = now; g_force = 0;
    }
    update_panel();
    lv_timer_handler();
}

/* ---------- autotest sans ecran : vrais clics de souris injectes dans SDL ---------- */
static void click_at(int x, int y)
{
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_MOUSEMOTION; e.motion.windowID = 1; e.motion.x = x * zoom; e.motion.y = y * zoom;
    SDL_PushEvent(&e);
    for (int i = 0; i < 3; i++) { SDL_Delay(8); frame(0.0); }
    memset(&e, 0, sizeof e);
    e.type = SDL_MOUSEBUTTONDOWN; e.button.windowID = 1; e.button.button = SDL_BUTTON_LEFT; e.button.state = SDL_PRESSED; e.button.x = x * zoom; e.button.y = y * zoom;
    SDL_PushEvent(&e);
    for (int i = 0; i < 6; i++) { SDL_Delay(12); frame(0.0); }
    memset(&e, 0, sizeof e);
    e.type = SDL_MOUSEBUTTONUP; e.button.windowID = 1; e.button.button = SDL_BUTTON_LEFT; e.button.state = SDL_RELEASED; e.button.x = x * zoom; e.button.y = y * zoom;
    SDL_PushEvent(&e);
    for (int i = 0; i < 10; i++) { SDL_Delay(12); frame(0.0); }      /* la lecture du doigt se fait toutes les 33 ms */
}
static void click_obj(lv_obj_t *o)
{
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    click_at((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
}
static char g_shot[200];
static void capture(const char *suffix)
{
    if (!g_shot[0]) return;
    SDL_Window *w = SDL_GetWindowFromID(1);
    SDL_Renderer *r = w ? SDL_GetRenderer(w) : NULL;
    if (!r) return;
    for (int i = 0; i < 30; i++) { SDL_Delay(12); frame(0.0); }       /* laisse finir les animations */
    int ww, hh;
    SDL_GetRendererOutputSize(r, &ww, &hh);
    uint8_t *px = (uint8_t *)malloc((size_t)ww * (size_t)hh * 4);
    if (!px || SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, px, ww * 4) != 0) { free(px); return; }
    char path[260];
    snprintf(path, sizeof path, "%s%s.ppm", g_shot, suffix);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", ww, hh);
    for (int i = 0; i < ww * hh; i++) { uint8_t rgb[3] = { px[i * 4 + 2], px[i * 4 + 1], px[i * 4] }; fwrite(rgb, 1, 3, f); }
    fclose(f);
    free(px);
}
static int fails;
static void check(const char *what, int ok, const char *d) { printf("  %-70s %s %s\n", what, ok ? "OK" : "ECHEC", d); if (!ok) fails++; }

static int selftest(void)
{
    char d[200];
    printf("AUTOTEST DU SIMULATEUR (clics de souris reels dans la fenetre SDL)\n");
    for (int i = 0; i < 10; i++) { SDL_Delay(10); frame(0.1); }
    check("la fenetre et l'appareil demarrent", app.cur == 0 && app.have_sample, "");

    click_obj(g_btn[CMD_NEXT]);
    snprintf(d, sizeof d, "(position %.0f m, depart a %.0f m)", (double)sim.pos_m[sim.idx], (double)climbs[0].start_m);
    check("clic sur 'Montée >' : 500 m avant la premiere montee", fabsf(sim.pos_m[sim.idx] - (climbs[0].start_m - 500.0f)) < 15.0f, d);

    click_obj(g_btn[CMD_X60]);
    check("clic sur 'x60' : vitesse de lecture", sim.speed == 60.0, "");
    int auto_ok = 0;
    for (int i = 0; i < 400 && !auto_ok; i++) { SDL_Delay(12); frame(-1.0); auto_ok = app.cur == 4 && app.in_climb; }
    snprintf(d, sizeof d, "(position %.0f m, page %d)", (double)sim.pos_m[sim.idx], app.cur);
    check("en rejouant, la page de montee s'ouvre seule au depart de la montee", auto_ok, d);
    capture("_1_montee");

    ui_app_goto(&app, 0, -1);                  /* l'utilisateur est sur la page de donnees quand la montee se termine */
    for (int i = 0; i < 5; i++) { SDL_Delay(12); frame(0.0); }
    sim_seek_pos(&sim, climbs[0].end_m - 150.0f);
    ui_app_jump(&app);
    g_force = 1;
    int sum_ok = 0;
    for (int i = 0; i < 400 && !sum_ok; i++) { SDL_Delay(12); frame(-1.0); sum_ok = app.summary_visible; }
    snprintf(d, sizeof d, "(%s, %.1f km, %.1f %%)", app.summary.name, (double)(app.summary.dist_m / 1000.0f), (double)app.summary.avg_grade_pct);
    check("au sommet, la fenetre de fin de montee s'affiche", sum_ok && fabsf(app.summary.dist_m - climbs[0].length_m) < 1.0f, d);
    click_obj(app.s_ok);
    snprintf(d, sizeof d, "(page %d)", app.cur);
    check("clic sur OK : la fenetre se ferme et la page d'avant revient", !app.summary_visible && app.cur == 0, d);

    sim_seek_pos(&sim, 30000.0f);
    ui_app_jump(&app);
    click_obj(g_btn[CMD_F_OFF]);
    int ban = 0;
    for (int i = 0; i < 300 && !ban; i++) { SDL_Delay(12); frame(-1.0); ban = !lv_obj_has_flag(app.b_panel, LV_OBJ_FLAG_HIDDEN); }
    snprintf(d, sizeof d, "(%s | %s)", lv_label_get_text(app.b_title), lv_label_get_text(app.b_sub));
    check("panne 'Hors parcours' : la banniere d'alerte apparait", ban && strcmp(lv_label_get_text(app.b_title), "HORS PARCOURS") == 0, d);
    click_obj(app.b_panel);
    check("clic sur la banniere : elle est acquittee", lv_obj_has_flag(app.b_panel, LV_OBJ_FLAG_HIDDEN), "");
    click_obj(g_btn[CMD_F_OFF]);
    click_obj(g_btn[CMD_F_GPS]);
    ban = 0;
    for (int i = 0; i < 300 && !ban; i++) { SDL_Delay(12); frame(-1.0); ban = !lv_obj_has_flag(app.b_panel, LV_OBJ_FLAG_HIDDEN); }
    check("panne 'Perte GPS' : la banniere apparait", ban && strcmp(lv_label_get_text(app.b_title), "SIGNAL GPS PERDU") == 0, "");
    capture("_2_alerte");
    if (app.map) {                             /* avec --carte : la page trace doit dessiner des routes de la carte */
        ui_app_goto(&app, 1, 1);
        for (int i = 0; i < 20; i++) { SDL_Delay(12); frame(-1.0); }
        snprintf(d, sizeof d, "(%u segments, %u tuiles chargees, %u erreurs)", app.map_segments, app.map->loads, app.map->errors);
        check("page trace : le fond de carte est dessine", app.map_segments > 0 && app.map->errors == 0, d);
        capture("_3_carte");
    }
    printf("%s\n", fails ? "ECHECS" : "TOUS LES TESTS REUSSIS");
    return fails != 0;
}

static int map_read(void *ctx, uint32_t off, void *dst, uint32_t len)
{
    FILE *f = (FILE *)ctx;
    if (fseek(f, (long)off, SEEK_SET) != 0) return -1;
    return fread(dst, 1, len, f) == len ? 0 : -1;
}

int main(int argc, char **argv)
{
    static uint8_t map_cache[MAPVIEW_SLOTS * MAPVIEW_SLOT_BYTES];
    static uint8_t map_big_cache[4 * 256u * 1024u]; /* simulateur PC : 4 grosses tuiles de 256 Ko */
    static uint16_t map_canvas[UI_MAP_W * UI_MAP_H];
    static gvt_t map_gvt;
    static mapview_t map_view;
    const char *course = NULL, *ride = NULL, *colsfile = NULL, *mapfile = NULL;
    int self = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--selftest") == 0) self = 1;
        else if (strcmp(argv[i], "--zoom") == 0 && i + 1 < argc) zoom = atoi(argv[++i]);
        else if (strcmp(argv[i], "--carte") == 0 && i + 1 < argc) mapfile = argv[++i];
        else if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) snprintf(g_shot, sizeof g_shot, "%s", argv[++i]);
        else if (!course) course = argv[i];
        else if (strlen(argv[i]) > 4 && strcmp(argv[i] + strlen(argv[i]) - 4, ".csv") == 0) colsfile = argv[i];
        else ride = argv[i];
    }
    if (zoom < 1) zoom = 1;
    if (zoom > 3) zoom = 3;
    if (!course) { fprintf(stderr, "Usage : gps_sim parcours.gpx [sortie.gpx] [cols.csv] [--carte carte.gvt] [--zoom 2]\n"); return 2; }
    if (!ride) ride = course;

    fctx cx = { fopen(course, "rb") };
    if (!cx.f) { perror(course); return 1; }
    route_init(&route, X, Y, E, 20000, 10.0f);
    if (gpx_load(rd, &cx, &route) < 0) { fprintf(stderr, "GPX illisible : %s\n", course); return 1; }
    fclose(cx.f);
    climb_cfg_default(&cfg);
    profile_smooth(E, route.n, cfg.smooth_half, W);
    n_climbs = climb_detect(E, route.n, W, &cfg, climbs, 64);
    cols_init(&db, ent, 20000, pool, sizeof pool);
    cols_filter_route(&db, &route, 3000.0f);
    if (colsfile) {
        fctx cf = { fopen(colsfile, "rb") };
        if (cf.f) {
            cols_load(rd, &cf, &db);
            fclose(cf.f);
            printf("Cols : %d entrées lues dans %s (%d hors du parcours ignorées, %d autres points d'intérêt ignorés, %d lignes non reconnues)\n", db.n, colsfile, db.skipped, db.other, db.rejected);
            if (db.truncated) fprintf(stderr, "ATTENTION : base de cols trop grande, la fin du fichier est ignorée.\n");
            if (db.n == 0 && db.rejected > 0)
                fprintf(stderr, "ATTENTION : aucun col lu : format de %s non reconnu (attendu : type;nom;lat;lon;alt, avec type = col ou lieu ;\n"
                                "des colonnes en plus sont acceptées si une ligne d'en-tête les nomme : type, nom, lat, lon, alt).\n", colsfile);
            else if (db.n == 0 && db.skipped > 0)
                fprintf(stderr, "ATTENTION : aucun col à moins de 3 km du parcours dans %s.\n", colsfile);
        } else perror(colsfile);
        cols_cfg_t cc;
        cols_cfg_default(&cc);
        cols_name_climbs(&db, &cc, &route, E, climbs, n_climbs);
    }
    printf("Parcours : %.1f km, %d montées", (double)(route.total_m / 1000.0f), n_climbs);
    for (int i = 0; i < n_climbs; i++) printf("%s%s", i ? ", " : " (", climbs[i].name[0] ? climbs[i].name : "sans nom");
    printf("%s\n", n_climbs ? ")" : "");
    int n = sim_load(&sim, ride, &route, climbs, n_climbs);
    if (n < 0) { fprintf(stderr, "Sortie illisible (il faut des heures dans le GPX) : %s\n", ride); return 1; }
    printf("Sortie rejouée : %d points, %.0f min\n", n, sim_duration(&sim) / 60.0);

    if (self) SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    lv_init();
    lv_display_t *disp = lv_sdl_window_create(PANEL_X + PANEL_W, UI_H);
    lv_sdl_window_set_zoom(disp, (uint8_t)zoom);
    lv_sdl_window_set_title(disp, "GPS vélo : simulateur");
    lv_sdl_mouse_create();

    ui_course_t uc = { &route, E, route.n, route.step_m, climbs, n_climbs, &cfg };
    ui_app_init(&app, &uc);
    ui_hooks.pause = h_pause; ui_hooks.lap = h_lap; ui_hooks.alert = h_alert;
    ui_hooks.config_changed = h_cfg; ui_hooks.alerts_changed = h_acfg; ui_hooks.map_cfg_changed = h_mcfg;
    if (mapfile) {
        FILE *mf = fopen(mapfile, "rb");
        if (!mf) { perror(mapfile); return 1; }
        if (gvt_open(&map_gvt, map_read, mf) != 0) { fprintf(stderr, "Carte illisible (pas un GVT1) : %s\n", mapfile); return 1; }
        mapview_init(&map_view, &map_gvt, map_cache);
        if (mapview_set_big_cache(&map_view, map_big_cache, 256u * 1024u, 4) != 0) {
            fprintf(stderr, "Configuration du cache des grosses tuiles impossible.\n");
            return 1;
        }
        ui_app_set_map(&app, &map_view, map_canvas);
        printf("Carte : %u tuiles, zooms %u a %u\\n", (unsigned)map_gvt.n_tiles, map_gvt.zmin, map_gvt.zmax);
        printf("Cache grandes tuiles simulateur : 4 x 256 Ko (configuration PC de test)\\n");
        {   /* la carte couvre-t-elle le parcours ? (un point tous les 500 m, tuiles du zoom 13) */
            int z = 13, total = 0, covered = 0, stride;
            if (z < map_gvt.zmin) z = map_gvt.zmin;
            if (z > map_gvt.zmax) z = map_gvt.zmax;
            stride = route.step_m > 0.0f ? (int)(500.0f / route.step_m) : 50;
            if (stride < 1) stride = 1;
            for (int i = 0; i < route.n; i += stride) {
                double lat, lon;
                uint32_t tx, ty;
                route_unproject(&route, route.x[i], route.y[i], &lat, &lon);
                gvt_tile_of(z, lat, lon, &tx, &ty, NULL, NULL);
                total++;
                { gvt_entry_t entry; if (gvt_find_entry(&map_gvt, z, tx, ty, &entry) == 1) covered++; }
            }
            printf("Carte : %d %% du parcours est couvert par la carte\n", total ? 100 * covered / total : 0);
            if (total && covered * 2 < total)
                fprintf(stderr, "ATTENTION : la carte ne couvre pas (ou peu) ce parcours : la page trace n'aura qu'un fond uni.\n"
                                "Generer la carte depuis un fichier OSM PBF avec tools/osm_pbf_to_gvt.py.\n");
        }
    }
    build_panel();
    last_ms = SDL_GetTicks();
    g_force = 1;
    if (self) { int rc = selftest(); sim_free(&sim); return rc; }
    ui_splash_show(&splash, app.home, UI_SPLASH_DEFAULT_MS);
    for (;;) { frame(-1.0); SDL_Delay(5); }
}
