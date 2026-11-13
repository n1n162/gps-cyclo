#include <math.h>
#include <stdio.h>
#include "../core/ble_decode.h"

static int fails;
static void check(const char *what, int ok) {
    printf(" %-70s %s\n", what, ok ? "OK" : "ECHEC");
    if (!ok) fails++;
}
static int near(float a, float b) { return fabsf(a - b) < 0.05f; }

int main(void) {
    ble_hr_t h;
    printf("Frequence cardiaque\n");
    { const uint8_t f[] = {0x00, 72};
      check("uint8 : 72 bpm, contact non supporte", ble_hr_parse(f, sizeof f, &h) == 0 && h.bpm == 72 && h.contact == -1 && h.n_rr == 0); }
    { const uint8_t f[] = {0x01, 0x90, 0x00};
      check("uint16 : 144 bpm", ble_hr_parse(f, sizeof f, &h) == 0 && h.bpm == 144); }
    { const uint8_t f[] = {0x06, 60};
      check("contact detecte", ble_hr_parse(f, sizeof f, &h) == 0 && h.contact == 1); }
    { const uint8_t f[] = {0x04, 60};
      check("contact non detecte", ble_hr_parse(f, sizeof f, &h) == 0 && h.contact == 0); }
    { const uint8_t f[] = {0x18, 80, 0x02, 0x01, 0x00, 0x04, 0x00, 0x02};
      check("energie 258 kJ + 2 RR (1000 ms, 500 ms)",
            ble_hr_parse(f, sizeof f, &h) == 0 && h.has_energy && h.energy_kj == 258 &&
            h.n_rr == 2 && h.rr_ms[0] == 1000 && h.rr_ms[1] == 500); }
    { const uint8_t f[] = {0x10, 70, 0x00, 0x04, 0xFF};
      check("octet RR en trop ignore", ble_hr_parse(f, sizeof f, &h) == 0 && h.n_rr == 1); }
    { const uint8_t f[] = {0x01, 0x90};
      check("uint16 tronque : refuse", ble_hr_parse(f, sizeof f, &h) == -1); }
    { const uint8_t f[] = {0x08, 70, 0x01};
      check("energie tronquee : refuse", ble_hr_parse(f, sizeof f, &h) == -1); }
    check("trame vide et NULL : refuses",
          ble_hr_parse((const uint8_t *)"", 0, &h) == -1 && ble_hr_parse(NULL, 4, &h) == -1);

    printf("CSC\n");
    ble_csc_t c;
    { const uint8_t f[] = {0x03, 0xE8,0x03,0x00,0x00, 0x00,0x08, 0x32,0x00, 0x00,0x04};
      check("roue + pedalier",
            ble_csc_parse(f, sizeof f, &c) == 0 && c.has_wheel && c.wheel_revs == 1000 &&
            c.wheel_time == 2048 && c.has_crank && c.crank_revs == 50 && c.crank_time == 1024); }
    { const uint8_t f[] = {0x02, 0x32,0x00, 0x00,0x04};
      check("pedalier seul", ble_csc_parse(f, sizeof f, &c) == 0 && !c.has_wheel && c.has_crank && c.crank_revs == 50); }
    { const uint8_t f[] = {0x01, 0xE8,0x03,0x00,0x00, 0x00};
      check("roue tronquee : refuse", ble_csc_parse(f, sizeof f, &c) == -1); }
    { const uint8_t f[] = {0x03, 0xE8,0x03,0x00,0x00, 0x00,0x08, 0x32,0x00};
      check("pedalier tronque : refuse", ble_csc_parse(f, sizeof f, &c) == -1); }

    printf("Puissance\n");
    ble_cp_t p;
    { const uint8_t f[] = {0x00,0x00, 0xFA,0x00};
      check("250 W", ble_cp_parse(f, sizeof f, &p) == 0 && p.power_w == 250 && !p.has_crank); }
    { const uint8_t f[] = {0x00,0x00, 0xFB,0xFF};
      check("puissance negative (-5 W)", ble_cp_parse(f, sizeof f, &p) == 0 && p.power_w == -5); }
    { const uint8_t f[] = {0x01,0x00, 0xC8,0x00, 0x64};
      check("repartition : 100 demi-% = 50 %", ble_cp_parse(f, sizeof f, &p) == 0 && p.has_balance && p.balance_half_pct == 100); }
    { const uint8_t f[] = {0x24,0x00, 0x96,0x00, 0x00,0x00, 0x0A,0x00, 0x00,0x04};
      check("couple saute, pedalier decode",
            ble_cp_parse(f, sizeof f, &p) == 0 && p.power_w == 150 && p.has_crank &&
            p.crank_revs == 10 && p.crank_time == 1024); }
    { const uint8_t f[] = {0x35,0x00, 0x64,0x00, 0x60, 0x00,0x00,
                           0x10,0x00,0x00,0x00, 0x00,0x08, 0x05,0x00, 0x00,0x04};
      check("tous champs : balance, couple, roue, pedalier",
            ble_cp_parse(f, sizeof f, &p) == 0 && p.power_w == 100 && p.balance_half_pct == 0x60 &&
            p.has_wheel && p.wheel_revs == 16 && p.wheel_time == 2048 &&
            p.has_crank && p.crank_revs == 5 && p.crank_time == 1024); }
    { const uint8_t f[] = {0x00,0x08, 0x64,0x00, 0x01,0x00};
      check("champs apres le pedalier non decodes mais acceptes", ble_cp_parse(f, sizeof f, &p) == 0 && p.power_w == 100); }
    { const uint8_t f[] = {0x20,0x00, 0x64,0x00, 0x0A,0x00};
      check("pedalier tronque : refuse", ble_cp_parse(f, sizeof f, &p) == -1); }
    { const uint8_t f[] = {0x00,0x00, 0x64};
      check("trame trop courte : refusee", ble_cp_parse(f, sizeof f, &p) == -1); }

    printf("Cadence\n");
    ble_crank_t cs = {0};
    float rpm = -1;
    check("premiere mesure : pas de valeur", ble_crank_cadence(&cs, 100, 0, &rpm) == 0);
    check("+1 tour en 1 s = 60 rpm", ble_crank_cadence(&cs, 101, 1024, &rpm) == 1 && near(rpm, 60));
    check("meme mesure repetee : pas d'evenement", ble_crank_cadence(&cs, 101, 1024, &rpm) == 0);
    check("+2 tours en 2 s = 60 rpm", ble_crank_cadence(&cs, 103, 3072, &rpm) == 1 && near(rpm, 60));
    { ble_crank_t r = {0};
      ble_crank_cadence(&r, 65535, 65000, &rpm);
      check("debordement compteur et temps : 120 rpm", ble_crank_cadence(&r, 1, 488, &rpm) == 1 && near(rpm, 120)); }
    { ble_crank_t r = {0};
      ble_crank_cadence(&r, 0, 0, &rpm);
      check("600 rpm : valeur ignoree", ble_crank_cadence(&r, 10, 1024, &rpm) == -1); }
    { ble_crank_t r = {0};
      ble_crank_cadence(&r, 0, 0, &rpm);
      check("tours sans temps : invalide", ble_crank_cadence(&r, 1, 0, &rpm) == -1); }

    printf("Vitesse\n");
    ble_wheel_t ws = {0};
    float kmh = 0, dist = 0;
    check("premiere mesure : pas de valeur", ble_wheel_speed(&ws, 1000, 0, BLE_CSC_TICKS_PER_S, 2.1f, &kmh, &dist) == 0);
    check("+5 tours en 1 s, 2,1 m : 37,8 km/h, 10,5 m",
          ble_wheel_speed(&ws, 1005, 1024, BLE_CSC_TICKS_PER_S, 2.1f, &kmh, &dist) == 1 && near(kmh, 37.8f) && near(dist, 10.5f));
    check("repetition : pas d'evenement", ble_wheel_speed(&ws, 1005, 1024, BLE_CSC_TICKS_PER_S, 2.1f, &kmh, &dist) == 0);
    { ble_wheel_t r = {0};
      ble_wheel_speed(&r, 0xFFFFFFFEu, 0, BLE_CP_WHEEL_TICKS_PER_S, 2.1f, &kmh, &dist);
      check("debordement 32 bits, ticks 1/2048 : 37,8 km/h",
            ble_wheel_speed(&r, 3, 2048, BLE_CP_WHEEL_TICKS_PER_S, 2.1f, &kmh, &dist) == 1 && near(kmh, 37.8f)); }
    { ble_wheel_t r = {0};
      ble_wheel_speed(&r, 0, 0, BLE_CSC_TICKS_PER_S, 2.1f, &kmh, &dist);
      check("302 km/h : valeur ignoree", ble_wheel_speed(&r, 40, 1024, BLE_CSC_TICKS_PER_S, 2.1f, &kmh, &dist) == -1); }
    check("circonference nulle : refusee", ble_wheel_speed(&ws, 1, 1, BLE_CSC_TICKS_PER_S, 0.0f, &kmh, &dist) == -1);

    printf("%s\n", fails ? "ECHECS" : "TOUT OK");
    return fails ? 1 : 0;
}
