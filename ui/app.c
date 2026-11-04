#include "app.h"
#include "guidance.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

ui_hooks_t ui_hooks;

#define COL_BG    lv_color_hex(0x0E1217)
#define COL_PANEL lv_color_hex(0x171D25)
#define COL_LINE  lv_color_hex(0x252D38)
#define COL_TEXT  lv_color_hex(0xF2F5F8)
#define COL_DIM   lv_color_hex(0x8A94A3)
#define COL_ACC   lv_color_hex(0x4AA3FF)
#define COL_OK    lv_color_hex(0x3FB950)
#define COL_BAD   lv_color_hex(0xE5484D)
#define COL_ORG   lv_color_hex(0xF0883E)

#define FONT_S  (&ui_font_14)
#define FONT_N  (&ui_font_16)
#define FONT_M  (&ui_font_20)
#define FONT_L  (&ui_font_28)
#define FONT_XL (&ui_font_48)

/* ---------- couleurs et categories ---------- */
lv_color_t ui_level_color(int level)
{
    static const uint32_t c[6] = {0x36B96B, 0x36B96B, 0xD8C52A, 0xF28C28, 0xE5484D, 0xA51D2D};
    if (level < 0) level = 0;
    if (level > 5) level = 5;
    return lv_color_hex(c[level]);
}

lv_color_t ui_category_color(int cat)
{
    static const uint32_t c[6] = {0xA51D2D, 0xE5484D, 0xF0883E, 0xE3C33B, 0x3FB950, 0x2E8B45};
    if (cat < 0 || cat > 5) cat = 5;
    return lv_color_hex(c[cat]);
}

static const char *category_name(int cat)
{
    static const char *n[6] = {"HC", "1", "2", "3", "4", "-"};
    return (cat < 0 || cat > 5) ? "-" : n[cat];
}

static int level_of(float g)
{
    if (g < 1.0f) return 0;
    if (g < 3.0f) return 1;
    if (g < 6.0f) return 2;
    if (g < 9.0f) return 3;
    if (g < 12.0f) return 4;
    return 5;
}

static int col_eq(lv_color_t a, lv_color_t b) { return a.red == b.red && a.green == b.green && a.blue == b.blue; }

/* ---------- briques ---------- */
static lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, lv_color_t c)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

static lv_obj_t *lbl(lv_obj_t *p, int x, int y, const lv_font_t *f, lv_color_t c, const char *t)
{
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_obj_set_pos(l, x, y);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

static lv_obj_t *lbl_c(lv_obj_t *p, int x, int y, int w, const lv_font_t *f, lv_color_t c, const char *t)
{
    lv_obj_t *l = lbl(p, x, y, f, c, t);
    lv_obj_set_width(l, w);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    return l;
}

static lv_obj_t *lbl_r(lv_obj_t *p, int right_margin, int y, const lv_font_t *f, lv_color_t c, const char *t)
{
    lv_obj_t *l = lbl(p, 0, y, f, c, t);
    lv_obj_align(l, LV_ALIGN_TOP_RIGHT, -right_margin, y);
    return l;
}

static lv_obj_t *lbl_dot(lv_obj_t *p, int x, int y, int w, int h, const lv_font_t *f, lv_color_t c, const char *t)
{
    lv_obj_t *l = lbl(p, x, y, f, c, t);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_size(l, w, h);
    return l;
}

static lv_obj_t *new_screen(void)
{
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s, COL_BG, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    return s;
}

static lv_obj_t *button(lv_obj_t *p, int x, int y, int w, int h, lv_color_t bg, lv_color_t fg, const char *txt,
                        const lv_font_t *font, lv_event_cb_t cb, void *ud, lv_obj_t **label_out)
{
    lv_obj_t *b = box(p, x, y, w, h, bg);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *l = lbl_c(b, 0, (h - (font == FONT_S ? 16 : 24)) / 2, w, font, fg, txt);
    if (label_out) *label_out = l;
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

/* ---------- mises a jour sans reecriture inutile ---------- */
static void settext(lv_obj_t *l, const char *fmt, ...)
{
    char b[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (strcmp(lv_label_get_text(l), b) != 0) lv_label_set_text(l, b);
}

static void settext_c(lv_obj_t *l, lv_color_t c, const char *fmt, ...)
{
    char b[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (strcmp(lv_label_get_text(l), b) != 0) lv_label_set_text(l, b);
    if (!col_eq(lv_obj_get_style_text_color(l, 0), c)) lv_obj_set_style_text_color(l, c, 0);
}

static void setbg(lv_obj_t *o, lv_color_t c)
{
    if (!col_eq(lv_obj_get_style_bg_color(o, 0), c)) lv_obj_set_style_bg_color(o, c, 0);
}

static void setfont(lv_obj_t *l, const lv_font_t *f)
{
    if (lv_obj_get_style_text_font(l, 0) != f) lv_obj_set_style_text_font(l, f, 0);
}

static void sethide(lv_obj_t *o, int hide)
{
    if (hide != (int)lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) {
        if (hide) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); else lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ---------- profil colore : courbe segmentee, sans barres verticales ---------- */
static void prof_draw(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target_obj(e);
    ui_prof_t *p = (ui_prof_t *)lv_event_get_user_data(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    /* Remplissage colore sous la courbe : une fine colonne par pixel horizontal.
       On dessine d'abord les remplissages, puis les lignes par-dessus. */
    for (int i = 0; i < p->n; i++) {
        const ui_bar_t *b = &p->bars[i];
        int x0 = b->x0;
        int x1 = b->x1;
        if (x1 <= x0) x1 = x0 + 1;
        lv_draw_rect_dsc_t fill;
        lv_draw_rect_dsc_init(&fill);
        fill.bg_color = ui_level_color(b->level);
        fill.bg_opa = b->dim ? 38 : 78;
        fill.border_width = 0;
        fill.radius = 0;
        for (int x = x0; x < x1; x++) {
            float t = (float)(x - b->x0) / (float)((b->x1 > b->x0) ? (b->x1 - b->x0) : 1);
            int y = (int)((float)b->y0 + ((float)b->y1 - (float)b->y0) * t + 0.5f);
            if (y < 0) y = 0;
            if (y >= p->h) y = p->h - 1;
            lv_area_t r = {
                .x1 = a.x1 + x,
                .y1 = a.y1 + y,
                .x2 = a.x1 + x,
                .y2 = a.y1 + p->h - 1
            };
            lv_draw_rect(layer, &fill, &r);
        }
    }
    /* Courbe nette au-dessus du remplissage. */
    for (int i = 0; i < p->n; i++) {
        const ui_bar_t *b = &p->bars[i];
        lv_draw_line_dsc_t d;
        lv_draw_line_dsc_init(&d);
        d.color = ui_level_color(b->level);
        d.width = 2;
        d.opa = b->dim ? 90 : LV_OPA_COVER;
        d.p1 = (lv_point_precise_t){ .x = a.x1 + b->x0, .y = a.y1 + b->y0 };
        d.p2 = (lv_point_precise_t){ .x = a.x1 + b->x1, .y = a.y1 + b->y1 };
        lv_draw_line(layer, &d);
    }
}

static void prof_init(ui_prof_t *p, lv_obj_t *parent, int x, int y, int w, int h)
{
    p->n = 0;
    p->w = w;
    p->h = h;
    p->obj = lv_obj_create(parent);
    lv_obj_remove_flag(p->obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(p->obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(p->obj, w, h);
    lv_obj_set_pos(p->obj, x, y);
    lv_obj_set_style_bg_opa(p->obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p->obj, 0, 0);
    lv_obj_set_style_pad_all(p->obj, 0, 0);
    lv_obj_set_style_radius(p->obj, 0, 0);
    lv_obj_add_event_cb(p->obj, prof_draw, LV_EVENT_DRAW_MAIN, p);
}

/* Construit la courbe depuis les sections. pos_dim_m < 0 : rien n'est estompe. */
static void prof_fill(ui_prof_t *p, const climb_section_t *sec, int n, const float *ele, int n_ele, float step_m,
                      float emin, float range, float pos_dim_m)
{
    int w = p->w, h = p->h;                 /* dimensions memorisees : l'objet n'est pas encore mis en page au premier appel */
    if (n > UI_PROF_MAX) n = UI_PROF_MAX;
    for (int i = 0; i < n; i++) {
        int ei = (int)(sec[i].end_m / step_m);
        if (ei >= n_ele) ei = n_ele - 1;
        if (ei < 0) ei = 0;
        int si = (int)(sec[i].start_m / step_m);
        if (si < 0) si = 0;
        if (si >= n_ele) si = n_ele - 1;
        float y_start = (ele[si] - emin) / range * (float)(h - 8);
        float y_end = (ele[ei] - emin) / range * (float)(h - 8);
        if (y_start < 0.0f) y_start = 0.0f;
        if (y_start > (float)(h - 8)) y_start = (float)(h - 8);
        if (y_end < 0.0f) y_end = 0.0f;
        if (y_end > (float)(h - 8)) y_end = (float)(h - 8);
        p->bars[i].x0 = (int16_t)((i * w) / n);
        p->bars[i].x1 = (int16_t)(((i + 1) * w) / n);
        p->bars[i].y0 = (int16_t)(h - 1 - (int)(y_start + 0.5f));
        p->bars[i].y1 = (int16_t)(h - 1 - (int)(y_end + 0.5f));
        p->bars[i].h = (int16_t)(h - p->bars[i].y1);
        p->bars[i].level = sec[i].level;
        p->bars[i].dim = (pos_dim_m >= 0.0f && sec[i].end_m <= pos_dim_m) ? 1 : 0;
    }
    p->n = n;
    lv_obj_invalidate(p->obj);
}

static float ele_interp(const ui_course_t *c, float pos_m)
{
    float x = pos_m / c->step_m;
    if (x <= 0.0f) return c->ele[0];
    if (x >= (float)(c->n_ele - 1)) return c->ele[c->n_ele - 1];
    int i = (int)x;
    float fr = x - (float)i;
    return c->ele[i] + fr * (c->ele[i + 1] - c->ele[i]);
}

/* ---------- icones vectorielles de guidage (sans dependance a une police Unicode) ---------- */
static void turn_line(lv_layer_t *layer, int x1, int y1, int x2, int y2, lv_color_t color, int width)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = color;
    d.width = width;
    d.round_start = 1;
    d.round_end = 1;
    d.p1 = (lv_point_precise_t){.x = x1, .y = y1};
    d.p2 = (lv_point_precise_t){.x = x2, .y = y2};
    lv_draw_line(layer, &d);
}

static void turn_arrow_draw(lv_layer_t *layer, const lv_area_t *a, const guidance_turn_t *t,
                            int off_route, int large)
{
    lv_color_t color = off_route ? COL_BAD : COL_TEXT;
    int cx = a->x1 + (large ? 25 : 13);
    int top = a->y1 + (large ? 20 : 4);
    int bot = a->y1 + (large ? 54 : 18);
    int w = large ? 4 : 2;
    if (off_route) {
        turn_line(layer, cx, top, cx, bot, color, w);
        turn_line(layer, cx, top, cx, top + (large ? 4 : 3), color, w + 1);
        turn_line(layer, cx, bot - (large ? 4 : 3), cx, bot, color, w + 1);
        return;
    }
    if (t && t->found && t->maneuver) {
        /* Rond-point : boucle circulaire polygonale avec une pointe tangentielle. */
        int r = large ? 12 : 6;
        int ox = cx, oy = a->y1 + (large ? 38 : 11);
        int px[9], py[9];
        for (int i = 0; i <= 8; i++) {
            float ang = (-135.0f + i * 38.0f) * 0.01745329252f;
            px[i] = ox + (int)(r * cosf(ang));
            py[i] = oy + (int)(r * sinf(ang));
        }
        for (int i = 0; i < 8; i++) turn_line(layer, px[i], py[i], px[i+1], py[i+1], color, w);
        turn_line(layer, px[8], py[8], px[8] - (large ? 5 : 3), py[8] - (large ? 1 : 1), color, w);
        turn_line(layer, px[8], py[8], px[8] - (large ? 1 : 1), py[8] - (large ? 5 : 3), color, w);
        return;
    }
    int dir = (t && t->found) ? t->direction : 0;
    int angle = (t && t->found) ? t->angle_deg : 0;
    int endx = cx, endy = top;
    if (!dir) {
        turn_line(layer, cx, bot, cx, top, color, w);
        turn_line(layer, cx, top, cx - (large ? 7 : 4), top + (large ? 8 : 5), color, w);
        turn_line(layer, cx, top, cx + (large ? 7 : 4), top + (large ? 8 : 5), color, w);
    } else if (angle < 50) {
        endx = cx + dir * (large ? 14 : 7);
        endy = top + (large ? 8 : 4);
        turn_line(layer, cx, bot, cx, a->y1 + (large ? 37 : 11), color, w);
        turn_line(layer, cx, a->y1 + (large ? 37 : 11), endx, endy, color, w);
        turn_line(layer, endx, endy, endx - dir * (large ? 8 : 4), endy, color, w);
        turn_line(layer, endx, endy, endx, endy + (large ? 8 : 4), color, w);
    } else if (angle < 120) {
        endx = cx + dir * (large ? 15 : 8);
        endy = a->y1 + (large ? 37 : 11);
        turn_line(layer, cx, bot, cx, endy, color, w);
        turn_line(layer, cx, endy, endx, endy, color, w);
        turn_line(layer, endx, endy, endx - dir * (large ? 8 : 4), endy - (large ? 7 : 4), color, w);
        turn_line(layer, endx, endy, endx - dir * (large ? 8 : 4), endy + (large ? 7 : 4), color, w);
    } else {
        /* Virage tres serre / demi-tour : fleche en U. */
        int r = large ? 11 : 6;
        turn_line(layer, cx, bot, cx, a->y1 + (large ? 34 : 10), color, w);
        turn_line(layer, cx, a->y1 + (large ? 34 : 10), cx + dir * r, a->y1 + (large ? 34 : 10), color, w);
        turn_line(layer, cx + dir * r, a->y1 + (large ? 34 : 10), cx + dir * r, top + (large ? 2 : 1), color, w);
        turn_line(layer, cx + dir * r, top + (large ? 2 : 1), cx + dir * r - dir * (large ? 7 : 4), top + (large ? 9 : 5), color, w);
        turn_line(layer, cx + dir * r, top + (large ? 2 : 1), cx + dir * r + dir * (large ? 1 : 1), top + (large ? 10 : 6), color, w);
    }
}

static void status_turn_draw(lv_event_t *e)
{
    ui_status_t *st = (ui_status_t *)lv_event_get_user_data(e);
    lv_obj_t *o = lv_event_get_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t area;
    lv_obj_get_coords(o, &area);
    if (st->turn_off_route) return; /* le libelle HORS TRACE occupe toute la largeur */
    turn_arrow_draw(layer, &area, &st->turn_state, 0, 0);
}

static void data_turn_draw(lv_event_t *e)
{
    ui_app_t *app = (ui_app_t *)lv_event_get_user_data(e);
    lv_obj_t *o = lv_event_get_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t area;
    lv_obj_get_coords(o, &area);
    int off = app->have_sample && app->last.gps_ok && !app->last.on_route && app->last.off_m >= 0.0f;
    turn_arrow_draw(layer, &area, &app->next_turn, off, 1);
}

/* ---------- barre d'etat ---------- */
static void status_build(lv_obj_t *s, ui_status_t *st, int page)
{
    st->bar = box(s, 0, 0, UI_W, 28, COL_PANEL);
    st->gps = lbl(s, 8, 6, FONT_S, COL_BAD, LV_SYMBOL_GPS);
    st->hr = lbl(s, 30, 6, FONT_S, COL_DIM, LV_SYMBOL_BLUETOOTH " --");
    st->clock = lbl_c(s, 90, 6, 60, FONT_S, COL_TEXT, "--:--");
    for (int i = 0; i < UI_PAGES; i++) {
        /* Zone de clic suffisamment large pour naviguer à la souris dans le simulateur. */
        lv_obj_t *hit = box(s, 158 + i * 11, 2, 10, 24, COL_PANEL);
        lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, 0);
        lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
        st->page_hit[i] = hit;
        lv_obj_t *dot = box(hit, 2, 9, 6, 6, i == page ? COL_TEXT : lv_color_hex(0x4B5563));
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    }
    st->home_hit = box(s, 218, 3, 22, 22, COL_PANEL);
    lv_obj_set_style_radius(st->home_hit, 5, 0);
    lv_obj_add_flag(st->home_hit, LV_OBJ_FLAG_CLICKABLE);
    lbl_c(st->home_hit, 0, 2, 22, FONT_S, COL_TEXT, "H");
    st->batt = lbl_r(s, 8, 6, FONT_S, COL_DIM, "");
    /* Indicateur compact de prochain virage, present sur chaque page. */
    st->turn_box = box(s, 242, 3, 74, 22, COL_PANEL);
    lv_obj_set_style_radius(st->turn_box, 6, 0);
    st->turn_lbl = lbl_c(st->turn_box, 25, 3, 46, FONT_S, COL_TEXT, "");
    lv_obj_add_event_cb(st->turn_box, status_turn_draw, LV_EVENT_DRAW_MAIN, st);
    lv_obj_add_flag(st->turn_box, LV_OBJ_FLAG_HIDDEN);
}

static void status_update(ui_status_t *st, const ui_sample_t *s)
{
    lv_color_t g = s->gps_ok ? COL_OK : COL_BAD;
    if (!col_eq(lv_obj_get_style_text_color(st->gps, 0), g)) lv_obj_set_style_text_color(st->gps, g, 0);
    settext_c(st->hr, s->hr >= 0 ? COL_OK : COL_DIM, LV_SYMBOL_BLUETOOTH " %s", s->hr >= 0 ? "FC" : "--");
    settext(st->clock, "%02d:%02d", s->hour, s->minute);
    const char *sym = s->battery_pct > 75 ? LV_SYMBOL_BATTERY_FULL : s->battery_pct > 50 ? LV_SYMBOL_BATTERY_3 :
                      s->battery_pct > 25 ? LV_SYMBOL_BATTERY_2 : s->battery_pct > 10 ? LV_SYMBOL_BATTERY_1 : LV_SYMBOL_BATTERY_EMPTY;
    settext_c(st->batt, s->battery_pct > 15 ? COL_DIM : COL_BAD, "%s %d%%", sym, s->battery_pct);
}


/* ======================= catalogue de champs et configuration ======================= */
static const char *g_field_title[UI_FIELDS] = {
    "TEMPS", "HEURE", "DISTANCE", "VITESSE", "VITESSE MOY.", "VITESSE MAX", "FRÉQ. CARDIAQUE", "FC MOYENNE", "CADENCE",
    "PUISSANCE", "DÉNIVELÉ +", "ALTITUDE", "PENTE", "RESTE", "ARRIVÉE ESTIMÉE", "PROCHAINE MONTÉE", "BATTERIE", "ZONE FC"};

const char *ui_field_title(int id) { return (id >= 0 && id < UI_FIELDS) ? g_field_title[id] : ""; }

void ui_config_default(ui_config_t *c)
{
    static const uint8_t d[UI_MAX_CELLS] = {0, 2, 6, 8, 10, 4, 11, 13};
    c->n_cells = 8;
    memcpy(c->cell, d, sizeof d);
}

int ui_config_encode(const ui_config_t *c, char *out, size_t n)
{
    int w = snprintf(out, n, "v1;%d;", c->n_cells);
    for (int i = 0; i < UI_MAX_CELLS; i++) {
        if (w < 0 || (size_t)w >= n) return -1;
        w += snprintf(out + w, n - (size_t)w, i ? ",%d" : "%d", c->cell[i]);
    }
    return (w < 0 || (size_t)w >= n) ? -1 : 0;
}

int ui_config_decode(const char *s, ui_config_t *c)
{
    ui_config_t t;
    char *e;
    if (strncmp(s, "v1;", 3) != 0) return -1;
    s += 3;
    long n = strtol(s, &e, 10);
    if (e == s || *e != ';' || (n != 4 && n != 6 && n != 8)) return -2;
    t.n_cells = (uint8_t)n;
    s = e + 1;
    for (int i = 0; i < UI_MAX_CELLS; i++) {
        long v = strtol(s, &e, 10);
        if (e == s || v < 0 || v >= UI_FIELDS) return -3;
        t.cell[i] = (uint8_t)v;
        s = e;
        if (i < UI_MAX_CELLS - 1) { if (*s != ',') return -4; s++; }
    }
    if (*s != '\0') return -5;
    *c = t;
    return 0;
}

static int hr_zone_for_bpm(const ui_app_t *a, int bpm)
{
    return bpm < a->hr_zones.z2_min_bpm ? 1 :
           bpm < a->hr_zones.z3_min_bpm ? 2 :
           bpm < a->hr_zones.z4_min_bpm ? 3 :
           bpm < a->hr_zones.z5_min_bpm ? 4 : 5;
}

static lv_color_t hr_zone_color(int zone)
{
    static const uint32_t colors[6] = {0x8A94A3, 0x4AA3FF, 0x3FB950, 0xE3C33B, 0xF0883E, 0xE5484D};
    if (zone < 1 || zone > 5) return COL_DIM;
    return lv_color_hex(colors[zone]);
}

static const char *hr_zone_name(int zone)
{
    static const char *names[6] = {"--", "Z1 RÉCUP", "Z2 ENDUR.", "Z3 TEMPO", "Z4 SEUIL", "Z5 MAX"};
    if (zone < 1 || zone > 5) return "--";
    return names[zone];
}

static void field_value(const ui_app_t *a, int id, char *out, size_t n, lv_color_t *col)
{
    const ui_sample_t *s = &a->last;
    *col = COL_TEXT;
    switch (id) {
    case 0: snprintf(out, n, "%u:%02u:%02u", (unsigned)(s->time_s / 3600), (unsigned)((s->time_s / 60) % 60), (unsigned)(s->time_s % 60)); break;
    case 1: snprintf(out, n, "%02d:%02d", s->hour, s->minute); break;
    case 2: snprintf(out, n, "%.1f km", (double)s->dist_km); break;
    case 3: snprintf(out, n, "%.1f km/h", (double)s->speed_kmh); break;
    case 4: snprintf(out, n, "%.1f km/h", (double)s->avg_kmh); break;
    case 5: snprintf(out, n, "%.1f km/h", (double)a->max_speed); break;
    case 6: if (s->hr >= 0) { snprintf(out, n, "%d bpm", s->hr); *col = hr_zone_color(hr_zone_for_bpm(a, s->hr)); } else { snprintf(out, n, "--"); *col = COL_DIM; } break;
    case 7: if (a->hr_total_n > 0) { snprintf(out, n, "%d bpm", (int)(a->hr_total_sum / (float)a->hr_total_n + 0.5f)); *col = COL_BAD; } else { snprintf(out, n, "--"); *col = COL_DIM; } break;
    case 8: if (s->cad >= 0) { snprintf(out, n, "%d rpm", s->cad); *col = COL_ACC; } else { snprintf(out, n, "--"); *col = COL_DIM; } break;
    case 9: if (s->power >= 0) snprintf(out, n, "%d W", s->power); else { snprintf(out, n, "--"); *col = COL_DIM; } break;
    case 10: snprintf(out, n, "%.0f m", (double)s->ascent_m); break;
    case 11: snprintf(out, n, "%.0f m", (double)a->alt_m); break;
    case 12: snprintf(out, n, "%.1f %%", (double)a->grade_now); *col = ui_level_color(level_of(a->grade_now)); break;
    case 13: snprintf(out, n, "%.1f km", (double)((a->course.route->total_m - s->pos_m) / 1000.0f)); break;
    case 14: {
        float left_km = (a->course.route->total_m - s->pos_m) / 1000.0f;
        if (s->avg_kmh >= 1.0f && left_km >= 0.0f) {
            int t = (s->hour * 60 + s->minute + (int)(left_km / s->avg_kmh * 60.0f + 0.5f)) % 1440;
            snprintf(out, n, "%02d:%02d", t / 60, t % 60);
        } else { snprintf(out, n, "--:--"); *col = COL_DIM; }
        break;
    }
    case 15:
        if (a->ci >= 0 && a->in_climb) { snprintf(out, n, "en cours"); *col = COL_ORG; }
        else if (a->ci >= 0) snprintf(out, n, "%.1f km", (double)((a->course.climbs[a->ci].start_m - s->pos_m) / 1000.0f));
        else { snprintf(out, n, "--"); *col = COL_DIM; }
        break;
    case 16: snprintf(out, n, "%d %%", s->battery_pct); if (s->battery_pct <= 15) *col = COL_BAD; break;
    case 17: {
        if (s->hr < 0) { snprintf(out, n, "--"); *col = COL_DIM; break; }
        int z = hr_zone_for_bpm(a, s->hr);
        snprintf(out, n, "%s", hr_zone_name(z));
        *col = hr_zone_color(z);
        break;
    }
    default: snprintf(out, n, "--"); *col = COL_DIM; break;
    }
}

/* ======================= construction des pages ======================= */
static void refresh_page(ui_app_t *a, int idx);
static void show_summary(ui_app_t *a, int show);

static void cb_pause(lv_event_t *e) { (void)e; if (ui_hooks.pause) ui_hooks.pause(); }
static void cb_lap(lv_event_t *e) { (void)e; if (ui_hooks.lap) ui_hooks.lap(); }

static void cb_map_mode(lv_event_t *e)
{
    ui_app_t *a = (ui_app_t *)lv_event_get_user_data(e);
    a->track_up = !a->track_up;
    settext(a->n_mode_lbl, "%s", a->track_up ? "TRACE" : "NORD");
    setbg(a->n_mode_btn, a->track_up ? COL_ACC : COL_PANEL);
    if (a->track_up) lv_obj_set_style_text_color(a->n_mode_lbl, COL_BG, 0);
    else lv_obj_set_style_text_color(a->n_mode_lbl, COL_TEXT, 0);
    refresh_page(a, 1);
}

static void cb_map_zoom(lv_event_t *e)
{
    ui_ctx_t *c = (ui_ctx_t *)lv_event_get_user_data(e);
    ui_app_t *a = c->app;
    float v = a->view_m * (c->v > 0.0f ? 0.5f : 2.0f);
    if (v < 375.0f) v = 375.0f;
    if (v > 12000.0f) v = 12000.0f;
    a->view_m = v;
    if (ui_hooks.map_zoom) ui_hooks.map_zoom((int)c->v);
    refresh_page(a, 1);
}

static void cb_elev_zoom(lv_event_t *e)
{
    ui_ctx_t *c = (ui_ctx_t *)lv_event_get_user_data(e);
    c->app->elev_window = c->v;
    if (ui_hooks.elev_zoom) ui_hooks.elev_zoom(c->v);
    refresh_page(c->app, 2);
}

static void cb_row(lv_event_t *e)
{
    ui_ctx_t *c = (ui_ctx_t *)lv_event_get_user_data(e);
    if (ui_hooks.open_climb) ui_hooks.open_climb((int)c->v);
}

static void summary_close(ui_app_t *a);

static void cb_summary_ok(lv_event_t *e)
{
    ui_ctx_t *c = (ui_ctx_t *)lv_event_get_user_data(e);
    summary_close(c->app);
}

static void cb_page_hit(lv_event_t *e)
{
    ui_ctx_t *c = (ui_ctx_t *)lv_event_get_user_data(e);
    ui_app_goto(c->app, (int)c->v, c->v >= c->app->cur ? 1 : -1);
}

static void cb_home_hit(lv_event_t *e)
{
    ui_ctx_t *c = (ui_ctx_t *)lv_event_get_user_data(e);
    lv_screen_load(c->app->home);
}

static void cb_gesture(lv_event_t *e)
{
    ui_app_t *a = ((ui_ctx_t *)lv_event_get_user_data(e))->app;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_LEFT && a->cur < UI_PAGES - 1) ui_app_goto(a, a->cur + 1, 1);
    else if (dir == LV_DIR_RIGHT && a->cur > 0) ui_app_goto(a, a->cur - 1, -1);
    else if (dir == LV_DIR_RIGHT && a->cur == 0) lv_screen_load(a->home);
}

static void cb_load_start(lv_event_t *e)
{
    ui_ctx_t *c = (ui_ctx_t *)lv_event_get_user_data(e);
    refresh_page(c->app, (int)c->v);
}

static void marker_dot(lv_obj_t *dot)
{
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 3, 0);
    lv_obj_set_style_border_color(dot, lv_color_hex(0xFF8A1F), 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
}

static void cb_cell_long(lv_event_t *e);

static void build_data(ui_app_t *a)
{
    lv_obj_t *s = a->page[0] = new_screen();
    status_build(s, &a->st[0], 0);
    a->d_speed = lbl(s, 8, 34, FONT_XL, COL_TEXT, "0.0");
    lbl(s, 136, 74, FONT_M, COL_DIM, "km/h");
    a->d_grade_box = box(s, 214, 36, 98, 68, ui_level_color(0));
    lv_obj_set_style_radius(a->d_grade_box, 10, 0);
    lbl_c(s, 214, 40, 98, FONT_S, COL_TEXT, "GUIDAGE");
    a->d_grade = lbl_c(s, 218, 49, 45, FONT_L, COL_TEXT, "");
    a->d_turn_dist = lbl_c(s, 258, 65, 50, FONT_S, COL_TEXT, "-- m");
    lv_obj_add_event_cb(a->d_grade_box, data_turn_draw, LV_EVENT_DRAW_MAIN, a);

    int ys[5] = {112, 166, 220, 274, 328};
    for (int r = 0; r < 5; r++) a->d_sep[r] = box(s, 8, ys[r] - 2, UI_W - 16, 1, COL_LINE);
    for (int i = 0; i < UI_MAX_CELLS; i++) {
        int r = i / 2, x = (i % 2) ? 170 : 10;
        a->cell_ctx[i] = (ui_ctx_t){a, (float)i};
        a->d_cellbox[i] = box(s, x - 4, ys[r] - 1, 154, 52, COL_BG);
        lv_obj_set_style_bg_opa(a->d_cellbox[i], LV_OPA_TRANSP, 0);
        lv_obj_add_flag(a->d_cellbox[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(a->d_cellbox[i], cb_cell_long, LV_EVENT_LONG_PRESSED, &a->cell_ctx[i]);
        a->d_title[i] = lbl(s, x, ys[r], FONT_S, COL_DIM, "");
        a->d_val[i] = lbl(s, x, ys[r] + 17, FONT_L, COL_TEXT, "--");
        a->d_subval[i] = lbl(s, x, ys[r] + 38, FONT_S, COL_DIM, "");
    }
    a->d_strip_title = lbl(s, 10, 334, FONT_S, COL_DIM, "PROCHAINS KM");
    prof_init(&a->d_strip, s, 8, 352, UI_W - 16, 58);
    a->d_strip_line = box(s, 8, 352, 2, 58, COL_TEXT);
    a->d_strip_dot = box(s, 3, 352 + 58 - 22, 12, 12, COL_TEXT);
    marker_dot(a->d_strip_dot);
    a->d_pause_box = button(s, 8, 422, 148, 50, lv_color_hex(0x3A2A1A), COL_ORG, "PAUSE", FONT_M, cb_pause, NULL, &a->d_pause_lbl);
    a->d_lap_box = button(s, 164, 422, 148, 50, lv_color_hex(0x16283A), COL_ACC, "TOUR", FONT_M, cb_lap, NULL, NULL);
}

/* Nombre de champs (4, 6 ou 8) : les cases en trop disparaissent et le profil des prochains km prend la place. */
static void relayout_data(ui_app_t *a)
{
    int n = a->config.n_cells, rows = n / 2;
    int ys[5] = {112, 166, 220, 274, 328};
    for (int i = 0; i < UI_MAX_CELLS; i++) {
        int hide = i >= n;
        sethide(a->d_cellbox[i], hide); sethide(a->d_title[i], hide); sethide(a->d_val[i], hide); sethide(a->d_subval[i], hide);
    }
    for (int r = 0; r < 5; r++) sethide(a->d_sep[r], r > rows);
    int sy = ys[rows] + 24, sh = 410 - sy;
    lv_obj_set_y(a->d_strip_title, ys[rows] + 6);
    lv_obj_set_pos(a->d_strip.obj, 8, sy);
    lv_obj_set_height(a->d_strip.obj, sh);
    a->d_strip.h = sh;
    lv_obj_set_pos(a->d_strip_line, 8, sy);
    lv_obj_set_height(a->d_strip_line, sh);
    lv_obj_set_pos(a->d_strip_dot, 3, sy + sh - 22);
}

/* ---- Fond de carte -------------------------------------------------------------------------------------------------
 * La carte est dessinee dans un canevas RGB565 (tampon fourni par l'appelant, UI_MAP_W x UI_MAP_H pixels : l'ecran plus une
 * marge) : surfaces remplies par core/polyfill, puis lignes lissees par core/linedraw (sans LVGL : ses files de taches de dessin
 * sont limitees en memoire et ont bloque sur certains enchainements de lignes). Le canevas n'est redessine que si l'echelle ou le cap
 * changent, ou si la position sort de la marge ; sinon il est simplement decale (docs/carte.md). */
typedef struct { uint32_t color; uint8_t width; } map_line_t;
typedef struct {
    uint32_t base;                          /* fond la ou la carte n'a pas de surface */
    uint32_t area[6];                       /* GVT_AREA_URBAN .. GVT_AREA_WATER */
    map_line_t line[GVT_AREA_URBAN];        /* classes de lignes 1 a 9 */
    uint32_t text;                          /* echelle et mention OpenStreetMap */
} map_theme_t;

static const map_theme_t map_themes[2] = {
    {   /* sombre colore */
        0x1B2229,
        {0x343B4B, 0x1F4C35, 0x34422B, 0x2A5C3B, 0x4A443C, 0x1C5C96},
        {[GVT_ROAD_MAJOR] = {0xF0B35A, 4}, [GVT_ROAD_SECONDARY] = {0xDCC57C, 3}, [GVT_ROAD_LOCAL] = {0xB9C1CD, 3},
         [GVT_STREET] = {0x8F99A8, 2}, [GVT_CYCLEWAY] = {0x34D399, 2}, [GVT_TRACK] = {0xB08D57, 1},
         [GVT_RIVER] = {0x4E92D4, 2}, [GVT_LAKE] = {0x4E92D4, 2}, [GVT_RAIL] = {0x9AA3AF, 1}},
        0xE6EAF0,
    },
    {   /* clair */
        0xF2EFE9,
        {0xE2DDD6, 0xB9DAA9, 0xEEF0D5, 0xCDEBB0, 0xDDD6CF, 0xAAD3EE},
        {[GVT_ROAD_MAJOR] = {0xE0893A, 4}, [GVT_ROAD_SECONDARY] = {0xE9B949, 3}, [GVT_ROAD_LOCAL] = {0x8A93A0, 3},
         [GVT_STREET] = {0xA3A8B0, 2}, [GVT_CYCLEWAY] = {0x9B4DCA, 2}, [GVT_TRACK] = {0x9C6B3A, 1},
         [GVT_RIVER] = {0x5B9BD5, 2}, [GVT_LAKE] = {0x5B9BD5, 2}, [GVT_RAIL] = {0x6B7280, 1}},
        0x2A3138,
    },
};

/* Ordre de dessin des surfaces : les champs en dessous, puis forets, roches, villes, parcs, et l'eau par-dessus. */
static const uint8_t map_area_order[6] = {GVT_AREA_FIELD, GVT_AREA_FOREST, GVT_AREA_ROCK, GVT_AREA_URBAN, GVT_AREA_PARK, GVT_AREA_WATER};
/* Passes des lignes : eaux et voies ferrees, puis chemins et petites routes, puis routes principales. */
static const uint8_t map_line_pass[GVT_AREA_URBAN] = {
    [GVT_RIVER] = 0, [GVT_LAKE] = 0, [GVT_RAIL] = 0, [GVT_STREET] = 1, [GVT_TRACK] = 1, [GVT_CYCLEWAY] = 1,
    [GVT_ROAD_LOCAL] = 1, [GVT_ROAD_SECONDARY] = 2, [GVT_ROAD_MAJOR] = 2,
};
#define MAP_REDRAW_COS 0.99966f             /* cos(1,5 degre) : changement de cap qui force un nouveau dessin */

static uint16_t rgb565(uint32_t c) { return (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F)); }

static void map_theme_texts(ui_app_t *a)
{
    uint32_t c = map_themes[a->map_theme ? 1 : 0].text;
    if (!a->map) {
        lv_obj_set_style_bg_color(a->n_scale, COL_TEXT, 0);
        lv_obj_set_style_text_color(a->n_scale_lbl, COL_DIM, 0);
        return;
    }
    lv_obj_set_style_bg_color(a->n_scale, lv_color_hex(c), 0);
    lv_obj_set_style_text_color(a->n_scale_lbl, lv_color_hex(c), 0);
    lv_obj_set_style_text_color(a->n_credit, lv_color_hex(c), 0);
}

static void map_render(ui_app_t *a)
{
    const map_theme_t *th = &map_themes[a->map_theme ? 1 : 0];
    static int16_t axy[2 * GVT_MAX_AREA_POINTS];
    static float fxy[2 * GVT_MAX_AREA_POINTS];
    static int16_t lxy[2 * GVT_MAX_POINTS];
    uint16_t ring_len[GVT_MAX_RINGS];
    mapview_tile_t tl[MAPVIEW_MAX_TILES];
    mapview_view_t v = a->map_view;                   /* meme vue, centree dans le canevas */
    uint16_t base = rgb565(th->base);
    v.cx = UI_W / 2 + UI_MAP_MARGIN;
    v.cy = 190 + UI_MAP_MARGIN;
    v.w = UI_MAP_W;
    v.h = UI_MAP_H;
    for (size_t i = 0; i < (size_t)UI_MAP_W * UI_MAP_H; i++) a->map_buf[i] = base;
    int z = mapview_zoom(a->map->g, &v);
    int nt = mapview_tiles(&v, z, tl, MAPVIEW_MAX_TILES);

    for (int o = 0; o < 6; o++) {                      /* surfaces, par remplissage direct du tampon */
        uint16_t col = rgb565(th->area[map_area_order[o] - GVT_AREA_URBAN]);
        for (int t = 0; t < nt; t++) {
            uint32_t size = 0;
            const uint8_t *blob = mapview_get(a->map, z, tl[t].x, tl[t].y, &size);
            gvt_tile_it_t it;
            uint8_t cls;
            uint16_t nr;
            float m[6];
            if (!blob || gvt_tile_begin(&it, blob, size) != 0) continue;
            mapview_xform(&v, z, tl[t].x, tl[t].y, m);
            while (gvt_tile_next_area(&it, &cls, &nr, ring_len, axy, GVT_MAX_AREA_POINTS) == 1) {
                int total = 0;
                if (cls != map_area_order[o]) continue;
                for (int r = 0; r < nr; r++) total += ring_len[r];
                for (int k = 0; k < total; k++) {
                    fxy[2 * k] = m[0] * axy[2 * k] + m[1] * axy[2 * k + 1] + m[2];
                    fxy[2 * k + 1] = m[3] * axy[2 * k] + m[4] * axy[2 * k + 1] + m[5];
                }
                polyfill_rgb565(a->map_buf, UI_MAP_W, UI_MAP_H, UI_MAP_W, fxy, ring_len, nr, col);
            }
        }
    }

    a->map_segments = 0;                               /* lignes : trace lisse par core/linedraw, directement dans le tampon */
    for (int pass = 0; pass < 3; pass++) {
        for (int t = 0; t < nt; t++) {
            uint32_t size = 0;
            const uint8_t *blob = mapview_get(a->map, z, tl[t].x, tl[t].y, &size);
            gvt_tile_it_t it;
            uint8_t cls;
            uint16_t n;
            float m[6];
            if (!blob || gvt_tile_begin(&it, blob, size) != 0) continue;
            mapview_xform(&v, z, tl[t].x, tl[t].y, m);
            while (gvt_tile_next(&it, &cls, &n, lxy, GVT_MAX_POINTS) == 1) {
                if (map_line_pass[cls] != pass) continue;
                uint16_t col = rgb565(th->line[cls].color);
                float width = (float)th->line[cls].width;
                float px = 0.0f, py = 0.0f;
                for (int k = 0; k < n; k++) {
                    float sx = m[0] * lxy[2 * k] + m[1] * lxy[2 * k + 1] + m[2];
                    float sy = m[3] * lxy[2 * k] + m[4] * lxy[2 * k + 1] + m[5];
                    if (k == 0) { px = sx; py = sy; continue; }
                    float dx = sx - px, dy = sy - py;
                    if (k < n - 1 && dx * dx + dy * dy < 2.25f) continue;       /* moins de 1,5 pixel : inutile */
                    int outside = (px < -8 && sx < -8) || (px > UI_MAP_W + 8 && sx > UI_MAP_W + 8) ||
                                  (py < -8 && sy < -8) || (py > UI_MAP_H + 8 && sy > UI_MAP_H + 8);
                    if (!outside) {
                        linedraw_rgb565(a->map_buf, UI_MAP_W, UI_MAP_H, UI_MAP_W, px, py, sx, sy, width, col);
                        a->map_segments++;
                    }
                    px = sx; py = sy;
                }
            }
        }
    }
    a->map_rendered = a->map_view;
    a->map_valid = 1;
    a->map_renders++;
    lv_obj_set_pos(a->n_map, -UI_MAP_MARGIN, -UI_MAP_MARGIN);
    lv_obj_remove_flag(a->n_map, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(a->n_map);
}

/* A appeler a chaque rafraichissement de la page trace, une fois map_view a jour. */
static void map_update(ui_app_t *a)
{
    const mapview_view_t *n = &a->map_view, *r = &a->map_rendered;
    int need = !a->map_valid || n->scale != r->scale;
    if (!need) need = n->cs * r->cs + n->sn * r->sn < MAP_REDRAW_COS;      /* le cap a tourne de plus de 1,5 degre */
    if (!need) {
        /* meme echelle et meme cap : le canevas se decale. Position, a l'ecran, du centre pour lequel il a ete dessine. */
        float e = r->cx_m - n->cx_m, nn = r->cy_m - n->cy_m;
        float sx = (e * n->cs - nn * n->sn) * n->scale, sy = -(e * n->sn + nn * n->cs) * n->scale;
        if (fabsf(sx) > UI_MAP_MARGIN - 4 || fabsf(sy) > UI_MAP_MARGIN - 4) need = 1;
        else lv_obj_set_pos(a->n_map, (int)lroundf(sx) - UI_MAP_MARGIN, (int)lroundf(sy) - UI_MAP_MARGIN);
    }
    if (need) map_render(a);
}

static void build_nav(ui_app_t *a)
{
    lv_obj_t *s = a->page[1] = new_screen();
    status_build(s, &a->st[1], 1);
    lv_obj_t *map = box(s, 0, 28, UI_W, 380, lv_color_hex(0x141A21));
    a->n_map = lv_canvas_create(map);              /* fond de carte : cree en premier, donc sous la trace */
    lv_obj_remove_flag(a->n_map, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_add_flag(a->n_map, LV_OBJ_FLAG_HIDDEN);
    static const uint32_t cols[3] = {0x4B5563, 0x4AA3FF, 0xF0883E};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *ln = a->n_line[i] = lv_line_create(map);
        lv_obj_set_style_line_width(ln, 6, 0);
        lv_obj_set_style_line_color(ln, lv_color_hex(cols[i]), 0);
        lv_obj_set_style_line_rounded(ln, true, 0);
        lv_obj_set_pos(ln, 0, 0);
        lv_obj_set_size(ln, UI_W, 380);
        lv_obj_remove_flag(ln, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(ln, LV_OBJ_FLAG_HIDDEN);
    }
    marker_dot(box(map, UI_W / 2 - 9, 190 - 9, 18, 18, COL_TEXT));
    a->track_up = 0;
    a->n_mode_btn = button(map, 8, 8, 64, 30, COL_PANEL, COL_TEXT,
                           "NORD", FONT_S, cb_map_mode, a, &a->n_mode_lbl);
    lv_obj_set_style_radius(a->n_mode_btn, 15, 0);
    a->m_ctx[0] = (ui_ctx_t){a, 1.0f};
    a->m_ctx[1] = (ui_ctx_t){a, -1.0f};
    lv_obj_t *zi = button(map, UI_W - 54, 8, 46, 46, COL_PANEL, COL_TEXT, "+", FONT_M, cb_map_zoom, &a->m_ctx[0], NULL);
    lv_obj_set_style_radius(zi, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *zo = button(map, UI_W - 54, 62, 46, 46, COL_PANEL, COL_TEXT, "-", FONT_M, cb_map_zoom, &a->m_ctx[1], NULL);
    lv_obj_set_style_radius(zo, LV_RADIUS_CIRCLE, 0);

    /* Carte de guidage : reste au-dessus de la trace sans masquer les commandes. */
    a->n_turn_card = box(map, 80, 8, 160, 52, COL_PANEL);
    lv_obj_set_style_radius(a->n_turn_card, 8, 0);
    lv_obj_set_style_border_width(a->n_turn_card, 1, 0);
    lv_obj_set_style_border_color(a->n_turn_card, COL_LINE, 0);
    a->n_turn_title = lbl_c(a->n_turn_card, 4, 5, 152, FONT_S, COL_DIM, "PROCHAIN VIRAGE");
    a->n_turn_value = lbl_c(a->n_turn_card, 4, 25, 152, FONT_N, COL_TEXT, "Recherche...");

    a->n_scale = box(map, 10, 380 - 18, 100, 3, COL_TEXT);
    a->n_scale_lbl = lbl(map, 116, 380 - 26, FONT_S, COL_DIM, "1 km");
    a->n_credit = lbl_r(map, 8, 380 - 46, FONT_S, COL_DIM, "© OpenStreetMap");
    lv_obj_add_flag(a->n_credit, LV_OBJ_FLAG_HIDDEN);

    box(s, 0, 408, UI_W, 72, COL_PANEL);
    lbl(s, 12, 414, FONT_S, COL_DIM, "RESTE");
    a->n_reste = lbl(s, 12, 432, FONT_L, COL_TEXT, "--");
    a->n_l1 = lbl_r(s, 12, 412, FONT_S, COL_ORG, "");
    a->n_name = lbl_dot(s, 142, 430, 170, 20, FONT_N, COL_TEXT, "");
    lv_obj_set_style_text_align(a->n_name, LV_TEXT_ALIGN_RIGHT, 0);
    a->n_l3 = lbl_r(s, 12, 452, FONT_S, COL_DIM, "");
}

static void build_elev(ui_app_t *a)
{
    lv_obj_t *s = a->page[2] = new_screen();
    status_build(s, &a->st[2], 2);
    lbl(s, 10, 38, FONT_M, COL_TEXT, "ALTITUDE");
    static const float zooms[3] = {2000.0f, 5000.0f, 10000.0f};
    static const char *names[3] = {"2 km", "5 km", "10 km"};
    for (int i = 0; i < 3; i++) {
        a->e_ctx[i] = (ui_ctx_t){a, zooms[i]};
        a->e_chip[i] = button(s, 140 + i * 59, 34, 54, 32, COL_PANEL, COL_DIM, names[i], FONT_S, cb_elev_zoom, &a->e_ctx[i], &a->e_chip_lbl[i]);
    }
    prof_init(&a->e_prof, s, 8, 76, UI_W - 16, 230);
    for (int i = 0; i < UI_FLAGS; i++) {
        a->e_flag_line[i] = box(s, 0, 76, 2, 230, COL_TEXT);
        a->e_flag_box[i] = box(s, 0, 76, 58, 18, COL_OK);
        lv_obj_set_style_radius(a->e_flag_box[i], 4, 0);
        a->e_flag_lbl[i] = lbl_c(s, 0, 78, 58, FONT_S, COL_BG, "");
        lv_obj_add_flag(a->e_flag_line[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(a->e_flag_box[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(a->e_flag_lbl[i], LV_OBJ_FLAG_HIDDEN);
    }
    box(s, 8, 76, 2, 230, COL_TEXT);
    marker_dot(box(s, 3, 76 + 230 - 22, 12, 12, COL_TEXT));
    for (int i = 0; i < UI_TICKS; i++) {
        a->e_tick[i] = lbl_c(s, 0, 310, 40, FONT_S, COL_DIM, "");
        lv_obj_add_flag(a->e_tick[i], LV_OBJ_FLAG_HIDDEN);
    }
    box(s, 0, 336, UI_W, UI_H - 336, COL_PANEL);
    a->e_alt = lbl_c(s, 0, 346, 106, FONT_L, COL_TEXT, "--");
    lbl_c(s, 0, 380, 106, FONT_S, COL_DIM, "ALTITUDE");
    box(s, 106, 350, 1, 50, COL_LINE);
    a->e_grade = lbl_c(s, 107, 346, 106, FONT_L, COL_TEXT, "--");
    lbl_c(s, 107, 380, 106, FONT_S, COL_DIM, "PENTE");
    box(s, 213, 350, 1, 50, COL_LINE);
    a->e_asc = lbl_c(s, 214, 346, 106, FONT_L, COL_TEXT, "--");
    lbl_c(s, 214, 380, 106, FONT_S, COL_DIM, "D+ À VENIR");
    box(s, 8, 414, UI_W - 16, 1, COL_LINE);
    a->e_ahead = lbl_c(s, 0, 430, UI_W, FONT_M, COL_TEXT, "");
}

static void build_list(ui_app_t *a)
{
    lv_obj_t *s = a->page[3] = new_screen();
    status_build(s, &a->st[3], 3);
    lbl(s, 10, 36, FONT_M, COL_TEXT, "MONTÉES");
    a->l_total = lbl_r(s, 10, 41, FONT_S, COL_DIM, "");
    for (int i = 0; i < UI_ROWS; i++) {
        int y = 64 + i * 69;
        a->l_ctx[i] = (ui_ctx_t){a, 0.0f};
        a->l_row[i] = box(s, 0, y, UI_W, 68, COL_BG);
        lv_obj_add_flag(a->l_row[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(a->l_row[i], cb_row, LV_EVENT_CLICKED, &a->l_ctx[i]);
        a->l_sep[i] = box(s, 8, y + 67, UI_W - 16, 1, COL_LINE);
        a->l_chip[i] = box(s, 8, y + 10, 46, 46, COL_OK);
        lv_obj_set_style_radius(a->l_chip[i], 8, 0);
        lv_obj_remove_flag(a->l_chip[i], LV_OBJ_FLAG_CLICKABLE);
        a->l_cat[i] = lbl_c(s, 8, y + 19, 46, FONT_M, COL_TEXT, "");
        a->l_title[i] = lbl_dot(s, 64, y + 6, 184, 20, FONT_N, COL_TEXT, "");
        a->l_l2[i] = lbl(s, 64, y + 27, FONT_S, COL_DIM, "");
        a->l_l3[i] = lbl(s, 64, y + 45, FONT_S, COL_DIM, "");
        prof_init(&a->l_prof[i], s, 254, y + 16, 58, 40);
    }
}

#define CPY 106
#define CPH 152
static void build_climb(ui_app_t *a)
{
    lv_obj_t *s = a->page[4] = new_screen();
    status_build(s, &a->st[4], 4);
    lv_obj_t *b = a->c_body = lv_obj_create(s);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(b, UI_W, UI_H);
    lv_obj_set_pos(b, 0, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_set_style_radius(b, 0, 0);

    a->c_name = lbl_dot(b, 10, 36, 196, 26, FONT_M, COL_TEXT, "");
    a->c_chip = box(b, 212, 32, 100, 34, ui_level_color(0));
    lv_obj_set_style_radius(a->c_chip, 9, 0);
    a->c_chip_lbl = lbl_c(b, 212, 37, 100, FONT_M, COL_TEXT, "");
    a->c_badge = box(b, 10, 64, 56, 20, ui_category_color(5));
    lv_obj_set_style_radius(a->c_badge, 6, 0);
    a->c_badge_lbl = lbl_c(b, 10, 66, 56, FONT_S, COL_TEXT, "");
    a->c_sub = lbl(b, 74, 66, FONT_S, COL_DIM, "");
    a->c_ahead = lbl_r(b, 10, 66, FONT_S, COL_TEXT, "");
    a->c_stats = lbl(b, 10, 88, FONT_S, COL_DIM, "");
    a->c_emax = lbl_r(b, 10, CPY - 17, FONT_S, COL_DIM, "");
    a->c_emin = lbl(b, 10, CPY + CPH + 2, FONT_S, COL_DIM, "");
    prof_init(&a->c_prof, b, 8, CPY, UI_W - 16, CPH);
    a->c_mline = box(b, 8, CPY - 2, 2, CPH + 2, COL_TEXT);
    a->c_mdot = box(b, 0, 0, 14, 14, COL_TEXT);
    marker_dot(a->c_mdot);

    box(b, 0, 286, UI_W, 100, COL_PANEL);
    a->c_rem = lbl_c(b, 0, 292, 130, FONT_XL, COL_TEXT, "");
    a->c_rem_u = lbl_c(b, 0, 346, 130, FONT_S, COL_DIM, "KM RESTANTS");
    box(b, 130, 298, 1, 76, COL_LINE);
    a->c_avg = lbl_c(b, 131, 302, 88, FONT_L, COL_TEXT, "");
    lbl_c(b, 131, 346, 88, FONT_S, COL_DIM, "PENTE MOY.");
    box(b, 220, 298, 1, 76, COL_LINE);
    a->c_asc = lbl_c(b, 221, 302, 99, FONT_L, COL_TEXT, "");
    lbl_c(b, 221, 346, 99, FONT_S, COL_DIM, "D+ RESTANT");
    a->c_hr = lbl_c(b, 0, 394, 106, FONT_L, COL_DIM, "--");
    lbl_c(b, 0, 428, 106, FONT_S, COL_DIM, "FC bpm");
    a->c_spd = lbl_c(b, 106, 394, 108, FONT_L, COL_TEXT, "--");
    lbl_c(b, 106, 428, 108, FONT_S, COL_DIM, "km/h");
    a->c_cad = lbl_c(b, 214, 394, 106, FONT_L, COL_DIM, "--");
    lbl_c(b, 214, 428, 106, FONT_S, COL_DIM, "CAD rpm");
    box(b, 0, 450, UI_W, 30, COL_PANEL);
    a->c_next_box = box(b, 8, 457, 16, 16, ui_category_color(5));
    lv_obj_set_style_radius(a->c_next_box, 4, 0);
    a->c_next = lbl_dot(b, 32, 458, 282, 16, FONT_S, COL_TEXT, "");

    a->c_empty = lbl_c(s, 0, 220, UI_W, FONT_M, COL_DIM, "Plus de montée sur le parcours");
    lv_obj_add_flag(a->c_empty, LV_OBJ_FLAG_HIDDEN);
}

static void build_summary(ui_app_t *a)
{
    lv_obj_t *top = lv_layer_top();
    lv_obj_t *p = a->s_panel = box(top, 16, 70, 288, 340, lv_color_hex(0x1B232D));
    lv_obj_set_style_radius(p, 14, 0);
    lv_obj_set_style_border_width(p, 2, 0);
    lv_obj_set_style_border_color(p, COL_ACC, 0);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
    a->s_title = lbl_dot(p, 14, 14, 256, 28, FONT_M, COL_TEXT, "");
    a->s_sub = lbl(p, 14, 46, FONT_S, COL_OK, "MONTÉE TERMINÉE");
    static const char *names[6] = {"DURÉE", "DISTANCE", "PENTE MOY.", "DÉNIVELÉ", "VITESSE MOY.", "FC MOY."};
    for (int i = 0; i < 6; i++) {
        int x = 14 + (i % 2) * 138, y = 78 + (i / 2) * 66;
        lbl(p, x, y, FONT_S, COL_DIM, names[i]);
        a->s_val[i] = lbl(p, x, y + 18, FONT_L, COL_TEXT, "");
    }
    a->s_ctx = (ui_ctx_t){a, 0.0f};
    a->s_ok = button(p, 14, 282, 260, 44, COL_ACC, COL_BG, "OK", FONT_M, cb_summary_ok, &a->s_ctx, NULL);
}

/* ======================= donnees derivees et mises a jour ======================= */
static void derive(ui_app_t *a, const ui_sample_t *s)
{
    const ui_course_t *c = &a->course;
    a->ci = c->n_climbs > 0 ? climb_find(c->climbs, c->n_climbs, s->pos_m) : -1;
    a->in_climb = a->ci >= 0 && s->pos_m >= c->climbs[a->ci].start_m && s->pos_m <= c->climbs[a->ci].end_m;
    a->grade_now = climb_grade_at(c->ele, c->n_ele, c->cfg, s->pos_m);
    a->grade_ahead = climb_grade_ahead(c->ele, c->n_ele, c->step_m, s->pos_m, 500.0f);
    a->alt_m = ele_interp(c, s->pos_m);
    memset(&a->prog, 0, sizeof(a->prog));
    if (a->ci >= 0) {
        const climb_t *cl = &c->climbs[a->ci];
        float p = s->pos_m;
        if (p < cl->start_m) p = cl->start_m;
        if (p > cl->end_m) p = cl->end_m;
        climb_progress(cl, c->ele, c->n_ele, c->step_m, p, &a->prog);
    }
}

static void range_of(const ui_course_t *c, const climb_section_t *sec, int n, float *emin, float *range)
{
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < n; i++) {
        int si = (int)(sec[i].start_m / c->step_m);
        int ei = (int)(sec[i].end_m / c->step_m);
        if (si < 0) si = 0;
        if (ei < 0) ei = 0;
        if (si >= c->n_ele) si = c->n_ele - 1;
        if (ei >= c->n_ele) ei = c->n_ele - 1;
        if (c->ele[si] < lo) lo = c->ele[si];
        if (c->ele[si] > hi) hi = c->ele[si];
        if (c->ele[ei] < lo) lo = c->ele[ei];
        if (c->ele[ei] > hi) hi = c->ele[ei];
    }
    if (n == 0) { lo = 0.0f; hi = 40.0f; }
    *emin = lo;
    *range = (hi - lo) < 40.0f ? 40.0f : (hi - lo);
}

static float ascent_between(const ui_course_t *c, float from_m, float to_m)
{
    int i0 = (int)(from_m / c->step_m), i1 = (int)(to_m / c->step_m);
    if (i0 < 0) i0 = 0;
    if (i1 > c->n_ele - 1) i1 = c->n_ele - 1;
    if (i0 >= i1) return 0.0f;
    float asc = 0.0f, ref = c->ele[i0];
    for (int i = i0; i <= i1; i++) {
        if (c->ele[i] - ref >= 1.0f) { asc += c->ele[i] - ref; ref = c->ele[i]; }
        else if (ref - c->ele[i] >= 1.0f) ref = c->ele[i];
    }
    return asc;
}

static void refresh_data(ui_app_t *a)
{
    const ui_sample_t *s = &a->last;
    const ui_course_t *c = &a->course;
    status_update(&a->st[0], s);
    settext(a->d_speed, "%.1f", (double)s->speed_kmh);
    /* Le cartouche en haut a droite est reserve au guidage, pas a la pente.
       La pente reste disponible dans les champs configurables. */
    setbg(a->d_grade_box, a->next_turn.found ?
          (a->next_turn.dist_m <= 50.0f ? COL_BAD : a->next_turn.dist_m <= 150.0f ? COL_ORG : COL_PANEL) : COL_PANEL);
    settext(a->d_grade, "");
    if (a->next_turn.found) settext(a->d_turn_dist, "%.0f m", (double)a->next_turn.dist_m);
    else settext(a->d_turn_dist, "-- m");
    for (int i = 0; i < a->config.n_cells; i++) {
        char b[64];
        lv_color_t col;
        int id = a->config.cell[i];
        field_value(a, id, b, sizeof b, &col);
        settext(a->d_title[i], "%s", ui_field_title(id));
        int cell_y = (int[]){112, 166, 220, 274, 328}[i / 2];
        if (id == 6) {
            setfont(a->d_val[i], FONT_M);
            lv_obj_set_y(a->d_val[i], cell_y + 16);
            settext_c(a->d_val[i], col, "%s", b);
            if (s->hr >= 0) {
                int z = hr_zone_for_bpm(a, s->hr);
                settext_c(a->d_subval[i], col, "%s", hr_zone_name(z));
            } else {
                settext_c(a->d_subval[i], COL_DIM, "--");
            }
            sethide(a->d_subval[i], 0);
        } else {
            setfont(a->d_val[i], FONT_L);
            lv_obj_set_y(a->d_val[i], cell_y + 17);
            settext_c(a->d_val[i], col, "%s", b);
            settext(a->d_subval[i], "");
            sethide(a->d_subval[i], 1);
        }
    }
    static climb_section_t sec[UI_PROF_MAX];
    int n = profile_sections(c->ele, c->n_ele, c->step_m, s->pos_m, s->pos_m + 5000.0f, 50.0f, sec, UI_PROF_MAX);
    float emin, range;
    range_of(c, sec, n, &emin, &range);
    prof_fill(&a->d_strip, sec, n, c->ele, c->n_ele, c->step_m, emin, range, -1.0f);

    settext(a->d_pause_lbl, "%s", s->paused ? "REPRENDRE" : "PAUSE");
    setbg(a->d_pause_box, s->paused ? COL_OK : lv_color_hex(0x3A2A1A));
    lv_color_t fg = s->paused ? COL_BG : COL_ORG;
    if (!col_eq(lv_obj_get_style_text_color(a->d_pause_lbl, 0), fg)) lv_obj_set_style_text_color(a->d_pause_lbl, fg, 0);
}

static void nav_project_point(ui_app_t *a, int idx, float cx, float cy,
                             float scale, float cs, float sn,
                             lv_point_precise_t *p)
{
    const route_t *r = a->course.route;
    float east = r->x[idx] - cx;
    float north = r->y[idx] - cy;
    if (a->track_up) {
        float rotated_east = east * cs - north * sn;
        float rotated_north = east * sn + north * cs;
        east = rotated_east;
        north = rotated_north;
    }
    p->x = (lv_value_precise_t)(UI_W / 2 + east * scale);
    p->y = (lv_value_precise_t)(190 - north * scale);
}

static void update_turn_guidance(ui_app_t *a, const ui_sample_t *s)
{
    const route_t *r = a->course.route;
    if (!r || r->n < 2) {
        memset(&a->next_turn, 0, sizeof(a->next_turn));
        return;
    }
    guidance_turn_t t;
    if (!guidance_next_turn(r, s->pos_m, 700.0f, 35.0f, &t)) {
        memset(&a->next_turn, 0, sizeof(a->next_turn));
        return;
    }
    a->next_turn = t;
    /* Une seule alerte par virage, meme si l ecran courant n est pas la carte. */
    if (s->gps_ok && s->on_route && t.dist_m <= 100.0f && t.dist_m >= 0.0f &&
        t.idx != a->turn_alerted_idx) {
        a->turn_alerted_idx = t.idx;
        if (ui_hooks.turn_alert) ui_hooks.turn_alert(t.direction, t.angle_deg, t.dist_m);
    }
}

static void refresh_turn_card(ui_app_t *a, const ui_sample_t *s)
{
    int off_route = s->gps_ok && !s->on_route && s->off_m >= 0.0f;
    const guidance_turn_t *t = &a->next_turn;

    /* La prochaine manoeuvre est visible dans la barre d'etat de chaque page.
       On masque temporairement la batterie pour liberer cette petite zone. */
    for (int i = 0; i < UI_PAGES; i++) {
        ui_status_t *st = &a->st[i];
        st->turn_state = *t;
        st->turn_off_route = off_route;
        if (off_route) {
            sethide(st->turn_box, 0);
            sethide(st->batt, 1);
            lv_obj_set_x(st->turn_lbl, 1);
            lv_obj_set_width(st->turn_lbl, 72);
            setbg(st->turn_box, COL_BAD);
            settext_c(st->turn_lbl, COL_TEXT, "HORS TRACE");
        } else if (t->found) {
            sethide(st->turn_box, 0);
            sethide(st->batt, 1);
            lv_obj_set_x(st->turn_lbl, 25);
            lv_obj_set_width(st->turn_lbl, 46);
            lv_color_t bg = t->dist_m <= 50.0f ? COL_BAD : (t->dist_m <= 150.0f ? COL_ORG : COL_PANEL);
            setbg(st->turn_box, bg);
            settext_c(st->turn_lbl, COL_TEXT, "%.0fm", (double)t->dist_m);
        } else {
            sethide(st->turn_box, 1);
            sethide(st->batt, 0);
        }
    }

    /* Carte de guidage detaillee sur l'ecran de navigation. */
    if (off_route) {
        setbg(a->n_turn_card, COL_BAD);
        settext_c(a->n_turn_title, COL_TEXT, "HORS PARCOURS");
        settext_c(a->n_turn_value, COL_TEXT, "À %.0f m de la trace", (double)s->off_m);
        return;
    }
    if (!t->found) {
        setbg(a->n_turn_card, COL_PANEL);
        settext_c(a->n_turn_title, COL_DIM, "PROCHAIN VIRAGE");
        settext_c(a->n_turn_value, COL_TEXT, "Pas de virage proche");
        return;
    }
    lv_color_t bg = t->dist_m <= 50.0f ? COL_BAD : (t->dist_m <= 150.0f ? COL_ORG : COL_PANEL);
    setbg(a->n_turn_card, bg);
    settext_c(a->n_turn_title, t->dist_m <= 150.0f ? COL_TEXT : COL_DIM,
              t->dist_m <= 50.0f ? "VIRAGE IMMINENT" : "PROCHAIN VIRAGE");
    settext_c(a->n_turn_value, COL_TEXT, "%s · %.0f m",
              t->maneuver ? "ROND-POINT" : (t->angle_deg < 50 ? (t->direction > 0 ? "LÉGER À DROITE" : "LÉGER À GAUCHE") :
              t->angle_deg < 120 ? (t->direction > 0 ? "À DROITE" : "À GAUCHE") :
              (t->direction > 0 ? "DEMI-TOUR DROITE" : "DEMI-TOUR GAUCHE")), (double)t->dist_m);
}

static void refresh_nav(ui_app_t *a)
{
    const ui_sample_t *s = &a->last;
    const ui_course_t *c = &a->course;
    const route_t *r = c->route;
    status_update(&a->st[1], s);
    refresh_turn_card(a, s);
    float step = r->step_m;
    int ci = (int)(s->pos_m / step);
    if (ci < 0) ci = 0;
    if (ci >= r->n) ci = r->n - 1;
    float cx = r->x[ci], cy = r->y[ci];
    float scale = (UI_W / 2.0f) / a->view_m;
    float cs = 1.0f, sn = 0.0f;
    if (a->track_up && r->n > 1) {
        int look = (int)(30.0f / step);
        if (look < 1) look = 1;
        int lo = ci, hi = ci + look;
        if (hi >= r->n) {
            hi = r->n - 1;
            lo = ci - look;
            if (lo < 0) lo = 0;
        }
        if (hi == lo) {
            if (hi < r->n - 1) hi++;
            else if (lo > 0) lo--;
        }
        float dx = r->x[hi] - r->x[lo];
        float dy = r->y[hi] - r->y[lo];
        if (dx * dx + dy * dy > 0.01f) {
            float heading = atan2f(dx, dy);
            cs = cosf(heading);
            sn = sinf(heading);
        }
    }
    if (a->map && a->map_buf) {
        a->map_view = (mapview_view_t){ r->lat0, r->lon0, r->cos0, cx, cy, scale, cs, sn, UI_W / 2, 190, UI_W, 380 };
        map_update(a);
    }
    int m = (int)(a->view_m * 3.0f / step) + 20;
    int lo = ci - m, hi = ci + m;
    if (lo < 0) lo = 0;
    if (hi > r->n - 1) hi = r->n - 1;
    int sub = 1;
    while ((ci - lo) / sub >= UI_PTS_MAX - 1 || (hi - ci) / sub >= UI_PTS_MAX - 1) sub++;
    int cnt[3] = {0, 0, 0};
    for (int i = lo; i <= ci; i += sub, cnt[0]++) {
        nav_project_point(a, i, cx, cy, scale, cs, sn, &a->pts[0][cnt[0]]);
    }
    for (int i = ci; i <= hi; i += sub, cnt[1]++) {
        nav_project_point(a, i, cx, cy, scale, cs, sn, &a->pts[1][cnt[1]]);
    }
    const climb_t *nx = (a->ci >= 0) ? &c->climbs[a->ci] : NULL;
    if (nx) {
        int i0 = nx->start_idx, i1 = nx->end_idx;
        if (i0 < lo) i0 = lo;
        if (i1 > hi) i1 = hi;
        for (int i = i0; i <= i1 && cnt[2] < UI_PTS_MAX; i += sub, cnt[2]++) {
            nav_project_point(a, i, cx, cy, scale, cs, sn, &a->pts[2][cnt[2]]);
        }
    }
    for (int k = 0; k < 3; k++) {
        if (cnt[k] >= 2) { lv_line_set_points(a->n_line[k], a->pts[k], (uint32_t)cnt[k]); sethide(a->n_line[k], 0); }
        else sethide(a->n_line[k], 1);
    }
    static const float nice[6] = {100.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f};
    float d = nice[0];
    for (int k = 0; k < 6; k++) if (nice[k] * scale <= 140.0f) d = nice[k];
    int w = (int)(d * scale);
    if (lv_obj_get_width(a->n_scale) != w) lv_obj_set_width(a->n_scale, w);
    if (d >= 1000.0f) settext(a->n_scale_lbl, "%g km", (double)(d / 1000.0f)); else settext(a->n_scale_lbl, "%.0f m", (double)d);
    lv_obj_set_x(a->n_scale_lbl, 10 + w + 6);

    settext(a->n_reste, "%.1f km", (double)((r->total_m - s->pos_m) / 1000.0f));
    if (nx && !a->in_climb) {
        settext(a->n_l1, "MONTÉE DANS %.1f km", (double)((nx->start_m - s->pos_m) / 1000.0f));
        settext(a->n_name, "%s", nx->name);
        settext(a->n_l3, "%.1f km  %.1f %%", (double)(nx->length_m / 1000.0f), (double)nx->avg_grade_pct);
    } else if (nx) {
        settext(a->n_l1, "MONTÉE EN COURS");
        settext(a->n_name, "%s", nx->name);
        settext(a->n_l3, "%.1f km restants", (double)(a->prog.dist_remaining_m / 1000.0f));
    } else {
        settext(a->n_l1, "");
        settext(a->n_name, "");
        settext(a->n_l3, "");
    }
}

static void refresh_elev(ui_app_t *a)
{
    const ui_sample_t *s = &a->last;
    const ui_course_t *c = &a->course;
    status_update(&a->st[2], s);
    float win = a->elev_window;
    for (int i = 0; i < 3; i++) {
        int active = fabsf(a->e_ctx[i].v - win) < 1.0f;
        setbg(a->e_chip[i], active ? COL_ACC : COL_PANEL);
        lv_color_t fg = active ? COL_BG : COL_DIM;
        if (!col_eq(lv_obj_get_style_text_color(a->e_chip_lbl[i], 0), fg)) lv_obj_set_style_text_color(a->e_chip_lbl[i], fg, 0);
    }
    static climb_section_t sec[UI_PROF_MAX];
    float sm = win / 100.0f;
    if (sm < c->step_m) sm = c->step_m;
    int n = profile_sections(c->ele, c->n_ele, c->step_m, s->pos_m, s->pos_m + win, sm, sec, UI_PROF_MAX);
    float emin, range;
    range_of(c, sec, n, &emin, &range);
    prof_fill(&a->e_prof, sec, n, c->ele, c->n_ele, c->step_m, emin, range, -1.0f);

    int used = 0;
    for (int i = 0; i < c->n_climbs && used < UI_FLAGS; i++) {
        float rel = c->climbs[i].start_m - s->pos_m;
        if (rel <= 0.0f || rel > win) continue;
        int x = 8 + (int)(rel / win * (UI_W - 16));
        int fx = (x + 58 <= UI_W) ? x : x - 58;
        lv_obj_set_x(a->e_flag_line[used], x - 1);
        lv_obj_set_x(a->e_flag_box[used], fx);
        lv_obj_set_x(a->e_flag_lbl[used], fx);
        setbg(a->e_flag_box[used], ui_category_color(climb_category(c->climbs[i].score)));
        settext(a->e_flag_lbl[used], "%.1f km", (double)(rel / 1000.0f));
        sethide(a->e_flag_line[used], 0); sethide(a->e_flag_box[used], 0); sethide(a->e_flag_lbl[used], 0);
        used++;
    }
    for (int i = used; i < UI_FLAGS; i++) { sethide(a->e_flag_line[i], 1); sethide(a->e_flag_box[i], 1); sethide(a->e_flag_lbl[i], 1); }

    float tick = win <= 2500.0f ? 500.0f : (win <= 6000.0f ? 1000.0f : 2000.0f);
    int k = 0;
    for (float t = 0.0f; t <= win + 1.0f && k < UI_TICKS; t += tick, k++) {
        int x = 8 + (int)(t / win * (UI_W - 16));
        int lx = x - 20;
        if (lx < 0) lx = 0;
        if (lx > UI_W - 40) lx = UI_W - 40;
        lv_obj_set_x(a->e_tick[k], lx);
        settext(a->e_tick[k], "%g", (double)(t / 1000.0f));
        sethide(a->e_tick[k], 0);
    }
    for (; k < UI_TICKS; k++) sethide(a->e_tick[k], 1);

    settext(a->e_alt, "%.0f m", (double)a->alt_m);
    settext_c(a->e_grade, ui_level_color(level_of(a->grade_now)), "%.1f %%", (double)a->grade_now);
    settext(a->e_asc, "+%.0f m", (double)ascent_between(c, s->pos_m, s->pos_m + win));
    settext_c(a->e_ahead, ui_level_color(level_of(a->grade_ahead)), "Pente des 500 m devant : %.1f %%", (double)a->grade_ahead);
}

static void refresh_list(ui_app_t *a)
{
    const ui_sample_t *s = &a->last;
    const ui_course_t *c = &a->course;
    status_update(&a->st[3], s);
    settext(a->l_total, "%d sur le parcours", c->n_climbs);
    int first = a->ci >= 0 ? a->ci : c->n_climbs;
    for (int i = 0; i < UI_ROWS; i++) {
        int idx = first + i;
        int hide = idx >= c->n_climbs;
        sethide(a->l_row[i], hide); sethide(a->l_sep[i], hide); sethide(a->l_chip[i], hide); sethide(a->l_cat[i], hide);
        sethide(a->l_title[i], hide); sethide(a->l_l2[i], hide); sethide(a->l_l3[i], hide); sethide(a->l_prof[i].obj, hide);
        if (hide) { a->l_prof[i].n = 0; continue; }
        const climb_t *cl = &c->climbs[idx];
        int cat = climb_category(cl->score);
        a->l_ctx[i].v = (float)(idx + 1);
        setbg(a->l_chip[i], ui_category_color(cat));
        settext_c(a->l_cat[i], cat == 3 ? COL_BG : COL_TEXT, "%s", category_name(cat));
        if (cl->name[0]) settext(a->l_title[i], "%s", cl->name); else settext(a->l_title[i], "Montée %d", idx + 1);
        int prog = s->pos_m >= cl->start_m && s->pos_m <= cl->end_m;
        if (prog) settext_c(a->l_l2[i], COL_ORG, "EN COURS"); else settext_c(a->l_l2[i], COL_DIM, "Dans %.1f km", (double)((cl->start_m - s->pos_m) / 1000.0f));
        settext(a->l_l3[i], "%.1f km  %.1f %%  +%.0f m", (double)(cl->length_m / 1000.0f), (double)cl->avg_grade_pct, (double)cl->ascent_m);
        climb_section_t sec[24];
        int ns = profile_sections(c->ele, c->n_ele, c->step_m, cl->start_m, cl->end_m, cl->length_m / 20.0f, sec, 24);
        float emin = c->ele[cl->start_idx], emax = c->ele[cl->end_idx];
        prof_fill(&a->l_prof[i], sec, ns, c->ele, c->n_ele, c->step_m, emin, emax - emin + 0.1f, -1.0f);
    }
}

static void refresh_climb(ui_app_t *a)
{
    const ui_sample_t *s = &a->last;
    const ui_course_t *c = &a->course;
    status_update(&a->st[4], s);
    if (a->ci < 0) { sethide(a->c_body, 1); sethide(a->c_empty, 0); return; }
    sethide(a->c_body, 0); sethide(a->c_empty, 1);
    const climb_t *cl = &c->climbs[a->ci];
    int cat = climb_category(cl->score);
    char title[48];
    if (cl->name[0]) snprintf(title, sizeof title, "%s", cl->name); else snprintf(title, sizeof title, "MONTÉE %d/%d", a->ci + 1, c->n_climbs);
    settext(a->c_name, "%s", title);
    setfont(a->c_name, strlen(title) > 15 ? FONT_N : FONT_M);
    lv_obj_set_y(a->c_name, strlen(title) > 15 ? 40 : 36);
    if (a->in_climb) {
        setbg(a->c_chip, ui_level_color(level_of(a->grade_now)));
        settext_c(a->c_chip_lbl, COL_TEXT, "%.1f %%", (double)a->grade_now);
        settext_c(a->c_ahead, ui_level_color(level_of(a->grade_ahead)), "500 m: %.1f %%", (double)a->grade_ahead);
    } else {
        setbg(a->c_chip, COL_PANEL);
        settext_c(a->c_chip_lbl, COL_DIM, "dans %.1f km", (double)((cl->start_m - s->pos_m) / 1000.0f));
        settext(a->c_ahead, "");
    }
    setbg(a->c_badge, ui_category_color(cat));
    settext_c(a->c_badge_lbl, cat == 3 ? COL_BG : COL_TEXT, "CAT %s", category_name(cat));
    settext(a->c_sub, "MONTÉE %d/%d", a->ci + 1, c->n_climbs);
    settext(a->c_stats, "%.1f km  %.1f %%  +%.0f m", (double)(cl->length_m / 1000.0f), (double)cl->avg_grade_pct, (double)cl->ascent_m);

    float emin = 1e9f, emax = -1e9f;
    for (int i = cl->start_idx; i <= cl->end_idx && i < c->n_ele; i++) {
        if (c->ele[i] < emin) emin = c->ele[i];
        if (c->ele[i] > emax) emax = c->ele[i];
    }
    settext(a->c_emax, "%.0f m", (double)emax);
    settext(a->c_emin, "%.0f m", (double)emin);
    static climb_section_t sec[UI_PROF_MAX];
    climb_cfg_t cc = *c->cfg;
    if (cl->length_m / cc.section_m > (float)UI_PROF_MAX) cc.section_m = cl->length_m / (float)UI_PROF_MAX + 1.0f;
    int ns = climb_sections(cl, c->ele, c->n_ele, &cc, sec, UI_PROF_MAX);
    float pos = s->pos_m;
    prof_fill(&a->c_prof, sec, ns, c->ele, c->n_ele, c->step_m, emin, emax - emin + 0.1f, a->in_climb ? pos : -1.0f);
    float frac = (pos - cl->start_m) / (cl->end_m - cl->start_m);
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    int xpos = 8 + (int)(frac * (UI_W - 16));
    int top = CPY + CPH;
    for (int i = 0; i < a->c_prof.n; i++)
        if (xpos - 8 >= a->c_prof.bars[i].x0 && xpos - 8 <= a->c_prof.bars[i].x1) { top = CPY + CPH - a->c_prof.bars[i].h; break; }
    lv_obj_set_x(a->c_mline, xpos - 1);
    lv_obj_set_pos(a->c_mdot, xpos - 7, top - 7);

    float rem = a->prog.dist_remaining_m;
    if (rem >= 1000.0f) settext(a->c_rem, "%.2f", (double)(rem / 1000.0f)); else settext(a->c_rem, "%.0f", (double)rem);
    settext(a->c_rem_u, "%s", rem >= 1000.0f ? "KM RESTANTS" : "M RESTANTS");
    settext_c(a->c_avg, ui_level_color(level_of(a->prog.avg_grade_remaining_pct)), "%.1f%%", (double)a->prog.avg_grade_remaining_pct);
    settext(a->c_asc, "%.0f m", (double)a->prog.ascent_remaining_m);
    if (s->hr >= 0) settext_c(a->c_hr, COL_BAD, "%d", s->hr); else settext_c(a->c_hr, COL_DIM, "--");
    settext(a->c_spd, "%.1f", (double)s->speed_kmh);
    if (s->cad >= 0) settext_c(a->c_cad, COL_ACC, "%d", s->cad); else settext_c(a->c_cad, COL_DIM, "--");
    if (a->ci + 1 < c->n_climbs) {
        const climb_t *nx = &c->climbs[a->ci + 1];
        setbg(a->c_next_box, ui_category_color(climb_category(nx->score)));
        if (nx->name[0]) settext(a->c_next, "Ensuite : %s, %.1f km", nx->name, (double)((nx->start_m - s->pos_m) / 1000.0f));
        else settext(a->c_next, "Ensuite : montée %d, %.1f km", a->ci + 2, (double)((nx->start_m - s->pos_m) / 1000.0f));
    } else {
        setbg(a->c_next_box, COL_DIM);
        settext(a->c_next, "Dernière montée du parcours");
    }
}

static void refresh_page(ui_app_t *a, int idx)
{
    if (!a->have_sample) return;
    switch (idx) {
    case 0: refresh_data(a); break;
    case 1: refresh_nav(a); break;
    case 2: refresh_elev(a); break;
    case 3: refresh_list(a); break;
    case 4: refresh_climb(a); break;
    default: break;
    }
}


/* ======================= selecteur de champ (appui long sur une case) ======================= */
static void notify_config(ui_app_t *a) { if (ui_hooks.config_changed) ui_hooks.config_changed(&a->config); }

static void picker_mark(ui_app_t *a)
{
    int cur = a->config.cell[a->picker_cell];
    for (int i = 0; i < UI_FIELDS; i++) {
        int sel = i == cur;
        setbg(a->p_row[i], sel ? COL_ACC : COL_PANEL);
        lv_color_t fg = sel ? COL_BG : COL_TEXT;
        if (!col_eq(lv_obj_get_style_text_color(a->p_row_lbl[i], 0), fg)) lv_obj_set_style_text_color(a->p_row_lbl[i], fg, 0);
    }
    for (int i = 0; i < 3; i++) {
        int act = a->config.n_cells == 4 + 2 * i;
        setbg(a->p_lay[i], act ? COL_ACC : COL_PANEL);
        lv_color_t fg = act ? COL_BG : COL_DIM;
        if (!col_eq(lv_obj_get_style_text_color(a->p_lay_lbl[i], 0), fg)) lv_obj_set_style_text_color(a->p_lay_lbl[i], fg, 0);
    }
    settext(a->p_title, "CHAMP %d", a->picker_cell + 1);
}

static void cb_cell_long(lv_event_t *e)
{
    ui_ctx_t *c = (ui_ctx_t *)lv_event_get_user_data(e);
    ui_app_t *a = c->app;
    a->picker_cell = (int)c->v;
    picker_mark(a);
    lv_obj_scroll_to_y(a->p_list, 0, LV_ANIM_OFF);
    sethide(a->p_panel, 0);
}

static void cb_pick(lv_event_t *e)
{
    ui_ctx_t *c = (ui_ctx_t *)lv_event_get_user_data(e);
    ui_app_t *a = c->app;
    a->config.cell[a->picker_cell] = (uint8_t)c->v;
    notify_config(a);
    sethide(a->p_panel, 1);
    refresh_page(a, 0);
}

static void cb_layout(lv_event_t *e)
{
    ui_ctx_t *c = (ui_ctx_t *)lv_event_get_user_data(e);
    ui_app_t *a = c->app;
    a->config.n_cells = (uint8_t)c->v;
    if (a->picker_cell >= a->config.n_cells) a->picker_cell = a->config.n_cells - 1;
    relayout_data(a);
    notify_config(a);
    picker_mark(a);
    refresh_page(a, 0);
}

static void cb_picker_close(lv_event_t *e) { sethide(((ui_ctx_t *)lv_event_get_user_data(e))->app->p_panel, 1); }

static void build_picker(ui_app_t *a)
{
    lv_obj_t *top = lv_layer_top();
    a->p_panel = box(top, 0, 0, UI_W, UI_H, lv_color_hex(0x000000));
    lv_obj_set_style_bg_opa(a->p_panel, 170, 0);
    lv_obj_add_flag(a->p_panel, LV_OBJ_FLAG_HIDDEN);
    a->p_card = box(a->p_panel, 16, 40, 288, 420, lv_color_hex(0x1B232D));
    lv_obj_set_style_radius(a->p_card, 14, 0);
    lv_obj_set_style_border_width(a->p_card, 2, 0);
    lv_obj_set_style_border_color(a->p_card, COL_ACC, 0);
    a->p_title = lbl(a->p_card, 14, 12, FONT_M, COL_TEXT, "CHAMP 1");
    lbl(a->p_card, 14, 50, FONT_S, COL_DIM, "NOMBRE DE CHAMPS");
    static const char *names[3] = {"4", "6", "8"};
    for (int i = 0; i < 3; i++) {
        a->lay_ctx[i] = (ui_ctx_t){a, (float)(4 + 2 * i)};
        a->p_lay[i] = button(a->p_card, 14 + i * 70, 72, 62, 34, COL_PANEL, COL_DIM, names[i], FONT_M, cb_layout, &a->lay_ctx[i], &a->p_lay_lbl[i]);
    }
    a->p_list = lv_obj_create(a->p_card);
    lv_obj_set_size(a->p_list, 260, 230);
    lv_obj_set_pos(a->p_list, 14, 118);
    lv_obj_set_style_bg_opa(a->p_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(a->p_list, 0, 0);
    lv_obj_set_style_pad_all(a->p_list, 0, 0);
    lv_obj_set_style_radius(a->p_list, 0, 0);
    lv_obj_set_scroll_dir(a->p_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(a->p_list, LV_SCROLLBAR_MODE_OFF);
    for (int i = 0; i < UI_FIELDS; i++) {
        a->p_ctx[i] = (ui_ctx_t){a, (float)i};
        a->p_row[i] = box(a->p_list, 0, i * 42, 260, 40, COL_PANEL);
        lv_obj_set_style_radius(a->p_row[i], 8, 0);
        lv_obj_add_flag(a->p_row[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(a->p_row[i], cb_pick, LV_EVENT_CLICKED, &a->p_ctx[i]);
        a->p_row_lbl[i] = lbl(a->p_row[i], 12, 10, FONT_N, COL_TEXT, ui_field_title(i));
    }
    a->close_ctx = (ui_ctx_t){a, 0.0f};
    a->p_close = button(a->p_card, 14, 362, 260, 44, COL_ACC, COL_BG, "FERMER", FONT_M, cb_picker_close, &a->close_ctx, NULL);
}

void ui_app_set_config(ui_app_t *a, const ui_config_t *c)
{
    if (c->n_cells != 4 && c->n_cells != 6 && c->n_cells != 8) return;
    for (int i = 0; i < UI_MAX_CELLS; i++) if (c->cell[i] >= UI_FIELDS) return;
    a->config = *c;
    relayout_data(a);
    refresh_page(a, 0);
}

const ui_config_t *ui_app_get_config(const ui_app_t *a) { return &a->config; }


/* ======================= alertes : banniere et reglages ======================= */
static void update_banner(ui_app_t *a)
{
    const alert_t *c = alerts_current(&a->alerts);
    if (!c) { sethide(a->b_panel, 1); return; }
    lv_color_t bg, bd;
    if (c->level == ALERT_CRIT) { bg = lv_color_hex(0x3A1418); bd = COL_BAD; }
    else if (c->level == ALERT_WARN) { bg = lv_color_hex(0x3A2A1A); bd = COL_ORG; }
    else { bg = lv_color_hex(0x16283A); bd = COL_ACC; }
    setbg(a->b_panel, bg);
    setbg(a->b_strip, bd);
    if (!col_eq(lv_obj_get_style_border_color(a->b_panel, 0), bd)) lv_obj_set_style_border_color(a->b_panel, bd, 0);
    settext(a->b_title, "%s", c->title);
    settext(a->b_sub, "%s", c->sub);
    int p = alerts_pending(&a->alerts);
    if (p > 0) settext(a->b_more, "+%d", p); else settext(a->b_more, "");
    sethide(a->b_panel, 0);
}

static void cb_banner(lv_event_t *e)
{
    ui_app_t *a = ((ui_ctx_t *)lv_event_get_user_data(e))->app;
    alerts_dismiss(&a->alerts);
    update_banner(a);
}

static void alerts_notify(ui_app_t *a) { if (ui_hooks.alerts_changed) ui_hooks.alerts_changed(&a->alerts.cfg); }

static void settings_mark(ui_app_t *a)
{
    for (int i = 0; i < ALERT_KINDS; i++) {
        int on = a->alerts.cfg.enabled[i];
        setbg(a->as_tog[i], on ? COL_OK : COL_PANEL);
        settext(a->as_tog_lbl[i], "%s", on ? "OUI" : "NON");
        lv_color_t fg = on ? COL_BG : COL_DIM;
        if (!col_eq(lv_obj_get_style_text_color(a->as_tog_lbl[i], 0), fg)) lv_obj_set_style_text_color(a->as_tog_lbl[i], fg, 0);
    }
    settext(a->as_hr_val, "%d", a->alerts.cfg.hr_max_bpm);
}

static void cb_alert_toggle(lv_event_t *e)
{
    ui_ctx_t *c = (ui_ctx_t *)lv_event_get_user_data(e);
    ui_app_t *a = c->app;
    int k = (int)c->v;
    a->alerts.cfg.enabled[k] = !a->alerts.cfg.enabled[k];
    alerts_notify(a);
    settings_mark(a);
}

static void cb_alert_hr(lv_event_t *e)
{
    ui_ctx_t *c = (ui_ctx_t *)lv_event_get_user_data(e);
    ui_app_t *a = c->app;
    int v = a->alerts.cfg.hr_max_bpm + (int)c->v;
    if (v < 80) v = 80;
    if (v > 230) v = 230;
    a->alerts.cfg.hr_max_bpm = v;
    alerts_notify(a);
    settings_mark(a);
}

static void hr_zones_mark(ui_app_t *a)
{
    settext(a->hz_val[0], "%d bpm", a->hr_zones.z2_min_bpm);
    settext(a->hz_val[1], "%d bpm", a->hr_zones.z3_min_bpm);
    settext(a->hz_val[2], "%d bpm", a->hr_zones.z4_min_bpm);
    settext(a->hz_val[3], "%d bpm", a->hr_zones.z5_min_bpm);
}

static int *hr_zone_threshold(ui_app_t *a, int idx)
{
    switch (idx) {
    case 0: return &a->hr_zones.z2_min_bpm;
    case 1: return &a->hr_zones.z3_min_bpm;
    case 2: return &a->hr_zones.z4_min_bpm;
    default: return &a->hr_zones.z5_min_bpm;
    }
}

static void cb_hr_zone_step(lv_event_t *e)
{
    ui_ctx_t *c = (ui_ctx_t *)lv_event_get_user_data(e);
    ui_app_t *a = c->app;
    int idx = (int)((int)c->v / 2);
    int delta = ((int)c->v % 2 == 0) ? -1 : 1;
    int *v = hr_zone_threshold(a, idx);
    int next = *v + delta;
    int low = idx == 0 ? 60 : *hr_zone_threshold(a, idx - 1) + 1;
    int high = idx == 3 ? 230 : *hr_zone_threshold(a, idx + 1) - 1;
    if (next < low) next = low;
    if (next > high) next = high;
    *v = next;
    hr_zones_mark(a);
    if (ui_hooks.hr_zones_changed) ui_hooks.hr_zones_changed(&a->hr_zones);
    refresh_page(a, 0);
}

static void cb_hr_zones_close(lv_event_t *e)
{
    ui_app_t *a = ((ui_ctx_t *)lv_event_get_user_data(e))->app;
    sethide(a->hz_panel, 1);
}

static void cb_hr_zones_open(lv_event_t *e)
{
    ui_app_t *a = ((ui_ctx_t *)lv_event_get_user_data(e))->app;
    sethide(a->as_panel, 1);
    hr_zones_mark(a);
    sethide(a->hz_panel, 0);
}

static void cb_alert_close(lv_event_t *e) { sethide(((ui_ctx_t *)lv_event_get_user_data(e))->app->as_panel, 1); }

static void cb_status_long(lv_event_t *e)
{
    ui_app_t *a = ((ui_ctx_t *)lv_event_get_user_data(e))->app;
    settings_mark(a);
    sethide(a->as_panel, 0);
}

static void build_settings(ui_app_t *a)
{
    lv_obj_t *top = lv_layer_top();
    a->as_panel = box(top, 0, 0, UI_W, UI_H, lv_color_hex(0x000000));
    lv_obj_set_style_bg_opa(a->as_panel, 170, 0);
    lv_obj_add_flag(a->as_panel, LV_OBJ_FLAG_HIDDEN);
    a->as_card = box(a->as_panel, 16, 40, 288, 420, lv_color_hex(0x1B232D));
    lv_obj_set_style_radius(a->as_card, 14, 0);
    lv_obj_set_style_border_width(a->as_card, 2, 0);
    lv_obj_set_style_border_color(a->as_card, COL_ACC, 0);
    lbl(a->as_card, 14, 12, FONT_M, COL_TEXT, "ALERTES");
    a->as_close_ctx = (ui_ctx_t){a, 0.0f};
    a->as_zones_btn = button(a->as_card, 154, 8, 120, 32, COL_PANEL, COL_ACC, "ZONES FC", FONT_S, cb_hr_zones_open, &a->as_close_ctx, NULL);
    static const char *names[ALERT_KINDS] = {"Hors parcours", "Batterie faible", "Fréq. cardiaque élevée", "Signal GPS perdu", "Ceinture cardio perdue", "Montée proche"};
    for (int i = 0; i < ALERT_KINDS; i++) {
        int y = 52 + i * 40;
        a->as_ctx[i] = (ui_ctx_t){a, (float)i};
        lbl(a->as_card, 14, y + 7, FONT_N, COL_TEXT, names[i]);
        a->as_tog[i] = button(a->as_card, 208, y, 66, 34, COL_PANEL, COL_DIM, "NON", FONT_N, cb_alert_toggle, &a->as_ctx[i], &a->as_tog_lbl[i]);
    }
    int y = 52 + ALERT_KINDS * 40;
    lbl(a->as_card, 14, y + 7, FONT_N, COL_TEXT, "Limite FC (bpm)");
    a->as_hr_ctx[0] = (ui_ctx_t){a, -5.0f};
    a->as_hr_ctx[1] = (ui_ctx_t){a, 5.0f};
    button(a->as_card, 160, y, 40, 36, COL_PANEL, COL_TEXT, "-", FONT_M, cb_alert_hr, &a->as_hr_ctx[0], NULL);
    a->as_hr_val = lbl_c(a->as_card, 200, y + 6, 40, FONT_M, COL_TEXT, "180");
    button(a->as_card, 238, y, 40, 36, COL_PANEL, COL_TEXT, "+", FONT_M, cb_alert_hr, &a->as_hr_ctx[1], NULL);
    lbl(a->as_card, 14, y + 48, FONT_S, COL_DIM, "Batterie : alerte à 20 % puis à 10 %");
    a->as_close = button(a->as_card, 14, 368, 260, 40, COL_ACC, COL_BG, "FERMER", FONT_M, cb_alert_close, &a->as_close_ctx, NULL);

    a->hz_panel = box(top, 0, 0, UI_W, UI_H, lv_color_hex(0x000000));
    lv_obj_set_style_bg_opa(a->hz_panel, 190, 0);
    lv_obj_add_flag(a->hz_panel, LV_OBJ_FLAG_HIDDEN);
    a->hz_card = box(a->hz_panel, 16, 48, 288, 384, lv_color_hex(0x1B232D));
    lv_obj_set_style_radius(a->hz_card, 14, 0);
    lv_obj_set_style_border_width(a->hz_card, 2, 0);
    lv_obj_set_style_border_color(a->hz_card, COL_ACC, 0);
    lbl(a->hz_card, 14, 12, FONT_M, COL_TEXT, "ZONES CARDIAQUES");
    lbl_dot(a->hz_card, 14, 42, 258, 32, FONT_S, COL_DIM, "Seuils personnalisés en bpm");
    static const char *znames[4] = {"Z1 / Z2", "Z2 / Z3", "Z3 / Z4", "Z4 / Z5"};
    for (int i = 0; i < 4; i++) {
        int y = 82 + i * 56;
        box(a->hz_card, 10, y, 268, 48, COL_PANEL);
        lbl(a->hz_card, 18, y + 15, FONT_N, COL_TEXT, znames[i]);
        a->hz_ctx[2 * i] = (ui_ctx_t){a, (float)(2 * i)};
        a->hz_ctx[2 * i + 1] = (ui_ctx_t){a, (float)(2 * i + 1)};
        button(a->hz_card, 126, y + 7, 34, 34, lv_color_hex(0x252D38), COL_TEXT, "-", FONT_M, cb_hr_zone_step, &a->hz_ctx[2 * i], NULL);
        a->hz_val[i] = lbl_c(a->hz_card, 160, y + 14, 76, FONT_N, COL_TEXT, "-- bpm");
        button(a->hz_card, 242, y + 7, 34, 34, lv_color_hex(0x252D38), COL_TEXT, "+", FONT_M, cb_hr_zone_step, &a->hz_ctx[2 * i + 1], NULL);
    }
    lbl_dot(a->hz_card, 14, 310, 260, 28, FONT_S, COL_DIM, "Ajustez par pas de 1 bpm.");
    a->hz_close_ctx = (ui_ctx_t){a, 0.0f};
    a->hz_close = button(a->hz_card, 14, 336, 260, 38, COL_ACC, COL_BG, "FERMER", FONT_M, cb_hr_zones_close, &a->hz_close_ctx, NULL);
    hr_zones_mark(a);
}

static void build_banner(ui_app_t *a)
{
    lv_obj_t *top = lv_layer_top();
    a->banner_ctx = (ui_ctx_t){a, 0.0f};
    a->b_panel = box(top, 8, 34, 304, 62, COL_PANEL);
    lv_obj_set_style_radius(a->b_panel, 10, 0);
    lv_obj_set_style_clip_corner(a->b_panel, true, 0);
    lv_obj_set_style_border_width(a->b_panel, 2, 0);
    lv_obj_set_style_border_color(a->b_panel, COL_ORG, 0);
    lv_obj_add_flag(a->b_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(a->b_panel, cb_banner, LV_EVENT_CLICKED, &a->banner_ctx);
    lv_obj_add_flag(a->b_panel, LV_OBJ_FLAG_HIDDEN);
    a->b_strip = box(a->b_panel, 0, 0, 8, 62, COL_ORG);
    lv_obj_remove_flag(a->b_strip, LV_OBJ_FLAG_CLICKABLE);
    a->b_title = lbl_dot(a->b_panel, 18, 8, 236, 22, FONT_N, COL_TEXT, "");
    a->b_sub = lbl_dot(a->b_panel, 18, 34, 272, 18, FONT_S, lv_color_hex(0xC5CBD3), "");
    a->b_more = lbl_r(a->b_panel, 12, 8, FONT_S, COL_DIM, "");
}

void ui_app_set_alerts_cfg(ui_app_t *a, const alerts_cfg_t *c) { alerts_set_cfg(&a->alerts, c); settings_mark(a); }
const alerts_cfg_t *ui_app_get_alerts_cfg(const ui_app_t *a) { return &a->alerts.cfg; }

void ui_app_set_hr_zones(ui_app_t *a, const ui_hr_zones_t *zones)
{
    if (!a || !zones) return;
    if (zones->z2_min_bpm < 60 || zones->z2_min_bpm > 225 ||
        zones->z3_min_bpm < zones->z2_min_bpm + 1 || zones->z3_min_bpm > 230 ||
        zones->z4_min_bpm < zones->z3_min_bpm + 1 || zones->z4_min_bpm > 230 ||
        zones->z5_min_bpm < zones->z4_min_bpm + 1 || zones->z5_min_bpm > 230) return;
    a->hr_zones = *zones;
    hr_zones_mark(a);
    refresh_page(a, 0);
}

const ui_hr_zones_t *ui_app_get_hr_zones(const ui_app_t *a) { return a ? &a->hr_zones : NULL; }

/* ======================= fenetre de fin de montee ======================= */
static void show_summary(ui_app_t *a, int show)
{
    if (show) {
        const ui_summary_t *m = &a->summary;
        if (m->name[0]) settext(a->s_title, "%s", m->name); else settext(a->s_title, "Montée %d", m->number);
        unsigned d = (unsigned)(m->duration_s + 0.5f);
        if (d >= 3600) settext(a->s_val[0], "%u:%02u:%02u", d / 3600, (d / 60) % 60, d % 60); else settext(a->s_val[0], "%u:%02u", d / 60, d % 60);
        settext(a->s_val[1], "%.1f km", (double)(m->dist_m / 1000.0f));
        settext(a->s_val[2], "%.1f %%", (double)m->avg_grade_pct);
        settext(a->s_val[3], "%.0f m", (double)m->ascent_m);
        settext(a->s_val[4], "%.1f km/h", (double)m->avg_speed_kmh);
        if (m->avg_hr >= 0) settext(a->s_val[5], "%d bpm", m->avg_hr); else settext(a->s_val[5], "--");
    }
    sethide(a->s_panel, !show);
    a->summary_visible = show;
}

static void summary_close(ui_app_t *a)
{
    show_summary(a, 0);
    if (a->climb_page_auto && a->cur == 4) ui_app_goto(a, a->before_page, -1);
    a->climb_page_auto = 0;
}

/* ---------- accueil ---------- */
static void home_back(lv_event_t *e)
{
    ui_app_t *a = ((ui_ctx_t *)lv_event_get_user_data(e))->app;
    lv_screen_load(a->home);
}

/* ---- Ecran de reglages (page d'accueil, RÉGLAGES) ---------------------------------------------------------------- */
static void settings_theme_mark(ui_app_t *a)
{
    for (int i = 0; i < 2; i++) {
        int on = a->map_cfg.theme == (i == 1);                 /* bouton 0 : SOMBRE, bouton 1 : CLAIR */
        setbg(a->cfg_theme_btn[i], on ? COL_ACC : COL_PANEL);
        lv_obj_set_style_text_color(a->cfg_theme_lbl[i], on ? COL_BG : COL_TEXT, 0);
    }
}

static void settings_theme_pick(lv_event_t *e)
{
    ui_ctx_t *ctx = (ui_ctx_t *)lv_event_get_user_data(e);
    ui_app_t *a = ctx->app;
    ui_map_cfg_t c = a->map_cfg;
    c.theme = (uint8_t)ctx->v;
    if (c.theme == a->map_cfg.theme) return;
    ui_app_set_map_cfg(a, &c);
    if (ui_hooks.map_cfg_changed) ui_hooks.map_cfg_changed(&a->map_cfg);
}

int ui_app_add_theme_picker(ui_app_t *a, lv_obj_t *parent, int x, int y, int w)
{
    static const char *names[2] = {"SOMBRE", "CLAIR"};
    int bw = (w - 8) / 2;
    lbl(parent, x, y, FONT_S, COL_DIM, "FOND DE CARTE");
    for (int i = 0; i < 2; i++) {
        a->cfg_theme_ctx[i] = (ui_ctx_t){a, i};
        a->cfg_theme_btn[i] = button(parent, x + i * (bw + 8), y + 28, bw, 64, COL_PANEL, COL_TEXT, names[i], FONT_M,
                                     settings_theme_pick, &a->cfg_theme_ctx[i], &a->cfg_theme_lbl[i]);
    }
    settings_theme_mark(a);
    return 28 + 64;
}

#if UI_HOME_SETTINGS_SCREEN
static void build_settings_screen(ui_app_t *a)
{
    lv_obj_t *s = new_screen();
    lbl_c(s, 16, 32, UI_W - 32, FONT_M, COL_ACC, "RÉGLAGES");
    box(s, 16, 74, UI_W - 32, 2, COL_LINE);
    ui_app_add_theme_picker(a, s, 20, 100, UI_W - 40);
    lv_obj_t *msg = lbl_c(s, 20, 212, UI_W - 40, FONT_S, COL_DIM, "Couleur du fond de la carte.\nSans carte chargée : sans effet.");
    lv_obj_set_style_text_line_space(msg, 6, 0);
    lv_obj_t *info = lbl_c(s, 20, 300, UI_W - 40, FONT_S, COL_DIM, "En sortie, les réglages sont\naccessibles par appui long sur\nla barre du haut.");
    lv_obj_set_style_text_line_space(info, 6, 0);
    button(s, 20, 398, UI_W - 40, 56, COL_PANEL, COL_TEXT, "ACCUEIL", FONT_M, home_back, &a->home_ctx[4], NULL);
    a->cfg_screen = s;
}
#endif

static void home_action(lv_event_t *e)
{
    ui_ctx_t *ctx = (ui_ctx_t *)lv_event_get_user_data(e);
    ui_app_t *a = ctx->app;
    int action = (int)ctx->v;
    if (action == 0) {
        ui_app_goto(a, 0, 1);
        return;
    }
    if (action == 1) {
        ui_app_goto(a, 1, 1);
        return;
    }
#if UI_HOME_SETTINGS_SCREEN
    if (action == 4) {
        if (!a->cfg_screen) build_settings_screen(a);
        settings_theme_mark(a);
        lv_screen_load(a->cfg_screen);
        return;
    }
#endif
    lv_obj_t *s = new_screen();
    const char *title = action == 2 ? "HISTORIQUE" : action == 3 ? "CAPTEURS" : "RÉGLAGES";
    const char *message = action == 2 ? "Les sorties enregistrées\nseront affichées ici." :
                          action == 3 ? "État des capteurs\nBluetooth : à connecter\nCardio • cadence • puissance" :
                          "Les réglages disponibles\npendant une sortie sont accessibles\npar appui long sur la barre du haut.";
    lbl_c(s, 16, 32, UI_W - 32, FONT_M, COL_ACC, title);
    box(s, 16, 74, UI_W - 32, 2, COL_LINE);
    lv_obj_t *msg = lbl_c(s, 20, 130, UI_W - 40, FONT_N, COL_TEXT, message);
    lv_obj_set_style_text_line_space(msg, 8, 0);
    ui_ctx_t *back_ctx = &a->home_ctx[4];
    button(s, 20, 398, UI_W - 40, 56, COL_PANEL, COL_TEXT, "ACCUEIL", FONT_M, home_back, back_ctx, NULL);
    lv_screen_load(s);
}

static void build_home(ui_app_t *a)
{
    a->home = new_screen();
    lbl(a->home, 20, 28, FONT_S, COL_ACC, "GPS VÉLO  •  DIY");
    lbl(a->home, 20, 55, FONT_L, COL_TEXT, "Prêt à rouler ?");
    lbl(a->home, 20, 98, FONT_S, COL_DIM, "Navigation et données de sortie");
    button(a->home, 18, 142, UI_W - 36, 62, lv_color_hex(0x1E6B45), COL_TEXT, "▶  DÉMARRER", FONT_M, home_action, &a->home_ctx[0], NULL);
    button(a->home, 18, 216, UI_W - 36, 52, COL_PANEL, COL_TEXT, "PARCOURS GPX", FONT_N, home_action, &a->home_ctx[1], NULL);
    button(a->home, 18, 278, (UI_W - 48) / 2, 52, COL_PANEL, COL_TEXT, "HISTORIQUE", FONT_S, home_action, &a->home_ctx[2], NULL);
    button(a->home, 30 + (UI_W - 48) / 2, 278, (UI_W - 48) / 2, 52, COL_PANEL, COL_TEXT, "CAPTEURS", FONT_S, home_action, &a->home_ctx[3], NULL);
    button(a->home, 18, 340, UI_W - 36, 52, COL_PANEL, COL_TEXT, "RÉGLAGES", FONT_N, home_action, &a->home_ctx[4], NULL);
    lbl_c(a->home, 12, 440, UI_W - 24, FONT_S, COL_DIM, "Prototype • certaines fonctions à venir");
    for (int i = 0; i < 5; i++) a->home_ctx[i] = (ui_ctx_t){a, (float)i};
}

/* ======================= API ======================= */
void ui_app_goto(ui_app_t *a, int page, int dir)
{
    if (page < 0 || page >= UI_PAGES) return;
    lv_screen_load_anim(a->page[page], dir >= 0 ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
    a->cur = page;
}

void ui_app_init(ui_app_t *a, const ui_course_t *course)
{
    memset(a, 0, sizeof(*a));
    a->course = *course;
    a->auto_page = 1;
    a->view_m = 1500.0f;
    a->elev_window = 5000.0f;
    a->ci = -1;
    a->turn_alerted_idx = -1;
    ui_config_default(&a->config);
    ui_map_cfg_default(&a->map_cfg);
    a->map_theme = a->map_cfg.theme;                  /* clair par defaut */
    a->hr_zones = (ui_hr_zones_t){120, 140, 160, 180};
    build_data(a); build_nav(a); build_elev(a); build_list(a); build_climb(a); build_summary(a);
    build_home(a);
    relayout_data(a);
    build_picker(a);
    alerts_cfg_t acfg;
    alerts_cfg_default(&acfg);
    alerts_init(&a->alerts, &acfg);
    build_settings(a);
    build_banner(a);
    a->status_ctx = (ui_ctx_t){a, 0.0f};
    for (int i = 0; i < UI_PAGES; i++) {
        lv_obj_add_flag(a->st[i].bar, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(a->st[i].bar, cb_status_long, LV_EVENT_LONG_PRESSED, &a->status_ctx);
    }
    settings_mark(a);
    for (int i = 0; i < UI_PAGES; i++) {
        a->pg_ctx[i] = (ui_ctx_t){a, (float)i};
        lv_obj_add_event_cb(a->page[i], cb_gesture, LV_EVENT_GESTURE, &a->pg_ctx[i]);
        lv_obj_add_event_cb(a->page[i], cb_load_start, LV_EVENT_SCREEN_LOAD_START, &a->pg_ctx[i]);
        for (int j = 0; j < UI_PAGES; j++)
            lv_obj_add_event_cb(a->st[i].page_hit[j], cb_page_hit, LV_EVENT_CLICKED, &a->pg_ctx[j]);
        lv_obj_add_event_cb(a->st[i].home_hit, cb_home_hit, LV_EVENT_CLICKED, &a->pg_ctx[i]);
    }
    lv_screen_load(a->home);
    a->cur = 0;
}

void ui_app_set_map(ui_app_t *a, mapview_t *mv, uint16_t *canvas_buf)
{
    a->map = canvas_buf ? mv : NULL;
    a->map_buf = canvas_buf;
    a->map_valid = 0;
    if (a->map) lv_canvas_set_buffer(a->n_map, canvas_buf, UI_MAP_W, UI_MAP_H, LV_COLOR_FORMAT_RGB565);
    else lv_obj_add_flag(a->n_map, LV_OBJ_FLAG_HIDDEN);
    sethide(a->n_credit, a->map == NULL);
    map_theme_texts(a);
}

void ui_map_cfg_default(ui_map_cfg_t *c) { c->theme = 1; }

int ui_map_cfg_encode(const ui_map_cfg_t *c, char *out, size_t n)
{
    return snprintf(out, n, "m1;%d", c->theme ? 1 : 0) < (int)n ? 0 : -1;
}

int ui_map_cfg_decode(const char *s, ui_map_cfg_t *c)
{
    if (!s || strlen(s) != 4 || strncmp(s, "m1;", 3) != 0 || (s[3] != '0' && s[3] != '1')) return -1;
    c->theme = (uint8_t)(s[3] - '0');
    return 0;
}

const ui_map_cfg_t *ui_app_get_map_cfg(const ui_app_t *a) { return &a->map_cfg; }

void ui_app_set_map_theme(ui_app_t *a, int theme)
{
    a->map_theme = theme ? 1 : 0;
    a->map_valid = 0;                              /* redessin au prochain rafraichissement de la page trace */
    map_theme_texts(a);
}

void ui_app_set_map_cfg(ui_app_t *a, const ui_map_cfg_t *c)
{
    a->map_cfg.theme = c->theme ? 1 : 0;
    ui_app_set_map_theme(a, a->map_cfg.theme);
    if (a->cfg_theme_btn[0]) settings_theme_mark(a);
}

void ui_app_update(ui_app_t *a, const ui_sample_t *s)
{
    a->last = *s;
    a->have_sample = 1;
    update_turn_guidance(a, s);
    derive(a, s);
    if (s->speed_kmh > a->max_speed) a->max_speed = s->speed_kmh;
    if (s->hr >= 0) { a->hr_total_sum += (float)s->hr; a->hr_total_n++; }
    const ui_course_t *c = &a->course;

    int started = a->in_climb && !a->prev_in_climb;
    int ended = !a->in_climb && a->prev_in_climb;
    if (started) {
        a->climb_t0 = s->time_s;
        a->hr_sum = 0.0f; a->hr_n = 0;
        a->cur_climb = a->ci;
        if (a->auto_page && a->cur != 4) {
            a->before_page = a->cur;
            a->climb_page_auto = 1;
            ui_app_goto(a, 4, 1);
        }
    }
    if (a->in_climb) {
        a->last_in_time = s->time_s;
        if (s->hr >= 0) { a->hr_sum += (float)s->hr; a->hr_n++; }
    }
    if (ended) {
        const climb_t *cl = &c->climbs[a->cur_climb];
        ui_summary_t *m = &a->summary;
        memset(m, 0, sizeof(*m));
        m->number = a->cur_climb + 1;
        snprintf(m->name, sizeof m->name, "%s", cl->name);
        m->duration_s = (float)(a->last_in_time - a->climb_t0);
        m->dist_m = cl->length_m;
        m->avg_grade_pct = cl->avg_grade_pct;
        m->ascent_m = cl->ascent_m;
        m->avg_speed_kmh = m->duration_s > 0.0f ? cl->length_m / m->duration_s * 3.6f : 0.0f;
        m->avg_hr = a->hr_n ? (int)(a->hr_sum / (float)a->hr_n + 0.5f) : -1;
        show_summary(a, 1);
        a->summary_until = s->time_s + 12;
    }
    if (a->summary_visible && s->time_s >= a->summary_until) summary_close(a);
    a->prev_in_climb = a->in_climb;

    /* alertes */
    alerts_input_t ai;
    memset(&ai, 0, sizeof ai);
    ai.time_s = s->time_s; ai.gps_ok = s->gps_ok; ai.on_route = s->on_route; ai.off_m = s->off_m;
    ai.hr = s->hr; ai.battery_pct = s->battery_pct; ai.in_climb = a->in_climb;
    ai.next_climb_id = (a->ci >= 0 && !a->in_climb) ? a->ci : -1;
    if (a->ci >= 0) {
        const climb_t *nc = &c->climbs[a->ci];
        ai.next_climb_dist_m = nc->start_m - s->pos_m;
        ai.next_climb_name = nc->name; ai.next_climb_len_m = nc->length_m; ai.next_climb_grade_pct = nc->avg_grade_pct;
    }
    int raised = alerts_update(&a->alerts, &ai);
    if (raised > 0 && ui_hooks.alert && alerts_current(&a->alerts)) ui_hooks.alert(alerts_current(&a->alerts));
    update_banner(a);
    refresh_turn_card(a, s);
    refresh_page(a, a->cur);
}

void ui_app_jump(ui_app_t *a)
{
    a->prev_in_climb = 0;
    a->in_climb = 0;
    a->climb_page_auto = 0;
    show_summary(a, 0);
    alerts_cfg_t keep = a->alerts.cfg;
    alerts_init(&a->alerts, &keep);
    update_banner(a);
}

static int count_obj(lv_obj_t *o)
{
    int n = 1;
    uint32_t k = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < k; i++) n += count_obj(lv_obj_get_child(o, (int32_t)i));
    return n;
}

int ui_app_object_count(const ui_app_t *a)
{
    int n = count_obj(lv_layer_top());
    for (int i = 0; i < UI_PAGES; i++) n += count_obj(a->page[i]);
    return n;
}
