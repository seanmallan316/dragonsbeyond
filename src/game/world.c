// world.c - building and running the worlds (see world.h).
// Beyond Expansion: 3.6 km worlds. Everything is placed chunk by chunk (100 m squares), in chunk order, so props and food
// come out already sorted by chunk and every "what is near here" question only looks at a few chunks.
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
static float fbm2(float x, float y, unsigned s) { return (vnoise(x, y, s) * 2 + vnoise(x * 2.3f, y * 2.3f, s + 7)) / 3; }   /* two octaves: the big shapes */
static float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
/* a number from 0..1 that is always the same for (a, b) in this world */
static float hrand(const World *w, int a, int b) { return hashf(a, b, w->seed * 31u + 977u); }

/* ---------- ground height ---------- */
/* the big shapes: wide valleys and plateaus, and in some regions a ridge of real mountains. Fades out near the start meadow
   so the first minutes look like the old Everleaf Wilds. */
static float macro_height(const World *w, float x, float z) {
    float m = fbm2(x * 0.0011f + 13, z * 0.0011f - 7, w->seed + 201);
    float rg = 1 - fabsf(fbm2(x * 0.0019f, z * 0.0019f, w->seed + 202) * 2 - 1); rg = rg * rg * rg;
    float h = (m - 0.5f) * 44.0f + rg * 36.0f * smooth01((m - 0.42f) / 0.25f);
    float sd = sqrtf(x * x + z * z); return h * smooth01((sd - 40) / 260.0f); }
static float raw_height(const World *w, float x, float z) {
    float d = sqrtf(x * x + z * z) / (W_SIZE * 0.5f);
    float h = (fbm(x * 0.012f + 3.1f, z * 0.012f + 7.7f, w->seed) - 0.5f) * 14.0f + (fbm(x * 0.05f, z * 0.05f, w->seed + 99) - 0.5f) * 2.2f;
    float M = macro_height(w, x, z);
    switch (w->kind) {
    case WK_SNOW: h = h * 1.6f + (fbm(x * 0.03f, z * 0.03f, w->seed + 5) - 0.5f) * 6.0f; M *= 1.35f; break;                 /* tall snowy hills, real peaks */
    case WK_CRYSTAL: { float r = 1 - fabsf(fbm(x * 0.025f, z * 0.025f, w->seed + 11) * 2 - 1); h = h * 0.9f + r * r * 9.0f - 3; M *= 0.45f; } break;   /* sharp ridges, under the cave roof */
    case WK_SAND: h = (fbm(x * 0.008f, z * 0.008f, w->seed + 3) - 0.5f) * 10.0f + sinf(x * 0.09f + 2.0f * fbm(x * 0.02f, z * 0.02f, w->seed + 4) * 6.28f) * 1.6f; M *= 0.8f; break;   /* long dunes with ripples */
    case WK_LAVA: { float r = fbm(x * 0.04f, z * 0.04f, w->seed + 21); h = h * 1.2f + (r - 0.5f) * 7.0f; } break;          /* rough volcanic ground */
    case WK_KAINE: { float r = 1 - fabsf(fbm(x * 0.02f, z * 0.02f, w->seed + 31) * 2 - 1); h = h * 1.1f + r * r * r * 12.0f - 2; } break;   /* black crags */
    case WK_WATER: { float sd0 = sqrtf(x * x + z * z); float bed = -17.0f + (fbm(x * 0.02f, z * 0.02f, w->seed + 41) - 0.5f) * 9.0f + (fbm(x * 0.07f, z * 0.07f, w->seed + 42) - 0.5f) * 2.5f;   /* the sea bed */
        float isl = clampf((46 - sd0) / 22.0f, 0, 1); isl = isl * isl * (3 - 2 * isl);                                    /* the sandy home island */
        float arch = fbm2(x * 0.0026f + 5, z * 0.0026f - 2, w->seed + 43); float ai = smooth01((arch - 0.66f) / 0.07f) * smooth01((sd0 - 160) / 120.0f);   /* an archipelago far out */
        isl = fmaxf(isl, ai); h = bed + (2.2f + 5.0f * ai * fbm(x * 0.03f, z * 0.03f, w->seed + 44) - bed) * isl;
        return h + smooth01((d - 0.84f) / 0.16f) * 85.0f; }   /* sea cliffs at the edge of the world */
    default: break; }
    h += M;
    h += smooth01((d - 0.8f) / 0.2f) * smooth01((d - 0.8f) / 0.2f) * 130.0f;   /* the edges rise into a wall of hills so the world feels enclosed */
    float sd = sqrtf(x * x + z * z); if (sd < W_START_R) h = lerp(h, w->pond_y + 1.2f, (1 - sd / W_START_R) * 0.8f);   /* a flat meadow to start on */
    return h; }

#define GX(i) (-W_SIZE / 2 + W_CELL * (i))
static int gi(float v) { int i = (int)floorf((v + W_SIZE / 2) / W_CELL); return i < 0 ? 0 : i >= W_GRID ? W_GRID - 1 : i; }
/* run f over the grid samples inside a box (cheap: only what is near the shape) */
#define GRID_BOX(cx, cz, R, I, J) for (int J = gi((cz) - (R)), _j1 = gi((cz) + (R)); J <= _j1; J++) for (int I = gi((cx) - (R)), _i1 = gi((cx) + (R)); I <= _i1; I++)

float world_height(const World *w, float x, float z) {
    float fx = (x + W_SIZE / 2) / W_SIZE * (W_GRID - 1), fz = (z + W_SIZE / 2) / W_SIZE * (W_GRID - 1);
    fx = clampf(fx, 0, W_GRID - 1.001f); fz = clampf(fz, 0, W_GRID - 1.001f);
    int i = (int)fx, j = (int)fz; float tx = fx - i, tz = fz - j;
    return lerp(lerp(w->h[j][i], w->h[j][i + 1], tx), lerp(w->h[j + 1][i], w->h[j + 1][i + 1], tx), tz); }
Vec3 world_normal(const World *w, float x, float z) {
    float e = 0.8f; float hx = world_height(w, x + e, z) - world_height(w, x - e, z), hz = world_height(w, x, z + e) - world_height(w, x, z - e);
    return v3_norm(v3(-hx, 2 * e, -hz)); }
int world_lake_at(const World *w, float x, float z) {
    for (int i = 0; i < w->nlake; i++) { float dx = x - w->lake_x[i], dz = z - w->lake_z[i]; if (dx * dx + dz * dz < w->lake_r[i] * w->lake_r[i]) return i; }
    return -1; }
int world_in_pond(const World *w, float x, float z) { return world_lake_at(w, x, z) >= 0; }
float world_pond_y(const World *w, float x, float z) { int k = world_lake_at(w, x, z); return k >= 0 ? w->lake_y[k] : w->pond_y; }

/* ---------- placing things ---------- */
static int near_spot(const Spot *s, int n, float x, float z, float r) { for (int i = 0; i < n; i++) { float dx = x - s[i].x, dz = z - s[i].z; if (dx * dx + dz * dz < r * r) return 1; } return 0; }
static int in_arena(const World *w, float x, float z, float pad) { float ax = x - w->arena_x, az = z - w->arena_z; return ax * ax + az * az < (w->arena_r + pad) * (w->arena_r + pad); }
static int near_lake(const World *w, float x, float z, float pad) { for (int i = 0; i < w->nlake; i++) { float dx = x - w->lake_x[i], dz = z - w->lake_z[i], R = w->lake_r[i] + pad; if (dx * dx + dz * dz < R * R) return 1; } return 0; }
/* somewhere a thing can stand: solid, not too steep, not in a lake, not in the boss clearing, not in the start meadow */
static int good_ground(const World *w, float x, float z) {
    if (fabsf(x) > W_SIZE * 0.4f || fabsf(z) > W_SIZE * 0.4f) return 0;
    float h = world_height(w, x, z); if (h < W_CLOUDS + 10) return 0;
    if (w->sea_on && h < w->sea + 0.5f && w->kind != WK_WATER) return 0;
    if (near_lake(w, x, z, 6) || in_arena(w, x, z, 10) || x * x + z * z < 40 * 40) return 0;
    if (world_normal(w, x, z).y < 0.82f) return 0;
    if (WK_ISLES(w->kind)) for (int k = 0; k < 6; k++) { float a = k * 1.047f; if (world_height(w, x + cosf(a) * 6, z + sinf(a) * 6) < W_CLOUDS) return 0; }   /* not on a cliff edge */
    return 1; }
/* pick n spots on good ground between rmin and rmax from the start, spread apart */
static void pick_spots(World *w, Spot *s, int n, float rmin, float rmax, float apart, const Spot *avoid, int navoid) {
    for (int i = 0; i < n; i++) {
        float bx = 0, bz = 0, bd = -1;
        for (int tries = 0; tries < 400; tries++) {
            float a = (i + frand() * 0.8f) * 6.2831853f / n + w->seed * 0.37f, r = rmin + frand() * (rmax - rmin); float x = cosf(a) * r, z = sinf(a) * r;
            if (!good_ground(w, x, z)) continue;
            float md = 1e9f; for (int k = 0; k < i; k++) { float dx = x - s[k].x, dz = z - s[k].z; md = fminf(md, dx * dx + dz * dz); }
            for (int k = 0; k < navoid; k++) { float dx = x - avoid[k].x, dz = z - avoid[k].z; md = fminf(md, dx * dx + dz * dz); }
            if (md > apart * apart) { bx = x; bz = z; bd = md; break; }
            if (md > bd) { bd = md; bx = x; bz = z; } }
        if (bd < 0) { float a = i * 6.2831853f / n; bx = cosf(a) * rmin * 0.5f; bz = sinf(a) * rmin * 0.5f; }   /* nowhere good: somewhere near home */
        s[i].x = bx; s[i].z = bz; s[i].y = world_height(w, bx, bz); s[i].found = 0; } }

/* props of one chunk, in a jittered grid; the special props (ring trees) that belong to this chunk go first */
typedef struct { int kind; float x, z, yaw, scale; int ring; } Special;
static Special g_spec[16]; static int g_nspec;
static int prop_kind(World *w, float x, float z) {
    float r = frand(); int kind = w->kind;
    if (kind == WK_FOREST) return r < 0.42f ? P_TREE : (r < 0.56f ? P_PALM : (r < 0.76f ? P_BUSH : (r < 0.88f ? P_FERN : P_ROCK)));
    if (kind == WK_SNOW) return r < 0.5f ? P_TREE : (r < 0.66f ? P_BUSH : (r < 0.78f ? P_FERN : (r < 0.9f ? P_ROCK : P_SPIRE)));        /* snowy pines, ice crystals */
    if (kind == WK_CRYSTAL) return r < 0.3f ? P_TREE : (r < 0.46f ? P_BUSH : (r < 0.66f ? P_FERN : (r < 0.8f ? P_ROCK : P_SPIRE)));   /* crystal clusters, ice shards */
    if (kind == WK_SAND) { int k = r < 0.32f ? P_TREE : (r < 0.44f ? P_PALM : (r < 0.58f ? P_BUSH : (r < 0.78f ? P_ROCK : P_SPIRE)));     /* cactus, palms by the oases, sandstone pillars */
        if (k == P_PALM && !near_lake(w, x, z, 28)) k = P_TREE; return k; }   /* palms only round the oases */
    if (kind == WK_WATER) { float h = world_height(w, x, z);
        if (h > 0.8f) return r < 0.5f ? P_PALM : (r < 0.8f ? P_BUSH : P_ROCK);   /* the islands: palms */
        if (h > -3) return -1;                                                    /* the beaches stay clear */
        return r < 0.34f ? P_TREE : (r < 0.6f ? P_FERN : (r < 0.78f ? P_ROCK : P_SPIRE)); }   /* the sea bed: kelp, shells, coral */
    if (kind == WK_SKY) return r < 0.30f ? P_TREE : (r < 0.55f ? P_BUSH : (r < 0.78f ? P_FERN : (r < 0.9f ? P_ROCK : P_SPIRE)));
    return r < 0.3f ? P_TREE : (r < 0.42f ? P_BUSH : (r < 0.62f ? P_ROCK : (r < 0.72f ? P_FERN : P_SPIRE)));   /* lava and the keep: burnt trees, lava rocks */ }
static int prop_ok(World *w, float x, float z) {
    float d2 = x * x + z * z; if (d2 < 14 * 14) return 0;
    if ((x + 12) * (x + 12) + (z - 7) * (z - 7) < 9 * 9) return 0;   /* keep the whirlwind spot clear */
    if (world_in_pond(w, x, z) || in_arena(w, x, z, 4)) return 0;
    if (near_spot(w->stone, W_NSTONES, x, z, 5) || near_spot(w->tower, W_NTOWERS, x, z, 7)) return 0;
    { float dx = x - w->crystal_x, dz = z - w->crystal_z; if (dx * dx + dz * dz < 16) return 0; }
    if (fabsf(x) > W_SIZE / 2 - 8 || fabsf(z) > W_SIZE / 2 - 8) return 0;
    if (WK_ISLES(w->kind)) { if (world_height(w, x, z) < W_CLOUDS + 10) return 0;
        for (int k = 0; k < 6; k++) { float a = k * 1.047f; if (world_height(w, x + cosf(a) * 4, z + sinf(a) * 4) < W_CLOUDS) return 0; } }   /* keep off the cliff edges */
    return 1; }
static void make_props(World *w) {
    const float CELL = WK_ISLES(w->kind) ? 6.0f : 6.5f; const int NC = (int)(W_CHS / CELL);
    w->nprop = 0;
    for (int c = 0; c < W_NCH; c++) {
        w->pch[c] = w->nprop; int cx = c % W_CHN, cz = c / W_CHN; float x0 = -W_SIZE / 2 + cx * W_CHS, z0 = -W_SIZE / 2 + cz * W_CHS;
        for (int s = 0; s < g_nspec; s++) { Special *S = &g_spec[s]; if (world_chunk_of(S->x, S->z) != c || w->nprop >= W_MAXPROPS) continue;
            Prop *p = &w->prop[w->nprop]; p->kind = S->kind; p->x = S->x; p->z = S->z; p->yaw = S->yaw; p->scale = S->scale; if (S->ring >= 0) w->arena_tree[S->ring] = w->nprop; w->nprop++; }
        for (int j = 0; j < NC; j++) for (int i = 0; i < NC; i++) {
            if (w->nprop >= W_MAXPROPS) break;
            float x = x0 + (i + 0.2f + 0.6f * hrand(w, cx * 64 + i, cz * 64 + j)) * CELL, z = z0 + (j + 0.2f + 0.6f * hrand(w, cz * 64 + j + 9999, cx * 64 + i)) * CELL;
            g_rs = ((unsigned)cx * 92821u + (unsigned)cz * 68917u + (unsigned)i * 7919u + (unsigned)j * 104729u) ^ (w->seed * 2654435761u);
            float d = sqrtf(x * x + z * z);
            /* density: open near the start; further out, real forests and open plains (big patches), with the small patches inside them */
            float forest = WK_ISLES(w->kind) ? 0.55f : 0.25f + 0.75f * fbm2(x * 0.004f + 3, z * 0.004f - 5, w->seed + 303);
            float density = (0.12f + 0.5f * clampf((d - 14) / 90.0f, 0, 1) * fbm(x * 0.03f, z * 0.03f, w->seed + 5)) * (0.45f + 0.9f * forest);
            if (WK_ISLES(w->kind)) density = 0.55f;   /* as dense as the first islands */
            if (frand() > density) continue;
            if (!prop_ok(w, x, z)) continue;
            int kind = prop_kind(w, x, z); if (kind < 0) continue;
            Prop *p = &w->prop[w->nprop++]; p->kind = kind; p->x = x; p->z = z; p->yaw = frand() * 6.2831853f;
            p->scale = kind == P_SPIRE ? 0.6f + frand() * 0.5f : (WK_ISLES(w->kind) ? 0.75f + frand() * 0.45f : 0.8f + frand() * 0.5f); } }
    w->pch[W_NCH] = w->nprop; }

/* food of every chunk: berry bushes and mushrooms, grubs on the open ground, fish in the lakes (and everywhere under the sea) */
static void make_food(World *w) {
    const float CELL = WK_ISLES(w->kind) ? 6.0f : 11.0f; const int NC = (int)(W_CHS / CELL);
    w->nfood = 0;
    for (int c = 0; c < W_NCH; c++) {
        w->fch[c] = w->nfood; int cx = c % W_CHN, cz = c / W_CHN; float x0 = -W_SIZE / 2 + cx * W_CHS, z0 = -W_SIZE / 2 + cz * W_CHS;
        for (int i = 0; i < 6 && w->nfood < W_MAXFOOD; i++) {   /* a few berry bushes right by the start so the first minute teaches eating (filed in whichever chunk each one is in) */
            float a = i * 1.047f + 0.3f, r = 6 + (i % 2) * 3, x = cosf(a) * r, z = sinf(a) * r; if (world_chunk_of(x, z) != c) continue;
            Food *f = &w->food[w->nfood++]; memset(f, 0, sizeof *f); f->kind = i % 3 == 2 ? F_MUSHROOM : F_BERRY; f->alive = 1; f->x = x; f->z = z; f->y = world_height(w, x, z); f->yaw = a; f->ch = c; }
        for (int j = 0; j < NC; j++) for (int i = 0; i < NC; i++) {
            if (w->nfood >= W_MAXFOOD) break;
            float x = x0 + (i + 0.15f + 0.7f * hrand(w, cx * 64 + i + 31, cz * 64 + j)) * CELL, z = z0 + (j + 0.15f + 0.7f * hrand(w, cz * 64 + j + 77, cx * 64 + i + 5)) * CELL;
            g_rs = ((unsigned)cx * 15485863u + (unsigned)cz * 32452843u + (unsigned)i * 49979687u + (unsigned)j * 67867967u) ^ (w->seed * 40503u);
            if (fabsf(x) > W_SIZE / 2 - 15 || fabsf(z) > W_SIZE / 2 - 15) continue;
            float rich = 0.55f + 0.9f * fbm2(x * 0.006f - 4, z * 0.006f + 9, w->seed + 404);   /* rich meadows and bare patches */
            if (frand() > 0.36f * rich) continue;
            if (in_arena(w, x, z, 2)) continue;
            if (WK_ISLES(w->kind) && world_height(w, x, z) < W_CLOUDS + 10) continue;
            int inpond = world_in_pond(w, x, z); int kind; float d = sqrtf(x * x + z * z);
            if (inpond) { if (frand() < 0.5f || w->kind == WK_LAVA || w->kind == WK_KAINE) continue; kind = F_FISH; }   /* nothing swims in lava */
            else if (w->sea_on && world_height(w, x, z) < w->sea - 3) { if (frand() < 0.45f) continue; kind = frand() < 0.75f ? F_FISH : F_GRUB; }   /* under the sea: fish everywhere */
            else if (w->sea_on && world_height(w, x, z) < w->sea + 0.6f) continue;
            else { float r = frand(); kind = r < 0.45f ? F_BERRY : (r < 0.72f ? F_MUSHROOM : F_GRUB); if (d > 150 && d < 400 && frand() < 0.3f) continue; }
            if (!WK_ISLES(w->kind) && !inpond && world_normal(w, x, z).y < 0.7f) continue;
            int ok = 1;   /* not on top of a prop (this chunk and its neighbours) */
            WORLD_FOR_CHUNKS(x, z, 3, pc) { for (int k = w->pch[pc]; k < w->pch[pc + 1] && ok; k++) { float dx = w->prop[k].x - x, dz = w->prop[k].z - z; if (dx * dx + dz * dz < 2.2f * 2.2f) ok = 0; } }
            if (!ok) continue;
            Food *f = &w->food[w->nfood++]; memset(f, 0, sizeof *f); f->kind = kind; f->alive = 1; f->x = x; f->z = z;
            f->y = inpond ? world_pond_y(w, x, z) : world_height(w, x, z) + (kind == F_FISH ? 0.9f : 0);
            f->yaw = frand() * 6.2831853f; f->phase = frand() * 6.2831853f; f->ch = c; } }
    w->fch[W_NCH] = w->nfood; }

/* Kaine's robots: a small patrol near the start (the first minute), a camp round every Signal Tower, and camps scattered over the world */
static void add_drone(World *w, float x, float z, int tower, float t) {
    if (w->ndrone >= W_MAXDRONES) return; Drone *d = &w->drone[w->ndrone++]; memset(d, 0, sizeof *d);
    d->hx = x; d->hz = z; d->x = x; d->z = z; d->y = world_height(w, x, z) + 9; d->yaw = t; d->t = t; d->tower = tower; }
static void add_hound(World *w, float x, float z, int tower, float t) {
    if (w->nhound >= W_MAXHOUNDS) return; if (world_in_pond(w, x, z) || (WK_ISLES(w->kind) && world_is_void(w, x, z))) return;
    Drone *h = &w->hound[w->nhound++]; memset(h, 0, sizeof *h);
    h->hx = x; h->hz = z; h->x = x; h->z = z; h->y = world_height(w, x, z); h->yaw = t; h->t = t; h->tower = tower; }
static void make_robots(World *w, int hounds) {
    w->ndrone = w->nhound = 0;
    for (int i = 0; i < 8; i++) { float a = i * 1.26f + 0.5f, r = 45 + (i % 2) * 25 + (i >= 5 ? 55 : 0); add_drone(w, cosf(a) * r, sinf(a) * r, -1, frand() * 6); }   /* the first patrol */
    if (hounds) for (int i = 0; i < 6; i++) { float a = i * 2.1f + 1.2f, r = 62 + (i % 3) * 8 + (i >= 3 ? 60 : 0); add_hound(w, cosf(a) * r, sinf(a) * r, -1, frand() * 5); }
    for (int t = 0; t < W_NTOWERS; t++) {   /* a camp round each tower: these switch off for good when it falls */
        for (int i = 0; i < 6; i++) { float a = i * 1.047f + t, r = 22 + 10 * (i % 2); add_drone(w, w->tower[t].x + cosf(a) * r, w->tower[t].z + sinf(a) * r, t, frand() * 6); }
        if (hounds) for (int i = 0; i < 4; i++) { float a = i * 1.57f + t * 0.5f, r = 16 + 6 * (i % 2); add_hound(w, w->tower[t].x + cosf(a) * r, w->tower[t].z + sinf(a) * r, t, frand() * 5); } }
    for (int tries = 0; tries < 400 && (w->ndrone < W_MAXDRONES - 2 || (hounds && w->nhound < W_MAXHOUNDS - 2)); tries++) {   /* camps all over */
        float x = (frand() - 0.5f) * W_SIZE * 0.78f, z = (frand() - 0.5f) * W_SIZE * 0.78f; if (x * x + z * z < 200 * 200 || in_arena(w, x, z, 40)) continue;
        if (WK_ISLES(w->kind) && world_is_void(w, x, z)) continue;
        int n = 2 + (int)(frand() * 3); for (int i = 0; i < n; i++) { float a = frand() * 6.2831853f, r = 8 + frand() * 18; add_drone(w, x + cosf(a) * r, z + sinf(a) * r, -1, frand() * 6); }
        if (hounds) { int m = 1 + (int)(frand() * 3); for (int i = 0; i < m; i++) { float a = frand() * 6.2831853f, r = 6 + frand() * 14; add_hound(w, x + cosf(a) * r, z + sinf(a) * r, -1, frand() * 5); } } } }

/* the hidden egg crystal: tucked on high ground far from home */
static void place_crystal(World *w, float rmin, float rmax) {
    float best = -1e9f; w->crystal_x = rmin; w->crystal_z = 0;
    for (int tries = 0; tries < 600; tries++) { float a = frand() * 6.2831853f, r = rmin + frand() * (rmax - rmin); float x = cosf(a) * r, z = sinf(a) * r;
        if (!good_ground(w, x, z)) continue; float h = world_height(w, x, z); if (h > best) { best = h; w->crystal_x = x; w->crystal_z = z; } }
    w->crystal_y = world_height(w, w->crystal_x, w->crystal_z); }

/* named regions: 0 the start meadow, 1 the boss clearing, the rest spread far apart */
static void place_regions(World *w) {
    w->region[0].x = 0; w->region[0].z = 0; w->region[1].x = w->arena_x; w->region[1].z = w->arena_z;
    for (int i = 2; i < W_NREGIONS; i++) { float bx = 0, bz = 0, bd = -1;
        for (int tries = 0; tries < 200; tries++) { float a = frand() * 6.2831853f, r = 500 + frand() * 900; float x = cosf(a) * r, z = sinf(a) * r; float md = 1e9f;
            for (int k = 0; k < i; k++) { float dx = x - w->region[k].x, dz = z - w->region[k].z; md = fminf(md, dx * dx + dz * dz); }
            if (md > bd) { bd = md; bx = x; bz = z; } }
        w->region[i].x = bx; w->region[i].z = bz; }
    for (int i = 0; i < W_NREGIONS; i++) w->region[i].y = world_height(w, w->region[i].x, w->region[i].z); }
int world_region_at(const World *w, float x, float z) {
    int best = 0; float bd = 1e18f; for (int i = 0; i < W_NREGIONS; i++) { float dx = x - w->region[i].x, dz = z - w->region[i].z, d = dx * dx + dz * dz; if (i == 0) d *= 9; if (i == 1) d *= 4; if (d < bd) { bd = d; best = i; } }   /* the meadow and the clearing are small places */
    return best; }

static void finish_world(World *w, int hounds) {
    pick_spots(w, w->tower, W_NTOWERS, 450, 1350, 500, NULL, 0); for (int i = 0; i < W_NTOWERS; i++) w->tower_hp[i] = 1;
    pick_spots(w, w->stone, W_NSTONES, 160, 1500, 380, w->tower, W_NTOWERS);
    if (!WK_ISLES(w->kind)) for (int t = 0; t < W_NTOWERS; t++) {   /* a flat pad under each tower so it stands straight */
        float H = world_height(w, w->tower[t].x, w->tower[t].z); GRID_BOX(w->tower[t].x, w->tower[t].z, 12, i, j) { float dx = GX(i) - w->tower[t].x, dz = GX(j) - w->tower[t].z; float k = smooth01((sqrtf(dx * dx + dz * dz) - 6) / 6); w->h[j][i] = H + (w->h[j][i] - H) * k; } }
    place_regions(w);
    make_props(w); make_food(w); make_robots(w, hounds); }

/* lakes: one by the start meadow (as always), more scattered over the world. Each is a round bowl pressed into the hills, the water just below its rim */
static void press_lake(World *w, int k) {
    float R = w->lake_r[k], lx = w->lake_x[k], lz = w->lake_z[k], Hr = 0;
    for (int s = 0; s < 16; s++) { float a = s * 0.3927f; Hr += world_height(w, lx + cosf(a) * R * 1.7f, lz + sinf(a) * R * 1.7f); } Hr /= 16;
    GRID_BOX(lx, lz, R * 1.95f, i, j) { float x = GX(i), z = GX(j); float pd = sqrtf((x - lx) * (x - lx) + (z - lz) * (z - lz)); if (pd > R * 1.9f) continue;
        float q = pd / (R * 1.25f), depth = q < 1 ? 4.5f * (1 - q * q) * (R > 20 ? 1.5f : 1) : 0, kk = clampf((R * 1.9f - pd) / (R * 0.6f), 0, 1); kk = kk * kk * (3 - 2 * kk);
        w->h[j][i] = w->h[j][i] + (Hr - 0.3f - depth - w->h[j][i]) * kk; }
    w->lake_y[k] = Hr - 0.9f; }

static void make_ground(World *w, unsigned seed, int kind) {
    memset(w, 0, sizeof *w); w->seed = seed; w->kind = kind; g_rs = seed * 7919u + 17; g_nspec = 0;
    w->pond_x = 38; w->pond_z = -30; w->pond_r = 14; w->pond_y = -2.2f;
    if (kind == WK_WATER) { w->pond_r = 0; w->pond_y = -60; w->sea_on = 1; w->sea = 0; }
    /* the boss clearing: far from home now, about a kilometre out, in a direction of its own in every world */
    { float a = hrand(w, 1, 2) * 6.2831853f, r = 900 + 220 * hrand(w, 3, 4); w->arena_x = cosf(a) * r; w->arena_z = sinf(a) * r; w->arena_r = 22; }
    for (int j = 0; j < W_GRID; j++) for (int i = 0; i < W_GRID; i++) w->h[j][i] = raw_height(w, GX(i), GX(j));
    /* the lakes */
    w->nlake = 0;
    if (w->pond_r > 0) { w->lake_x[0] = w->pond_x; w->lake_z[0] = w->pond_z; w->lake_r[0] = w->pond_r; w->nlake = 1; press_lake(w, 0); w->pond_y = w->lake_y[0];
        for (int tries = 0; tries < 300 && w->nlake < W_MAXLAKES; tries++) {
            float a = frand() * 6.2831853f, r = 220 + frand() * 1250; float x = cosf(a) * r, z = sinf(a) * r, R = 16 + frand() * 26;
            if (in_arena(w, x, z, R + 40)) continue; int clash = 0; for (int k = 0; k < w->nlake; k++) { float dx = x - w->lake_x[k], dz = z - w->lake_z[k]; if (dx * dx + dz * dz < (R + w->lake_r[k] + 120) * (R + w->lake_r[k] + 120)) clash = 1; } if (clash) continue;
            if (world_normal(w, x, z).y < 0.8f) continue;
            int k = w->nlake++; w->lake_x[k] = x; w->lake_z[k] = z; w->lake_r[k] = R; press_lake(w, k); } }
    /* flatten the clearing, ring it with big trees, hang the cage from the biggest */
    w->arena_y = world_height(w, w->arena_x, w->arena_z);
    GRID_BOX(w->arena_x, w->arena_z, w->arena_r + 12, i, j) { float x = GX(i), z = GX(j);
        float d = sqrtf((x - w->arena_x) * (x - w->arena_x) + (z - w->arena_z) * (z - w->arena_z)); float k = clampf((d - w->arena_r) / 10.0f, 0, 1); k = k * k * (3 - 2 * k);
        w->h[j][i] = w->arena_y + (w->h[j][i] - w->arena_y) * k; }
    for (int i = 0; i < 7; i++) { float a = i * 6.2831853f / 7 + 0.3f; Special *S = &g_spec[g_nspec++]; S->kind = kind == WK_WATER ? P_SPIRE : P_TREE; S->x = w->arena_x + cosf(a) * (w->arena_r - 2); S->z = w->arena_z + sinf(a) * (w->arena_r - 2);
        S->yaw = a * 2.3f; S->scale = (i == 0 ? 1.55f : 1.2f + 0.1f * (i % 3)) * (kind == WK_WATER ? 1.5f : 1); S->ring = i; }   /* a ring of coral towers in the sea */
    w->narena_tree = 7;
    { const Special *ct = &g_spec[0]; float dx = w->arena_x - ct->x, dz = w->arena_z - ct->z, l = sqrtf(dx * dx + dz * dz);
      w->cage_x = ct->x + dx / l * 3.2f; w->cage_z = ct->z + dz / l * 3.2f; w->cage_y = w->arena_y + 6.5f; }
    place_crystal(w, 1150, 1600);
    finish_world(w, 1); }

void world_make(World *w, unsigned seed) { make_ground(w, seed, WK_FOREST); }

/* ---------- critters, robots ---------- */
int food_is_meat(int kind) { return kind == F_GRUB || kind == F_FISH; }
float food_growth(int kind) { return kind == F_FISH ? 90.0f : kind == F_GRUB ? 60.0f : kind == F_MUSHROOM ? 30.0f : 25.0f; }   /* everything fills you up faster now; meat counts most (fish most of all) */

void world_update(World *w, float dt, float px, float pz) {
    for (int i = 0; i < w->nfood; i++) { Food *f = &w->food[i];
        if (!f->alive) { f->regrow -= dt; if (f->regrow <= 0) { f->alive = 1; if (f->kind == F_GRUB) { float nx = f->x + (frand() - 0.5f) * 20, nz = f->z + (frand() - 0.5f) * 20; if (world_chunk_of(nx, nz) == f->ch && !world_in_pond(w, nx, nz) && !(WK_ISLES(w->kind) && world_is_void(w, nx, nz))) { f->x = nx; f->z = nz; } f->y = world_height(w, f->x, f->z); } } continue; }
        if (f->kind != F_GRUB && f->kind != F_FISH) continue;
        float dx = f->x - px, dz = f->z - pz; if (fabsf(dx) > 220 || fabsf(dz) > 220) continue;   /* far critters wait where they are: nobody sees them */
        f->phase += dt;
        {   /* critters wander, and scurry off when the dragon comes close (meat has to be caught) */
            float d = sqrtf(dx * dx + dz * dz);
            float spd = 0.6f, ang = f->yaw;
            if (d < 7 && d > 0.01f) { ang = atan2f(dx, dz) + sinf(f->phase * 3) * 0.6f; spd = f->kind == F_FISH ? 2.2f : 1.6f; }
            else if (sinf(f->phase * 0.7f) > 0.6f) { ang += dt * 1.5f; }
            f->yaw = ang; float nx = f->x + sinf(ang) * spd * dt, nz = f->z + cosf(ang) * spd * dt;
            int pondnext = world_in_pond(w, nx, nz), sea = w->sea_on && world_height(w, nx, nz) < w->sea - 2;
            int home = world_chunk_of(nx, nz) == f->ch;   /* each critter stays in its own chunk (so it can always be found there) */
            int water_ok = f->kind == F_FISH ? (pondnext || sea) : !pondnext;
            if (water_ok && home && !(WK_ISLES(w->kind) && world_is_void(w, nx, nz))) { f->x = nx; f->z = nz; }
            else f->yaw += 2.4f;
            f->y = f->kind == F_FISH ? (sea && !world_in_pond(w, f->x, f->z) ? world_height(w, f->x, f->z) + 0.9f + 0.3f * sinf(f->phase) : world_pond_y(w, f->x, f->z) - 0.15f + 0.1f * sinf(f->phase * 2)) : world_height(w, f->x, f->z); } }
}
int world_nearest_food(const World *w, float x, float z, float reach) {
    int best = -1; float bd = reach * reach;
    WORLD_FOR_CHUNKS(x, z, reach, c) for (int i = w->fch[c]; i < w->fch[c + 1]; i++) { const Food *f = &w->food[i]; if (!f->alive) continue; float dx = f->x - x, dz = f->z - z, d = dx * dx + dz * dz; if (d < bd) { bd = d; best = i; } }
    return best; }

static void note_kill(World *w, float x, float y, float z);
static void knock_dir(Drone *d, float fx, float fz);
static float air_floor(const World *w, float x, float z) { float h = world_height(w, x, z); return WK_ISLES(w->kind) ? fmaxf(h, W_CLOUDS + 8) : (w->sea_on ? fmaxf(h, w->sea) : h); }   /* flyers keep above the cloud sea (and the real sea) */
static int atk_hits(Attack a, float x, float y, float z, float pad) { if (a.r <= 0) return 0; float dx = x - a.x, dy = y - a.y, dz = z - a.z; float r = a.r + pad; return dx * dx + dz * dz < r * r && fabsf(dy) < r + 0.5f; }
#define FAR_ROBOT(dx, dz) (fabsf(dx) > 300 || fabsf(dz) > 300)   /* robots far from the dragon just wait at home */
int world_drones(World *w, float dt, float px, float py, float pz, float reach, Attack atk, int roar) {
    int ev = 0;
    for (int i = 0; i < w->ndrone; i++) { Drone *d = &w->drone[i]; d->t += dt; d->flap += dt * (d->state == 1 ? 14 : 9);
        float dx = px - d->x, dz = pz - d->z;
        if (d->off) { if (d->y > air_floor(w, d->x, d->z) + 0.2f) { d->y -= 6 * dt; d->yaw += 2 * dt; } d->state = 3; continue; }   /* its tower is down: switched off for good */
        if (FAR_ROBOT(dx, dz) && d->state != 3) { d->state = 0; continue; }
        float dist = sqrtf(dx * dx + dz * dz), dy = py - d->y;
        float gy = air_floor(w, d->x, d->z);
        if (d->state == 3) { if (d->y > gy + 0.2f) { d->y -= 6 * dt; d->yaw += 5 * dt; } if (d->t > 25) { d->state = 0; d->x = d->hx; d->z = d->hz; d->y = air_floor(w, d->x, d->z) + 9; d->t = 0; } continue; }
        if (roar && dist < 14) { d->state = 2; d->t = 0; }
        if (atk_hits(atk, d->x, d->y, d->z, 0.3f)) { d->state = 3; d->t = 0; ev |= 2; knock_dir(d, atk.x, atk.z); note_kill(w, d->x, d->y, d->z); continue; }
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

static int hound_step_ok(const World *w, const Drone *h, float x, float z) {
    if (world_in_pond(w, x, z) || fabsf(x) > W_SIZE / 2 - 8 || fabsf(z) > W_SIZE / 2 - 8) return 0;
    if (WK_ISLES(w->kind) && world_is_void(w, x, z)) return 0;
    float hx = x - h->hx, hz = z - h->hz; return hx * hx + hz * hz < 70 * 70; }   /* stays near its camp */
int world_hounds(World *w, float dt, float px, float py, float pz, float reach, Attack atk, int roar, int in_air) {
    int ev = 0; (void)py;
    for (int i = 0; i < w->nhound; i++) { Drone *h = &w->hound[i]; h->t += dt;
        float dx = px - h->x, dz = pz - h->z;
        if (h->off) { h->state = 3; continue; }
        if (FAR_ROBOT(dx, dz) && h->state != 3) { h->state = 0; continue; }
        float dist = sqrtf(dx * dx + dz * dz);
        if (h->state == 3) { h->flap += dt * 3; if (h->t > 22) { h->state = 0; h->t = 0; } continue; }
        if (roar && dist < 12) { h->state = 2; h->t = 0; }
        if (atk_hits(atk, h->x, h->y + 0.5f, h->z, 0.3f)) { h->state = 3; h->t = 0; ev |= 2; knock_dir(h, atk.x, atk.z); note_kill(w, h->x, h->y, h->z); continue; }
        float sp = 0;
        if (h->state == 0) { float ang = h->t * 0.35f; float tx = h->hx + cosf(ang) * 10, tz = h->hz + sinf(ang) * 10; h->yaw = atan2f(tx - h->x, tz - h->z); sp = 2.2f; if (dist < 15 && !in_air) { h->state = 1; h->t = 0; } }
        else if (h->state == 1) { h->yaw = atan2f(dx, dz); sp = 3.4f; if (dist < 3.4f + 0.3f * reach && !in_air) { h->state = 5; h->t = 0; } if (h->t > 12 || dist > 40 || in_air) { h->state = 2; h->t = 0; } }   /* stalks, slower than a running dragonet */
        else if (h->state == 5) { h->yaw = atan2f(dx, dz); sp = 0; if (h->t > 0.8f) { h->state = 6; h->t = 0; } }   /* crouches and crackles: about to lunge */
        else if (h->state == 6) { sp = 8.0f; if (dist < reach * 0.6f && !in_air) { ev |= 1; h->state = 2; h->t = 0; } if (h->t > 0.45f) { h->state = 2; h->t = 0; } }   /* the lunge: a straight dash, dodge it sideways */
        else { h->yaw = atan2f(h->hx - h->x, h->hz - h->z); sp = 4.0f; if (h->t > 6) { h->state = 0; h->t = 0; } }
        float nx = h->x + sinf(h->yaw) * sp * dt, nz = h->z + cosf(h->yaw) * sp * dt;
        if (hound_step_ok(w, h, nx, nz)) { h->x = nx; h->z = nz; }
        h->y = world_height(w, h->x, h->z); h->flap += dt * sp * 2.2f; }
    return ev; }

float world_fbm(float x, float y, unsigned s) { return fbm(x, y, s); }

/* ---------- the Sky Kingdom: floating islands over a sea of cloud, joined by narrow stone bridges ---------- */
static float isle_height(const World *w, int i, float x, float z) {
    const Isle *I = &w->isle[i];
    float wob = 1 + 0.18f * (fbm(x * 0.05f + i * 3.1f, z * 0.05f, w->seed + 31) - 0.5f) * 2;   /* ragged edges */
    float d = sqrtf((x - I->x) * (x - I->x) + (z - I->z) * (z - I->z)) / (I->r * wob); float m = 1 - d;
    if (m <= 0) return W_VOID;
    float top = I->top + (fbm(x * 0.04f, z * 0.04f, w->seed + 7) - 0.5f) * 5.0f * smooth01(m * 3);
    return W_VOID + (top - W_VOID) * smooth01(m / 0.08f); }
static float bridge_height(const World *w, int b, float x, float z) {
    const Isle *A = &w->isle[w->bridge[b][0]], *B = &w->isle[w->bridge[b][1]];
    float ex = B->x - A->x, ez = B->z - A->z, L2 = ex * ex + ez * ez; float t = clampf(((x - A->x) * ex + (z - A->z) * ez) / L2, 0, 1);
    float px = A->x + ex * t, pz = A->z + ez * t; float d = sqrtf((x - px) * (x - px) + (z - pz) * (z - pz));
    if (d >= 4.6f) return W_VOID;
    float top = A->top + (B->top - A->top) * smooth01(t) - 0.3f; return W_VOID + (top - W_VOID) * smooth01((4.6f - d) / 0.5f); }   /* 9 m wide walkways */
int world_is_void(const World *w, float x, float z) { return world_height(w, x, z) < W_CLOUDS + 4; }
void world_safe_spot(const World *w, float x, float z, float *ox, float *oz) {
    if (!WK_ISLES(w->kind) || !world_is_void(w, x, z)) { *ox = x; *oz = z; return; }
    float bd = 1e18f; *ox = 0; *oz = 0;
    for (int i = 0; i < w->nisle; i++) { const Isle *I = &w->isle[i]; float dx = x - I->x, dz = z - I->z, d = sqrtf(dx * dx + dz * dz);
        float k = d > 1e-3f ? fminf(1, (I->r * 0.6f) / d) : 0; float sx = I->x + dx * k, sz = I->z + dz * k; float dd = (sx - x) * (sx - x) + (sz - z) * (sz - z);
        if (dd < bd && !world_is_void(w, sx, sz)) { bd = dd; *ox = sx; *oz = sz; } } }
static int add_isle(World *w, float x, float z, float r, float top) { if (w->nisle >= W_MAXISLES) return -1; Isle *I = &w->isle[w->nisle]; I->x = x; I->z = z; I->r = r; I->top = top; return w->nisle++; }
static void make_sky(World *w) {
    w->pond_r = 0; w->pond_x = 1e6f; w->pond_z = 1e6f; w->pond_y = W_VOID; w->nlake = 0;
    /* the islands: home in the middle, Stormtalon's big island about a kilometre out, the high island (the egg crystal) far off, and dozens more */
    add_isle(w, 0, 0, 24, 4);
    { float a = hrand(w, 1, 2) * 6.2831853f, r = 900 + 200 * hrand(w, 3, 4); w->arena_x = cosf(a) * r; w->arena_z = sinf(a) * r; add_isle(w, w->arena_x, w->arena_z, 30, 10); }
    { float a = hrand(w, 5, 6) * 6.2831853f, r = 1250 + 250 * hrand(w, 7, 8); add_isle(w, cosf(a) * r, sinf(a) * r, 16, 18); }
    const float CELL = 190; int N = (int)(W_SIZE * 0.82f / CELL);
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
        float x = -W_SIZE * 0.41f + (i + 0.2f + 0.6f * hrand(w, i, j + 50)) * CELL, z = -W_SIZE * 0.41f + (j + 0.2f + 0.6f * hrand(w, i + 50, j)) * CELL;
        if (hrand(w, i + 100, j + 100) < 0.35f) continue;
        float r = 22 + 44 * hrand(w, i + 200, j), top = 2 + 20 * hrand(w, i, j + 200);   /* big islands: together about 100x the land of the first eight */
        int clash = 0; for (int k = 0; k < w->nisle; k++) { float dx = x - w->isle[k].x, dz = z - w->isle[k].z, R = r + w->isle[k].r + 30; if (dx * dx + dz * dz < R * R) clash = 1; } if (clash) continue;
        add_isle(w, x, z, r, top); }
    /* bridges: join every island to the nearest one already joined (a tree, so you can walk from home to anywhere), then a few extra short ones */
    { static int in[W_MAXISLES]; static float best[W_MAXISLES]; static int from[W_MAXISLES]; memset(in, 0, sizeof in); w->nbridge = 0; in[0] = 1;
      for (int i = 0; i < w->nisle; i++) { float dx = w->isle[i].x - w->isle[0].x, dz = w->isle[i].z - w->isle[0].z; best[i] = dx * dx + dz * dz; from[i] = 0; }
      for (int step = 1; step < w->nisle && w->nbridge < W_MAXBRIDGES; step++) { int k = -1; for (int i = 0; i < w->nisle; i++) if (!in[i] && (k < 0 || best[i] < best[k])) k = i; if (k < 0) break;
          in[k] = 1; w->bridge[w->nbridge][0] = from[k]; w->bridge[w->nbridge][1] = k; w->nbridge++;
          for (int i = 0; i < w->nisle; i++) if (!in[i]) { float dx = w->isle[i].x - w->isle[k].x, dz = w->isle[i].z - w->isle[k].z, d = dx * dx + dz * dz; if (d < best[i]) { best[i] = d; from[i] = k; } } }
      for (int a = 0; a < w->nisle && w->nbridge < W_MAXBRIDGES; a++) for (int b = a + 1; b < w->nisle && w->nbridge < W_MAXBRIDGES; b++) { float dx = w->isle[a].x - w->isle[b].x, dz = w->isle[a].z - w->isle[b].z; float gap = sqrtf(dx * dx + dz * dz) - w->isle[a].r - w->isle[b].r;
          if (gap > 70 || hrand(w, a + 300, b) > 0.35f) continue; int dup = 0; for (int q = 0; q < w->nbridge; q++) if ((w->bridge[q][0] == a && w->bridge[q][1] == b) || (w->bridge[q][0] == b && w->bridge[q][1] == a)) dup = 1;
          if (!dup) { w->bridge[w->nbridge][0] = a; w->bridge[w->nbridge][1] = b; w->nbridge++; } } }
    /* draw them into the height grid: each island and bridge only touches the grid samples under it */
    for (int j = 0; j < W_GRID; j++) for (int i = 0; i < W_GRID; i++) w->h[j][i] = W_VOID;
    for (int k = 0; k < w->nisle; k++) { const Isle *I = &w->isle[k]; GRID_BOX(I->x, I->z, I->r * 1.25f + 3, i, j) { float h = isle_height(w, k, GX(i), GX(j)); if (h > w->h[j][i]) w->h[j][i] = h; } }
    for (int b = 0; b < w->nbridge; b++) { const Isle *A = &w->isle[w->bridge[b][0]], *B = &w->isle[w->bridge[b][1]];
        float x0 = fminf(A->x, B->x) - 6, x1 = fmaxf(A->x, B->x) + 6, z0 = fminf(A->z, B->z) - 6, z1 = fmaxf(A->z, B->z) + 6;
        for (int j = gi(z0); j <= gi(z1); j++) for (int i = gi(x0); i <= gi(x1); i++) { float h = bridge_height(w, b, GX(i), GX(j)); if (h > w->h[j][i]) w->h[j][i] = h; } }
    /* the boss island: flat in the middle, ringed by rock spires; the cage hangs from the tallest */
    w->arena_r = 20; w->arena_y = isle_height(w, 1, w->arena_x, w->arena_z);
    GRID_BOX(w->arena_x, w->arena_z, w->arena_r + 7, i, j) { float x = GX(i), z = GX(j);
        float d = sqrtf((x - w->arena_x) * (x - w->arena_x) + (z - w->arena_z) * (z - w->arena_z)); if (d < w->arena_r + 6 && w->h[j][i] > W_CLOUDS) { float k = smooth01((d - w->arena_r) / 6); w->h[j][i] = w->arena_y + (w->h[j][i] - w->arena_y) * k; } }
    for (int i = 0; i < 7; i++) { float a = i * 6.2831853f / 7 + 0.5f; Special *S = &g_spec[g_nspec++]; S->kind = P_SPIRE; S->x = w->arena_x + cosf(a) * (w->arena_r - 1); S->z = w->arena_z + sinf(a) * (w->arena_r - 1);
        S->yaw = a * 1.7f; S->scale = i == 0 ? 1.5f : 1.0f + 0.15f * (i % 3); S->ring = i; }
    w->narena_tree = 7;
    { const Special *ct = &g_spec[0]; float dx = w->arena_x - ct->x, dz = w->arena_z - ct->z, l = sqrtf(dx * dx + dz * dz);
      w->cage_x = ct->x + dx / l * 3.0f; w->cage_z = ct->z + dz / l * 3.0f; w->cage_y = w->arena_y + 7.0f; }
    /* the egg crystal: on the high island */
    w->crystal_x = w->isle[2].x + 3; w->crystal_z = w->isle[2].z - 2; w->crystal_y = world_height(w, w->crystal_x, w->crystal_z) + 1.2f;
    /* Signal Towers and Story Stones stand on islands of their own */
    { int used[W_MAXISLES] = {0}; used[0] = used[1] = used[2] = 1; Spot *S[2] = {w->tower, w->stone}; int NS[2] = {W_NTOWERS, W_NSTONES}; float RMIN[2] = {450, 150};
      for (int g = 0; g < 2; g++) for (int s = 0; s < NS[g]; s++) { int bi = -1; float bs = -1;
          for (int k = 3; k < w->nisle; k++) { if (used[k]) continue; float d = sqrtf(w->isle[k].x * w->isle[k].x + w->isle[k].z * w->isle[k].z); if (d < RMIN[g]) continue;
              float a = atan2f(w->isle[k].z, w->isle[k].x), want = s * 6.2831853f / NS[g] + g * 0.5f; float da = fabsf(remainderf(a - want, 6.2831853f)); float score = 10 - da + hrand(w, k, g * 10 + s) * 0.5f;
              if (score > bs) { bs = score; bi = k; } }
          if (bi < 0) bi = 3 + s; used[bi] = 1; S[g][s].x = w->isle[bi].x + 2; S[g][s].z = w->isle[bi].z - 2; S[g][s].y = world_height(w, S[g][s].x, S[g][s].z); S[g][s].found = 0; } }
    for (int i = 0; i < W_NTOWERS; i++) w->tower_hp[i] = 1;
    place_regions(w);
    make_props(w); make_food(w);
    make_robots(w, 0);   /* Feather Drones circle the islands; no Shock Hounds up here */
    /* Scale Thieves lurk on the islands, never on Stormtalon's */
    w->nthief = 0; for (int k = 3; k < w->nisle && w->nthief < W_MAXTHIEVES; k++) { if (k % 3 && k > 6) continue; const Isle *I = &w->isle[k];
        Drone *t = &w->thief[w->nthief++]; memset(t, 0, sizeof *t); t->hx = I->x + 3; t->hz = I->z - 2; t->x = t->hx; t->z = t->hz; t->y = world_height(w, t->x, t->z); t->t = w->nthief * 2.1f; t->tower = -1; } }
void world_make_kind(World *w, unsigned seed, int kind) {
    if (!WK_ISLES(kind)) { make_ground(w, seed, kind); return; }
    memset(w, 0, sizeof *w); w->seed = seed; w->kind = kind; g_rs = seed * 7919u + 23; g_nspec = 0; make_sky(w); }

int world_blast(World *w, float x, float y, float z, float dx, float dz, float range, float cos_cone) {
    int n = 0; float dl = sqrtf(dx * dx + dz * dz); if (dl > 1e-4f) { dx /= dl; dz /= dl; }
    for (int pass = 0; pass < 3; pass++) { Drone *arr = pass == 2 ? w->thief : (pass ? w->hound : w->drone); int cnt = pass == 2 ? w->nthief : (pass ? w->nhound : w->ndrone);
        for (int i = 0; i < cnt; i++) { Drone *d = &arr[i]; if (d->state == 3) continue;
            float ex = d->x - x, ey = d->y - y, ez = d->z - z; float dist = sqrtf(ex * ex + ey * ey + ez * ez); if (dist > range) continue;
            if (cos_cone > -1 && dist > 1.0f && (ex * dx + ez * dz) / sqrtf(ex * ex + ez * ez + 1e-6f) < cos_cone) continue;
            if (pass == 2) continue;   /* thieves are handled by world_thieves (they drop what they stole) */
            d->state = 3; d->t = 0; n++; knock_dir(d, x, z); note_kill(w, d->x, d->y, d->z); } }
    return n; }
static void knock_dir(Drone *d, float fx, float fz) { float dx = d->x - fx, dz = d->z - fz, l = sqrtf(dx * dx + dz * dz); if (l < 1e-3f) { dx = sinf(d->yaw + 3.14159f); dz = cosf(d->yaw + 3.14159f); l = 1; } d->kx = dx / l; d->kz = dz / l; }
static void note_kill(World *w, float x, float y, float z) { if (w->nkill < 8) { w->kill_x[w->nkill] = x; w->kill_y[w->nkill] = y; w->kill_z[w->nkill] = z; w->nkill++; } }
static int thief_step_ok(const World *w, float x, float z) { return !(WK_ISLES(w->kind) && world_is_void(w, x, z)) && !world_in_pond(w, x, z) && fabsf(x) < W_SIZE / 2 - 4 && fabsf(z) < W_SIZE / 2 - 4; }
int world_thieves(World *w, float dt, float px, float py, float pz, float reach, Attack atk, float carry, float *stolen, float *recovered) {
    int ev = 0; *stolen = 0; *recovered = 0;
    for (int i = 0; i < w->nthief; i++) { Drone *t = &w->thief[i]; t->t += dt;
        float dx = px - t->x, dz = pz - t->z; if (FAR_ROBOT(dx, dz) && t->state != 3 && t->state != 4) continue;
        float dist = sqrtf(dx * dx + dz * dz); float sp = 0, yaw = t->yaw;
        if (t->state == 3) { if (t->t > 12) { t->state = 4; t->t = 0; } continue; }
        if (t->state == 4) { if (t->t > 18) { t->state = 0; t->t = 0; t->x = t->hx; t->z = t->hz; t->flap = 0; } continue; }
        if (atk_hits(atk, t->x, t->y + 0.5f, t->z, 0.6f)) {   /* caught! it drops what it stole */
            if (t->flap > 0) { *recovered += t->flap; t->flap = 0; } t->state = 3; t->t = 0; ev |= 2; knock_dir(t, atk.x, atk.z); note_kill(w, t->x, t->y, t->z); continue; }
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

/* ---------- Kaine's Signal Towers ---------- */
int world_hit_tower(World *w, Attack atk, float damage) {
    if (atk.r <= 0) return -1;
    for (int i = 0; i < W_NTOWERS; i++) { Spot *T = &w->tower[i]; if (T->found) continue;
        float dx = T->x - atk.x, dz = T->z - atk.z, R = atk.r + 2.2f; if (dx * dx + dz * dz > R * R || atk.y < T->y - 2 || atk.y > T->y + 16) continue;
        w->tower_hp[i] -= damage; if (w->tower_hp[i] > 0) return -2 - i;   /* hit but still standing: -2 - index */
        T->found = 1;
        for (int k = 0; k < w->ndrone; k++) if (w->drone[k].tower == i) { w->drone[k].off = 1; w->drone[k].state = 3; }
        for (int k = 0; k < w->nhound; k++) if (w->hound[k].tower == i) { w->hound[k].off = 1; w->hound[k].state = 3; }
        return i; }
    return -1; }
int world_towers_down(const World *w) { int n = 0; for (int i = 0; i < W_NTOWERS; i++) n += w->tower[i].found != 0; return n; }

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
    int hit = 0; float qx = *x, qz = *z;
    WORLD_FOR_CHUNKS(qx, qz, 14, c) for (int i = w->pch[c]; i < w->pch[c + 1]; i++) { const Prop *p = &w->prop[i]; float dx = *x - p->x, dz = *z - p->z; if (dx * dx + dz * dz > 14 * 14) continue;
        float s = p->scale, gy = world_height(w, p->x, p->z);
        if (w->kind >= WK_SNOW) {   /* each world's own plants and rocks are solid in their own shape (crystals, cactus, coral...) */
            static const float SH[WK_N][P_N][3] = {   /* per world, per prop kind: {radius, height, 1 = post / 0 = mound}; radius 0 = default shape below */
                {{0}}, {{0}},
                /* snow    */ {{0.9f, 6.5f, 1}, {1.2f, 8.5f, 1}, {0}, {0}, {0.7f, 1.4f, 0}, {1.5f, 3.4f, 0}},
                /* crystal */ {{2.0f, 4.6f, 0}, {1.5f, 3.6f, 0}, {0}, {0}, {0.9f, 1.8f, 0}, {2.2f, 5.0f, 0}},
                /* sand    */ {{0.75f, 5.0f, 1}, {0}, {0}, {0}, {0.45f, 2.5f, 1}, {1.7f, 9.0f, 1}},
                /* water   */ {{-1, 0, 0}, {0}, {0}, {0}, {0.9f, 1.3f, 0}, {1.6f, 3.4f, 0}},
                /* lava    */ {{0.6f, 6.5f, 1}, {0.6f, 6.5f, 1}, {0}, {0}, {1.3f, 2.8f, 0}, {2.1f, 8.0f, 1}},
                /* keep    */ {{0.6f, 6.5f, 1}, {0.6f, 6.5f, 1}, {0}, {0}, {1.3f, 2.8f, 0}, {2.1f, 8.0f, 1}} };
            const float *sh = SH[w->kind][p->kind];
            if (sh[0] < 0) continue;   /* soft things (kelp) you swim straight through */
            if (sh[0] > 0) { if (sh[2] > 0) hit |= push_post(x, y, z, p->x, p->z, gy, gy + sh[1] * s, sh[0] * s, r); else hit |= push_ball(x, y, z, p->x, gy + sh[1] * s * 0.3f, p->z, sh[0] * s, sh[1] * s * 0.8f, sh[0] * s, r, air); continue; } }
        switch (p->kind) {
        case P_TREE: hit |= push_post(x, y, z, p->x, p->z, gy, gy + 5.5f * s, 0.55f * s, r);
                     if (air) hit |= push_ball(x, y, z, p->x, gy + 6.3f * s, p->z, 3.4f * s, 2.4f * s, 3.4f * s, r, 1); break;
        case P_PALM: hit |= push_post(x, y, z, p->x, p->z, gy, gy + 8.5f * s, 0.4f * s, r); break;
        case P_ROCK: hit |= push_ball(x, y, z, p->x, gy - 0.3f * s, p->z, 1.3f * s, 1.35f * s, 1.1f * s, r, air); break;
        case P_SPIRE: { float H = 9.0f * s; hit |= push_ball(x, y, z, p->x, gy + H * 0.45f, p->z, 2.0f * s, H, 1.8f * s, r, air); } break;
        default: break; } }
    for (int i = 0; i < W_NTOWERS; i++) { const Spot *T = &w->tower[i]; if (T->found || (T->x == 0 && T->z == 0)) continue; hit |= push_post(x, y, z, T->x, T->z, T->y, T->y + 14, 1.6f, r); }   /* the Signal Towers are solid until they fall */
    return hit; }

/* multiplayer side levels: one flat floor (island worlds keep their gaps, but every island top is the same height) */
void world_flatten(World *w, float H) {
    for (int j = 0; j < W_GRID; j++) for (int i = 0; i < W_GRID; i++) if (!(WK_ISLES(w->kind) && w->h[j][i] < W_CLOUDS + 4)) w->h[j][i] = H;
    float dy = H - w->arena_y; w->arena_y = H; w->cage_y += dy; w->pond_r = 0; w->pond_y = H - 50; w->sea_on = 0; w->nlake = 0;
    for (int i = 0; i < w->nfood; i++) w->food[i].y = world_height(w, w->food[i].x, w->food[i].z);
    for (int i = 0; i < w->nhound; i++) w->hound[i].y = H;
    for (int i = 0; i < w->nthief; i++) w->thief[i].y = H; }
