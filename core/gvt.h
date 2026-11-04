#ifndef GVT_H
#define GVT_H

/* Lecteur de cartes vectorielles en tuiles GVT1 (format : docs/carte.md).
 * Le fichier est lu par une fonction fournie par l'appelant (fopen sur ordinateur, carte SD sur l'appareil).
 * L'index (16 octets par tuile) est charge une fois dans un tampon fourni par l'appelant (PSRAM sur la carte). */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GVT_VERSION     2       /* version du format ecrite par le convertisseur ; la version 1 (lignes seules) reste lisible */
#define GVT_EXTENT      4096    /* unites par tuile */
#define GVT_BUFFER      64      /* marge de decoupe autour de la tuile, en unites */
#define GVT_MAX_POINTS  1024    /* points maximum par ligne */
#define GVT_MAX_AREA_POINTS 4096 /* points maximum d'une surface dans une tuile (tous anneaux) */
#define GVT_MAX_RINGS   64      /* anneaux maximum d'une surface */
#define GVT_HEADER_SIZE 36u
#define GVT_ENTRY_SIZE  16u

enum {
    GVT_ROAD_MAJOR = 1, GVT_ROAD_SECONDARY, GVT_ROAD_LOCAL, GVT_STREET, GVT_CYCLEWAY,
    GVT_TRACK, GVT_RIVER, GVT_LAKE, GVT_RAIL,
    GVT_AREA_URBAN = 10, GVT_AREA_FOREST, GVT_AREA_FIELD, GVT_AREA_PARK, GVT_AREA_ROCK, GVT_AREA_WATER,
    GVT_CLASS_COUNT
};

/* Lit len octets a partir du decalage offset. Retourne 0 si tout est lu. */
typedef int (*gvt_read_fn)(void *ctx, uint32_t offset, void *dst, uint32_t len);

typedef struct { uint64_t key; uint32_t offset, size; } gvt_entry_t;

typedef struct {
    gvt_read_fn read;
    void *ctx;
    uint16_t extent, version;       /* version 1 : lignes ; version 2 : lignes et surfaces */
    uint8_t zmin, zmax;
    uint32_t n_tiles, index_offset;
    int32_t west_e7, south_e7, east_e7, north_e7;
    gvt_entry_t *index;
} gvt_t;

#define GVT_KEY(z, x, y) (((uint64_t)(z) << 32) | ((uint64_t)(x) << 16) | (uint64_t)(y))

/* Lit l'en-tete. Retourne 0 si le fichier est un GVT1 valide, sinon -1. */
int gvt_open(gvt_t *g, gvt_read_fn read, void *ctx);
/* Charge l'index dans buf (au moins g->n_tiles entrees). Retourne 0, ou -1 si invalide ou trop petit. */
int gvt_load_index(gvt_t *g, gvt_entry_t *buf, uint32_t capacity);
/* Recherche dichotomique. Retourne NULL si la tuile n'existe pas. */
const gvt_entry_t *gvt_find(const gvt_t *g, int z, uint32_t x, uint32_t y);
/* Lit le contenu d'une tuile. Retourne le nombre d'octets lus, ou -1. */
int gvt_read_tile(const gvt_t *g, const gvt_entry_t *e, uint8_t *buf, uint32_t capacity);

/* Tuile contenant un point, et position du point dans la tuile (0 a 4096). */
void gvt_tile_of(int z, double lat, double lon, uint32_t *x, uint32_t *y, float *fx, float *fy);

typedef struct { const uint8_t *p, *end; uint32_t remaining; } gvt_tile_it_t;

/* Demarre la lecture d'une tuile deja chargee en memoire. Retourne 0, ou -1 si corrompue. */
int gvt_tile_begin(gvt_tile_it_t *it, const uint8_t *blob, uint32_t size);
/* Ligne suivante (les surfaces sont sautees) : points x,y alternes ecrits dans xy (capacity points au plus).
 * Retourne 1 (ligne lue), 0 (fin), -1 (tuile corrompue), -2 (tampon trop petit, ligne non consommee). */
int gvt_tile_next(gvt_tile_it_t *it, uint8_t *cls, uint16_t *n_points, int16_t *xy, uint16_t capacity);

/* Surface suivante (les lignes sont sautees) : n_rings anneaux, ring_len[i] points pour l'anneau i (au moins GVT_MAX_RINGS
 * cases), tous les points x,y alternes les uns apres les autres dans xy (capacity points au plus, GVT_MAX_AREA_POINTS suffit).
 * Remplissage pair-impair : un anneau a l'interieur d'un autre est un trou. Memes codes de retour que gvt_tile_next. */
int gvt_tile_next_area(gvt_tile_it_t *it, uint8_t *cls, uint16_t *n_rings, uint16_t *ring_len, int16_t *xy, uint16_t capacity);

#ifdef __cplusplus
}
#endif
#endif
