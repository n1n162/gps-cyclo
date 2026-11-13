/* ble_decode : decodeurs des mesures BLE standard (pur C, sans dependance BLE).
 * Entree : octets bruts d'une notification GATT. Sortie : valeurs physiques.
 * Les formats suivent les caracteristiques GATT Heart Rate Measurement (0x2A37),
 * CSC Measurement (0x2A5B) et Cycling Power Measurement (0x2A63). */
#ifndef BLE_DECODE_H
#define BLE_DECODE_H

#include <stddef.h>
#include <stdint.h>

#define BLE_UUID_HR_SERVICE  0x180D
#define BLE_UUID_HR_MEAS     0x2A37
#define BLE_UUID_CSC_SERVICE 0x1816
#define BLE_UUID_CSC_MEAS    0x2A5B
#define BLE_UUID_CP_SERVICE  0x1818
#define BLE_UUID_CP_MEAS     0x2A63

#define BLE_CSC_TICKS_PER_S      1024u /* temps CSC et crank CP */
#define BLE_CP_WHEEL_TICKS_PER_S 2048u /* temps roue du Cycling Power */

#define BLE_MAX_CADENCE_RPM 250.0f
#define BLE_MAX_SPEED_KMH   120.0f

/* ---- Frequence cardiaque ---- */
#define BLE_HR_MAX_RR 9
typedef struct {
    int bpm;
    int contact;   /* -1 non supporte, 0 non detecte, 1 detecte */
    int has_energy;
    int energy_kj;
    int n_rr;
    int rr_ms[BLE_HR_MAX_RR];
} ble_hr_t;

/* 0 si ok, -1 si trame invalide ou tronquee */
int ble_hr_parse(const uint8_t *d, size_t len, ble_hr_t *out);

/* ---- Vitesse / cadence (CSC) ---- */
typedef struct {
    int has_wheel;
    uint32_t wheel_revs;
    uint16_t wheel_time;   /* 1/1024 s */
    int has_crank;
    uint16_t crank_revs;
    uint16_t crank_time;   /* 1/1024 s */
} ble_csc_t;

int ble_csc_parse(const uint8_t *d, size_t len, ble_csc_t *out);

/* ---- Puissance (Cycling Power) ---- */
typedef struct {
    int power_w;
    int has_balance;
    int balance_half_pct;  /* unite 1/2 % */
    int has_wheel;
    uint32_t wheel_revs;
    uint16_t wheel_time;   /* 1/2048 s */
    int has_crank;
    uint16_t crank_revs;
    uint16_t crank_time;   /* 1/1024 s */
} ble_cp_t;

int ble_cp_parse(const uint8_t *d, size_t len, ble_cp_t *out);

/* ---- Calculs a partir de deux mesures successives ----
 * Retour : 1 = nouvelle valeur calculee, 0 = pas de nouvel evenement
 * (capteur qui repete la derniere mesure, ou premiere mesure), -1 = valeur
 * invalide ignoree. Sans evenement, l'appelant garde la derniere valeur et
 * la remet a zero apres un delai sans evenement (de l'ordre de 3 s). */
typedef struct { int init; uint16_t revs; uint16_t time; } ble_crank_t;
typedef struct { int init; uint32_t revs; uint16_t time; } ble_wheel_t;

int ble_crank_cadence(ble_crank_t *s, uint16_t revs, uint16_t time, float *rpm);

/* dist_m recoit la distance parcourue depuis la mesure precedente */
int ble_wheel_speed(ble_wheel_t *s, uint32_t revs, uint16_t time,
                    unsigned ticks_per_s, float circ_m,
                    float *kmh, float *dist_m);

#endif
