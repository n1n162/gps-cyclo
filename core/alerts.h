#ifndef ALERTS_H
#define ALERTS_H

/* Moteur d'alertes du GPS velo, sans affichage : on lui donne une mesure par seconde, il dit quelle alerte afficher.
 * Chaque alerte a un delai avant de se declencher (pas de fausse alerte), une hysteresis pour s'effacer,
 * un acquittement (toucher la banniere) et, pour certaines, un rappel tant que le probleme dure. */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ALERT_OFF_ROUTE = 0,     /* hors parcours */
    ALERT_BATTERY,           /* batterie faible, puis critique */
    ALERT_HR_HIGH,           /* frequence cardiaque au-dessus de la limite */
    ALERT_GPS_LOST,          /* signal GPS perdu */
    ALERT_HR_SENSOR,         /* ceinture cardio perdue */
    ALERT_CLIMB_SOON,        /* une montee commence bientot */
    ALERT_KINDS
} alert_kind_t;

typedef enum { ALERT_INFO = 0, ALERT_WARN = 1, ALERT_CRIT = 2 } alert_level_t;

typedef struct {
    uint8_t enabled[ALERT_KINDS];
    int hr_max_bpm;                       /* limite de frequence cardiaque */
    int battery_warn_pct, battery_crit_pct;
    int off_route_s, gps_lost_s, hr_high_s, hr_sensor_s;   /* delais avant declenchement */
    int repeat_s;                         /* rappel d'une alerte ignoree tant que le probleme dure (0 = jamais) */
    float climb_soon_m;                   /* distance d'annonce d'une montee */
    int info_timeout_s, warn_timeout_s;   /* disparition automatique (les alertes critiques restent) */
} alerts_cfg_t;

typedef struct {
    uint32_t time_s;
    int gps_ok;
    int on_route; float off_m;            /* off_m < 0 : inconnu */
    int hr;                               /* -1 : pas de mesure */
    int battery_pct;
    int in_climb;
    int next_climb_id; float next_climb_dist_m;     /* id < 0 : plus de montee */
    const char *next_climb_name; float next_climb_len_m, next_climb_grade_pct;
} alerts_input_t;

typedef struct {
    alert_kind_t kind;
    alert_level_t level;
    char title[40], sub[64];
    uint32_t raised_s;
} alert_t;

typedef struct {
    uint8_t active, acked, cond, clearing;
    uint32_t cond_since, clear_since, raised_s;
    alert_level_t level;
    char title[40], sub[64];
} alert_state_t;

typedef struct {
    alerts_cfg_t cfg;
    alert_state_t st[ALERT_KINDS];
    int batt_level, hr_seen, hr_ok_run, last_soon_id, raised_now;
    uint32_t hr_ok_since;
    alert_t cur;
    int has_cur, pending;
} alerts_t;

void alerts_cfg_default(alerts_cfg_t *cfg);
void alerts_init(alerts_t *a, const alerts_cfg_t *cfg);
void alerts_set_cfg(alerts_t *a, const alerts_cfg_t *cfg);
int  alerts_update(alerts_t *a, const alerts_input_t *in);   /* retourne le nombre d'alertes nouvelles ou rappelees */
const alert_t *alerts_current(const alerts_t *a);            /* l'alerte a afficher, ou NULL */
int  alerts_pending(const alerts_t *a);                      /* autres alertes en attente */
void alerts_dismiss(alerts_t *a);                            /* acquitte l'alerte affichee */

/* Reglages enregistrables : "a1;111111;180;20;10" */
int alerts_cfg_encode(const alerts_cfg_t *c, char *out, size_t n);
int alerts_cfg_decode(const char *s, alerts_cfg_t *c);       /* 0 = valide, <0 = refuse (c inchange) */

#ifdef __cplusplus
}
#endif
#endif
