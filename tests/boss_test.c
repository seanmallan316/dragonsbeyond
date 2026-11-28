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
    {   /* a young player who never dodges: stands in the clearing and keeps attacking. It must still win (the boss gets dizzy). */
        Boss k; boss_init(&k, &w); float px = w.arena_x - 3, pz = w.arena_z - 3, py = world_height(&w, px, pz); int hits_taken = 0, won = 0;
        for (int i = 0; i < 60 * 240 && !won; i++) { int ev = boss_update(&k, &w, 1.0f / 60, px, py, pz, 1.4f, (i % 30) == 0, 0); if (ev & 2) hits_taken++; if (ev & 8) won = 1; }
        printf("clumsy player: won %d after %d hits taken\n", won, hits_taken); if (!won) { printf("FAIL: a player who never dodges can never win\n"); return 1; } }
    Boss b; boss_init(&b, &w);
    /* a player who fights like the game teaches: dodge the charge so it rams a tree, jump the shock waves, hit it back when it is open */
    const Prop *tree = &w.prop[w.arena_tree[2]]; float tx = tree->x + (w.arena_x - tree->x) * 0.15f, tz = tree->z + (w.arena_z - tree->z) * 0.15f;
    float px = tx, pz = tz, dt = 1.0f / 30; int hits_taken = 0, hits = 0, stuck = 0, won = 0, whip_cd = 0, combo = 0, phases = 0, tells = 0, glanced = 0;
    for (int f = 0; f < 30 * 300 && !won; f++) {
        int dmg = 0; float speed = 10, py = world_height(&w, px, pz);   /* a dragon running (ZR) */
        if (b.state == B_CHARGE || (b.state == B_WINDUP && b.atk == BA_CHARGE && b.t > b.tell - 0.2f)) { float sx = -b.cz, sz = b.cx; if (b.state == B_WINDUP) { sx = -cosf(b.yaw); sz = sinf(b.yaw); } px += sx * speed * dt; pz += sz * speed * dt; }   /* sidestep */
        else if (boss_vulnerable(&b)) { Vec3 c = boss_core(&b); float dx = c.x - px, dz = c.z - pz, d = sqrtf(dx * dx + dz * dz); if (d > 1.5f) { px += dx / d * speed * dt; pz += dz / d * speed * dt; } else if (whip_cd <= 0) { dmg = (combo % 3 == 2) ? 3 : 1; combo++; whip_cd = 12; } }
        else if (b.state == B_WINDUP && (b.atk == BA_SWIPE)) { float dx = px - b.x, dz = pz - b.z, d = sqrtf(dx * dx + dz * dz) + 1e-3f; px += dx / d * speed * dt; pz += dz / d * speed * dt; }   /* back off from a swipe */
        else { float dx = tx - px, dz = tz - pz, d = sqrtf(dx * dx + dz * dz); if (d > 0.5f) { px += dx / d * speed * dt; pz += dz / d * speed * dt; } }
        for (int i = 0; i < 2; i++) if (b.ring_r[i] > 0) { float rx = px - b.ring_x, rz = pz - b.ring_z; if (fabsf(sqrtf(rx * rx + rz * rz) - b.ring_r[i]) < 3) py += 2.5f; }   /* jump the shock wave */
        if (whip_cd > 0) whip_cd--;
        int ev = boss_update(&b, &w, dt, px, py, pz, 1.5f, dmg, 0);
        if (ev & 2) hits_taken++; if (ev & 16) stuck++; if (ev & 4) hits++; if (ev & 2048) phases++; if (ev & 128) tells++; if (ev & 8192) glanced++;
        if (ev & 8) { won = 1; printf("defeated after %.0f s: %d tells, %d times stuck, %d hits, %d phases, %d hits taken\n", f * dt, tells, stuck, hits, phases, hits_taken); } }
    if (!won) { printf("FAIL: could not beat Ironhorn (state %d hp %d, stuck %d, hits %d, hits taken %d)\n", b.state, b.hp, stuck, hits, hits_taken); return 1; }
    if (phases < 2) { printf("FAIL: it never got angrier (%d phases)\n", phases); return 1; }
    for (int f = 0; f < 30 * 4; f++) boss_update(&b, &w, dt, px, 0, pz, 1.5f, 0, 0);
    if (b.cage_open < 0.99f) { printf("FAIL: cage did not open\n"); return 1; }
    /* hits outside an opening glance off */
    boss_init(&b, &w); b.state = B_STALK; b.woke = 1; { int ev = boss_update(&b, &w, dt, b.x + 2, b.y, b.z, 1.5f, 2, 0); if (!(ev & 8192) || b.hp != b.max_hp) { printf("FAIL: a hit outside an opening did damage\n"); return 1; } }
    /* roar: does nothing while it prowls, but breaks its tell */
    boss_init(&b, &w); int stunned = 0, shrug = 0, roared = 0;
    for (int f = 0; f < 30 * 10 && !stunned; f++) { float gx = b.x + 6, gz = b.z; int r = 0; if (b.state == B_STALK && f == 40) r = 1; if (b.state == B_WINDUP && !roared) { r = 1; roared = 1; }
        int ev = boss_update(&b, &w, dt, gx, b.y, gz, 1.5f, 0, r); if (ev & 4096) shrug = 1; if (ev & 1024) stunned = 1; }
    if (!stunned) { printf("FAIL: roar during the tell did not stun\n"); return 1; }
    if (!shrug) { printf("FAIL: roar while prowling was not shrugged off\n"); return 1; }
    /* a bolt hit back hurts it */
    boss_init(&b, &w); { int back = 0, hurt = 0; float qx = w.arena_x - 16, qz = w.arena_z; float qy = world_height(&w, qx, qz) + 8;   /* flying above it: it shoots */
        for (int f = 0; f < 30 * 20 && !hurt; f++) { b.swinging = 0; for (int i = 0; i < BOSS_SHOTS; i++) if (b.shot[i].on && !b.shot[i].back) { float sx = qx - b.shot[i].x, sz = qz - b.shot[i].z; if (sx * sx + sz * sz < 9) b.swinging = 1; }
            int ev = boss_update(&b, &w, dt, qx, qy, qz, 1.5f, 0, 0); if (ev & 16384) back++; if ((ev & 4) && back) hurt = 1; }
        if (!hurt) { printf("FAIL: hitting a bolt back did not hurt it (%d sent back, hp %d)\n", back, b.hp); return 1; }
        printf("bolt hit back: hp %d of %d\n", b.hp, b.max_hp); }
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
