#include "guidance.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define N 121
static float x[N], y[N], e[N];
static route_t route;

static void straight(void)
{
    route_init(&route, x, y, e, N, 5.0f);
    route.n = N; route.total_m = (N - 1) * 5.0f;
    for (int i = 0; i < N; i++) { x[i] = 0.0f; y[i] = i * 5.0f; e[i] = 0.0f; }
}

int main(void)
{
    guidance_turn_t t;
    straight();
    assert(!guidance_next_turn(&route, 0.0f, 500.0f, 35.0f, &t));

    /* Ligne nord puis virage a droite vers l'est au point 200 m. */
    straight();
    for (int i = 40; i < N; i++) { x[i] = (i - 40) * 5.0f; y[i] = 200.0f; }
    assert(guidance_next_turn(&route, 0.0f, 500.0f, 35.0f, &t));
    assert(t.found && t.direction == 1 && t.angle_deg >= 60 && t.dist_m > 150 && t.dist_m < 250);

    /* Ligne nord puis virage a gauche vers l'ouest. */
    straight();
    for (int i = 40; i < N; i++) { x[i] = -(i - 40) * 5.0f; y[i] = 200.0f; }
    assert(guidance_next_turn(&route, 0.0f, 500.0f, 35.0f, &t));
    assert(t.found && t.direction == -1 && t.angle_deg >= 60);

    /* Le virage deja passe ne doit pas etre annonce comme prochain. */
    assert(!guidance_next_turn(&route, 210.0f, 200.0f, 35.0f, &t));
    assert(!guidance_next_turn(&route, 300.0f, 200.0f, 35.0f, &t));
    puts("test_guidance: OK");
    return 0;
}
