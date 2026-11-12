// boss.c - see boss.h
#include "boss.h"
#include <math.h>
#include <string.h>
static float wrapa(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static int room_behind(const Boss *b, const World *w, float ux, float uz) { float cx = b->x + ux * 0.3f - w->arena_x, cz = b->z + uz * 0.3f - w->arena_z; return cx * cx + cz * cz < (w->arena_r - 3) * (w->arena_r - 3); }
static void set(Boss *b, int s) { b->state = s; b->t = 0; }
void boss_init(Boss *b, const World *w) { memset(b, 0, sizeof *b); b->hp = BOSS_HP; b->x = w->arena_x + 4; b->z = w->arena_z + 4; b->y = world_height(w, b->x, b->z); b->yaw = 3.9f; b->core_fwd = -0.6f; b->core_h = 2.6f; b->max_speed = 13; b->tired_need = 2; b->orbit = 1; b->prowl = 3; }
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
        /* prowl round the dragon at a distance, walking where it looks; then turn to face, paw the ground, and charge */
        float want = atan2f(dx, dz), gx = 0, gz = 0, gs = 0;
        if (!in_arena) { float hx = w->arena_x - b->x, hz = w->arena_z - b->z, hl = sqrtf(hx * hx + hz * hz); if (hl > 2) { gx = hx / hl; gz = hz / hl; gs = 4.5f; } b->t = 0; }
        else if (b->t < b->prowl) {
            float ux = dist > 0.01f ? dx / dist : 0, uz = dist > 0.01f ? dz / dist : 1; float tx = -uz * b->orbit, tz = ux * b->orbit;   /* tangent round the dragon */
            float rad = (dist - 13) * 0.25f; if (rad > 1) rad = 1; if (rad < -1) rad = -1;   /* keep about 13 m away */
            gx = tx + ux * rad; gz = tz + uz * rad; float gl = sqrtf(gx * gx + gz * gz) + 1e-4f; gx /= gl; gz /= gl; gs = dist < 7 ? 7.5f : 5.5f;
            if (!room_behind(b, w, gx, gz)) { b->orbit = -b->orbit; gx = -gx; gz = -gz; } }   /* the clearing's edge: turn round */
        b->vx += (gx * gs - b->vx) * (1 - expf(-dt * 3)); b->vz += (gz * gs - b->vz) * (1 - expf(-dt * 3));
        b->x += b->vx * dt; b->z += b->vz * dt; float sp = sqrtf(b->vx * b->vx + b->vz * b->vz); b->speed = sp;
        float face = (b->t < b->prowl && sp > 0.8f) || !in_arena ? atan2f(b->vx, b->vz) : want; b->yaw += wrapa(face - b->yaw) * (1 - expf(-dt * (b->t < b->prowl ? 4 : 6)));
        if (in_arena && b->t > b->prowl + 0.7f && sp < 1.2f && low && fabsf(wrapa(want - b->yaw)) < 0.2f) {
            b->cx = sinf(b->yaw); b->cz = cosf(b->yaw); set(b, B_CHARGE); b->speed = 2; ev |= 64;
            b->orbit = ((int)(b->x * 7 + b->z * 3) & 1) ? 1 : -1; b->prowl = 1.6f + fmodf(fabsf(b->x * 13.7f + b->z * 5.3f), 2.2f); }
        if (roar && dist < 14 && b->t > 0.3f) { set(b, B_STUNNED); ev |= 32; } }
    else if (b->state == B_CHARGE) {
        float acc = b->t < 0.25f ? 8 : 26; b->speed = fminf(b->max_speed, b->speed + acc * dt);   /* a lunge that builds up, not an instant dash */
        float want = atan2f(dx, dz), head = atan2f(b->cx, b->cz); float steer = wrapa(want - head);
        if (fabsf(steer) < 1.2f && dist > 4) { float turn = (b->t < 0.6f ? 1.6f : 0.55f) * dt; if (steer > turn) steer = turn; if (steer < -turn) steer = -turn; head += steer; b->cx = sinf(head); b->cz = cosf(head); }   /* it curves after the dragon, but too slowly to follow a sidestep */
        b->vx = b->cx * b->speed; b->vz = b->cz * b->speed; b->x += b->vx * dt; b->z += b->vz * dt;
        b->yaw = head;
        if (dist < reach + 1.3f && low) { ev |= 2; if (++b->tired >= b->tired_need) { b->tired = 0; b->vx = b->vz = 0; b->speed = 0; set(b, B_STUNNED); ev |= 32; } else { set(b, B_STALK); b->t = -0.6f; } return ev; }   /* one hit per charge, then it runs on past and turns */
        for (int i = 0; i < w->narena_tree; i++) { const Prop *p = &w->prop[w->arena_tree[i]]; float tx = p->x - b->x, tz = p->z - b->z;
            if (tx * tx + tz * tz < 2.6f * 2.6f) { set(b, B_STUCK); b->tired = 0; b->speed = 0; b->vx = b->vz = 0; ev |= 16; break; } }
        float cx = b->x - w->arena_x, cz = b->z - w->arena_z, cd = sqrtf(cx * cx + cz * cz);
        if (b->state == B_CHARGE && cd > w->arena_r) {   /* a dodged charge ends in the trees round the clearing: antlers stuck */
            b->x = w->arena_x + cx / cd * w->arena_r; b->z = w->arena_z + cz / cd * w->arena_r; set(b, B_STUCK); b->tired = 0; b->speed = 0; b->vx = b->vz = 0; ev |= 16; }
        else if (b->state == B_CHARGE && b->t > 3.0f) set(b, B_STALK); }
    else if (b->state == B_STUCK || b->state == B_STUNNED) {
        b->speed = 0;
        Vec3 c = boss_core(b); float hx = px - c.x, hy = py - c.y, hz = pz - c.z;
        float bx = px - b->x, bz = pz - b->z;   /* any attack near the core or anywhere on its body counts */
        if (whip && (hx * hx + hz * hz < (reach + 2.2f) * (reach + 2.2f) || bx * bx + bz * bz < (reach + 3.2f) * (reach + 3.2f)) && fabsf(hy) < reach + 3.5f) { b->hp--; ev |= 4; if (b->hp <= 0) { set(b, B_DOWN); ev |= 8; } else set(b, B_HURT); }
        else if (b->t > (b->state == B_STUCK ? 6.0f : 4.5f)) { if (b->state == B_STUCK) { b->vx = -b->cx * 5; b->vz = -b->cz * 5; } set(b, B_STALK); } }
    else if (b->state == B_HURT) { float k = b->t < 0.4f ? 3.0f : 0; b->x -= b->cx * k * dt; b->z -= b->cz * k * dt; b->speed = -k; if (b->t > 1.0f) { b->vx = -b->cx * 3; b->vz = -b->cz * 3; set(b, B_STALK); } }   /* staggers back */
    b->y = world_height(w, b->x, b->z);
    return ev; }
