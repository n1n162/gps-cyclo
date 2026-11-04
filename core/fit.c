#include "fit.h"
#include <math.h>
#include <string.h>

#define FIT_EPOCH_UNIX 631065600u

/* ---------- CRC (algorithme du SDK Garmin) ---------- */
static const uint16_t crc_tab[16] = {
    0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
    0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400};

static uint16_t crc_byte(uint16_t crc, uint8_t b)
{
    uint16_t t = crc_tab[crc & 0xF];
    crc = (crc >> 4) & 0x0FFF;
    crc = crc ^ t ^ crc_tab[b & 0xF];
    t = crc_tab[crc & 0xF];
    crc = (crc >> 4) & 0x0FFF;
    crc = crc ^ t ^ crc_tab[(b >> 4) & 0xF];
    return crc;
}

/* Le CRC est lineaire : CRC(en-tete + donnees) = (en-tete suivi de n zeros) XOR (CRC des donnees
 * seules, depart a zero). Cela evite de relire le fichier pour calculer le CRC final. */
static uint16_t crc_zeros(uint16_t crc, uint32_t n)
{
    while (n--) crc = crc_byte(crc, 0);
    return crc;
}

/* ---------- ecriture bas niveau ---------- */
static void p16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void p32(uint8_t *p, uint32_t v) { p16(p, (uint16_t)v); p16(p + 2, (uint16_t)(v >> 16)); }

static int flush_buf(fit_writer_t *w)
{
    if (w->used == 0) return 0;
    int r = w->io.write(w->io.ctx, w->buf, w->used);
    w->used = 0;
    if (r) w->err = r;
    return r;
}

static int emit(fit_writer_t *w, const uint8_t *d, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (w->used == sizeof(w->buf) && flush_buf(w)) return w->err;
        w->buf[w->used++] = d[i];
        w->crc0 = crc_byte(w->crc0, d[i]);
    }
    w->data_size += (uint32_t)n;
    return w->err;
}

/* Message de definition : fields = triplets {numero, taille, type de base} */
static int def_msg(fit_writer_t *w, uint8_t local, uint16_t global, const uint8_t *fields, uint8_t nf)
{
    uint8_t h[6];
    h[0] = (uint8_t)(0x40 | local);
    h[1] = 0;            /* reserve */
    h[2] = 0;            /* petit-boutiste */
    p16(h + 3, global);
    h[5] = nf;
    if (emit(w, h, 6)) return w->err;
    return emit(w, fields, (size_t)nf * 3);
}

/* Types de base (verifies sur le SDK) */
#define T_ENUM 0x00
#define T_U8   0x02
#define T_S32  0x85
#define T_U16  0x84
#define T_U32  0x86
#define T_U32Z 0x8C

/* numeros locaux de messages */
#define L_FILEID 0
#define L_EVENT  1
#define L_RECORD 2
#define L_LAP    3
#define L_SESSION 4
#define L_ACTIVITY 5

static const uint8_t F_FILEID[] = {0,1,T_ENUM, 1,2,T_U16, 2,2,T_U16, 3,4,T_U32Z, 4,4,T_U32};
static const uint8_t F_EVENT[]  = {253,4,T_U32, 0,1,T_ENUM, 1,1,T_ENUM};
static const uint8_t F_RECORD[] = {253,4,T_U32, 0,4,T_S32, 1,4,T_S32, 2,2,T_U16, 3,1,T_U8, 4,1,T_U8,
                                   5,4,T_U32, 6,2,T_U16, 7,2,T_U16};
static const uint8_t F_LAP[]    = {254,2,T_U16, 253,4,T_U32, 0,1,T_ENUM, 1,1,T_ENUM, 2,4,T_U32,
                                   3,4,T_S32, 4,4,T_S32, 5,4,T_S32, 6,4,T_S32, 7,4,T_U32, 8,4,T_U32, 9,4,T_U32,
                                   15,1,T_U8, 16,1,T_U8, 21,2,T_U16, 22,2,T_U16, 25,1,T_ENUM};
static const uint8_t F_SESSION[] = {254,2,T_U16, 253,4,T_U32, 0,1,T_ENUM, 1,1,T_ENUM, 2,4,T_U32,
                                   3,4,T_S32, 4,4,T_S32, 5,1,T_ENUM, 6,1,T_ENUM, 7,4,T_U32, 8,4,T_U32, 9,4,T_U32,
                                   14,2,T_U16, 15,2,T_U16, 16,1,T_U8, 17,1,T_U8, 18,1,T_U8, 19,1,T_U8,
                                   20,2,T_U16, 21,2,T_U16, 22,2,T_U16, 23,2,T_U16, 25,2,T_U16, 26,2,T_U16};
static const uint8_t F_ACTIVITY[] = {253,4,T_U32, 0,4,T_U32, 1,2,T_U16, 2,1,T_ENUM, 3,1,T_ENUM, 4,1,T_ENUM, 5,4,T_U32};

#define NF(a) ((uint8_t)(sizeof(a) / 3))

static uint32_t fit_time(uint32_t unix_t) { return unix_t > FIT_EPOCH_UNIX ? unix_t - FIT_EPOCH_UNIX : 0u; }

/* ---------- utilitaires ---------- */
static int32_t semicircles(double deg) { return (int32_t)lround(deg * (2147483648.0 / 180.0)); }

static uint16_t clamp16(long v) { return (uint16_t)(v < 0 ? 0 : (v > 65534 ? 65534 : v)); }

static void acc_reset(fit_acc_t *a)
{
    memset(a, 0, sizeof(*a));
}

static int event_msg(fit_writer_t *w, uint32_t t_unix, uint8_t ev, uint8_t type)
{
    uint8_t m[1 + 4 + 1 + 1];
    m[0] = L_EVENT;
    p32(m + 1, fit_time(t_unix));
    m[5] = ev; m[6] = type;
    return emit(w, m, sizeof(m));
}

/* ---------- API ---------- */
int fit_begin(fit_writer_t *w, const fit_io_t *io, uint32_t start_unix, int32_t tz_offset_s)
{
    memset(w, 0, sizeof(*w));
    w->io = *io;
    w->tz_offset_s = tz_offset_s;
    w->begin_unix = start_unix;
    acc_reset(&w->ses); acc_reset(&w->lap);

    uint8_t hdr[14] = {14, 0x20, 0, 0, 0, 0, 0, 0, '.', 'F', 'I', 'T', 0, 0};   /* en-tete provisoire, taille 0 */
    if (io->write(io->ctx, hdr, sizeof(hdr))) return -1;

    if (def_msg(w, L_FILEID, 0, F_FILEID, NF(F_FILEID))) return -1;
    uint8_t m[1 + 1 + 2 + 2 + 4 + 4];
    m[0] = L_FILEID;
    m[1] = 4;                         /* type : activite */
    p16(m + 2, 255);                  /* fabricant : development */
    p16(m + 4, 1);                    /* produit */
    p32(m + 6, 1);                    /* numero de serie */
    p32(m + 10, fit_time(start_unix));
    if (emit(w, m, sizeof(m))) return -1;

    if (def_msg(w, L_EVENT, 21, F_EVENT, NF(F_EVENT))) return -1;
    if (def_msg(w, L_RECORD, 20, F_RECORD, NF(F_RECORD))) return -1;
    if (event_msg(w, start_unix, 0, 0)) return -1;     /* minuterie : debut */
    return 0;
}

static void acc_add(fit_acc_t *a, const fit_sample_t *s, int32_t lat, int32_t lon)
{
    if (!a->started) {
        a->started = 1;
        a->start_unix = s->time_unix;
        a->start_dist = s->dist_m;
        if (s->has_fix) { a->start_lat = lat; a->start_lon = lon; a->has_start_pos = 1; }
    }
    a->end_dist = s->dist_m;
    if (s->has_fix) { a->end_lat = lat; a->end_lon = lon; a->has_end_pos = 1; }
    if (s->speed_mps > a->max_speed) a->max_speed = s->speed_mps;
    if (s->hr >= 0)    { a->hr_sum += (uint32_t)s->hr;       a->hr_n++;  if ((uint32_t)s->hr > a->hr_max) a->hr_max = (uint32_t)s->hr; }
    if (s->cad >= 0)   { a->cad_sum += (uint32_t)s->cad;     a->cad_n++; if ((uint32_t)s->cad > a->cad_max) a->cad_max = (uint32_t)s->cad; }
    if (s->power >= 0) { a->pw_sum += (uint32_t)s->power;    a->pw_n++;  if ((uint32_t)s->power > a->pw_max) a->pw_max = (uint32_t)s->power; }
    if (s->has_alt) {
        if (!a->has_ref) { a->ref_alt = s->alt_m; a->has_ref = 1; }
        else if (s->alt_m - a->ref_alt >= 1.0f)  { a->ascent  += s->alt_m - a->ref_alt; a->ref_alt = s->alt_m; }
        else if (a->ref_alt - s->alt_m >= 1.0f)  { a->descent += a->ref_alt - s->alt_m; a->ref_alt = s->alt_m; }
    }
}

int fit_add_sample(fit_writer_t *w, const fit_sample_t *s)
{
    int32_t lat = 0x7FFFFFFF, lon = 0x7FFFFFFF;
    if (s->has_fix) { lat = semicircles(s->lat_deg); lon = semicircles(s->lon_deg); }

    uint8_t m[1 + 24];
    m[0] = L_RECORD;
    p32(m + 1, fit_time(s->time_unix));
    p32(m + 5, (uint32_t)lat);
    p32(m + 9, (uint32_t)lon);
    p16(m + 13, s->has_alt ? clamp16(lround(((double)s->alt_m + 500.0) * 5.0)) : 0xFFFF);
    m[15] = s->hr >= 0 ? (uint8_t)(s->hr > 254 ? 254 : s->hr) : 0xFF;
    m[16] = s->cad >= 0 ? (uint8_t)(s->cad > 254 ? 254 : s->cad) : 0xFF;
    p32(m + 17, (uint32_t)lround((double)s->dist_m * 100.0));
    p16(m + 21, clamp16(lround((double)s->speed_mps * 1000.0)));
    p16(m + 23, s->power >= 0 ? clamp16(s->power) : 0xFFFF);
    if (emit(w, m, sizeof(m))) return w->err;

    acc_add(&w->ses, s, lat, lon);
    acc_add(&w->lap, s, lat, lon);
    w->have_sample = 1;
    w->last_unix = s->time_unix;
    w->last_dist = s->dist_m;
    if (s->has_fix) { w->last_lat = lat; w->last_lon = lon; w->has_last_pos = 1; }
    return 0;
}

int fit_pause(fit_writer_t *w, uint32_t t_unix)
{
    if (w->paused) return 0;
    w->paused = 1; w->pause_start = t_unix;
    return event_msg(w, t_unix, 0, 4);          /* minuterie : arret */
}

int fit_resume(fit_writer_t *w, uint32_t t_unix)
{
    if (!w->paused) return 0;
    w->paused = 0;
    uint32_t d = t_unix > w->pause_start ? t_unix - w->pause_start : 0;
    w->ses.paused_s += d; w->lap.paused_s += d;
    return event_msg(w, t_unix, 0, 0);          /* minuterie : debut */
}

static int write_lap(fit_writer_t *w, uint32_t end_unix)
{
    const fit_acc_t *a = &w->lap;
    if (!w->lap_def) {
        if (def_msg(w, L_LAP, 19, F_LAP, NF(F_LAP))) return w->err;
        w->lap_def = 1;
    }
    uint32_t elapsed = end_unix > a->start_unix ? end_unix - a->start_unix : 0;
    uint32_t timer = elapsed > a->paused_s ? elapsed - a->paused_s : 0;
    uint8_t m[1 + 2 + 4 + 1 + 1 + 4 + 16 + 4 + 4 + 4 + 1 + 1 + 2 + 2 + 1];
    uint8_t *p = m;
    *p++ = L_LAP;
    p16(p, (uint16_t)w->laps); p += 2;
    p32(p, fit_time(end_unix)); p += 4;
    *p++ = 9;  *p++ = 1;                                   /* evenement : tour, type : arret */
    p32(p, fit_time(a->start_unix)); p += 4;
    p32(p, a->has_start_pos ? (uint32_t)a->start_lat : 0x7FFFFFFFu); p += 4;
    p32(p, a->has_start_pos ? (uint32_t)a->start_lon : 0x7FFFFFFFu); p += 4;
    p32(p, a->has_end_pos ? (uint32_t)a->end_lat : 0x7FFFFFFFu); p += 4;
    p32(p, a->has_end_pos ? (uint32_t)a->end_lon : 0x7FFFFFFFu); p += 4;
    p32(p, elapsed * 1000u); p += 4;
    p32(p, timer * 1000u); p += 4;
    p32(p, (uint32_t)lround((double)(a->end_dist - a->start_dist) * 100.0)); p += 4;
    *p++ = a->hr_n ? (uint8_t)(a->hr_sum / a->hr_n) : 0xFF;
    *p++ = a->hr_n ? (uint8_t)a->hr_max : 0xFF;
    p16(p, clamp16(lround(a->ascent))); p += 2;
    p16(p, clamp16(lround(a->descent))); p += 2;
    *p++ = 2;                                              /* sport : velo */
    w->laps++;
    return emit(w, m, (size_t)(p - m));
}

int fit_lap(fit_writer_t *w, uint32_t t_unix)
{
    if (!w->have_sample) return 0;
    if (write_lap(w, t_unix)) return w->err;
    acc_reset(&w->lap);
    w->lap.started = 1;
    w->lap.start_unix = t_unix;
    w->lap.start_dist = w->last_dist;
    if (w->has_last_pos) { w->lap.start_lat = w->last_lat; w->lap.start_lon = w->last_lon; w->lap.has_start_pos = 1; }
    return 0;
}

int fit_flush(fit_writer_t *w)
{
    if (flush_buf(w)) return w->err;
    if (w->io.flush) { int r = w->io.flush(w->io.ctx); if (r) w->err = r; return r; }
    return 0;
}

int fit_end(fit_writer_t *w)
{
    if (!w->have_sample) return -2;
    const fit_acc_t *a = &w->ses;
    uint32_t end = w->last_unix;
    if (w->paused) fit_resume(w, end);

    if (event_msg(w, end, 0, 4)) return w->err;          /* minuterie : arret final */
    if (write_lap(w, end)) return w->err;

    if (!w->ses_def) { if (def_msg(w, L_SESSION, 18, F_SESSION, NF(F_SESSION))) return w->err; w->ses_def = 1; }
    uint32_t elapsed = end > a->start_unix ? end - a->start_unix : 0;
    uint32_t timer = elapsed > a->paused_s ? elapsed - a->paused_s : 0;
    float dist = a->end_dist - a->start_dist;
    uint8_t m[1 + 2 + 4 + 1 + 1 + 4 + 4 + 4 + 1 + 1 + 4 + 4 + 4 + 2 + 2 + 1 + 1 + 1 + 1 + 2 + 2 + 2 + 2 + 2 + 2];
    uint8_t *p = m;
    *p++ = L_SESSION;
    p16(p, 0); p += 2;
    p32(p, fit_time(end)); p += 4;
    *p++ = 8;  *p++ = 1;                                  /* evenement : session, type : arret */
    p32(p, fit_time(a->start_unix)); p += 4;
    p32(p, a->has_start_pos ? (uint32_t)a->start_lat : 0x7FFFFFFFu); p += 4;
    p32(p, a->has_start_pos ? (uint32_t)a->start_lon : 0x7FFFFFFFu); p += 4;
    *p++ = 2;  *p++ = 0;                                  /* sport : velo, sous-sport : general */
    p32(p, elapsed * 1000u); p += 4;
    p32(p, timer * 1000u); p += 4;
    p32(p, (uint32_t)lround((double)dist * 100.0)); p += 4;
    p16(p, timer ? clamp16(lround((double)dist / timer * 1000.0)) : 0); p += 2;
    p16(p, clamp16(lround((double)a->max_speed * 1000.0))); p += 2;
    *p++ = a->hr_n ? (uint8_t)(a->hr_sum / a->hr_n) : 0xFF;
    *p++ = a->hr_n ? (uint8_t)a->hr_max : 0xFF;
    *p++ = a->cad_n ? (uint8_t)(a->cad_sum / a->cad_n) : 0xFF;
    *p++ = a->cad_n ? (uint8_t)a->cad_max : 0xFF;
    p16(p, a->pw_n ? (uint16_t)(a->pw_sum / a->pw_n) : 0xFFFF); p += 2;
    p16(p, a->pw_n ? (uint16_t)a->pw_max : 0xFFFF); p += 2;
    p16(p, clamp16(lround(a->ascent))); p += 2;
    p16(p, clamp16(lround(a->descent))); p += 2;
    p16(p, 0); p += 2;                                    /* premier tour */
    p16(p, (uint16_t)w->laps); p += 2;                    /* nombre de tours */
    if (emit(w, m, (size_t)(p - m))) return w->err;

    if (!w->act_def) { if (def_msg(w, L_ACTIVITY, 34, F_ACTIVITY, NF(F_ACTIVITY))) return w->err; w->act_def = 1; }
    uint8_t am[1 + 4 + 4 + 2 + 1 + 1 + 1 + 4];
    am[0] = L_ACTIVITY;
    p32(am + 1, fit_time(end));
    p32(am + 5, timer * 1000u);
    p16(am + 9, 1);
    am[11] = 0; am[12] = 26; am[13] = 1;                  /* type manuel, evenement activite, arret */
    p32(am + 14, fit_time((uint32_t)((int64_t)end + w->tz_offset_s)));
    if (emit(w, am, sizeof(am))) return w->err;

    if (flush_buf(w)) return w->err;

    /* en-tete definitif et CRC du fichier */
    uint8_t hdr[14] = {14, 0x20, 0, 0, 0, 0, 0, 0, '.', 'F', 'I', 'T', 0, 0};
    p16(hdr + 2, 21218);                                  /* version du profil : 21.218 */
    p32(hdr + 4, w->data_size);
    uint16_t hc = 0;
    for (int i = 0; i < 12; i++) hc = crc_byte(hc, hdr[i]);
    p16(hdr + 12, hc);
    uint16_t fc = 0;
    for (int i = 0; i < 14; i++) fc = crc_byte(fc, hdr[i]);
    fc = (uint16_t)(crc_zeros(fc, w->data_size) ^ w->crc0);
    uint8_t tail[2];
    p16(tail, fc);
    if (w->io.write(w->io.ctx, tail, 2)) return -1;
    if (w->io.write_at(w->io.ctx, 0, hdr, sizeof(hdr))) return -1;
    if (w->io.flush) return w->io.flush(w->io.ctx);
    return 0;
}

/* ---------- recuperation ---------- */
typedef struct { uint16_t gnum; uint8_t nf, fnum[24], fsize[24]; uint16_t total; int ok; } fdef_t;

static uint16_t g16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t g32(const uint8_t *p) { return (uint32_t)g16(p) | ((uint32_t)g16(p + 2) << 16); }

/* Parcourt les messages complets ; cb recoit chaque message record. cb retourne non nul pour arreter. */
static int scan(const fit_src_t *s, int (*cb)(void *, const fit_sample_t *), void *u)
{
    uint8_t hdr[14];
    if (s->size < 14 || s->read_at(s->ctx, 0, hdr, 14)) return -1;
    uint32_t hs = hdr[0];
    if ((hs != 12 && hs != 14) || memcmp(hdr + 8, ".FIT", 4) != 0) return -1;
    fdef_t defs[16];
    memset(defs, 0, sizeof(defs));
    uint32_t p = hs;
    int n = 0;
    while (p < s->size) {
        uint8_t h;
        if (s->read_at(s->ctx, p, &h, 1)) break;
        if (h & 0x80) {                                   /* horodatage compresse : non produit par ce module */
            fdef_t *d = &defs[(h >> 5) & 3];
            if (!d->ok || p + 1 + d->total > s->size) break;
            p += 1u + d->total;
            continue;
        }
        if (h & 0x40) {                                   /* definition */
            uint8_t b[5];
            if (p + 6 > s->size || s->read_at(s->ctx, p + 1, b, 5)) break;
            if (b[1] != 0) return -2;                     /* gros-boutiste : non gere */
            uint8_t nf = b[4];
            uint32_t len = 6u + 3u * nf;
            uint8_t dev = 0;
            if (h & 0x20) {
                if (p + len + 1 > s->size || s->read_at(s->ctx, p + len, &dev, 1)) break;
                len += 1u + 3u * dev;
            }
            if (p + len > s->size || nf > 24) break;
            fdef_t *d = &defs[h & 0x0F];
            memset(d, 0, sizeof(*d));
            d->gnum = g16(b + 2); d->nf = nf;
            for (int i = 0; i < nf; i++) {
                uint8_t f[3];
                if (s->read_at(s->ctx, p + 6u + 3u * (uint32_t)i, f, 3)) return -1;
                d->fnum[i] = f[0]; d->fsize[i] = f[1]; d->total = (uint16_t)(d->total + f[1]);
            }
            for (int i = 0; i < dev; i++) {
                uint8_t f[3];
                if (s->read_at(s->ctx, p + 7u + 3u * (uint32_t)nf + 3u * (uint32_t)i, f, 3)) return -1;
                d->total = (uint16_t)(d->total + f[1]);
            }
            d->ok = 1;
            p += len;
            continue;
        }
        fdef_t *d = &defs[h & 0x0F];                      /* donnees */
        if (!d->ok || p + 1u + d->total > s->size) break;
        if (d->gnum == 20 && d->total <= 64) {
            uint8_t b[64];
            if (s->read_at(s->ctx, p + 1, b, d->total)) break;
            fit_sample_t sm;
            memset(&sm, 0, sizeof(sm));
            sm.hr = sm.cad = sm.power = -1;
            int32_t lat = 0x7FFFFFFF, lon = 0x7FFFFFFF;
            uint32_t o = 0;
            for (int i = 0; i < d->nf; i++) {
                const uint8_t *v = b + o;
                switch (d->fnum[i]) {
                case 253: sm.time_unix = g32(v) + FIT_EPOCH_UNIX; break;
                case 0:   lat = (int32_t)g32(v); break;
                case 1:   lon = (int32_t)g32(v); break;
                case 2:   if (g16(v) != 0xFFFF) { sm.has_alt = 1; sm.alt_m = (float)g16(v) / 5.0f - 500.0f; } break;
                case 3:   if (v[0] != 0xFF) sm.hr = v[0]; break;
                case 4:   if (v[0] != 0xFF) sm.cad = v[0]; break;
                case 5:   sm.dist_m = (float)g32(v) / 100.0f; break;
                case 6:   sm.speed_mps = (float)g16(v) / 1000.0f; break;
                case 7:   if (g16(v) != 0xFFFF) sm.power = g16(v); break;
                default: break;
                }
                o += d->fsize[i];
            }
            if (lat != 0x7FFFFFFF && lon != 0x7FFFFFFF) {
                sm.has_fix = 1;
                sm.lat_deg = (double)lat * (180.0 / 2147483648.0);
                sm.lon_deg = (double)lon * (180.0 / 2147483648.0);
            }
            n++;
            if (cb(u, &sm)) return n;
        }
        p += 1u + d->total;
    }
    return n;
}

static int cb_first(void *u, const fit_sample_t *s) { *(fit_sample_t *)u = *s; return 1; }
static int cb_add(void *u, const fit_sample_t *s) { return fit_add_sample((fit_writer_t *)u, s); }

int fit_recover(const fit_src_t *src, const fit_io_t *dst, int32_t tz_offset_s, fit_writer_t *w)
{
    fit_sample_t first;
    int n = scan(src, cb_first, &first);
    if (n < 0) return n;
    if (n == 0) return -3;
    if (fit_begin(w, dst, first.time_unix, tz_offset_s)) return -4;
    n = scan(src, cb_add, w);
    if (n < 0) return n;
    if (fit_end(w)) return -5;
    return n;
}
