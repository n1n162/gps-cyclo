/* Apercu de la page trace avec fond de carte, sur ordinateur, sans GPX personnel.
 * La trace suit des objets de la carte synthetique (tests/make_gvt_fixture.py) : elle doit recouvrir exactement la route
 * ou la riviere correspondante. Ecrit screen_map_<scenario>_<n>.rgb565 dans le dossier courant (voir rgb565_to_png.py).
 * Usage : map_preview carte.gvt autoroute|riviere|cyclable */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "app.h"

static uint16_t fb[UI_H][UI_W];
static int fails;
static int hook_calls, hook_theme = -1;
static void map_cfg_hook(const ui_map_cfg_t *c) { hook_calls++; hook_theme = c->theme; }
static void check(const char *name, int ok, const char *detail)
{
    printf("  %-72s %s %s\n", name, ok ? "OK" : "ECHEC", detail);
    if (!ok) fails++;
}
/* Appuie sur le premier objet dont un enfant est une etiquette portant ce texte. */
static lv_obj_t *find_button(lv_obj_t *screen, const char *text)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(screen); i++) {
        lv_obj_t *c = lv_obj_get_child(screen, (int32_t)i);
        if (lv_obj_get_child_count(c) > 0) {
            lv_obj_t *l = lv_obj_get_child(c, 0);
            if (lv_obj_check_type(l, &lv_label_class) && strcmp(lv_label_get_text(l), text) == 0) return c;
        }
    }
    return NULL;
}
static uint16_t canvas[UI_MAP_W * UI_MAP_H];
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    const uint16_t *src = (const uint16_t *)px;
    int w = area->x2 - area->x1 + 1;
    for (int y = area->y1; y <= area->y2; y++)
        for (int x = area->x1; x <= area->x2; x++) fb[y][x] = src[(y - area->y1) * w + (x - area->x1)];
    lv_display_flush_ready(disp);
}
static void settle(void) { for (int i = 0; i < 14; i++) { lv_tick_inc(30); lv_timer_handler(); } }
static void save(const char *name) { FILE *f = fopen(name, "wb"); fwrite(fb, sizeof fb, 1, f); fclose(f); }

static int read_file(void *ctx, uint32_t off, void *dst, uint32_t len)
{
    FILE *f = ctx;
    if (fseek(f, (long)off, SEEK_SET) != 0) return -1;
    return fread(dst, 1, len, f) == len ? 0 : -1;
}

static void metres(double lon, double lat, double dx, double dy, double *olon, double *olat)
{
    *olon = lon + dx / (111320.0 * cos(lat * M_PI / 180.0));
    *olat = lat + dy / 110574.0;
}

int main(int argc, char **argv)
{
    static float X[4000], Y[4000], E[4000];
    static uint8_t cache[MAPVIEW_SLOTS * MAPVIEW_SLOT_BYTES];
    static ui_app_t app;
    route_t r;
    gvt_t g;
    mapview_t mv;
    FILE *f;
    const char *scen;
    int theme;

    if (argc < 3 || argc > 4) { fprintf(stderr, "usage : %s carte.gvt autoroute|riviere|cyclable|demo|grenoble [sombre|clair]\n", argv[0]); return 2; }
    scen = argv[2];
    theme = argc == 4 && strcmp(argv[3], "clair") == 0;
    f = fopen(argv[1], "rb");
    if (!f || fseek(f, 0, SEEK_END) != 0 || ftell(f) < 0 || (uint64_t)ftell(f) > UINT32_MAX || gvt_open(&g, read_file, f, (uint32_t)ftell(f)) != 0) { fprintf(stderr, "carte illisible : %s\n", argv[1]); return 1; }
    mapview_init(&mv, &g, cache);

    route_init(&r, X, Y, E, 4000, 10.0f);
    if (strcmp(scen, "autoroute") == 0) {
        for (int i = 0; i <= 40; i++)
            route_add_point(&r, i == 0 ? 45.20 : 45.20 + 0.08 * i / 40 + 0.0006 * sin(i), 5.00 + 0.12 * i / 40, 0, 0);
    } else if (strcmp(scen, "riviere") == 0) {
        for (int i = 0; i < 200; i++) {
            double lon, lat;
            metres(5.20, 45.60, 150.0 * i, 300.0 * sin(i / 8.0), &lon, &lat);
            route_add_point(&r, lat, lon, 0, 0);
        }
    } else if (strcmp(scen, "grenoble") == 0) {          /* trace autour du centre de Grenoble : demande une carte qui le couvre */
        for (double t = -3000.0; t <= 3000.0; t += 25.0) {
            double lon, lat;
            metres(5.7294, 45.1859, t, 0.35 * t + 300.0 * sin(t / 800.0), &lon, &lat);
            route_add_point(&r, lat, lon, 0, 0);
        }
    } else if (strcmp(scen, "demo") == 0) {
        for (double t = -2300.0; t <= 2300.0; t += 25.0) {
            double lon, lat;
            metres(5.60, 45.20, t, 0.6 * t + 500.0 * sin(t / 700.0), &lon, &lat);
            route_add_point(&r, lat, lon, 0, 0);
        }
    } else {
        for (int i = 0; i < 30; i++) {
            double lon, lat;
            metres(5.30, 45.30, 120.0 * i, 40.0 * sin(i / 2.0), &lon, &lat);
            route_add_point(&r, lat, lon, 0, 0);
        }
    }
    printf("trace : %d points, %.1f km\n", r.n, r.total_m / 1000.0);

    lv_init();
    lv_display_t *disp = lv_display_create(UI_W, UI_H);
    static uint16_t buf[UI_W * UI_H];
    lv_display_set_buffers(disp, buf, NULL, sizeof buf, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);

    climb_cfg_t cfg;
    climb_cfg_default(&cfg);
    ui_course_t course = { &r, E, r.n, r.step_m, NULL, 0, &cfg };
    ui_app_init(&app, &course);
    ui_app_set_map(&app, &mv, canvas);
    {   /* reglage du fond de carte : clair par defaut, enregistrement, ecran de reglages */
        char enc[16];
        ui_map_cfg_t c, t;
        static const char *bad[] = {"", "m1;", "m1;2", "m2;1", "m1;1;", "m1;-1", "v1;1", "m1;10"};
        int all_bad = 1;
        lv_obj_t *b;
        ui_hooks.map_cfg_changed = map_cfg_hook;
        ui_map_cfg_default(&c);
        check("theme clair par defaut", c.theme == 1 && app.map_cfg.theme == 1 && app.map_theme == 1, "");
        ui_map_cfg_encode(&c, enc, sizeof enc);
        check("enregistrement du theme : m1;1", strcmp(enc, "m1;1") == 0 && ui_map_cfg_decode(enc, &t) == 0 && t.theme == 1, enc);
        t.theme = 0;
        check("relecture de m1;0", ui_map_cfg_decode("m1;0", &t) == 0 && t.theme == 0, "");
        t.theme = 1;
        for (unsigned q = 0; q < sizeof bad / sizeof bad[0]; q++) if (ui_map_cfg_decode(bad[q], &t) == 0 || t.theme != 1) all_bad = 0;
        check("8 enregistrements invalides refuses sans rien modifier", all_bad, "");

        b = find_button(app.home, "RÉGLAGES");
        check("la page d'accueil a un bouton RÉGLAGES", b != NULL, "");
        if (b) lv_obj_send_event(b, LV_EVENT_CLICKED, NULL);
        settle();
        check("RÉGLAGES ouvre l'ecran de reglages", app.cfg_screen != NULL && lv_screen_active() == app.cfg_screen, "");
        if (app.cfg_screen) {
            check("CLAIR est selectionne au depart", lv_color_eq(lv_obj_get_style_bg_color(app.cfg_theme_btn[1], 0), lv_color_hex(0x4AA3FF)) &&
                  !lv_color_eq(lv_obj_get_style_bg_color(app.cfg_theme_btn[0], 0), lv_color_hex(0x4AA3FF)), "");
            save("screen_reglages.rgb565");
            hook_calls = 0;
            lv_obj_send_event(app.cfg_theme_btn[0], LV_EVENT_CLICKED, NULL);
            settle();
            check("SOMBRE : theme applique, enregistre une fois", app.map_cfg.theme == 0 && app.map_theme == 0 && hook_calls == 1 && hook_theme == 0, "");
            check("SOMBRE : le bouton SOMBRE est selectionne", lv_color_eq(lv_obj_get_style_bg_color(app.cfg_theme_btn[0], 0), lv_color_hex(0x4AA3FF)), "");
            lv_obj_send_event(app.cfg_theme_btn[0], LV_EVENT_CLICKED, NULL);
            check("meme choix une seconde fois : rien n'est reenregistre", hook_calls == 1, "");
            lv_obj_send_event(app.cfg_theme_btn[1], LV_EVENT_CLICKED, NULL);
            settle();
            check("CLAIR : retour au theme clair, enregistre", app.map_cfg.theme == 1 && app.map_theme == 1 && hook_calls == 2 && hook_theme == 1, "");
            ui_map_cfg_default(&c);
            c.theme = 0;
            ui_app_set_map_cfg(&app, &c);
            check("relecture d'un reglage enregistre : applique sans le reenregistrer", app.map_theme == 0 && hook_calls == 2, "");
            ui_app_set_map_cfg(&app, ui_app_get_map_cfg(&app));
            c.theme = 1;
            ui_app_set_map_cfg(&app, &c);
            b = find_button(app.cfg_screen, "ACCUEIL");
            if (b) lv_obj_send_event(b, LV_EVENT_CLICKED, NULL);
            settle();
            check("ACCUEIL ramene a la page d'accueil", b != NULL && lv_screen_active() == app.home, "");
        }
    }
    ui_app_set_map_theme(&app, theme);
    ui_app_goto(&app, 1, 1);
    settle();

    static const struct { float frac, view_m; int track_up; const char *what; } frames[] = {
        { 0.50f, 1500.0f, 0, "nord en haut, 1,5 km" },
        { 0.50f, 600.0f, 1, "trace devant, 600 m" },
        { 0.30f, 5000.0f, 0, "nord en haut, 5 km" },
        { 0.70f, 250.0f, 1, "trace devant, 250 m" },
    };
    for (unsigned n = 0; n < sizeof frames / sizeof frames[0]; n++) {
        ui_sample_t s;
        char name[80];
        memset(&s, 0, sizeof s);
        s.pos_m = frames[n].frac * r.total_m;
        s.time_s = 600 + n; s.hour = 9; s.minute = 30; s.speed_kmh = 24; s.gps_ok = 1; s.battery_pct = 78; s.on_route = 1;
        app.view_m = frames[n].view_m;
        app.track_up = frames[n].track_up;
        lv_label_set_text(app.n_mode_lbl, app.track_up ? "TRACE" : "NORD");
        ui_app_update(&app, &s);
        settle();
        {   /* temps d'un affichage complet de la carte sur ce PC (indicatif : pas une mesure sur l'ESP32-S3) */
            struct timespec t0, t1;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            for (int k = 0; k < 20; k++) { app.map_valid = 0; ui_app_update(&app, &s); lv_tick_inc(40); lv_timer_handler(); }
            clock_gettime(CLOCK_MONOTONIC, &t1);
            printf("  cadre %u : %u segments dessines, %.2f ms par dessin complet de la carte sur ce PC\n", n + 1, app.map_segments,
                   ((t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6) / 20.0);
        }
        snprintf(name, sizeof name, "screen_map_%s_%s_%u.rgb565", scen, theme ? "clair" : "sombre", n + 1);
        save(name);
        for (int k = 0; k < 3; k++) lv_obj_add_flag(app.n_line[k], LV_OBJ_FLAG_HIDDEN);   /* meme image sans la trace */
        lv_obj_invalidate(app.n_map);
        settle();
        snprintf(name, sizeof name, "screen_map_%s_%s_%u_carte.rgb565", scen, theme ? "clair" : "sombre", n + 1);
        save(name);
        printf("%s : %s, zoom de tuile %d, cache %u chargements %u succes %u erreurs\n", name, frames[n].what,
               mapview_zoom(&g, &app.map_view), mv.loads, mv.hits, mv.errors);
    }
    {   /* sortie simulee : 5 minutes a 6 m/s, un echantillon par seconde ; combien de fois la carte est-elle redessinee ? */
        static const struct { float view_m; int track_up; const char *what; } rides[] = {
            { 250.0f, 0, "nord en haut, 250 m" }, { 600.0f, 0, "nord en haut, 600 m" }, { 1500.0f, 0, "nord en haut, 1,5 km" },
            { 250.0f, 1, "trace devant, 250 m" }, { 600.0f, 1, "trace devant, 600 m" }, { 1500.0f, 1, "trace devant, 1,5 km" },
        };
        for (unsigned k = 0; k < sizeof rides / sizeof rides[0]; k++) {
            ui_sample_t s;
            unsigned r0, seconds = 0;
            float start = 0.25f * r.total_m;
            app.view_m = rides[k].view_m;
            app.track_up = rides[k].track_up;
            lv_label_set_text(app.n_mode_lbl, app.track_up ? "TRACE" : "NORD");
            memset(&s, 0, sizeof s);
            s.hour = 9; s.minute = 30; s.speed_kmh = 22; s.gps_ok = 1; s.battery_pct = 78; s.on_route = 1;
            s.pos_m = start;
            app.map_valid = 0;
            ui_app_update(&app, &s);
            settle();
            r0 = app.map_renders;
            for (int sec = 0; sec < 300; sec++) {
                s.pos_m = start + 6.0f * sec;
                if (s.pos_m > r.total_m - 50.0f) break;
                s.time_s = 700 + sec;
                ui_app_update(&app, &s);
                lv_tick_inc(30);
                lv_timer_handler();
                seconds++;
            }
            printf("sortie de %u s a 6 m/s, %s : %u dessins de la carte%s\n", seconds, rides[k].what, app.map_renders - r0,
                   (app.map_renders - r0) ? "" : " (jamais redessinee)");
        }
    }
    if (fails) { printf("%d ECHEC(S)\n", fails); return 1; }
    return 0;
}
