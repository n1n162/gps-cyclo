/* Ecran de demarrage : image plein ecran 320 x 480, titre dans la marge haute, barre de progression dans la marge basse.
 * Les marges de l'image sont blanches : le texte est fonce. Sans image (UI_SPLASH_NO_IMAGE), fond uni et texte clair. */
#include "splash.h"
#include <string.h>

LV_FONT_DECLARE(ui_font_14)
LV_FONT_DECLARE(ui_font_16)
LV_FONT_DECLARE(ui_font_28)

#ifndef UI_SPLASH_NO_IMAGE
extern const lv_image_dsc_t ui_splash_img;      /* ui/splash_img.c, produit par tools/splash_to_c.py */
#define SP_BG       lv_color_hex(0xFFFFFF)
#define SP_TITLE    lv_color_hex(0x0B4F6C)
#define SP_SUB      lv_color_hex(0x4A7C86)
#define SP_TRACK    lv_color_hex(0xD3E5E8)
#define SP_FILL     lv_color_hex(0x2FA37A)
#define SP_STATUS   lv_color_hex(0x4A7C86)
#else
#define SP_BG       lv_color_hex(0x0B4F6C)
#define SP_TITLE    lv_color_hex(0xFFFFFF)
#define SP_SUB      lv_color_hex(0xB8D8DE)
#define SP_TRACK    lv_color_hex(0x1F6F86)
#define SP_FILL     lv_color_hex(0x2FA37A)
#define SP_STATUS   lv_color_hex(0xB8D8DE)
#endif

#define SP_W      320
#define SP_TITLE_Y 14
#define SP_SUB_Y   52
#define BAR_X      60
#define BAR_W      200
#define BAR_Y      418
#define BAR_H      6
#define STATUS_Y   434
#define FADE_MS    300
#define TICK_MS    30

static lv_obj_t *sp_label(lv_obj_t *p, int y, const lv_font_t *f, lv_color_t c, const char *t)
{
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(l, SP_W);
    lv_obj_set_pos(l, 0, y);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

static lv_obj_t *sp_box(lv_obj_t *p, int x, int y, int w, int h, lv_color_t c)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, h / 2, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

static void sp_progress(ui_splash_t *s, uint32_t elapsed_ms)
{
    uint32_t w = s->duration_ms ? (uint32_t)(((uint64_t)BAR_W * elapsed_ms) / s->duration_ms) : BAR_W;
    if (w > BAR_W) w = BAR_W;
    if (s->bar_fill) lv_obj_set_width(s->bar_fill, (int32_t)w);
}

static void sp_tick(lv_timer_t *t)
{
    ui_splash_t *s = (ui_splash_t *)lv_timer_get_user_data(t);
    uint32_t el = lv_tick_elaps(s->t0_ms);
    if (el >= s->duration_ms) ui_splash_skip(s);
    else sp_progress(s, el);
}

static void sp_click(lv_event_t *e)
{
    ui_splash_skip((ui_splash_t *)lv_event_get_user_data(e));
}

/* L'objet est detruit : on oublie ses pointeurs, sans toucher a un ecran de demarrage plus recent. */
static void sp_deleted(lv_event_t *e)
{
    ui_splash_t *s = (ui_splash_t *)lv_event_get_user_data(e);
    if (lv_event_get_target(e) != s->screen) return;
    s->screen = NULL;
    s->bar_fill = NULL;
    s->status = NULL;
}

void ui_splash_show(ui_splash_t *s, lv_obj_t *next, uint32_t duration_ms)
{
    memset(s, 0, sizeof *s);
    s->next = next;
    s->duration_ms = duration_ms ? duration_ms : UI_SPLASH_DEFAULT_MS;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(scr, SP_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
#ifndef UI_SPLASH_NO_IMAGE
    lv_obj_t *img = lv_image_create(scr);
    lv_image_set_src(img, &ui_splash_img);
    lv_obj_set_pos(img, 0, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
#endif
    sp_label(scr, SP_TITLE_Y, &ui_font_28, SP_TITLE, "GPS VÉLO");
    sp_label(scr, SP_SUB_Y, &ui_font_16, SP_SUB, "Compteur GPS fait maison");
    sp_box(scr, BAR_X, BAR_Y, BAR_W, BAR_H, SP_TRACK);
    s->bar_fill = sp_box(scr, BAR_X, BAR_Y, 0, BAR_H, SP_FILL);
    s->status = sp_label(scr, STATUS_Y, &ui_font_14, SP_STATUS, "Démarrage...");
    s->screen = scr;

    lv_obj_add_event_cb(scr, sp_click, LV_EVENT_CLICKED, s);
    lv_obj_add_event_cb(scr, sp_deleted, LV_EVENT_DELETE, s);
    s->t0_ms = lv_tick_get();
    s->active = 1;
    s->timer = lv_timer_create(sp_tick, TICK_MS, s);
    lv_screen_load(scr);
}

void ui_splash_skip(ui_splash_t *s)
{
    if (!s->active) return;
    s->active = 0;
    if (s->timer) { lv_timer_delete(s->timer); s->timer = NULL; }
    if (s->bar_fill) lv_obj_set_width(s->bar_fill, BAR_W);
    if (s->status) lv_label_set_text(s->status, "Prêt");
    if (s->next) {
        /* L'ecran de demarrage est supprime apres le fondu (et non par auto_del, qui supprimerait l'ecran actif quel qu'il soit). */
        lv_screen_load_anim(s->next, LV_SCR_LOAD_ANIM_FADE_ON, FADE_MS, 0, false);
        if (s->screen) lv_obj_delete_delayed(s->screen, FADE_MS + 60);
    }
}

int ui_splash_done(const ui_splash_t *s)
{
    return !s->active;
}

void ui_splash_set_status(ui_splash_t *s, const char *text)
{
    if (s->status && text) lv_label_set_text(s->status, text);
}
