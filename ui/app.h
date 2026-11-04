#ifndef UI_APP_H
#define UI_APP_H
/* Application d'ecrans du GPS velo (LVGL 9, portrait 320 x 480).
 * Les pages sont construites une seule fois ; ui_app_update() ne modifie que ce qui change.
 * Une mesure par seconde suffit. Au debut d'une montee la page de montee s'ouvre seule ;
 * a son sommet, une fenetre de fin de montee s'affiche puis disparait. */

#include "lvgl.h"
#include "climb.h"
#include "route.h"
#include "alerts.h"
#include "guidance.h"
#include "mapview.h"
#include "polyfill.h"
#include "linedraw.h"

LV_FONT_DECLARE(ui_font_14)
LV_FONT_DECLARE(ui_font_16)
LV_FONT_DECLARE(ui_font_20)
LV_FONT_DECLARE(ui_font_28)
LV_FONT_DECLARE(ui_font_48)

#define UI_W 320
#define UI_H 480
#define UI_PAGES 5
#define UI_PROF_MAX 160
#define UI_PTS_MAX 520
#define UI_ROWS 6
#define UI_FLAGS 4
#define UI_TICKS 7
#define UI_MAX_CELLS 8
#define UI_FIELDS 18

/* Champs de la page de donnees : choix et nombre (4, 6 ou 8). Appui long sur un champ pour le changer. */
typedef struct ui_config {
    uint8_t n_cells;                 /* 4, 6 ou 8 */
    uint8_t cell[UI_MAX_CELLS];      /* numero de champ affiche dans chaque case */
} ui_config_t;

void ui_config_default(ui_config_t *c);
int  ui_config_encode(const ui_config_t *c, char *out, size_t n);     /* ex. "v1;8;0,2,6,8,10,4,11,13" */
int  ui_config_decode(const char *s, ui_config_t *c);                 /* 0 = valide, <0 = refuse (c inchange) */
const char *ui_field_title(int id);

/* Seuils cardiaques personnalisables : seuils d'entree en Z2, Z3, Z4 et Z5 (bpm). */
typedef struct ui_hr_zones {
    int z2_min_bpm;
    int z3_min_bpm;
    int z4_min_bpm;
    int z5_min_bpm;
} ui_hr_zones_t;

/* Reglage du fond de carte, enregistre a part (le format de la configuration des champs reste "v1;..."). */
typedef struct ui_map_cfg {
    uint8_t theme;                      /* 0 = sombre colore, 1 = clair (defaut) */
} ui_map_cfg_t;
void ui_map_cfg_default(ui_map_cfg_t *c);
int  ui_map_cfg_encode(const ui_map_cfg_t *c, char *out, size_t n);   /* "m1;1" */
int  ui_map_cfg_decode(const char *s, ui_map_cfg_t *c);               /* 0 si valide ; sinon c reste inchange */

/* Actions declenchees par le toucher : a brancher sur le reste du programme. */
typedef struct {
    void (*pause)(void);
    void (*lap)(void);
    void (*elev_zoom)(float window_m);
    void (*map_zoom)(int dir);
    void (*open_climb)(int number);
    void (*config_changed)(const struct ui_config *cfg);   /* a brancher sur l'enregistrement (SD ou memoire flash) */
    void (*alert)(const alert_t *alert);                    /* une alerte apparait : bip ou vibration */
    void (*alerts_changed)(const alerts_cfg_t *cfg);        /* reglages d'alertes modifies : a enregistrer */
    void (*hr_zones_changed)(const ui_hr_zones_t *zones);   /* seuils FC modifies : a enregistrer */
    void (*turn_alert)(int direction, int angle_deg, float distance_m); /* annonce/bip a l approche d un virage */
    void (*map_cfg_changed)(const ui_map_cfg_t *cfg);       /* theme du fond de carte modifie : a enregistrer */
} ui_hooks_t;
extern ui_hooks_t ui_hooks;

/* Parcours charge : trace, profil, montees (nommees). Les pointeurs doivent rester valides. */
typedef struct {
    const route_t *route;
    const float *ele; int n_ele; float step_m;
    const climb_t *climbs; int n_climbs;
    const climb_cfg_t *cfg;
} ui_course_t;

/* Une mesure par seconde. */
typedef struct {
    uint32_t time_s;                 /* temps de sortie ecoule */
    int hour, minute;                /* heure locale */
    float pos_m;                     /* distance sur le parcours (positionnement) */
    float speed_kmh, avg_kmh, dist_km, ascent_m;
    int hr, cad, power;              /* -1 = pas de capteur */
    int gps_ok, battery_pct, paused;
    int on_route; float off_m;       /* resultat du positionnement (off_m < 0 : inconnu) */
} ui_sample_t;

/* Resume affiche au sommet. */
typedef struct {
    int number;
    char name[40];
    float duration_s, dist_m, avg_grade_pct, ascent_m, avg_speed_kmh;
    int avg_hr;                      /* -1 si pas de capteur */
} ui_summary_t;

/* --- structures internes --- */
typedef struct { int16_t x0, x1, y0, y1, h; uint8_t level, dim; } ui_bar_t;
typedef struct { lv_obj_t *obj; int w, h, n; ui_bar_t bars[UI_PROF_MAX]; } ui_prof_t;
typedef struct { lv_obj_t *bar, *clock, *gps, *hr, *batt, *turn_box, *turn_lbl, *page_hit[UI_PAGES], *home_hit; guidance_turn_t turn_state; int turn_off_route; } ui_status_t;
struct ui_app;
typedef struct { struct ui_app *app; float v; } ui_ctx_t;

#define UI_MAP_MARGIN 32                                    /* marge du canevas de carte : permet de le decaler sans le redessiner */
#define UI_MAP_W (UI_W + 2 * UI_MAP_MARGIN)
#define UI_MAP_H (380 + 2 * UI_MAP_MARGIN)
#define UI_MAP_BUF_BYTES ((size_t)UI_MAP_W * UI_MAP_H * 2)

typedef struct ui_app {
    ui_course_t course;
    ui_sample_t last;
    int have_sample;
    lv_obj_t *page[UI_PAGES];
    lv_obj_t *home, *home_info;
    ui_ctx_t home_ctx[5];
    int cur;
    int auto_page;                   /* 1 = ouverture automatique de la page de montee */
    /* etat de montee */
    int ci, in_climb, prev_in_climb, before_page, climb_page_auto, cur_climb;
    uint32_t climb_t0, last_in_time; float hr_sum; int hr_n;
    ui_summary_t summary; int summary_visible; uint32_t summary_until;
    float grade_now, grade_ahead, alt_m;
    climb_progress_t prog;
    float view_m, elev_window, max_speed, hr_total_sum; int hr_total_n;
    ui_config_t config;
    ui_hr_zones_t hr_zones;
    /* page donnees */
    ui_status_t st[UI_PAGES];
    lv_obj_t *d_speed, *d_grade_box, *d_grade, *d_turn_dist, *d_val[8], *d_subval[8], *d_title[8], *d_cellbox[8], *d_sep[5], *d_pause_box, *d_pause_lbl, *d_lap_box;
    lv_obj_t *d_strip_title, *d_strip_line, *d_strip_dot;
    ui_prof_t d_strip; ui_ctx_t cell_ctx[8];
    /* page trace */
    lv_obj_t *n_line[3], *n_scale, *n_scale_lbl, *n_reste, *n_l1, *n_name, *n_l3;
    lv_obj_t *n_turn_card, *n_turn_title, *n_turn_value;
    guidance_turn_t next_turn; int turn_alerted_idx;
    lv_obj_t *n_mode_btn, *n_mode_lbl;
    lv_obj_t *n_map, *n_credit;       /* fond de carte (canevas) et mention OpenStreetMap */
    mapview_t *map; mapview_view_t map_view, map_rendered;
    uint16_t *map_buf;                /* tampon du canevas : UI_MAP_W x UI_MAP_H pixels RGB565 */
    int map_valid, map_theme;         /* canevas a jour ; 0 = sombre colore, 1 = clair */
    ui_map_cfg_t map_cfg;             /* reglage enregistre (theme) */
    lv_obj_t *cfg_screen, *cfg_theme_btn[2], *cfg_theme_lbl[2];   /* ecran de reglages (cree au premier appui) */
    ui_ctx_t cfg_theme_ctx[2];
    unsigned map_segments, map_renders;   /* diagnostic : segments du dernier dessin, nombre de dessins */
    int track_up;                    /* 0 = nord en haut, 1 = trace devant */
    lv_point_precise_t pts[3][UI_PTS_MAX];
    /* page altitude */
    lv_obj_t *e_chip[3], *e_chip_lbl[3], *e_tick[UI_TICKS], *e_flag_line[UI_FLAGS], *e_flag_box[UI_FLAGS], *e_flag_lbl[UI_FLAGS];
    lv_obj_t *e_alt, *e_grade, *e_asc, *e_ahead;
    ui_prof_t e_prof; ui_ctx_t e_ctx[3], m_ctx[2];
    /* page liste */
    lv_obj_t *l_row[UI_ROWS], *l_chip[UI_ROWS], *l_cat[UI_ROWS], *l_title[UI_ROWS], *l_l2[UI_ROWS], *l_l3[UI_ROWS], *l_sep[UI_ROWS], *l_total;
    ui_prof_t l_prof[UI_ROWS]; ui_ctx_t l_ctx[UI_ROWS];
    /* page montee */
    lv_obj_t *c_body, *c_empty, *c_name, *c_chip, *c_chip_lbl, *c_badge, *c_badge_lbl, *c_sub, *c_ahead, *c_stats, *c_emax, *c_emin;
    lv_obj_t *c_mline, *c_mdot, *c_rem, *c_rem_u, *c_avg, *c_asc, *c_hr, *c_spd, *c_cad, *c_next_box, *c_next;
    ui_prof_t c_prof;
    /* fenetre de fin de montee */
    lv_obj_t *s_panel, *s_title, *s_sub, *s_val[6], *s_ok;
    /* selecteur de champ */
    lv_obj_t *p_panel, *p_card, *p_title, *p_list, *p_row[UI_FIELDS], *p_row_lbl[UI_FIELDS], *p_lay[3], *p_lay_lbl[3], *p_close;
    ui_ctx_t p_ctx[UI_FIELDS], lay_ctx[3], close_ctx; int picker_cell;
    ui_ctx_t pg_ctx[UI_PAGES], s_ctx;
    /* alertes */
    alerts_t alerts;
    lv_obj_t *b_panel, *b_strip, *b_title, *b_sub, *b_more;
    lv_obj_t *as_panel, *as_card, *as_tog[ALERT_KINDS], *as_tog_lbl[ALERT_KINDS], *as_hr_val, *as_close, *as_zones_btn;
    lv_obj_t *hz_panel, *hz_card, *hz_val[4], *hz_close;
    ui_ctx_t banner_ctx, as_ctx[ALERT_KINDS], as_hr_ctx[2], as_close_ctx, status_ctx;
    ui_ctx_t hz_ctx[8], hz_close_ctx;
} ui_app_t;

void ui_app_init(ui_app_t *a, const ui_course_t *course);       /* construit toutes les pages */
void ui_app_update(ui_app_t *a, const ui_sample_t *s);          /* a appeler environ chaque seconde */
/* Fond de carte (tuiles GVT1) sous la trace. canvas_buf : UI_MAP_W x UI_MAP_H pixels RGB565 (UI_MAP_BUF_BYTES octets, a
 * mettre en PSRAM), garde par l'application. mv NULL pour enlever la carte. */
void ui_app_set_map(ui_app_t *a, mapview_t *mv, uint16_t *canvas_buf);
void ui_app_set_map_theme(ui_app_t *a, int theme);              /* 0 = sombre colore, 1 = clair (sans enregistrer) */
void ui_app_set_map_cfg(ui_app_t *a, const ui_map_cfg_t *c);   /* applique un reglage relu depuis l'enregistrement */
const ui_map_cfg_t *ui_app_get_map_cfg(const ui_app_t *a);
/* Ajoute dans le conteneur parent le choix du fond de carte (etiquette et deux boutons SOMBRE / CLAIR), a partir du point
 * (x, y), sur la largeur w. A appeler une seule fois, pour integrer le choix dans une page de reglages existante.
 * Retourne la hauteur occupee, en pixels. */
int ui_app_add_theme_picker(ui_app_t *a, lv_obj_t *parent, int x, int y, int w);
/* Mettre a 0 (option de compilation) pour que le bouton RÉGLAGES de la page d'accueil garde son comportement d'origine :
 * l'application n'ouvre alors aucun ecran de reglages de son cru. */
#ifndef UI_HOME_SETTINGS_SCREEN
#define UI_HOME_SETTINGS_SCREEN 1
#endif
void ui_app_goto(ui_app_t *a, int page, int dir);               /* dir > 0 : glissement vers la gauche */
void ui_app_set_config(ui_app_t *a, const ui_config_t *c);      /* applique et affiche une configuration */
const ui_config_t *ui_app_get_config(const ui_app_t *a);
void ui_app_set_alerts_cfg(ui_app_t *a, const alerts_cfg_t *c);
const alerts_cfg_t *ui_app_get_alerts_cfg(const ui_app_t *a);
void ui_app_set_hr_zones(ui_app_t *a, const ui_hr_zones_t *zones);
const ui_hr_zones_t *ui_app_get_hr_zones(const ui_app_t *a);
void ui_app_jump(ui_app_t *a);                                  /* apres un saut dans la sortie (simulateur) : efface montee en cours, fenetre de fin et alertes */
int  ui_app_object_count(const ui_app_t *a);                    /* nombre d'objets LVGL (test de stabilite) */

lv_color_t ui_level_color(int level);
lv_color_t ui_category_color(int category);
#endif
