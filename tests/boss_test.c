// boss_test.c - Ironhorn can be beaten the way the game teaches it (dodge into a tree, whip the core), and a roar stuns it.
#include "../src/game/boss.h"
#include <stdio.h>
#include <math.h>
static World w;
int main(void) {
    world_make(&w, 2026);
    printf("arena at %.0f,%.0f r %.0f, %d ring trees, cage at %.1f,%.1f,%.1f\n", w.arena_x, w.arena_z, w.arena_r, w.narena_tree, w.cage_x, w.cage_y, w.cage_z);
    if (w.narena_tree < 5) { printf("FAIL: ring trees\n"); return 1; }
    for (int i = 0; i < w.nprop; i++) { int ring = 0; for (int k = 0; k < w.narena_tree; k++) ring |= w.arena_tree[k] == i;
        float dx = w.prop[i].x - w.arena_x, dz = w.prop[i].z - w.arena_z; if (!ring && dx * dx + dz * dz < w.arena_r * w.arena_r) { printf("FAIL: prop %d inside the clearing\n", i); return 1; } }
    Boss b; boss_init(&b, &w);
    /* the player stands in front of a ring tree; when Ironhorn charges, step aside; when it is stuck, walk to its core and whip */
    const Prop *tree = &w.prop[w.arena_tree[2]]; float tx = tree->x + (w.arena_x - tree->x) * 0.15f, tz = tree->z + (w.arena_z - tree->z) * 0.15f;
    float px = tx, pz = tz, dt = 1.0f / 30; int hits_taken = 0, hits = 0, stuck = 0, won = 0, whip_cd = 0;
    for (int f = 0; f < 30 * 180 && !won; f++) {
        int whip = 0; float speed = 10;   /* a dragon running (ZR) */
        if (b.state == B_CHARGE) { float sx = -b.cz, sz = b.cx; px += sx * speed * dt; pz += sz * speed * dt; }   /* sidestep */
        else if (boss_vulnerable(&b)) { Vec3 c = boss_core(&b); float dx = c.x - px, dz = c.z - pz, d = sqrtf(dx * dx + dz * dz); if (d > 1.5f) { px += dx / d * speed * dt; pz += dz / d * speed * dt; } else if (whip_cd <= 0) { whip = 1; whip_cd = 20; } }
        else { float dx = tx - px, dz = tz - pz, d = sqrtf(dx * dx + dz * dz); if (d > 0.5f) { px += dx / d * speed * dt; pz += dz / d * speed * dt; } }
        if (whip_cd > 0) whip_cd--;
        int ev = boss_update(&b, &w, dt, px, world_height(&w, px, pz), pz, 1.5f, whip, 0);
        if (ev & 2) hits_taken++; if (ev & 16) stuck++; if (ev & 4) hits++; if (ev & 8) { won = 1; printf("defeated after %.0f s: %d times stuck, %d hits taken\n", f * dt, stuck, hits_taken); } }
    if (!won) { printf("FAIL: could not beat Ironhorn (state %d hp %d, stuck %d, hits %d, hits taken %d)\n", b.state, b.hp, stuck, hits, hits_taken); return 1; }
    for (int f = 0; f < 30 * 4; f++) boss_update(&b, &w, dt, px, 0, pz, 1.5f, 0, 0);
    if (b.cage_open < 0.99f) { printf("FAIL: cage did not open\n"); return 1; }
    /* roar: stand close while it stalks and roar */
    boss_init(&b, &w); int stunned = 0; px = w.arena_x; pz = w.arena_z;
    for (int f = 0; f < 30 * 3; f++) { float gx = b.x + 6, gz = b.z; int ev = boss_update(&b, &w, dt, gx, b.y, gz, 1.5f, 0, f == 45); if (ev & 32) stunned = 1; }
    if (!stunned) { printf("FAIL: roar did not stun\n"); return 1; }
    /* Scale Thieves (sky world): one sneaks up and steals food; catching it with the tail gives it back */
    static World sw; world_make_kind(&sw, 2127, WK_SKY); if (sw.nthief < 1) { printf("FAIL: no thieves\n"); return 1; }
    Drone *th = &sw.thief[0]; float qx = th->x + 6, qz = th->z, st = 0, rc = 0, got = 0, back = 0;
    if (world_is_void(&sw, qx, qz)) qx = th->x - 6;
    for (int f = 0; f < 30 * 20 && !got; f++) { float sv, rv; int ev = world_thieves(&sw, dt, qx, world_height(&sw, qx, qz), qz, 1.5f, (Attack){0, 0, 0, 0}, 30, &sv, &rv); if (ev & 1) { got = 1; st = sv; } }
    if (!got) { printf("FAIL: the thief never stole\n"); return 1; }
    for (int f = 0; f < 30 * 2 && !back; f++) { qx = th->x; qz = th->z; float sv, rv; int ev = world_thieves(&sw, dt, qx, th->y, qz, 1.5f, (Attack){qx, th->y + 0.5f, qz, 1.5f}, 20, &sv, &rv); if (ev & 2) { back = 1; rc = rv; } }
    if (!back || rc < st) { printf("FAIL: catching the thief did not give the food back (%.0f of %.0f)\n", rc, st); return 1; }
    printf("scale thief stole %.0f and gave %.0f back\n", st, rc);
    printf("boss test OK\n"); return 0; }
