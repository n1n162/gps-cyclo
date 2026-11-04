#ifndef FIT_H
#define FIT_H

/* Enregistrement d'une sortie au format FIT (activite velo), en C simple, sans allocation.
 * Messages ecrits : file_id, event (debut, pause, reprise, fin), record (1 par seconde),
 * lap, session, activity. Numeros de messages et de champs verifies sur le SDK Garmin. */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int (*write)(void *ctx, const void *buf, size_t n);                   /* ajoute a la fin du fichier */
    int (*write_at)(void *ctx, uint32_t off, const void *buf, size_t n);  /* ecrit a une position (en-tete) */
    int (*flush)(void *ctx);                                              /* facultatif (NULL possible) */
    void *ctx;
} fit_io_t;                                                               /* toutes les fonctions : 0 = succes */

typedef struct {
    uint32_t time_unix;
    int      has_fix;               /* 1 si lat/lon valides */
    double   lat_deg, lon_deg;
    int      has_alt;
    float    alt_m;
    float    dist_m;                /* distance cumulee depuis le depart */
    float    speed_mps;
    int      hr, cad, power;        /* -1 = absent */
} fit_sample_t;

typedef struct {
    uint32_t start_unix;
    int      started;
    int32_t  start_lat, start_lon, end_lat, end_lon;
    int      has_start_pos, has_end_pos;
    float    start_dist, end_dist, max_speed;
    uint32_t hr_sum, hr_n, hr_max, cad_sum, cad_n, cad_max, pw_sum, pw_n, pw_max;
    float    ascent, descent, ref_alt;
    int      has_ref;
    uint32_t paused_s;
} fit_acc_t;

typedef struct {
    fit_io_t io;
    uint8_t  buf[256];
    size_t   used;
    uint32_t data_size;
    uint16_t crc0;                  /* CRC des seules donnees, depart a zero */
    int32_t  tz_offset_s;
    int      lap_def, ses_def, act_def;
    int      have_sample, paused, laps;
    uint32_t last_unix, pause_start, begin_unix;
    float    last_dist;
    int32_t  last_lat, last_lon;
    int      has_last_pos;
    int      err;
    fit_acc_t ses, lap;
} fit_writer_t;

int fit_begin(fit_writer_t *w, const fit_io_t *io, uint32_t start_unix, int32_t tz_offset_s);
int fit_add_sample(fit_writer_t *w, const fit_sample_t *s);
int fit_pause(fit_writer_t *w, uint32_t t_unix);
int fit_resume(fit_writer_t *w, uint32_t t_unix);
int fit_lap(fit_writer_t *w, uint32_t t_unix);
int fit_flush(fit_writer_t *w);      /* a appeler toutes les ~10 s : pousse le tampon vers la carte */
int fit_end(fit_writer_t *w);        /* ecrit lap, session, activity, puis finalise en-tete et CRC */

/* Recuperation d'un fichier interrompu (en-tete non finalise, derniere ecriture tronquee).
 * Relit les messages complets du fichier source et ecrit un fichier neuf, complet et valide. */
typedef struct {
    int (*read_at)(void *ctx, uint32_t off, void *buf, size_t n);
    void *ctx;
    uint32_t size;
} fit_src_t;

/* Retourne le nombre de points recuperes, ou une valeur negative en cas d'erreur. */
int fit_recover(const fit_src_t *src, const fit_io_t *dst, int32_t tz_offset_s, fit_writer_t *w);

#ifdef __cplusplus
}
#endif
#endif
