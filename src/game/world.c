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
    /* trees and rocks: scattered, denser toward the edges, none in the start meadow or the pond */
    for (int tries = 0; tries < 4000 && w->nprop < W_MAXPROPS; tries++) {
        float x = (frand() - 0.5f) * (W_SIZE - 16), z = (frand() - 0.5f) * (W_SIZE - 16); float d = sqrtf(x * x + z * z);
        if (d < 14) continue;
        if (world_in_pond(w, x, z)) continue;
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
