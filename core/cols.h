#ifndef COLS_H
#define COLS_H

/* Base locale de cols et de lieux, pour nommer les montees (sans reseau).
 * Fichier CSV UTF-8, une ligne par entree :  type;nom;lat;lon;alt
 * Les autres types ecrits par tools/convertir_cols.py (eau, boulangerie, commerce, toilette, velociste) sont reconnus et ignores.
 * Colonnes supplementaires (identifiant OpenStreetMap par exemple) : acceptees en fin de ligne, ou devant le type, ou n'importe ou
 * si le fichier a une ligne d'en-tete qui nomme les colonnes (type, nom ou name, lat ou latitude, lon ou longitude, alt ou ele).
 *   type = col ou lieu, lat/lon en degres decimaux (point), alt en metres (peut etre vide). */

#include <stdint.h>
#include "route.h"
#include "climb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define COLS_NO_ELE INT16_MIN
#define COLS_KIND_COL 0
#define COLS_KIND_LIEU 1

/* Quel lieu nomme une montee qui n'est pas un col */
#define COLS_PLACE_SUMMIT 0   /* lieu au sommet : "Montee du Collet" (par defaut) */
#define COLS_PLACE_START  1   /* lieu au depart : "Montee du Revel" */
#define COLS_PLACE_BOTH   2   /* depart et sommet : "Le Revel > Le Collet" */

typedef struct {
    float lat, lon;
    int16_t ele;
    uint8_t kind;
    uint32_t name_off;
} cols_entry_t;

typedef struct {
    cols_entry_t *e;  int max_e;  int n;
    char *pool;  uint32_t pool_size, pool_used;
    int truncated;
    int filter;                       /* 1 = ne garder que les entrees dans le cadre ci-dessous */
    float lat_min, lat_max, lon_min, lon_max;
    int skipped;                      /* entrees ecartees par le filtre */
    int rejected;                     /* lignes de donnees non reconnues (type inconnu, coordonnees illisibles) */
    int other;                        /* points d'interet d'autres types (eau, boulangerie...), reconnus mais non utilises */
    int idx_type, idx_name, idx_lat, idx_lon, idx_alt, has_header;   /* colonnes, d'apres l'en-tete s'il y en a un */
} cols_db_t;

typedef struct {
    float radius_m;        /* col : distance maximale au sommet (300 m) */
    float ele_tol_m;       /* col : ecart d'altitude maximal (40 m, les sources different de 15 a 20 m) */
    float ele_weight;      /* poids de l'ecart d'altitude dans le choix du meilleur candidat */
    float place_radius_m;  /* lieu : distance maximale au point de reference de la montee (1500 m) */
    int   place_mode;      /* COLS_PLACE_SUMMIT, COLS_PLACE_START ou COLS_PLACE_BOTH */
} cols_cfg_t;

void cols_cfg_default(cols_cfg_t *cfg);
void cols_init(cols_db_t *db, cols_entry_t *entries, int max_entries, char *pool, uint32_t pool_size);
/* Limite la base aux entrees situees a moins de margin_m du parcours (a appeler avant cols_load). */
void cols_filter_route(cols_db_t *db, const route_t *r, float margin_m);
int  cols_load(gpx_read_fn rd, void *ctx, cols_db_t *db);          /* retourne le nombre d'entrees lues */
const char *cols_name(const cols_db_t *db, int index);

/* Indice du meilleur col pour un sommet, ou -1. */
int cols_match_summit(const cols_db_t *db, const cols_cfg_t *cfg, double lat, double lon, float ele_m, float *dist_m);
/* Indice du lieu le plus proche, ou -1. */
int cols_match_place(const cols_db_t *db, const cols_cfg_t *cfg, double lat, double lon, float *dist_m);

/* "Le Revel" -> "Montee du Revel", "Arbin" -> "Montee d'Arbin", etc. */
void cols_montee_de(const char *place, char *out, size_t out_size);

/* Donne un nom a chaque montee : col au sommet, sinon "Montee de <lieu>" selon place_mode, sinon laisse vide.
 * Retourne le nombre de montees nommees. */
int cols_name_climbs(const cols_db_t *db, const cols_cfg_t *cfg, const route_t *r, const float *ele,
                     climb_t *climbs, int n);

#ifdef __cplusplus
}
#endif
#endif
