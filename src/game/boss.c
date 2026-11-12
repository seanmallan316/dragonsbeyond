// boss.c - see boss.h
#include "boss.h"
#include <math.h>
#include <string.h>
static float wrapa(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static int room_behind(const Boss *b, const World *w, float ux, float uz) { float cx = b->x + ux * 0.3f - w->arena_x, cz = b->z + uz * 0.3f - w->arena_z; return cx * cx + cz * cz < (w->arena_r - 3) * (w->arena_r - 3); }
static void set(Boss *b, int s) { b->state = s; b->t = 0; }
void boss_init(Boss *b, const World *w) { memset(b, 0, sizeof *b); b->hp = BOSS_HP; b->x = w->arena_x + 4; b->z = w->arena_z + 4; b->y = world_height(w, b->x, b->z); b->yaw = 3.9f; b->core_fwd = -0.6f; b->core_h = 2.6f; b->max_speed = 13; }
Vec3 boss_core(const Boss *b) { return v3(b->x + sinf(b->yaw) * b->core_fwd, b->y + b->core_h, b->z + cosf(b->yaw) * b->core_fwd); }
int boss_vulnerable(const Boss *b) { return b->state == B_STUCK || b->state == B_STUNNED; }
int boss_update(Boss *b, const World *w, float dt, float px, float py, float pz, float reach, int whip, int roar) {
    int ev = 0; b->t += dt;
    if (b->state == B_DOWN) { if (b->t > 1.5f) b->cage_open = fminf(1, b->cage_open + dt * 0.5f); b->speed = 0; return 0; }
    float dx = px - b->x, dz = pz - b->z, dist = sqrtf(dx * dx + dz * dz);
    float ax = px - w->arena_x, az = pz - w->arena_z; int in_arena = ax * ax + az * az < (w->arena_r + 6) * (w->arena_r + 6);
    int low = py - b->y < 4.5f;   /* a dragon flying high is out of its reach */
    if (b->state == B_SLEEP) { if (in_arena) { set(b, B_STALK); b->woke = 1; ev |= 1; } b->speed = 0; }
    else if (b->state == B_STALK) {
        float want = atan2f(dx, dz); b->yaw += wrapa(want - b->yaw) * (1 - expf(-dt * 3)); b->speed = 0;
        if (!in_arena) {   /* the dragon left: walk back to the middle and wait */
            float hx = w->arena_x - b->x, hz = w->arena_z - b->z, hl = sqrtf(hx * hx + hz * hz);
            if (hl > 2) { b->x += hx / hl * 4 * dt; b->z += hz / hl * 4 * dt; b->yaw = atan2f(hx, hz); b->speed = 4; } b->t = 0; }
        else if (dist < 9 && dist > 0.01f) {   /* too close for a charge: back away for a run-up, or toward the middle when its back is to the edge */
            if (room_behind(b, w, -dx / dist, -dz / dist)) { b->x -= dx / dist * 5 * dt; b->z -= dz / dist * 5 * dt; }
            else { float mx = w->arena_x - b->x, mz = w->arena_z - b->z, ml = sqrtf(mx * mx + mz * mz) + 1e-3f; float sx = -dz / dist, sz = dx / dist; if (sx * mx + sz * mz < 0) { sx = -sx; sz = -sz; }
                   b->x += sx * 5 * dt + mx / ml * 2 * dt; b->z += sz * 5 * dt + mz / ml * 2 * dt; }
            b->speed = -5; if (b->t > 1.0f) b->t = 1.0f; }
        else if (b->t > 1.8f && low && fabsf(wrapa(want - b->yaw)) < 0.25f) { b->cx = sinf(b->yaw); b->cz = cosf(b->yaw); set(b, B_CHARGE); ev |= 64; }
        if (roar && dist < 14 && b->t > 0.3f) { set(b, B_STUNNED); ev |= 32; } }
    else if (b->state == B_CHARGE) {
        b->speed = fminf(b->max_speed, b->speed + 22 * dt); b->x += b->cx * b->speed * dt; b->z += b->cz * b->speed * dt;
        float want = atan2f(dx, dz); float steer = wrapa(want - atan2f(b->cx, b->cz)); if (b->t < 0.35f && fabsf(steer) < 0.6f) { float a = atan2f(b->cx, b->cz) + steer * dt * 0.8f; b->cx = sinf(a); b->cz = cosf(a); }   /* homes in only at the very start */
        b->yaw = atan2f(b->cx, b->cz);
        if (dist < reach + 1.3f && low) { ev |= 2; b->speed = 0; if (++b->tired >= 1) { b->tired = 0; set(b, B_STUNNED); ev |= 32; } else { set(b, B_STALK); b->t = -1.2f; } return ev; }   /* one hit per charge, then it backs off a moment */
        for (int i = 0; i < w->narena_tree; i++) { const Prop *p = &w->prop[w->arena_tree[i]]; float tx = p->x - b->x, tz = p->z - b->z;
            if (tx * tx + tz * tz < 2.6f * 2.6f) { set(b, B_STUCK); b->tired = 0; b->speed = 0; ev |= 16; break; } }
        float cx = b->x - w->arena_x, cz = b->z - w->arena_z, cd = sqrtf(cx * cx + cz * cz);
        if (b->state == B_CHARGE && cd > w->arena_r) {   /* a dodged charge ends in the trees round the clearing: antlers stuck */
            b->x = w->arena_x + cx / cd * w->arena_r; b->z = w->arena_z + cz / cd * w->arena_r; set(b, B_STUCK); b->tired = 0; b->speed = 0; ev |= 16; }
        else if (b->state == B_CHARGE && b->t > 3.0f) set(b, B_STALK); }
    else if (b->state == B_STUCK || b->state == B_STUNNED) {
        b->speed = 0;
        Vec3 c = boss_core(b); float hx = px - c.x, hy = py - c.y, hz = pz - c.z;
        float bx = px - b->x, bz = pz - b->z;   /* any attack near the core or anywhere on its body counts */
        if (whip && (hx * hx + hz * hz < (reach + 2.2f) * (reach + 2.2f) || bx * bx + bz * bz < (reach + 3.2f) * (reach + 3.2f)) && fabsf(hy) < reach + 3.5f) { b->hp--; ev |= 4; if (b->hp <= 0) { set(b, B_DOWN); ev |= 8; } else set(b, B_HURT); }
        else if (b->t > (b->state == B_STUCK ? 6.0f : 4.5f)) { if (b->state == B_STUCK) { b->x -= b->cx * 1.5f; b->z -= b->cz * 1.5f; } set(b, B_STALK); } }
    else if (b->state == B_HURT) { b->speed = 0; if (b->t > 1.0f) { b->x -= b->cx * 1.2f; b->z -= b->cz * 1.2f; set(b, B_STALK); } }
    b->y = world_height(w, b->x, b->z);
    return ev; }
