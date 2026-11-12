// world.c - building and running the Everleaf Wilds patch (see world.h).
#include "world.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static unsigned g_rs;
static float frand(void) { g_rs = g_rs * 1664525u + 1013904223u; return (float)((g_rs >> 8) & 0xFFFFFF) / 16777216.0f; }
static float lerp(float a, float b, float t) { return a + (b - a) * t; }
/* smooth value noise */
static float hashf(int x, int y, unsigned s) { unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + s * 982451653u; h = (h ^ (h >> 13)) * 1274126177u; return (float)((h ^ (h >> 16)) & 0xFFFF) / 65535.0f; }
static float vnoise(float x, float y, unsigned s) {
    int xi = (int)floorf(x), yi = (int)floorf(y); float fx = x - xi, fy = y - yi; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    return lerp(lerp(hashf(xi, yi, s), hashf(xi + 1, yi, s), fx), lerp(hashf(xi, yi + 1, s), hashf(xi + 1, yi + 1, s), fx), fy); }
float world_fbm(float x, float y, unsigned s);
static float fbm(float x, float y, unsigned s) { float v = 0, a = 1, f = 1, n = 0; for (int i = 0; i < 4; i++) { v += a * vnoise(x * f, y * f, s + i * 7); n += a; a *= 0.5f; f *= 2.1f; } return v / n; }

static float raw_height(const World *w, float x, float z) {
    float d = sqrtf(x * x + z * z) / (W_SIZE * 0.5f);
    float h = (fbm(x * 0.012f + 3.1f, z * 0.012f + 7.7f, w->seed) - 0.5f) * 14.0f + (fbm(x * 0.05f, z * 0.05f, w->seed + 99) - 0.5f) * 2.2f;
    h += d * d * d * 26.0f;                                     /* the edges rise into hills so the patch feels enclosed */
    float pd = sqrtf((x - w->pond_x) * (x - w->pond_x) + (z - w->pond_z) * (z - w->pond_z));
    if (pd < w->pond_r * 1.6f) { float t = clampf(1 - pd / (w->pond_r * 1.6f), 0, 1); h -= t * t * 5.0f; }   /* the pond's dip */
    float sd = sqrtf(x * x + z * z); if (sd < 18) h = lerp(h, w->pond_y + 1.2f, (1 - sd / 18) * 0.8f);   /* a flat meadow to start on */
    return h; }

void world_make(World *w, unsigned seed) {
    memset(w, 0, sizeof *w); w->seed = seed; g_rs = seed * 7919u + 17;
    w->pond_x = 38; w->pond_z = -30; w->pond_r = 14; w->pond_y = -2.2f;
    for (int j = 0; j < W_GRID; j++) for (int i = 0; i < W_GRID; i++) {
        float x = -W_SIZE / 2 + W_SIZE * i / (W_GRID - 1), z = -W_SIZE / 2 + W_SIZE * j / (W_GRID - 1);
        w->h[j][i] = raw_height(w, x, z); }
    /* the boss clearing: flatten the ground there, ring it with big trees, hang the cage from the biggest */
    w->arena_x = -62; w->arena_z = 60; w->arena_r = 22; w->arena_y = raw_height(w, w->arena_x, w->arena_z);
    for (int j = 0; j < W_GRID; j++) for (int i = 0; i < W_GRID; i++) {
        float x = -W_SIZE / 2 + W_SIZE * i / (W_GRID - 1), z = -W_SIZE / 2 + W_SIZE * j / (W_GRID - 1);
        float d = sqrtf((x - w->arena_x) * (x - w->arena_x) + (z - w->arena_z) * (z - w->arena_z)); float k = clampf((d - w->arena_r) / 10.0f, 0, 1); k = k * k * (3 - 2 * k);
        w->h[j][i] = w->arena_y + (w->h[j][i] - w->arena_y) * k; }
    for (int i = 0; i < 7; i++) { float a = i * 6.2831853f / 7 + 0.3f; Prop *p = &w->prop[w->nprop]; p->kind = P_TREE; p->x = w->arena_x + cosf(a) * (w->arena_r - 2); p->z = w->arena_z + sinf(a) * (w->arena_r - 2);
        p->yaw = a * 2.3f; p->scale = i == 0 ? 1.55f : 1.2f + 0.1f * (i % 3); w->arena_tree[w->narena_tree++] = w->nprop++; }
    { const Prop *ct = &w->prop[w->arena_tree[0]]; float dx = w->arena_x - ct->x, dz = w->arena_z - ct->z, l = sqrtf(dx * dx + dz * dz);
      w->cage_x = ct->x + dx / l * 3.2f; w->cage_z = ct->z + dz / l * 3.2f; w->cage_y = w->arena_y + 6.5f; }
    /* trees and rocks: scattered, denser toward the edges, none in the start meadow or the pond */
    for (int tries = 0; tries < 4000 && w->nprop < W_MAXPROPS; tries++) {
        float x = (frand() - 0.5f) * (W_SIZE - 16), z = (frand() - 0.5f) * (W_SIZE - 16); float d = sqrtf(x * x + z * z);
        if (d < 14) continue;
        if (world_in_pond(w, x, z)) continue;
        { float ax = x - w->arena_x, az = z - w->arena_z; if (ax * ax + az * az < (w->arena_r + 4) * (w->arena_r + 4)) continue; }
        float density = 0.25f + 0.75f * clampf((d - 14) / 90.0f, 0, 1) * fbm(x * 0.03f, z * 0.03f, seed + 5);
        if (frand() > density) continue;
        int ok = 1; for (int k = 0; k < w->nprop; k++) { float dx = w->prop[k].x - x, dz = w->prop[k].z - z; if (dx * dx + dz * dz < 5.5f * 5.5f) { ok = 0; break; } }
        if (!ok) continue;
        float r = frand(); Prop *p = &w->prop[w->nprop++];
        p->kind = r < 0.42f ? P_TREE : (r < 0.56f ? P_PALM : (r < 0.76f ? P_BUSH : (r < 0.88f ? P_FERN : P_ROCK)));
        p->x = x; p->z = z; p->yaw = frand() * 6.2831853f; p->scale = 0.8f + frand() * 0.5f; }
    /* food: berry bushes and mushrooms near the meadow and paths, grubs on the open ground, fish in the pond */
    for (int tries = 0; tries < 3000 && w->nfood < W_MAXFOOD; tries++) {
        float x = (frand() - 0.5f) * (W_SIZE - 30), z = (frand() - 0.5f) * (W_SIZE - 30); float d = sqrtf(x * x + z * z);
        { float ax = x - w->arena_x, az = z - w->arena_z; if (ax * ax + az * az < (w->arena_r + 2) * (w->arena_r + 2)) continue; }
        int inpond = world_in_pond(w, x, z); int kind;
        if (inpond) { if (frand() < 0.5f) continue; kind = F_FISH; }
        else { float r = frand(); kind = r < 0.45f ? F_BERRY : (r < 0.72f ? F_MUSHROOM : F_GRUB); if (d > 95 && frand() < 0.6f) continue; }
        int ok = 1; for (int k = 0; k < w->nfood; k++) { float dx = w->food[k].x - x, dz = w->food[k].z - z; if (dx * dx + dz * dz < 4.0f * 4.0f) { ok = 0; break; } }
        for (int k = 0; k < w->nprop && ok; k++) { float dx = w->prop[k].x - x, dz = w->prop[k].z - z; if (dx * dx + dz * dz < 2.2f * 2.2f) ok = 0; }
        if (!ok) continue;
        Food *f = &w->food[w->nfood++]; f->kind = kind; f->alive = 1; f->x = x; f->z = z; f->y = inpond ? w->pond_y : world_height(w, x, z);
        f->yaw = frand() * 6.2831853f; f->phase = frand() * 6.2831853f; }
    /* the hidden egg crystal: tucked on the high ground at the far side, behind the trees */
    { float best = -1e9f; for (int tries = 0; tries < 400; tries++) { float a = frand() * 6.2831853f, r = 80 + frand() * 22; float x = cosf(a) * r, z = sinf(a) * r; float h = world_height(w, x, z); if (h > best && !world_in_pond(w, x, z)) { best = h; w->crystal_x = x; w->crystal_z = z; } } w->crystal_y = world_height(w, w->crystal_x, w->crystal_z); }
    /* Kaine's drones patrol the middle distance, never the start meadow */
    w->ndrone = 5; for (int i = 0; i < w->ndrone; i++) { Drone *d = &w->drone[i]; float a = i * 1.26f + 0.5f, r = 45 + (i % 2) * 25; d->hx = cosf(a) * r; d->hz = sinf(a) * r; d->x = d->hx; d->z = d->hz; d->y = world_height(w, d->x, d->z) + 9; d->yaw = a; d->t = frand() * 6; }
    /* Shock Hounds guard the outer woods */
    w->nhound = 3; for (int i = 0; i < w->nhound; i++) { Drone *h = &w->hound[i]; float a = i * 2.1f + 1.2f, r = 62 + i * 8; h->hx = cosf(a) * r; h->hz = sinf(a) * r; h->x = h->hx; h->z = h->hz; h->y = world_height(w, h->x, h->z); h->yaw = a; h->t = frand() * 5; }
    /* a few berry bushes right by the start so the first minute teaches eating */
    for (int i = 0; i < 6 && w->nfood < W_MAXFOOD; i++) { float a = i * 1.047f + 0.3f, r = 6 + (i % 2) * 3; Food *f = &w->food[w->nfood++]; f->kind = i % 3 == 2 ? F_MUSHROOM : F_BERRY; f->alive = 1; f->x = cosf(a) * r; f->z = sinf(a) * r; f->y = world_height(w, f->x, f->z); f->yaw = a; }
}

float world_height(const World *w, float x, float z) {
    float fx = (x + W_SIZE / 2) / W_SIZE * (W_GRID - 1), fz = (z + W_SIZE / 2) / W_SIZE * (W_GRID - 1);
    fx = clampf(fx, 0, W_GRID - 1.001f); fz = clampf(fz, 0, W_GRID - 1.001f);
    int i = (int)fx, j = (int)fz; float tx = fx - i, tz = fz - j;
    float h = lerp(lerp(w->h[j][i], w->h[j][i + 1], tx), lerp(w->h[j + 1][i], w->h[j + 1][i + 1], tx), tz);
    return h; }
Vec3 world_normal(const World *w, float x, float z) {
    float e = 0.8f; float hx = world_height(w, x + e, z) - world_height(w, x - e, z), hz = world_height(w, x, z + e) - world_height(w, x, z - e);
    return v3_norm(v3(-hx, 2 * e, -hz)); }
int world_in_pond(const World *w, float x, float z) { float dx = x - w->pond_x, dz = z - w->pond_z; return dx * dx + dz * dz < w->pond_r * w->pond_r; }

int food_is_meat(int kind) { return kind == F_GRUB || kind == F_FISH; }
float food_growth(int kind) { return food_is_meat(kind) ? 30.0f : 10.0f; }   /* meat grows you three times faster */

void world_update(World *w, float dt, float px, float pz) {
    for (int i = 0; i < w->nfood; i++) { Food *f = &w->food[i];
        if (!f->alive) { f->regrow -= dt; if (f->regrow <= 0) { f->alive = 1; if (f->kind == F_GRUB) { f->x += (frand() - 0.5f) * 20; f->z += (frand() - 0.5f) * 20; f->y = world_height(w, f->x, f->z); } } continue; }
        f->phase += dt;
        if (f->kind == F_GRUB || f->kind == F_FISH) {   /* critters wander, and scurry off when the dragon comes close (meat has to be caught) */
            float dx = f->x - px, dz = f->z - pz, d = sqrtf(dx * dx + dz * dz);
            float spd = 0.6f, ang = f->yaw;
            if (d < 7 && d > 0.01f) { ang = atan2f(dx, dz) + sinf(f->phase * 3) * 0.6f; spd = f->kind == F_FISH ? 2.2f : 1.6f; }
            else if (sinf(f->phase * 0.7f) > 0.6f) { ang += dt * 1.5f; }
            f->yaw = ang; float nx = f->x + sinf(ang) * spd * dt, nz = f->z + cosf(ang) * spd * dt;
            int pondnow = world_in_pond(w, f->x, f->z), pondnext = world_in_pond(w, nx, nz);
            if ((f->kind == F_FISH) == pondnext && fabsf(nx) < W_SIZE / 2 - 10 && fabsf(nz) < W_SIZE / 2 - 10) { f->x = nx; f->z = nz; }
            else f->yaw += 2.4f;
            (void)pondnow; f->y = f->kind == F_FISH ? w->pond_y - 0.15f + 0.1f * sinf(f->phase * 2) : world_height(w, f->x, f->z); } }
}
int world_nearest_food(const World *w, float x, float z, float reach) {
    int best = -1; float bd = reach * reach;
    for (int i = 0; i < w->nfood; i++) { const Food *f = &w->food[i]; if (!f->alive) continue; float dx = f->x - x, dz = f->z - z, d = dx * dx + dz * dz; if (d < bd) { bd = d; best = i; } }
    return best; }

static float air_floor(const World *w, float x, float z) { float h = world_height(w, x, z); return w->kind == WK_SKY ? fmaxf(h, W_CLOUDS + 8) : h; }   /* flyers keep above the cloud sea */
static int atk_hits(Attack a, float x, float y, float z, float pad) { if (a.r <= 0) return 0; float dx = x - a.x, dy = y - a.y, dz = z - a.z; float r = a.r + pad; return dx * dx + dz * dz < r * r && fabsf(dy) < r + 0.5f; }
int world_drones(World *w, float dt, float px, float py, float pz, float reach, Attack atk, int roar) {
    int ev = 0;
    for (int i = 0; i < w->ndrone; i++) { Drone *d = &w->drone[i]; d->t += dt; d->flap += dt * (d->state == 1 ? 14 : 9);
        float dx = px - d->x, dz = pz - d->z, dist = sqrtf(dx * dx + dz * dz), dy = py - d->y;
        float gy = air_floor(w, d->x, d->z);
        if (d->state == 3) { if (d->y > gy + 0.2f) { d->y -= 6 * dt; d->yaw += 5 * dt; } if (d->t > 25) { d->state = 0; d->x = d->hx; d->z = d->hz; d->y = air_floor(w, d->x, d->z) + 9; d->t = 0; } continue; }
        if (roar && dist < 14) { d->state = 2; d->t = 0; }
        if (atk_hits(atk, d->x, d->y, d->z, 0.3f)) { d->state = 3; d->t = 0; ev |= 2; continue; }
        if (d->state == 0) {   /* circle the home spot; when the dragon comes near, stop and wind up */
            float ang = d->t * 0.4f; float tx = d->hx + cosf(ang) * 12, tz = d->hz + sinf(ang) * 12; float ty = air_floor(w, tx, tz) + 8 + 1.5f * sinf(d->t);
            float ddx = tx - d->x, ddz = tz - d->z; d->yaw = atan2f(ddx, ddz); d->x += sinf(d->yaw) * 4.5f * dt; d->z += cosf(d->yaw) * 4.5f * dt; d->y += (ty - d->y) * dt * 2;
            if (dist < 18 && d->t > 3) { d->state = 4; d->t = 0; } }
        else if (d->state == 4) {   /* wind-up: hovers in place facing the dragon, eye flashing red, for over a second */
            d->yaw = atan2f(dx, dz); d->y += (py + 4.5f - d->y) * dt * 1.5f; d->x -= sinf(d->yaw) * 0.8f * dt; d->z -= cosf(d->yaw) * 0.8f * dt;
            if (d->t > 1.3f) { d->state = 1; d->t = 0; } }
        else if (d->state == 1) {   /* dive at the dragon, slow enough to see coming; a hit knocks half a heart, then it climbs away */
            d->yaw = atan2f(dx, dz); float sp = 7.0f; d->x += sinf(d->yaw) * sp * dt; d->z += cosf(d->yaw) * sp * dt; d->y += (py + 0.6f - d->y) * dt * 2;
            if (d->y < gy + 0.5f) d->y = gy + 0.5f;
            if (dist < reach * 0.7f && fabsf(dy) < 1.5f) { ev |= 1; d->state = 2; d->t = 0; }
            if (d->t > 5) { d->state = 2; d->t = 0; } }
        else {   /* flee: climb and head home, then patrol again */
            float ddx = d->hx - d->x, ddz = d->hz - d->z; d->yaw = atan2f(ddx, ddz); d->x += sinf(d->yaw) * 6 * dt; d->z += cosf(d->yaw) * 6 * dt; d->y += (gy + 14 - d->y) * dt * 1.5f;
            if (d->t > 8) { d->state = 0; d->t = 0; } } }
    return ev; }

int world_hounds(World *w, float dt, float px, float py, float pz, float reach, Attack atk, int roar, int in_air) {
    int ev = 0; (void)py;
    for (int i = 0; i < w->nhound; i++) { Drone *h = &w->hound[i]; h->t += dt;
        float dx = px - h->x, dz = pz - h->z, dist = sqrtf(dx * dx + dz * dz);
        if (h->state == 3) { h->flap += dt * 3; if (h->t > 22) { h->state = 0; h->t = 0; } continue; }
        if (roar && dist < 12) { h->state = 2; h->t = 0; }
        if (atk_hits(atk, h->x, h->y + 0.5f, h->z, 0.3f)) { h->state = 3; h->t = 0; ev |= 2; continue; }
        float sp = 0;
        if (h->state == 0) { float ang = h->t * 0.35f; float tx = h->hx + cosf(ang) * 10, tz = h->hz + sinf(ang) * 10; h->yaw = atan2f(tx - h->x, tz - h->z); sp = 2.2f; if (dist < 15 && !in_air) { h->state = 1; h->t = 0; } }
        else if (h->state == 1) { h->yaw = atan2f(dx, dz); sp = 3.4f; if (dist < 3.4f + 0.3f * reach && !in_air) { h->state = 5; h->t = 0; } if (h->t > 12 || dist > 40 || in_air) { h->state = 2; h->t = 0; } }   /* stalks, slower than a running dragonet */
        else if (h->state == 5) { h->yaw = atan2f(dx, dz); sp = 0; if (h->t > 0.8f) { h->state = 6; h->t = 0; } }   /* crouches and crackles: about to lunge */
        else if (h->state == 6) { sp = 8.0f; if (dist < reach * 0.6f && !in_air) { ev |= 1; h->state = 2; h->t = 0; } if (h->t > 0.45f) { h->state = 2; h->t = 0; } }   /* the lunge: a straight dash, dodge it sideways */
        else { h->yaw = atan2f(h->hx - h->x, h->hz - h->z); sp = 4.0f; if (h->t > 6) { h->state = 0; h->t = 0; } }
        float nx = h->x + sinf(h->yaw) * sp * dt, nz = h->z + cosf(h->yaw) * sp * dt;
        if (!world_in_pond(w, nx, nz) && fabsf(nx) < W_SIZE / 2 - 8 && fabsf(nz) < W_SIZE / 2 - 8) { h->x = nx; h->z = nz; }
        h->y = world_height(w, h->x, h->z); h->flap += dt * sp * 2.2f; }
    return ev; }

float world_fbm(float x, float y, unsigned s) { return fbm(x, y, s); }

/* ---------- the Sky Kingdom: floating islands over a sea of cloud, joined by narrow stone bridges ---------- */
typedef struct { float x, z, r, top; } Isle;
static const Isle ISLES[] = {
    {   0,    0, 24, 4 },    /* where you arrive */
    {  58,  -48, 30, 10 },   /* Stormtalon's island: the big one */
    { -46,   38, 18, 14 },
    {  -62,  -30, 15, 8 },
    {   20,   62, 16, 18 },   /* the high island (the egg crystal) */
    {   78,   30, 13, 6 },
    {  -14,  -70, 14, 12 },
    {  -84,   78, 10, 22 } };
#define N_ISLES (int)(sizeof ISLES / sizeof ISLES[0])
static const int BRIDGES[][2] = { {0, 1}, {0, 2}, {0, 3}, {2, 4}, {1, 5}, {3, 6} };   /* pairs of islands joined by a walkway */
#define N_BRIDGES (int)(sizeof BRIDGES / sizeof BRIDGES[0])
static float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
static float sky_height(const World *w, float x, float z) {
    float best = W_VOID, bestm = 0;
    for (int i = 0; i < N_ISLES; i++) { const Isle *I = &ISLES[i];
        float wob = 1 + 0.18f * (fbm(x * 0.05f + i * 3.1f, z * 0.05f, w->seed + 31) - 0.5f) * 2;   /* ragged edges */
        float d = sqrtf((x - I->x) * (x - I->x) + (z - I->z) * (z - I->z)) / (I->r * wob); float m = 1 - d;
        if (m > 0 && m > bestm - 0.05f) { float top = I->top + (fbm(x * 0.04f, z * 0.04f, w->seed + 7) - 0.5f) * 5.0f * smooth01(m * 3);
            float h = W_VOID + (top - W_VOID) * smooth01(m / 0.08f); if (h > best) { best = h; bestm = m; } } }
    for (int b = 0; b < N_BRIDGES; b++) { const Isle *A = &ISLES[BRIDGES[b][0]], *B = &ISLES[BRIDGES[b][1]];
        float ex = B->x - A->x, ez = B->z - A->z, L2 = ex * ex + ez * ez; float t = clampf(((x - A->x) * ex + (z - A->z) * ez) / L2, 0, 1);
        float px = A->x + ex * t, pz = A->z + ez * t; float d = sqrtf((x - px) * (x - px) + (z - pz) * (z - pz));
        if (d < 4.6f) { float top = A->top + (B->top - A->top) * smooth01(t) - 0.3f; float h = W_VOID + (top - W_VOID) * smooth01((4.6f - d) / 0.5f); if (h > best) best = h; } }   /* 9 m wide walkways */
    return best; }
int world_is_void(const World *w, float x, float z) { return world_height(w, x, z) < W_CLOUDS + 4; }
void world_safe_spot(const World *w, float x, float z, float *ox, float *oz) {
    if (w->kind != WK_SKY || !world_is_void(w, x, z)) { *ox = x; *oz = z; return; }
    float bd = 1e9f; *ox = 0; *oz = 0;
    for (int i = 0; i < N_ISLES; i++) { const Isle *I = &ISLES[i]; float dx = x - I->x, dz = z - I->z, d = sqrtf(dx * dx + dz * dz);
        float k = d > 1e-3f ? fminf(1, (I->r * 0.6f) / d) : 0; float sx = I->x + dx * k, sz = I->z + dz * k; float dd = (sx - x) * (sx - x) + (sz - z) * (sz - z);
        if (dd < bd && !world_is_void(w, sx, sz)) { bd = dd; *ox = sx; *oz = sz; } } }
static void make_sky(World *w) {
    w->pond_r = 0; w->pond_x = 1e6f; w->pond_z = 1e6f; w->pond_y = W_VOID;
    for (int j = 0; j < W_GRID; j++) for (int i = 0; i < W_GRID; i++) {
        float x = -W_SIZE / 2 + W_SIZE * i / (W_GRID - 1), z = -W_SIZE / 2 + W_SIZE * j / (W_GRID - 1); w->h[j][i] = sky_height(w, x, z); }
    /* the boss island: flat in the middle, ringed by rock spires; the cage hangs from the tallest */
    const Isle *BI = &ISLES[1]; w->arena_x = BI->x; w->arena_z = BI->z; w->arena_r = 20; w->arena_y = sky_height(w, BI->x, BI->z);
    for (int j = 0; j < W_GRID; j++) for (int i = 0; i < W_GRID; i++) {
        float x = -W_SIZE / 2 + W_SIZE * i / (W_GRID - 1), z = -W_SIZE / 2 + W_SIZE * j / (W_GRID - 1);
        float d = sqrtf((x - w->arena_x) * (x - w->arena_x) + (z - w->arena_z) * (z - w->arena_z)); if (d < w->arena_r + 6 && w->h[j][i] > W_CLOUDS) { float k = smooth01((d - w->arena_r) / 6); w->h[j][i] = w->arena_y + (w->h[j][i] - w->arena_y) * k; } }
    for (int i = 0; i < 7; i++) { float a = i * 6.2831853f / 7 + 0.5f; Prop *p = &w->prop[w->nprop]; p->kind = P_SPIRE; p->x = w->arena_x + cosf(a) * (w->arena_r - 1); p->z = w->arena_z + sinf(a) * (w->arena_r - 1);
        p->yaw = a * 1.7f; p->scale = i == 0 ? 1.5f : 1.0f + 0.15f * (i % 3); w->arena_tree[w->narena_tree++] = w->nprop++; }
    { const Prop *ct = &w->prop[w->arena_tree[0]]; float dx = w->arena_x - ct->x, dz = w->arena_z - ct->z, l = sqrtf(dx * dx + dz * dz);
      w->cage_x = ct->x + dx / l * 3.0f; w->cage_z = ct->z + dz / l * 3.0f; w->cage_y = w->arena_y + 7.0f; }
    /* island plants and rocks: oaks, ferns, bushes, a few spires; nothing on the bridges or in the arena */
    for (int tries = 0; tries < 3000 && w->nprop < W_MAXPROPS; tries++) {
        float x = (frand() - 0.5f) * (W_SIZE - 16), z = (frand() - 0.5f) * (W_SIZE - 16); float h = sky_height(w, x, z);
        if (h < W_CLOUDS + 10) continue; if (sqrtf(x * x + z * z) < 9) continue;
        { float ax = x - w->arena_x, az = z - w->arena_z; if (ax * ax + az * az < (w->arena_r + 3) * (w->arena_r + 3)) continue; }
        int edge = 0; for (int k = 0; k < 6; k++) { float a = k * 1.047f; if (sky_height(w, x + cosf(a) * 4, z + sinf(a) * 4) < W_CLOUDS) { edge = 1; break; } } if (edge) continue;   /* keep off the cliff edges */
        int ok = 1; for (int k = 0; k < w->nprop; k++) { float dx = w->prop[k].x - x, dz = w->prop[k].z - z; if (dx * dx + dz * dz < 6.0f * 6.0f) { ok = 0; break; } } if (!ok) continue;
        if (frand() > 0.55f) continue;
        float r = frand(); Prop *p = &w->prop[w->nprop++];
        p->kind = r < 0.30f ? P_TREE : (r < 0.55f ? P_BUSH : (r < 0.78f ? P_FERN : (r < 0.9f ? P_ROCK : P_SPIRE)));
        p->x = x; p->z = z; p->yaw = frand() * 6.2831853f; p->scale = p->kind == P_SPIRE ? 0.6f + frand() * 0.5f : 0.75f + frand() * 0.45f; }
    /* food on the islands: berries and mushrooms, grubs in the open */
    for (int tries = 0; tries < 3000 && w->nfood < 120; tries++) {
        float x = (frand() - 0.5f) * (W_SIZE - 20), z = (frand() - 0.5f) * (W_SIZE - 20); if (sky_height(w, x, z) < W_CLOUDS + 10) continue;
        { float ax = x - w->arena_x, az = z - w->arena_z; if (ax * ax + az * az < (w->arena_r + 2) * (w->arena_r + 2)) continue; }
        float r = frand(); int kind = r < 0.45f ? F_BERRY : (r < 0.75f ? F_MUSHROOM : F_GRUB);
        Food *f = &w->food[w->nfood++]; f->kind = kind; f->alive = 1; f->x = x; f->z = z; f->y = world_height(w, x, z); f->yaw = frand() * 6.28f; f->phase = frand() * 6.28f; }
    /* the egg crystal: on the high island */
    w->crystal_x = ISLES[4].x + 3; w->crystal_z = ISLES[4].z - 2; w->crystal_y = world_height(w, w->crystal_x, w->crystal_z) + 1.2f;
    /* Feather Drones circle the islands; no Shock Hounds up here */
    w->ndrone = 6; for (int i = 0; i < w->ndrone; i++) { Drone *d = &w->drone[i]; const Isle *I = &ISLES[2 + i % (N_ISLES - 2)];   /* not over Stormtalon's island */ d->hx = I->x; d->hz = I->z; d->x = d->hx; d->z = d->hz; d->y = I->top + 9; d->t = i * 1.3f; }
    w->nhound = 0;
    w->nthief = 4; for (int i = 0; i < w->nthief; i++) { Drone *t = &w->thief[i]; const Isle *I = &ISLES[2 + i]; t->hx = I->x + 3; t->hz = I->z - 2; t->x = t->hx; t->z = t->hz; t->y = world_height(w, t->x, t->z); t->t = i * 2.1f; } }
void world_make_kind(World *w, unsigned seed, int kind) {
    if (kind != WK_SKY) { world_make(w, seed); return; }
    memset(w, 0, sizeof *w); w->seed = seed; w->kind = WK_SKY; g_rs = seed * 7919u + 23; make_sky(w); }
int world_blast(World *w, float x, float y, float z, float dx, float dz, float range, float cos_cone) {
    int n = 0; float dl = sqrtf(dx * dx + dz * dz); if (dl > 1e-4f) { dx /= dl; dz /= dl; }
    for (int pass = 0; pass < 3; pass++) { Drone *arr = pass == 2 ? w->thief : (pass ? w->hound : w->drone); int cnt = pass == 2 ? w->nthief : (pass ? w->nhound : w->ndrone);
        for (int i = 0; i < cnt; i++) { Drone *d = &arr[i]; if (d->state == 3) continue;
            float ex = d->x - x, ey = d->y - y, ez = d->z - z; float dist = sqrtf(ex * ex + ey * ey + ez * ez); if (dist > range) continue;
            if (cos_cone > -1 && dist > 1.0f && (ex * dx + ez * dz) / sqrtf(ex * ex + ez * ez + 1e-6f) < cos_cone) continue;
            if (pass == 2) continue;   /* thieves are handled by world_thieves (they drop what they stole) */
            d->state = 3; d->t = 0; n++; } }
    return n; }
static int thief_step_ok(const World *w, float x, float z) { return !(w->kind == WK_SKY && world_is_void(w, x, z)) && !world_in_pond(w, x, z) && fabsf(x) < W_SIZE / 2 - 4 && fabsf(z) < W_SIZE / 2 - 4; }
int world_thieves(World *w, float dt, float px, float py, float pz, float reach, Attack atk, float carry, float *stolen, float *recovered) {
    int ev = 0; *stolen = 0; *recovered = 0;
    for (int i = 0; i < w->nthief; i++) { Drone *t = &w->thief[i]; t->t += dt;
        float dx = px - t->x, dz = pz - t->z, dist = sqrtf(dx * dx + dz * dz); float sp = 0, yaw = t->yaw;
        if (t->state == 3) { if (t->t > 12) { t->state = 4; t->t = 0; } continue; }
        if (t->state == 4) { if (t->t > 18) { t->state = 0; t->t = 0; t->x = t->hx; t->z = t->hz; t->flap = 0; } continue; }
        if (atk_hits(atk, t->x, t->y + 0.5f, t->z, 0.6f)) {   /* caught! it drops what it stole */
            if (t->flap > 0) { *recovered += t->flap; t->flap = 0; } t->state = 3; t->t = 0; ev |= 2; continue; }
        if (t->state == 0) { float a = t->t * 0.7f + i; float tx = t->hx + cosf(a) * 4, tz = t->hz + sinf(a * 1.3f) * 4; yaw = atan2f(tx - t->x, tz - t->z); sp = 1.5f;
            if (dist < 18 && carry >= 10 && fabsf(py - t->y) < 3) { t->state = 1; t->t = 0; } }
        else if (t->state == 1) { yaw = atan2f(dx, dz); sp = 4.0f;
            if (dist < reach * 0.6f + 0.5f && fabsf(py - t->y) < 2) { t->flap = 10; *stolen += 10; ev |= 1; t->state = 2; t->t = 0; }
            else if (dist > 26 || t->t > 10 || fabsf(py - t->y) > 4) { t->state = 0; t->t = 0; } }
        else if (t->state == 2) { yaw = atan2f(-dx, -dz) + 0.6f * sinf(t->t * 2); sp = 5.5f; if (t->t > 14) { t->state = 4; t->t = 0; } }   /* gone with the food */
        float nx = t->x + sinf(yaw) * sp * dt, nz = t->z + cosf(yaw) * sp * dt;
        if (thief_step_ok(w, nx, nz)) { t->x = nx; t->z = nz; } else t->yaw += 1.5f * dt;
        t->yaw = yaw; t->y = world_height(w, t->x, t->z); }
    return ev; }


/* keep a body of radius r out of the scenery: tree trunks and leafy crowns, palm trunks, rocks and rock spires (bushes and ferns are soft).
   On the ground only sideways pushes (the ground sets the height); in the air the dragon slides round crowns and over rocks.
   Shapes match what is drawn (game.c builds rocks and spires as balls of these sizes; the oak model is ~9 m x scale). Returns 1 if pushed. */
static int push_ball(float *x, float *y, float *z, float cx, float cy, float cz, float rx, float ry, float rz, float r, int air) {
    float dx = (*x - cx) / (rx + r), dy = (*y - cy) / (ry + r), dz = (*z - cz) / (rz + r);
    if (!air) dy = 0;
    float d2 = dx * dx + dy * dy + dz * dz; if (d2 >= 1.0f) return 0;
    float d = sqrtf(d2); if (d < 1e-4f) { dx = 1; dy = 0; dz = 0; d = 1; }
    *x = cx + dx / d * (rx + r); *z = cz + dz / d * (rz + r); if (air) *y = cy + dy / d * (ry + r);
    return 1; }
static int push_post(float *x, float *y, float *z, float cx, float cz, float base, float top, float pr, float r) {
    if (*y > top || *y < base - 2) return 0;
    float dx = *x - cx, dz = *z - cz, R = pr + r, d2 = dx * dx + dz * dz; if (d2 >= R * R) return 0;
    float d = sqrtf(d2); if (d < 1e-4f) { dx = 1; dz = 0; d = 1; }
    *x = cx + dx / d * R; *z = cz + dz / d * R; return 1; }
int world_push_out(const World *w, float *x, float *y, float *z, float r, int air) {
    int hit = 0;
    for (int i = 0; i < w->nprop; i++) { const Prop *p = &w->prop[i]; float dx = *x - p->x, dz = *z - p->z; if (dx * dx + dz * dz > 14 * 14) continue;
        float s = p->scale, gy = world_height(w, p->x, p->z);
        switch (p->kind) {
        case P_TREE: hit |= push_post(x, y, z, p->x, p->z, gy, gy + 5.5f * s, 0.55f * s, r);
                     if (air) hit |= push_ball(x, y, z, p->x, gy + 6.3f * s, p->z, 3.4f * s, 2.4f * s, 3.4f * s, r, 1); break;
        case P_PALM: hit |= push_post(x, y, z, p->x, p->z, gy, gy + 8.5f * s, 0.4f * s, r); break;
        case P_ROCK: hit |= push_ball(x, y, z, p->x, gy - 0.3f * s, p->z, 1.3f * s, 1.35f * s, 1.1f * s, r, air); break;
        case P_SPIRE: { float H = 9.0f * s; hit |= push_ball(x, y, z, p->x, gy + H * 0.45f, p->z, 2.0f * s, H, 1.8f * s, r, air); } break;
        default: break; } }
    return hit; }
