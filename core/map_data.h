#ifndef MAP_DATA_H
#define MAP_DATA_H

/* Lecteur en flux du format cartographique GVM1. Ne charge pas la carte entiere en RAM. */
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAP_GVM_VERSION 1
#define MAP_FEATURE_MAX_POINTS 512

typedef enum {
    MAP_FEATURE_ROAD = 1,
    MAP_FEATURE_CYCLE = 2,
    MAP_FEATURE_PATH = 3,
    MAP_FEATURE_WATER = 4,
    MAP_FEATURE_RAIL = 5,
    MAP_FEATURE_GREEN = 6,
    MAP_FEATURE_BUILDING = 7,
    MAP_FEATURE_POI = 8
} map_feature_type_t;

typedef struct { int32_t lat_e7, lon_e7; } map_point_t;
typedef struct {
    uint8_t type;
    uint16_t n_points;
    int32_t min_lat_e7, max_lat_e7, min_lon_e7, max_lon_e7;
} map_feature_t;
typedef struct {
    FILE *file;
    uint32_t feature_count, index;
    int valid;
} map_reader_t;
typedef struct {
    int32_t min_lat_e7, max_lat_e7, min_lon_e7, max_lon_e7;
} map_bounds_t;

/* Retourne 0 si le fichier GVM1 est valide, sinon -1. */
int map_reader_open(map_reader_t *r, const char *path);
void map_reader_close(map_reader_t *r);
/* Lit la prochaine géométrie qui intersecte la zone. Retourne 1 trouvé, 0 fin, -1 erreur.
   Les objets plus longs que capacity sont ignorés sans allocation. */
int map_reader_next_visible(map_reader_t *r, const map_bounds_t *bounds,
                            map_feature_t *feature, map_point_t *points, uint16_t capacity);

#ifdef __cplusplus
}
#endif
#endif
