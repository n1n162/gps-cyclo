#ifndef CLIMB_H
#define CLIMB_H

/* Detection de montees type ClimbPro, en C simple (sans allocation).
 * Le profil est un tableau d'altitudes a pas de distance constant (step_m).
 * Deux niveaux de lissage :
 *   - detection des montees : pente sur ~300 m (slope_half), stable
 *   - affichage : pente sur ~100 m (display_window_m), fait ressortir les passages raides */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float step_m;          /* pas du profil (m) */
    int   smooth_half;     /* lissage altitude, demi-fenetre en points (2 = 5 points) */
    int   slope_half;      /* detection : demi-fenetre de pente en points (15 = 300 m a 10 m de pas) */
    float lp_alpha;        /* detection : coefficient du filtre passe-bas de la pente */
    float start_pct;       /* debut de montee quand la pente depasse ce seuil (%) */
    float end_pct;         /* fin de montee quand la pente retombe a ce seuil (%) */
    float gap_max_m;       /* fusion : ecart maximal entre deux montees (m) */
    float drop_max_m;      /* fusion : descente maximale autorisee entre deux montees (m) */
    float backoff_max_m;   /* depart : recul maximal vers le point bas precedent (m) */
    float backoff_tol_m;   /* depart : bosse tolerée pendant le recul (m) */
    float min_len_m;       /* longueur minimale d'une montee retenue (m) */
    float min_grade_pct;   /* pente moyenne minimale d'une montee retenue (%) */
    float display_window_m;/* affichage : fenetre de la pente actuelle (m) */
    float section_m;       /* affichage : longueur des barres colorees du profil (m) */
} climb_cfg_t;

typedef struct {
    int   start_idx, end_idx;   /* indices dans le profil */
    float start_m, end_m;       /* distance depuis le debut du parcours (m) */
    float length_m;
    float avg_grade_pct;
    float ascent_m;
    float score;                /* longueur (m) x pente moyenne (%) */
    char  name[40];             /* nom (UTF-8), vide si inconnu */
} climb_t;

typedef struct {
    float dist_done_m;
    float dist_remaining_m;
    float ascent_remaining_m;
    float avg_grade_remaining_pct;
} climb_progress_t;

/* Une barre du profil de montee (ex. 100 m) avec sa couleur. */
typedef struct {
    float start_m, end_m;
    float grade_pct;
    uint8_t level;   /* 0 plat/descente, 1 (<3 %), 2 (<6 %), 3 (<9 %), 4 (<12 %), 5 (>=12 %) */
} climb_section_t;

void climb_cfg_default(climb_cfg_t *cfg);

int  route_resample(const float *dist_m, const float *ele_m, int n,
                    float step_m, float *out, int max_out);
void profile_smooth(float *ele, int n, int half, float *work);

/* Detecte les montees ('work' : tampon de n floats). Retourne le nombre de montees dans out. */
int  climb_detect(const float *ele, int n, float *work, const climb_cfg_t *cfg,
                  climb_t *out, int max_out);

/* Indice de la montee en cours ou de la prochaine devant soi, -1 s'il n'y en a plus. */
int  climb_find(const climb_t *list, int count, float pos_m);

/* Valeurs restantes pour l'ecran de montee. Retourne 1 si pos_m est dans la montee. */
int  climb_progress(const climb_t *c, const float *ele, int n, float step_m,
                    float pos_m, climb_progress_t *out);

/* Pente actuelle (%) sur la fenetre d'affichage (~100 m) centree sur pos_m. */
float climb_grade_at(const float *ele, int n, const climb_cfg_t *cfg, float pos_m);

/* Decoupe une montee en barres colorees (profil affiche). Retourne le nombre de barres. */
int  climb_sections(const climb_t *c, const float *ele, int n, const climb_cfg_t *cfg,
                    climb_section_t *out, int max_out);

/* Categorie de la montee selon son score (bareme Garmin ClimbPro, documentation du Edge 1050) :
 * 0 = HC (>80000), 1..4 = categories 1 a 4 (>64000, >32000, >16000, >8000), 5 = non categorisee. */
int  climb_category(float score);

/* Pente moyenne (%) sur les dist_m metres devant pos_m. */
float climb_grade_ahead(const float *ele, int n, float step_m, float pos_m, float dist_m);

/* Barres de profil quelconques (profil a venir) : de from_m a to_m, par barres de section_m. */
int  profile_sections(const float *ele, int n, float step_m, float from_m, float to_m,
                      float section_m, climb_section_t *out, int max_out);

#ifdef __cplusplus
}
#endif
#endif
