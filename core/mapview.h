#ifndef MAPVIEW_H
#define MAPVIEW_H

/* Vue d'une carte GVT1 : choix du niveau de zoom, tuiles a lire, passage tuile -> pixels, cache de tuiles.
 * Aucune dependance a LVGL : testable sur ordinateur. Le dessin se fait dans ui/app.c.
 *
 * La vue utilise la meme projection locale que la trace (route_t : lat0, lon0, cos0, x vers l'est, y vers le nord,
 * en metres), pour que les routes de la carte tombent exactement sous la trace. */

#include "gvt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAPVIEW_SLOTS       16
#define MAPVIEW_SLOT_BYTES  65536u
#define MAPVIEW_MAX_TILES   16
#define MAPVIEW_BIG_SLOTS_MAX 4
#define MAPVIEW_EARTH_R     6371008.8   /* meme valeur que core/route.c */

typedef struct { uint8_t used, z; uint32_t x, y, size, stamp; } mapview_slot_t;

typedef struct {
    gvt_t *g;
    uint8_t *mem;                       /* MAPVIEW_SLOTS x MAPVIEW_SLOT_BYTES octets fournis par l'appelant */
    mapview_slot_t slot[MAPVIEW_SLOTS];
    uint8_t *big_mem;                   /* cache optionnel des grosses tuiles, fourni par l'appelant */
    uint32_t big_slot_bytes;
    uint8_t big_slots;
    mapview_slot_t big_slot[MAPVIEW_BIG_SLOTS_MAX];
    uint32_t clock;
    uint32_t loads, hits, errors, too_big;   /* statistiques globales */
    uint32_t big_loads, big_hits;            /* statistiques du cache grandes tuiles */
} mapview_t;

typedef struct {
    double lat0, lon0, cos0;            /* projection locale de la trace */
    float cx_m, cy_m;                   /* point de la trace au centre de la zone, dans cette projection (m) */
    float scale;                        /* pixels par metre */
    float cs, sn;                       /* cap : cos et sin (1 et 0 = nord en haut) */
    int cx, cy;                         /* pixel du centre dans la zone */
    int w, h;                           /* taille de la zone en pixels */
} mapview_view_t;

typedef struct { uint32_t x, y; } mapview_tile_t;

void mapview_init(mapview_t *mv, gvt_t *g, uint8_t *mem);

/* Configure un second cache LRU pour les tuiles plus grandes que 64 Ko.
 * mem doit fournir slots * slot_bytes octets ; slots <= MAPVIEW_BIG_SLOTS_MAX.
 * Passer slots=0 pour le désactiver. Retourne 0 ou -1 si la configuration est invalide. */
int mapview_set_big_cache(mapview_t *mv, uint8_t *mem, uint32_t slot_bytes, uint8_t slots);

/* Niveau de tuile dont la resolution est la plus proche de celle de l'ecran, borne par les niveaux du fichier. */
int mapview_zoom(const gvt_t *g, const mapview_view_t *v);

/* Tuiles qui couvrent la zone (rotation comprise). Retourne le nombre ecrit dans out (au plus max). */
int mapview_tiles(const mapview_view_t *v, int z, mapview_tile_t *out, int max);

/* Transformation d'une tuile en pixels : sx = m[0]*px + m[1]*py + m[2] ; sy = m[3]*px + m[4]*py + m[5],
 * avec (px, py) en unites de tuile (0 a 4096). */
void mapview_xform(const mapview_view_t *v, int z, uint32_t tx, uint32_t ty, float m[6]);

/* Contenu d'une tuile (lue sur la carte SD si elle n'est pas en cache). NULL si absente,
 * trop grande pour les deux caches configurés, ou si la lecture échoue. */
const uint8_t *mapview_get(mapview_t *mv, int z, uint32_t x, uint32_t y, uint32_t *size);

#ifdef __cplusplus
}
#endif
#endif
