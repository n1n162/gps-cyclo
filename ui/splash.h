#ifndef UI_SPLASH_H
#define UI_SPLASH_H
/* Ecran de demarrage du GPS velo (LVGL 9, portrait 320 x 480).
 * Image : docs/ecran acceuil.jpg, convertie par tools/splash_to_c.py en ui/splash_img.c (tableau RGB565 en memoire flash).
 * L'ecran s'affiche avec une barre de progression, puis laisse la place a l'ecran suivant (fondu). Un toucher le passe.
 * Pendant l'affichage, l'application ne doit pas encore appeler ui_app_update() : voir docs/ecran_demarrage.md.
 * Compiler avec -DUI_SPLASH_NO_IMAGE pour un ecran sans image (fond uni) quand ui/splash_img.c n'a pas pu etre genere. */

#include "lvgl.h"

#define UI_SPLASH_DEFAULT_MS 2500       /* duree d'affichage par defaut */

typedef struct ui_splash {
    lv_obj_t *screen, *bar_fill, *status;
    lv_obj_t *next;                     /* ecran charge a la fin (NULL : aucun) */
    lv_timer_t *timer;
    uint32_t t0_ms, duration_ms;
    int active;                         /* 1 entre ui_splash_show() et la fin */
} ui_splash_t;

/* Charge l'ecran de demarrage. next : ecran a afficher ensuite (par exemple app.home). duration_ms : 0 = duree par defaut. */
void ui_splash_show(ui_splash_t *s, lv_obj_t *next, uint32_t duration_ms);
/* Passe tout de suite a l'ecran suivant (appele aussi par un toucher). Sans effet si l'ecran est deja termine. */
void ui_splash_skip(ui_splash_t *s);
/* 1 si l'ecran de demarrage n'est pas (ou plus) affiche. Une structure remplie de zeros compte comme terminee. */
int  ui_splash_done(const ui_splash_t *s);
/* Change la ligne d'etat sous la barre (par exemple "Recherche du signal GPS..."). */
void ui_splash_set_status(ui_splash_t *s, const char *text);

#endif
