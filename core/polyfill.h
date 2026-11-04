#ifndef POLYFILL_H
#define POLYFILL_H

/* Remplissage de surfaces (plusieurs anneaux, regle pair-impair) dans un tampon RGB565, sans LVGL.
 * Un pixel est rempli quand son centre (x + 0,5 ; y + 0,5) est a l'interieur. Les parties hors du tampon sont ignorees. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POLYFILL_MAX_EDGES 4096   /* = GVT_MAX_AREA_POINTS ; environ 88 Ko de memoire de travail statique (a mettre en PSRAM) */

/* xy : points (x, y) en pixels, flottants, les anneaux les uns apres les autres ; ring_len[i] : points de l'anneau i
 * (chaque anneau est ferme implicitement). stride_px : pixels entre deux lignes du tampon.
 * Retourne 0, ou -1 si la surface a trop d'aretes (rien n'est dessine). */
int polyfill_rgb565(uint16_t *buf, int w, int h, int stride_px, const float *xy, const uint16_t *ring_len, int n_rings, uint16_t color);

#ifdef __cplusplus
}
#endif
#endif
