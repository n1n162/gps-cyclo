#include "ble_decode.h"

#include <string.h>

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int ble_hr_parse(const uint8_t *d, size_t len, ble_hr_t *o) {
    if (!d || !o || len < 2) return -1;
    memset(o, 0, sizeof *o);
    uint8_t fl = d[0];
    size_t p = 1;
    int wide = fl & 0x01;
    if (len < p + (wide ? 2u : 1u)) return -1;
    o->bpm = wide ? rd16(d + p) : d[p];
    p += wide ? 2 : 1;
    int sc = (fl >> 1) & 3;
    o->contact = (sc == 3) ? 1 : (sc == 2) ? 0 : -1;
    if (fl & 0x08) {
        if (len < p + 2) return -1;
        o->has_energy = 1;
        o->energy_kj = rd16(d + p);
        p += 2;
    }
    if (fl & 0x10) {
        size_t n = (len - p) / 2;
        if (n > BLE_HR_MAX_RR) n = BLE_HR_MAX_RR;
        for (size_t i = 0; i < n; i++)
            o->rr_ms[i] = (int)((uint32_t)rd16(d + p + 2 * i) * 1000u / 1024u);
        o->n_rr = (int)n;
    }
    return 0;
}

int ble_csc_parse(const uint8_t *d, size_t len, ble_csc_t *o) {
    if (!d || !o || len < 1) return -1;
    memset(o, 0, sizeof *o);
    uint8_t fl = d[0];
    size_t p = 1;
    if (fl & 0x01) {
        if (len < p + 6) return -1;
        o->has_wheel = 1;
        o->wheel_revs = rd32(d + p);
        o->wheel_time = rd16(d + p + 4);
        p += 6;
    }
    if (fl & 0x02) {
        if (len < p + 4) return -1;
        o->has_crank = 1;
        o->crank_revs = rd16(d + p);
        o->crank_time = rd16(d + p + 2);
    }
    return 0;
}

int ble_cp_parse(const uint8_t *d, size_t len, ble_cp_t *o) {
    if (!d || !o || len < 4) return -1;
    memset(o, 0, sizeof *o);
    uint16_t fl = rd16(d);
    o->power_w = (int16_t)rd16(d + 2);
    size_t p = 4;
    if (fl & 0x0001) { /* repartition de puissance pedale */
        if (len < p + 1) return -1;
        o->has_balance = 1;
        o->balance_half_pct = d[p];
        p += 1;
    }
    if (fl & 0x0004) { /* couple cumule, ignore */
        if (len < p + 2) return -1;
        p += 2;
    }
    if (fl & 0x0010) {
        if (len < p + 6) return -1;
        o->has_wheel = 1;
        o->wheel_revs = rd32(d + p);
        o->wheel_time = rd16(d + p + 4);
        p += 6;
    }
    if (fl & 0x0020) {
        if (len < p + 4) return -1;
        o->has_crank = 1;
        o->crank_revs = rd16(d + p);
        o->crank_time = rd16(d + p + 2);
    }
    return 0; /* champs suivants (forces, angles, energie) non decodes */
}

int ble_crank_cadence(ble_crank_t *s, uint16_t revs, uint16_t time, float *rpm) {
    if (!s || !rpm) return -1;
    if (!s->init) {
        s->init = 1; s->revs = revs; s->time = time;
        return 0;
    }
    uint16_t dr = (uint16_t)(revs - s->revs);
    uint16_t dt = (uint16_t)(time - s->time);
    if (dt == 0) return dr == 0 ? 0 : -1;
    s->revs = revs; s->time = time;
    float r = (float)dr * 60.0f * (float)BLE_CSC_TICKS_PER_S / (float)dt;
    if (r > BLE_MAX_CADENCE_RPM) return -1;
    *rpm = r;
    return 1;
}

int ble_wheel_speed(ble_wheel_t *s, uint32_t revs, uint16_t time,
                    unsigned ticks_per_s, float circ_m,
                    float *kmh, float *dist_m) {
    if (!s || !kmh || !dist_m || ticks_per_s == 0 || circ_m <= 0.0f) return -1;
    if (!s->init) {
        s->init = 1; s->revs = revs; s->time = time;
        return 0;
    }
    uint32_t dr = revs - s->revs;
    uint16_t dt = (uint16_t)(time - s->time);
    if (dt == 0) return dr == 0 ? 0 : -1;
    s->revs = revs; s->time = time;
    float d = (float)dr * circ_m;
    float v = d / ((float)dt / (float)ticks_per_s) * 3.6f;
    if (v > BLE_MAX_SPEED_KMH) return -1;
    *kmh = v;
    *dist_m = d;
    return 1;
}
