// boss.c - see boss.h
#include "boss.h"
#include "nav.h"
#include <math.h>
#include <string.h>
static float wrapa(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static int room_behind(const Boss *b, const World *w, float ux, float uz) { float cx = b->x + ux * 0.3f - w->arena_x, cz = b->z + uz * 0.3f - w->arena_z; return cx * cx + cz * cz < (w->arena_r - 3) * (w->arena_r - 3); }
static void set(Boss *b, int s) { b->state = s; b->t = 0; }
static float rnd(Boss *b) { b->rng = b->rng * 1664525u + 1013904223u; return (float)((b->rng >> 8) & 0xFFFF) / 65536.0f; }
static void halt(Boss *b) { b->vx = b->vz = 0; b->speed = 0; }
void boss_init(Boss *b, const World *w) {
    memset(b, 0, sizeof *b); b->hp = b->max_hp = BOSS_HP; b->x = w->arena_x + 4; b->z = w->arena_z + 4; b->y = world_height(w, b->x, b->z); b->yaw = 3.9f;
    b->core_fwd = -0.6f; b->core_h = 2.6f; b->max_speed = 13; b->tired_need = 2; b->orbit = 1; b->prowl = 3; b->aggro = 1; b->rng = 12345u + (unsigned)(w->arena_x * 31 + w->arena_z * 17);
    b->ring_r[0] = b->ring_r[1] = -1000; }
Vec3 boss_core(const Boss *b) { return v3(b->x + sinf(b->yaw) * b->core_fwd, b->y + b->core_h, b->z + cosf(b->yaw) * b->core_fwd); }
int boss_vulnerable(const Boss *b) { return b->state == B_STUCK || b->state == B_STUNNED || (b->state == B_STRIKE && b->open_t > 0); }
static float phase_mul(const Boss *b) { return (b->phase == 0 ? 1.0f : b->phase == 1 ? 0.85f : 0.72f) / sqrtf(b->aggro); }
static void stun(Boss *b, float secs) { set(b, B_STUNNED); b->open_t = secs; b->poise = 0; halt(b); }
/* damage. open = it was open to the hit (counts towards breaking free, a heavy hit staggers); otherwise a chip */
static int take_hit(Boss *b, int dmg, float dirx, float dirz, int open) {
    int ev = 4; b->hp -= dmg; b->tired = 0;
    if (b->hp <= 0) { b->hp = 0; set(b, B_DOWN); halt(b); for (int i = 0; i < BOSS_SHOTS; i++) b->shot[i].on = 0; return ev | 8; }
    int ph = b->hp * 3 <= b->max_hp ? 2 : b->hp * 3 <= b->max_hp * 2 ? 1 : 0;
    if (ph > b->phase) { b->phase = ph; set(b, B_PHASE); halt(b); b->open_t = 0; b->poise = 0; return ev | 2048; }   /* angrier: rears up and roars */
    if (!open) return ev;
    b->cx = dirx; b->cz = dirz; b->poise++;
    int cap = b->state == B_STUCK ? 4 : 3;
    if (dmg >= 3) { set(b, B_HURT); b->atk = b->poise >= cap ? 99 : -1; b->open_t = 0; }   /* a heavy hit knocks it back */
    else if (b->poise >= cap) { set(b, B_HURT); b->atk = 99; b->open_t = 0; }   /* enough: it shakes you off */
    return ev; }
static void pick_attack(Boss *b, float dist, int low) {
    float r = rnd(b); int a;
    if (!low) a = BA_VOLLEY;   /* a dragon flying above it: it shoots */
    else if (dist < 7) a = r < 0.6f ? BA_SWIPE : BA_SLAM;
    else if (dist < 16) a = r < 0.4f ? BA_SLAM : r < 0.8f ? BA_CHARGE : BA_VOLLEY;
    else a = r < 0.55f ? BA_CHARGE : BA_VOLLEY;
    static const float TELL[4] = {0.95f, 1.05f, 0.55f, 0.8f};
    b->atk = a; b->tell = TELL[a] * phase_mul(b); b->hit_done = 0; set(b, B_WINDUP); }
static void fire(Boss *b, float px, float py, float pz, int k) {
    for (int i = 0; i < BOSS_SHOTS; i++) if (!b->shot[i].on) { BossShot *s = &b->shot[i];
        float fx = sinf(b->yaw), fz = cosf(b->yaw); s->x = b->x + fx * 2.4f; s->z = b->z + fz * 2.4f; s->y = b->y + 2.8f;
        float dx = px - s->x, dy = py + 0.8f - s->y, dz = pz - s->z, l = sqrtf(dx * dx + dy * dy + dz * dz) + 1e-4f;
        float spread = (k % 3 == 1 ? 0.16f : k % 3 == 2 ? -0.16f : 0), sp = 14.0f + 1.5f * b->aggro + 1.0f * b->phase;
        float a = atan2f(dx, dz) + spread, hz = sqrtf(dx * dx + dz * dz) / l;
        s->vx = sinf(a) * hz * sp; s->vz = cosf(a) * hz * sp; s->vy = dy / l * sp; s->life = 3.2f; s->on = 1; s->back = 0; return; } }
int boss_power_hit(Boss *b, int dmg, int big) {
    if (b->state == B_DOWN || b->state == B_SLEEP || b->state == B_PHASE || dmg <= 0) return 0;
    float fx = -sinf(b->yaw), fz = -cosf(b->yaw);
    if (boss_vulnerable(b)) return take_hit(b, dmg, -fx, -fz, 1);
    int ev = take_hit(b, big ? (dmg + 1) / 2 : (dmg + 2) / 3, fx, fz, 0);   /* not open: the blast only chips it */
    if (ev & (8 | 2048)) return ev;
    if (b->state == B_WINDUP || big) { stun(b, big ? 3.0f : 2.2f); ev |= 32 | (b->state == B_WINDUP ? 1024 : 0); }   /* a blast in the face breaks its tell; a mega blast floors it */
    return ev; }
int boss_stagger(Boss *b, float secs) {
    if (b->state == B_STALK || b->state == B_WINDUP || b->state == B_STRIKE || b->state == B_CHARGE) { stun(b, secs); return 32; }
    return 0; }
int boss_update(Boss *b, const World *w, float dt, float px, float py, float pz, float reach, int dmg, int roar) {
    int ev = 0; b->t += dt; if (b->roar_cd > 0) b->roar_cd -= dt;
    int roar_new = roar && !b->roar_prev; b->roar_prev = roar;
    if (b->state == B_DOWN) { if (b->t > 1.5f) b->cage_open = fminf(1, b->cage_open + dt * 0.5f); b->speed = 0; b->ring_r[0] = b->ring_r[1] = -1000; return 0; }
    float dx = px - b->x, dz = pz - b->z, dist = sqrtf(dx * dx + dz * dz), ux = dist > 0.01f ? dx / dist : 0, uz = dist > 0.01f ? dz / dist : 1;
    float ax = px - w->arena_x, az = pz - w->arena_z; int in_arena = ax * ax + az * az < (w->arena_r + 6) * (w->arena_r + 6);
    float pg = world_height(w, px, pz);
    int low = py - b->y < 4.5f;   /* a dragon flying high is out of reach of its horns */
    float want = atan2f(dx, dz);
    /* the dragon's attack: on the boss's body or core? */
    Vec3 c = boss_core(b); float hx = px - c.x, hy = py - c.y, hz = pz - c.z;
    int touching = (hx * hx + hz * hz < (reach + 2.2f) * (reach + 2.2f) || dist * dist < (reach + 3.2f) * (reach + 3.2f)) && fabsf(hy) < reach + 3.5f;
    if (dmg > 0 && touching && b->state != B_SLEEP) {
        if (boss_vulnerable(b)) ev |= take_hit(b, dmg, ux, uz, 1);
        else ev |= 8192;   /* glances off: wait for an opening */
        if (b->state == B_DOWN) return ev; }
    if (roar_new && b->woke) {
        if (b->state == B_WINDUP && dist < 18 && b->roar_cd <= 0) { stun(b, 3.0f); b->roar_cd = 9; ev |= 1024 | 32; }
        else if (dist < 24 && b->state != B_STUNNED && b->state != B_STUCK) ev |= 4096; }
    if (b->state == B_SLEEP) { if (in_arena) { set(b, B_STALK); b->woke = 1; ev |= 1; } b->speed = 0; }
    else if (b->state == B_STALK) {
        /* prowl round the dragon at a distance, walking where it looks; then pick an attack and show its tell */
        float gx = 0, gz = 0, gs = 0;
        if (!in_arena) { float qx = w->arena_x - b->x, qz = w->arena_z - b->z, hl = sqrtf(qx * qx + qz * qz), sx, sz;   /* back to its clearing, round the trees on the way */
            if (hl > 6 && nav_steer(0, b->x, b->y, b->z, w->arena_x, w->arena_y, w->arena_z, &sx, &sz)) { qx = sx - b->x; qz = sz - b->z; hl = sqrtf(qx * qx + qz * qz); }
            if (hl > 2) { gx = qx / hl; gz = qz / hl; gs = 4.5f; } b->t = 0; }
        else if (b->t < b->prowl) {
            float tx = -uz * b->orbit, tz = ux * b->orbit;   /* tangent round the dragon */
            float keep = b->phase == 2 ? 9 : 12, rad = (dist - keep) * 0.25f; if (rad > 1) rad = 1; if (rad < -1) rad = -1;
            gx = tx + ux * rad; gz = tz + uz * rad; float gl = sqrtf(gx * gx + gz * gz) + 1e-4f; gx /= gl; gz /= gl; gs = (dist < 7 ? 7.0f : 5.0f) * (1 + 0.12f * b->phase);
            if (!room_behind(b, w, gx, gz)) { b->orbit = -b->orbit; gx = -gx; gz = -gz; } }   /* the clearing's edge: turn round */
        b->vx += (gx * gs - b->vx) * (1 - expf(-dt * 3)); b->vz += (gz * gs - b->vz) * (1 - expf(-dt * 3));
        b->x += b->vx * dt; b->z += b->vz * dt; float sp = sqrtf(b->vx * b->vx + b->vz * b->vz); b->speed = sp;
        float face = (b->t < b->prowl && sp > 0.8f) || !in_arena ? atan2f(b->vx, b->vz) : want; b->yaw += wrapa(face - b->yaw) * (1 - expf(-dt * (b->t < b->prowl ? 4 : 6)));
        if (in_arena && b->t > b->prowl + 0.3f && fabsf(wrapa(want - b->yaw)) < 0.5f) {
            pick_attack(b, dist, low); ev |= 128;
            b->orbit = rnd(b) < 0.5f ? 1 : -1; b->prowl = (1.2f + 1.8f * rnd(b)) / b->aggro * (1 - 0.18f * b->phase); } }
    else if (b->state == B_WINDUP) {   /* the tell: it stops, turns to face the dragon and gathers itself */
        b->vx *= expf(-dt * 6); b->vz *= expf(-dt * 6); b->x += b->vx * dt; b->z += b->vz * dt; b->speed = sqrtf(b->vx * b->vx + b->vz * b->vz);
        int commit = b->atk == BA_CHARGE && b->t > b->tell - 0.25f;   /* the last moment before a charge: it has picked its line */
        if (!commit) b->yaw += wrapa(want - b->yaw) * (1 - expf(-dt * (b->atk == BA_SWIPE ? 4.0f : 7.0f)));
        if (b->t >= b->tell) {
            if (b->atk == BA_CHARGE) { b->cx = sinf(b->yaw); b->cz = cosf(b->yaw); set(b, B_CHARGE); b->speed = 2; ev |= 64; }
            else { set(b, B_STRIKE); b->open_t = 0; b->shots_left = b->atk == BA_VOLLEY ? 3 + 2 * b->phase + (b->aggro > 1.15f) : 0; b->hit_done = 0;
                if (b->atk == BA_SLAM) { b->ring_x = b->x; b->ring_z = b->z; b->ring_r[0] = 1.5f; b->ring_hit[0] = 0; ev |= 256;
                    if (b->phase >= 2) { b->ring_r[1] = -6.0f; b->ring_hit[1] = 0; } } } } }
    else if (b->state == B_STRIKE) {
        if (b->open_t > 0) { b->open_t -= dt; if (b->open_t < 0) b->open_t = 0; }
        b->speed = 0;
        if (b->atk == BA_SLAM) {
            if (b->t - dt < 0.4f && b->t >= 0.4f) b->open_t = 1.5f;   /* after the slam it is out of breath */
            if (b->t > 0.4f && b->open_t <= 0) { set(b, B_STALK); b->poise = 0; } }
        else if (b->atk == BA_SWIPE) {
            if (b->t < 0.3f) { float fx = sinf(b->yaw), fz = cosf(b->yaw); b->x += fx * 7 * dt; b->z += fz * 7 * dt; b->speed = 7; }
            if (!b->hit_done && b->t > 0.08f && b->t < 0.32f && dist < 4.8f + 0.5f * reach && low && fabsf(wrapa(want - b->yaw)) < 1.1f) { b->hit_done = 1; ev |= 2; b->tired++; }
            if (b->t - dt < 0.38f && b->t >= 0.38f) b->open_t = 0.9f;
            if (b->t > 0.38f && b->open_t <= 0) { set(b, B_STALK); b->poise = 0; } }
        else {   /* volley: a burst of bolts at the dragon */
            b->yaw += wrapa(want - b->yaw) * (1 - expf(-dt * 3));
            float gap = 0.24f * phase_mul(b); int k = 0;
            while (b->shots_left > 0 && b->t >= gap * (float)b->hit_done) { fire(b, px, py, pz, b->hit_done); b->hit_done++; b->shots_left--; ev |= 512; if (++k > 3) break; }
            if (b->shots_left <= 0 && b->t > gap * (float)b->hit_done + 0.5f) set(b, B_STALK); } }
    else if (b->state == B_CHARGE) {
        float acc = b->t < 0.25f ? 8 : 26; b->speed = fminf(b->max_speed * (1 + 0.1f * b->phase), b->speed + acc * dt);   /* a lunge that builds up, not an instant dash */
        float head = atan2f(b->cx, b->cz); float steer = wrapa(want - head);
        if (fabsf(steer) < 1.2f && dist > 4) { float turn = (b->t < 0.6f ? 1.6f : 0.55f) * dt; if (steer > turn) steer = turn; if (steer < -turn) steer = -turn; head += steer; b->cx = sinf(head); b->cz = cosf(head); }   /* it curves after the dragon, but too slowly to follow a sidestep */
        b->vx = b->cx * b->speed; b->vz = b->cz * b->speed; b->x += b->vx * dt; b->z += b->vz * dt;
        b->yaw = head;
        if (dist < reach + 1.3f && low) { ev |= 2; if (++b->tired >= b->tired_need) { b->tired = 0; stun(b, 4.0f); ev |= 32; } else { set(b, B_STALK); b->t = -0.6f; } }   /* one hit per charge, then it runs on past and turns */
        else {
            for (int i = 0; i < w->narena_tree; i++) { const Prop *p = &w->prop[w->arena_tree[i]]; float tx = p->x - b->x, tz = p->z - b->z;
                if (tx * tx + tz * tz < 2.6f * 2.6f) { set(b, B_STUCK); b->tired = 0; b->poise = 0; halt(b); ev |= 16; break; } }
            float cx = b->x - w->arena_x, cz = b->z - w->arena_z, cd = sqrtf(cx * cx + cz * cz);
            if (b->state == B_CHARGE && cd > w->arena_r) {   /* a dodged charge ends in the trees round the clearing: antlers stuck */
                b->x = w->arena_x + cx / cd * w->arena_r; b->z = w->arena_z + cz / cd * w->arena_r; set(b, B_STUCK); b->tired = 0; b->poise = 0; halt(b); ev |= 16; }
            else if (b->state == B_CHARGE && b->t > 3.0f) set(b, B_STALK); } }
    else if (b->state == B_STUCK) { b->speed = 0; if (b->t > 5.5f - 0.7f * b->phase) { b->vx = -b->cx * 5; b->vz = -b->cz * 5; set(b, B_STALK); b->poise = 0; } }
    else if (b->state == B_STUNNED) { b->speed = 0; b->open_t -= dt; if (b->open_t <= 0) { b->open_t = 0; set(b, B_STALK); b->poise = 0; } }
    else if (b->state == B_HURT) {   /* staggers back from the dragon */
        float k = b->t < 0.4f ? 3.0f : 0; b->x -= b->cx * k * dt; b->z -= b->cz * k * dt; b->speed = -k;
        if (b->t > 1.0f) { if (b->atk == 99) { b->ring_x = b->x; b->ring_z = b->z; b->ring_r[0] = 1.5f; b->ring_hit[0] = 0; ev |= 256 | 32768; }   /* breaks free with a shock wave: get back! */
            b->vx = -b->cx * 3; b->vz = -b->cz * 3; set(b, B_STALK); b->poise = 0; b->atk = 0; } }
    else if (b->state == B_PHASE) {   /* angrier: rears up, roars, and a shock wave rolls out */
        b->speed = 0;
        if (b->t - dt < 1.4f && b->t >= 1.4f) { b->ring_x = b->x; b->ring_z = b->z; b->ring_r[0] = 1.5f; b->ring_hit[0] = 0; ev |= 256; }
        if (b->t > 1.9f) { set(b, B_STALK); b->t = -0.3f; b->prowl = 1.0f; } }
    /* shock waves rolling along the ground: jump or fly over them, or dodge through */
    for (int i = 0; i < 2; i++) if (b->ring_r[i] > -100) {
        float was = b->ring_r[i]; b->ring_r[i] += 13.0f * (1 + 0.1f * b->phase) * dt;
        if (was < 0 && b->ring_r[i] >= 0) { b->ring_x = b->x; b->ring_z = b->z; ev |= 256; }
        if (b->ring_r[i] > 0) { float rx = px - b->ring_x, rz = pz - b->ring_z, rd = sqrtf(rx * rx + rz * rz);
            if (!b->ring_hit[i] && fabsf(b->ring_r[i] - rd) < 1.2f && py - pg < 1.4f) { b->ring_hit[i] = 1; ev |= 2; b->tired++; } }
        if (b->ring_r[i] > BOSS_RING_MAX) b->ring_r[i] = -1000; }
    /* power bolts */
    for (int i = 0; i < BOSS_SHOTS; i++) { BossShot *s = &b->shot[i]; if (!s->on) continue;
        s->life -= dt; if (s->life <= 0) { s->on = 0; continue; }
        if (s->back) { Vec3 q = boss_core(b); float qx = q.x - s->x, qy = q.y - s->y, qz = q.z - s->z, l = sqrtf(qx * qx + qy * qy + qz * qz) + 1e-4f;
            s->vx = qx / l * 26; s->vy = qy / l * 26; s->vz = qz / l * 26;
            if (l < 2.6f) { s->on = 0; if (b->state != B_PHASE) { ev |= take_hit(b, 3, -s->vx / 26, -s->vz / 26, 0); if (b->state == B_DOWN) return ev;
                if (b->state != B_PHASE) { stun(b, 2.0f); ev |= 32; } } continue; } }
        else if (b->phase >= 2) {   /* angry: the bolts curve after the dragon a little */
            float tx = px - s->x, tz = pz - s->z, a = atan2f(tx, tz), va = atan2f(s->vx, s->vz), hs = sqrtf(s->vx * s->vx + s->vz * s->vz), d = wrapa(a - va), m = 0.7f * dt;
            if (d > m) d = m; if (d < -m) d = -m; va += d; s->vx = sinf(va) * hs; s->vz = cosf(va) * hs; }
        s->x += s->vx * dt; s->y += s->vy * dt; s->z += s->vz * dt;
        if (!s->back) {
            float sx = px - s->x, sy = py + 0.6f - s->y, sz = pz - s->z, d2 = sx * sx + sy * sy + sz * sz;
            if (b->swinging && d2 < (reach + 1.8f) * (reach + 1.8f)) { s->back = 1; s->life = 3; ev |= 16384; continue; }   /* hit it back! */
            float hr = 0.9f + 0.35f * reach; if (d2 < hr * hr) { s->on = 0; ev |= 2; b->tired++; continue; }
            if (s->y < world_height(w, s->x, s->z) - 0.2f) s->on = 0; } }
    if (b->state != B_STUNNED && b->state != B_STUCK && b->tired >= b->tired_need + 1 && b->state == B_STALK) { b->tired = 0; stun(b, 4.0f); ev |= 32; }   /* a young player who keeps getting hit: it tires itself out */
    b->y = world_height(w, b->x, b->z);
    return ev; }
