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

int world_drones(World *w, float dt, float px, float py, float pz, float reach, int whip, int roar) {
    int ev = 0;
    for (int i = 0; i < w->ndrone; i++) { Drone *d = &w->drone[i]; d->t += dt; d->flap += dt * (d->state == 1 ? 14 : 9);
        float dx = px - d->x, dz = pz - d->z, dist = sqrtf(dx * dx + dz * dz), dy = py - d->y;
        float gy = world_height(w, d->x, d->z);
        if (d->state == 3) { if (d->y > gy + 0.2f) { d->y -= 6 * dt; d->yaw += 5 * dt; } if (d->t > 25) { d->state = 0; d->x = d->hx; d->z = d->hz; d->y = world_height(w, d->x, d->z) + 9; d->t = 0; } continue; }
        if (roar && dist < 14) { d->state = 2; d->t = 0; }
        if (whip && dist < reach && fabsf(dy) < reach) { d->state = 3; d->t = 0; ev |= 2; continue; }
        if (d->state == 0) {   /* circle the home spot, dive when the dragon comes near */
            float ang = d->t * 0.5f; float tx = d->hx + cosf(ang) * 12, tz = d->hz + sinf(ang) * 12; float ty = world_height(w, tx, tz) + 8 + 1.5f * sinf(d->t);
            float ddx = tx - d->x, ddz = tz - d->z; d->yaw = atan2f(ddx, ddz); d->x += sinf(d->yaw) * 7 * dt; d->z += cosf(d->yaw) * 7 * dt; d->y += (ty - d->y) * dt * 2;
            if (dist < 16 && d->t > 3) { d->state = 1; d->t = 0; } }
        else if (d->state == 1) {   /* dive at the dragon; a hit knocks half a heart, then it climbs away */
            d->yaw = atan2f(dx, dz); float sp = 13; d->x += sinf(d->yaw) * sp * dt; d->z += cosf(d->yaw) * sp * dt; d->y += (py + 0.6f - d->y) * dt * 3;
            if (d->y < gy + 0.5f) d->y = gy + 0.5f;
            if (dist < reach * 0.7f && fabsf(dy) < 1.5f) { ev |= 1; d->state = 2; d->t = 0; }
            if (d->t > 6) { d->state = 2; d->t = 0; } }
        else {   /* flee: climb and head home, then patrol again */
            float ddx = d->hx - d->x, ddz = d->hz - d->z; d->yaw = atan2f(ddx, ddz); d->x += sinf(d->yaw) * 11 * dt; d->z += cosf(d->yaw) * 11 * dt; d->y += (gy + 14 - d->y) * dt * 1.5f;
            if (d->t > 8) { d->state = 0; d->t = 0; } } }
    return ev; }

int world_hounds(World *w, float dt, float px, float py, float pz, float reach, int whip, int roar, int in_air) {
    int ev = 0; (void)py;
    for (int i = 0; i < w->nhound; i++) { Drone *h = &w->hound[i]; h->t += dt;
        float dx = px - h->x, dz = pz - h->z, dist = sqrtf(dx * dx + dz * dz);
        if (h->state == 3) { h->flap += dt * 3; if (h->t > 22) { h->state = 0; h->t = 0; } continue; }
        if (roar && dist < 12) { h->state = 2; h->t = 0; }
        if (whip && dist < reach) { h->state = 3; h->t = 0; ev |= 2; continue; }
        float sp = 0;
        if (h->state == 0) { float ang = h->t * 0.35f; float tx = h->hx + cosf(ang) * 10, tz = h->hz + sinf(ang) * 10; h->yaw = atan2f(tx - h->x, tz - h->z); sp = 2.2f; if (dist < 15 && !in_air) { h->state = 1; h->t = 0; } }
        else if (h->state == 1) { h->yaw = atan2f(dx, dz); sp = 5.6f; if (dist < reach * 0.6f && !in_air) { ev |= 1; h->state = 2; h->t = 0; } if (h->t > 9 || dist > 40 || in_air) { h->state = 2; h->t = 0; } }
        else { h->yaw = atan2f(h->hx - h->x, h->hz - h->z); sp = 4.0f; if (h->t > 6) { h->state = 0; h->t = 0; } }
        float nx = h->x + sinf(h->yaw) * sp * dt, nz = h->z + cosf(h->yaw) * sp * dt;
        if (!world_in_pond(w, nx, nz) && fabsf(nx) < W_SIZE / 2 - 8 && fabsf(nz) < W_SIZE / 2 - 8) { h->x = nx; h->z = nz; }
        h->y = world_height(w, h->x, h->z); h->flap += dt * sp * 2.2f; }
    return ev; }

float world_fbm(float x, float y, unsigned s) { return fbm(x, y, s); }
