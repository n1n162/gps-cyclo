#ifndef LINEDRAW_H
#define LINEDRAW_H

/* Segments epais lisses (extremites arrondies) dans un tampon RGB565, sans LVGL.
 * Couverture d'un pixel = clamp(largeur/2 + 0,5 - distance du centre du pixel au segment, 0, 1), melangee sur 5 bits. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void linedraw_rgb565(uint16_t *buf, int w, int h, int stride_px, float x0, float y0, float x1, float y1, float width, uint16_t color);

#ifdef __cplusplus
}
#endif
#endif
