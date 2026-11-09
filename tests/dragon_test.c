// dragon_test.c - drives the dragon with no graphics: the world builds, the dragonet walks to berries, eats five, and grows.
#include "../src/game/world.h"
#include "../src/game/dragon.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
int main(void) {
    static World w; world_make(&w, 7);
    printf("props %d food %d\n", w.nprop, w.nfood);
    CHECK(w.nprop > 150 && w.nfood > 40);
    int berries = 0, meat = 0; for (int i = 0; i < w.nfood; i++) { if (food_is_meat(w.food[i].kind)) meat++; else berries++; }
    CHECK(berries > 20 && meat > 5);
    for (int i = 0; i < 50; i++) { float x = (rand() % 200) - 100, z = (rand() % 200) - 100; float h = world_height(&w, x, z); CHECK(h > -40 && h < 60); Vec3 n = world_normal(&w, x, z); CHECK(n.y > 0.3f); }
    Dragon d; dragon_init(&d, &w, 0, 0); CHECK(d.stage == ST_DRAGONET);
    int ate = 0, grew = 0; float t = 0;
    for (int step = 0; step < 20000 && d.stage == ST_DRAGONET; step++) {
        float dt = 1.0f / 60; t += dt;
        int near = world_nearest_food(&w, d.x, d.z, 400); CHECK(near >= 0);
        Food *f = &w.food[near]; float dx = f->x - d.x, dz = f->z - d.z, dist = sqrtf(dx * dx + dz * dz);
        DragonInput in = {0}; float ang = atan2f(dx, dz);
        if (dist > 0.9f) { in.sx = sinf(ang); in.sy = cosf(ang); in.zr = 1; } else if (d.act_t <= 0) in.x_press = 1;
        int ev = dragon_update(&d, &w, &in, 0, dt);
        if (ev & 1) ate++;
        if (ev & 2) grew++;
        world_update(&w, dt, d.x, d.z);
    }
    printf("ate %d in %.1f s, stage %d growth %.0f\n", ate, t, d.stage, d.growth);
    CHECK(ate >= 2 && grew == 1 && d.stage == ST_DRAGON);
    /* a dragon can fly: hold A and climb (after the grow-up moment) */
    for (int i = 0; i < 150; i++) { DragonInput in = {0}; dragon_update(&d, &w, &in, 0, 1.0f / 60); }
    for (int i = 0; i < 180; i++) { DragonInput in = {0}; in.a = 1; in.a_press = i == 0; dragon_update(&d, &w, &in, 0, 1.0f / 60); }
    printf("after flapping 3 s: mode %d height above ground %.1f\n", d.mode, d.y - world_height(&w, d.x, d.z));
    CHECK(d.mode == DM_AIR && d.y - world_height(&w, d.x, d.z) > 2);
    /* let go: it glides down and lands */
    int landed = 0; for (int i = 0; i < 1200 && !landed; i++) { DragonInput in = {0}; in.sy = -1; landed = dragon_update(&d, &w, &in, 0, 1.0f / 60) & 8; }
    CHECK(landed);
    /* drones: one dives when the dragon walks under it, and a tail whip knocks it down */
    CHECK(w.ndrone >= 3);
    Drone *dr = &w.drone[0]; d.x = dr->hx; d.z = dr->hz; d.y = world_height(&w, d.x, d.z); d.mode = DM_GROUND;
    int hit = 0, down = 0;
    for (int i = 0; i < 1200; i++) { int ev = world_drones(&w, 1.0f / 60, d.x, d.y + 1, d.z, 2.5f, 0, 0); if (ev & 1) hit++; }
    printf("drone hits in 20 s standing still: %d\n", hit); CHECK(hit >= 1);
    dr->state = 1; dr->x = d.x + 1; dr->z = d.z; dr->y = d.y + 1;
    for (int i = 0; i < 10; i++) { int ev = world_drones(&w, 1.0f / 60, d.x, d.y + 1, d.z, 2.5f, 1, 0); if (ev & 2) down++; }
    CHECK(down == 1 && dr->state == 3);
    printf("dragon test OK\n"); return 0; }
