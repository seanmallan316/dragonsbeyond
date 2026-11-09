// game.c - Dragons Beyond, proof of concept: Everleaf Wilds with the forest dragonet. Walk, eat, grow, fly.
#include "game.h"
#include "world.h"
#include "dragon.h"
#include "anim3d.h"
#include "../render/gfx.h"
#include "../render/batch.h"
#include "../render/assets.h"
#include "../audio/sfx.h"
#include "../audio/music.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int g_gfx_status;
static World g_world;
static Dragon g_dragon;
static Anim3D g_model[ST_N]; static int g_model_ok[ST_N];
static float g_time, g_cam_yaw; static int g_cam_mode;   /* 0 rider, 1 overhead, 2 dragon's eyes */
static Vec3 g_eye, g_at;
static Pad g_prev[KK_PADS];
static char g_msg[64]; static float g_msg_t; static float g_title_t = 4.0f;

/* textures and meshes */
static Texture *tx_grass, *tx_dirt, *tx_bark, *tx_leaves, *tx_fern, *tx_berry, *tx_bush, *tx_rock, *tx_water, *tx_mush, *tx_grub, *tx_fish, *tx_spark, *tx_font;
static Mesh *m_ground, *m_water, *m_bark, *m_leaves, *m_fern, *m_bushes, *m_rock, *m_sky;
static Mesh *m_dyn;        /* food, sparkles, HUD: refilled per view */
#define DYN_CAP 24000
static Vertex g_dynv[DYN_CAP]; static Batch g_b;
static uint32_t n_sky;

static float frand01(unsigned *s) { *s = *s * 1664525u + 1013904223u; return (float)((*s >> 8) & 0xFFFFFF) / 16777216.0f; }

/* ---------- building the world's meshes ---------- */
static Vertex *tmpv; static uint32_t tmpn, tmpcap;
static void tv_begin(uint32_t cap) { tmpcap = cap; tmpn = 0; tmpv = realloc(tmpv, sizeof(Vertex) * cap); }
static void tv_push(Vec3 p, Vec3 n, float u, float v, float r, float g, float b, float a) { if (tmpn < tmpcap) { Vertex q = {p.x, p.y, p.z, n.x, n.y, n.z, u, v, r, g, b, a}; tmpv[tmpn++] = q; } }
static void tv_tri(Vec3 a, Vec3 b, Vec3 c, Vec3 na, Vec3 nb, Vec3 nc, float ua, float va, float ub, float vb, float uc, float vc, float r, float g, float bl) { tv_push(a, na, ua, va, r, g, bl, 1); tv_push(b, nb, ub, vb, r, g, bl, 1); tv_push(c, nc, uc, vc, r, g, bl, 1); }
static Mesh *tv_mesh(void) { return tmpn ? gfx_mesh_static(tmpv, tmpn) : NULL; }

static void build_ground(void) {
    tv_begin((W_GRID - 1) * (W_GRID - 1) * 6);
    for (int j = 0; j < W_GRID - 1; j++) for (int i = 0; i < W_GRID - 1; i++) {
        Vec3 p[4]; Vec3 n[4]; float c[4][3];
        for (int k = 0; k < 4; k++) { int ii = i + (k & 1), jj = j + (k >> 1); float x = -W_SIZE / 2 + W_SIZE * ii / (W_GRID - 1), z = -W_SIZE / 2 + W_SIZE * jj / (W_GRID - 1);
            p[k] = v3(x, g_world.h[jj][ii], z); n[k] = world_normal(&g_world, x, z);
            float slope = 1 - n[k].y; float hgt = clampf((p[k].y + 4) / 30.0f, 0, 1);
            /* vertex colours: lush green low, drier yellow-green up the hills, grey on steep rock */
            float r = 0.78f + 0.3f * hgt, g = 1.0f - 0.12f * hgt, b = 0.7f + 0.2f * hgt;
            float rk = clampf((slope - 0.35f) * 4, 0, 1); r = r * (1 - rk) + 0.75f * rk; g = g * (1 - rk) + 0.72f * rk; b = b * (1 - rk) + 0.7f * rk;
            if (world_in_pond(&g_world, x, z)) { r *= 0.8f; g *= 0.85f; b *= 0.8f; }
            c[k][0] = r; c[k][1] = g; c[k][2] = b; }
        float s = 0.25f; float u0 = i * s, v0 = j * s, u1 = u0 + s, v1 = v0 + s;
        tv_tri(p[0], p[2], p[1], n[0], n[2], n[1], u0, v0, u0, v1, u1, v0, (c[0][0] + c[2][0] + c[1][0]) / 3, (c[0][1] + c[2][1] + c[1][1]) / 3, (c[0][2] + c[2][2] + c[1][2]) / 3);
        tv_tri(p[1], p[2], p[3], n[1], n[2], n[3], u1, v0, u0, v1, u1, v1, (c[1][0] + c[2][0] + c[3][0]) / 3, (c[1][1] + c[2][1] + c[3][1]) / 3, (c[1][2] + c[2][2] + c[3][2]) / 3); }
    m_ground = tv_mesh();
    /* the pond: a disc at water level */
    tv_begin(64 * 3); Vec3 cc = v3(g_world.pond_x, g_world.pond_y, g_world.pond_z); float R = g_world.pond_r * 1.25f;
    for (int i = 0; i < 64; i++) { float a0 = i * 6.2831853f / 64, a1 = (i + 1) * 6.2831853f / 64;
        tv_tri(cc, v3(cc.x + sinf(a1) * R, cc.y, cc.z + cosf(a1) * R), v3(cc.x + sinf(a0) * R, cc.y, cc.z + cosf(a0) * R), v3(0, 1, 0), v3(0, 1, 0), v3(0, 1, 0), 0.5f, 0.5f, 0.5f + sinf(a1) * 0.5f, 0.5f + cosf(a1) * 0.5f, 0.5f + sinf(a0) * 0.5f, 0.5f + cosf(a0) * 0.5f, 1, 1, 1); }
    m_water = tv_mesh(); }

/* a cylinder/cone segment (trunks, stalks) */
static void tv_cone(Vec3 base, Vec3 top, float r0, float r1, int seg, float uscale, float sway) {
    for (int i = 0; i < seg; i++) { float a0 = i * 6.2831853f / seg, a1 = (i + 1) * 6.2831853f / seg;
        Vec3 n0 = v3(sinf(a0), 0, cosf(a0)), n1 = v3(sinf(a1), 0, cosf(a1));
        Vec3 b0 = v3(base.x + n0.x * r0, base.y, base.z + n0.z * r0), b1 = v3(base.x + n1.x * r0, base.y, base.z + n1.z * r0);
        Vec3 t0 = v3(top.x + n0.x * r1, top.y, top.z + n0.z * r1), t1 = v3(top.x + n1.x * r1, top.y, top.z + n1.z * r1);
        Vec3 sn0 = v3(n0.x * (1 + sway), n0.y * (1 + sway), n0.z * (1 + sway)), sn1 = v3(n1.x * (1 + sway), n1.y * (1 + sway), n1.z * (1 + sway));
        float u0 = (float)i / seg * uscale, u1 = (float)(i + 1) / seg * uscale;
        tv_tri(b0, t0, b1, n0, sn0, n1, u0, 1, u0, 0, u1, 1, 1, 1, 1); tv_tri(b1, t0, t1, n1, sn0, sn1, u1, 1, u0, 0, u1, 0, 1, 1, 1); } }
/* a lumpy ball (canopies, rocks): lat-long sphere with noise */
static void tv_ball(Vec3 c, float rx, float ry, float rz, unsigned seed, float lump, float uvs, float r, float g, float b, float sway) {
    const int LA = 7, LO = 10; Vec3 pts[8][11]; unsigned s = seed;
    for (int i = 0; i <= LA; i++) for (int j = 0; j <= LO; j++) { float th = 3.14159265f * i / LA, ph = 6.2831853f * j / LO; if (j == LO) { pts[i][j] = pts[i][0]; continue; }
        float k = 1 + (frand01(&s) - 0.5f) * 2 * lump; if (i == 0 || i == LA) k = 1;
        pts[i][j] = v3(c.x + sinf(th) * sinf(ph) * rx * k, c.y + cosf(th) * ry * k, c.z + sinf(th) * cosf(ph) * rz * k); }
    for (int i = 0; i < LA; i++) for (int j = 0; j < LO; j++) {
        Vec3 p0 = pts[i][j], p1 = pts[i][j + 1], p2 = pts[i + 1][j], p3 = pts[i + 1][j + 1];
        Vec3 n0 = v3_norm(v3_sub(p0, c)), n1 = v3_norm(v3_sub(p1, c)), n2 = v3_norm(v3_sub(p2, c)), n3 = v3_norm(v3_sub(p3, c));
        float sw = 1 + sway; n0 = v3(n0.x * sw, n0.y * sw, n0.z * sw); n1 = v3(n1.x * sw, n1.y * sw, n1.z * sw); n2 = v3(n2.x * sw, n2.y * sw, n2.z * sw); n3 = v3(n3.x * sw, n3.y * sw, n3.z * sw);
        float u0 = (float)j / LO * uvs, u1 = (float)(j + 1) / LO * uvs, v0 = (float)i / LA * uvs * 0.5f, v1 = (float)(i + 1) / LA * uvs * 0.5f;
        if (i > 0) tv_tri(p0, p2, p1, n0, n2, n1, u0, v0, u0, v1, u1, v0, r, g, b);
        if (i < LA - 1) tv_tri(p1, p2, p3, n1, n2, n3, u1, v0, u0, v1, u1, v1, r, g, b); } }
/* crossed upright quads (bushes, ferns, leafy fluff): 3 planes at 60 degrees */
static void tv_cross(Vec3 base, float w, float h, float yaw, float sway, int planes, float r, float g, float b) {
    for (int k = 0; k < planes; k++) { float a = yaw + k * 3.14159265f / planes; Vec3 ax = v3(cosf(a) * w / 2, 0, -sinf(a) * w / 2); Vec3 n = v3(sinf(a) * (1 + sway), 0, cosf(a) * (1 + sway));
        Vec3 p0 = v3(base.x - ax.x, base.y, base.z - ax.z), p1 = v3(base.x + ax.x, base.y, base.z + ax.z), p2 = v3(base.x + ax.x, base.y + h, base.z + ax.z), p3 = v3(base.x - ax.x, base.y + h, base.z - ax.z);
        Vec3 nb = v3(sinf(a), 0, cosf(a));
        tv_tri(p0, p1, p2, nb, nb, n, 0, 1, 1, 1, 1, 0, r, g, b); tv_tri(p0, p2, p3, nb, n, n, 0, 1, 1, 0, 0, 0, r, g, b);
        tv_tri(p0, p2, p1, nb, n, nb, 0, 1, 1, 0, 1, 1, r, g, b); tv_tri(p0, p3, p2, nb, n, n, 0, 1, 0, 0, 1, 0, r, g, b); } }

static void build_props(void) {
    unsigned s = 77;
    /* trunks (bark) */
    tv_begin(W_MAXPROPS * 8 * 6 * 2);
    for (int i = 0; i < g_world.nprop; i++) { const Prop *p = &g_world.prop[i]; float y = world_height(&g_world, p->x, p->z); Vec3 b = v3(p->x, y - 0.5f, p->z);
        if (p->kind == P_TREE) { float h = 7.5f * p->scale; tv_cone(b, v3(p->x + sinf(p->yaw) * 0.6f, y + h * 0.75f, p->z + cosf(p->yaw) * 0.6f), 0.55f * p->scale, 0.25f * p->scale, 8, 2, 0);
            for (int k = 0; k < 3; k++) { float a = p->yaw + k * 2.1f; Vec3 t = v3(p->x + sinf(a) * 2.2f * p->scale, y + h * (0.75f + 0.2f), p->z + cosf(a) * 2.2f * p->scale); tv_cone(v3(p->x + sinf(p->yaw) * 0.6f, y + h * 0.7f, p->z + cosf(p->yaw) * 0.6f), t, 0.22f * p->scale, 0.08f * p->scale, 6, 1, 0.1f); } }
        else if (p->kind == P_PALM) { float h = 9.0f * p->scale; tv_cone(b, v3(p->x + sinf(p->yaw) * 1.5f, y + h, p->z + cosf(p->yaw) * 1.5f), 0.4f * p->scale, 0.25f * p->scale, 7, 2, 0.15f); } }
    m_bark = tv_mesh();
    /* canopies: lumpy leaf balls plus fluff, and the palm crowns */
    tv_begin(W_MAXPROPS * 260 * 3);
    for (int i = 0; i < g_world.nprop; i++) { const Prop *p = &g_world.prop[i]; float y = world_height(&g_world, p->x, p->z);
        if (p->kind == P_TREE) { float h = 7.5f * p->scale; Vec3 c = v3(p->x + sinf(p->yaw) * 0.6f, y + h * 0.95f, p->z + cosf(p->yaw) * 0.6f);
            float tint = 0.85f + frand01(&s) * 0.3f; tv_ball(c, 3.6f * p->scale, 2.6f * p->scale, 3.6f * p->scale, i * 31 + 5, 0.22f, 2.5f, tint, tint, tint * 0.9f, 0.25f);
            for (int k = 0; k < 3; k++) { float a = p->yaw + k * 2.1f; Vec3 t = v3(p->x + sinf(a) * 2.2f * p->scale, y + h * 0.95f + 0.4f, p->z + cosf(a) * 2.2f * p->scale); tv_ball(t, 2.0f * p->scale, 1.5f * p->scale, 2.0f * p->scale, i * 31 + k, 0.25f, 1.5f, tint, tint, tint * 0.9f, 0.3f); } }
        else if (p->kind == P_PALM) { float h = 9.0f * p->scale; Vec3 top = v3(p->x + sinf(p->yaw) * 1.5f, y + h, p->z + cosf(p->yaw) * 1.5f);
            for (int k = 0; k < 7; k++) { float a = p->yaw + k * 0.9f; Vec3 ax = v3(cosf(a) * 1.1f, 0, -sinf(a) * 1.1f); Vec3 tip = v3(top.x + sinf(a) * 4.5f * p->scale, top.y - 1.6f * p->scale, top.z + cosf(a) * 4.5f * p->scale); Vec3 mid = v3(top.x + sinf(a) * 2.2f * p->scale, top.y + 0.6f * p->scale, top.z + cosf(a) * 2.2f * p->scale);
                Vec3 n = v3(0, 1.0f, 0), ns = v3(0, 1.5f, 0);
                tv_tri(v3(top.x - ax.x, top.y, top.z - ax.z), v3(top.x + ax.x, top.y, top.z + ax.z), mid, n, n, n, 0, 1, 1, 1, 0.5f, 0.5f, 0.9f, 1, 0.8f);
                tv_tri(v3(mid.x - ax.x * 0.8f, mid.y, mid.z - ax.z * 0.8f), v3(mid.x + ax.x * 0.8f, mid.y, mid.z + ax.z * 0.8f), tip, n, n, ns, 0, 0.5f, 1, 0.5f, 0.5f, 0, 0.9f, 1, 0.8f);
                tv_tri(v3(top.x + ax.x, top.y, top.z + ax.z), v3(top.x - ax.x, top.y, top.z - ax.z), mid, n, n, n, 1, 1, 0, 1, 0.5f, 0.5f, 0.9f, 1, 0.8f);
                tv_tri(v3(mid.x + ax.x * 0.8f, mid.y, mid.z + ax.z * 0.8f), v3(mid.x - ax.x * 0.8f, mid.y, mid.z - ax.z * 0.8f), tip, n, n, ns, 1, 0.5f, 0, 0.5f, 0.5f, 0, 0.9f, 1, 0.8f); } } }
    m_leaves = tv_mesh();
    /* ferns and bushes: crossed quads */
    tv_begin(W_MAXPROPS * 36);
    for (int i = 0; i < g_world.nprop; i++) { const Prop *p = &g_world.prop[i]; float y = world_height(&g_world, p->x, p->z);
        if (p->kind == P_FERN) tv_cross(v3(p->x, y - 0.1f, p->z), 2.6f * p->scale, 2.0f * p->scale, p->yaw, 0.25f, 3, 1, 1, 1); }
    m_fern = tv_mesh();
    tv_begin(W_MAXPROPS * 36);
    for (int i = 0; i < g_world.nprop; i++) { const Prop *p = &g_world.prop[i]; float y = world_height(&g_world, p->x, p->z);
        if (p->kind == P_BUSH) tv_cross(v3(p->x, y - 0.2f, p->z), 2.8f * p->scale, 2.0f * p->scale, p->yaw, 0.12f, 3, 1, 1, 1); }
    m_bushes = tv_mesh();
    /* rocks */
    tv_begin(W_MAXPROPS * 140 * 3);
    for (int i = 0; i < g_world.nprop; i++) { const Prop *p = &g_world.prop[i]; float y = world_height(&g_world, p->x, p->z);
        if (p->kind == P_ROCK) tv_ball(v3(p->x, y - 0.3f * p->scale, p->z), 1.4f * p->scale, 0.9f * p->scale, 1.1f * p->scale, i * 17 + 3, 0.3f, 1.0f, 1, 1, 1, 0); }
    m_rock = tv_mesh();
    /* sky dome: unlit, coloured by height */
    tv_begin(16 * 24 * 6);
    for (int i = 0; i < 8; i++) for (int j = 0; j < 24; j++) {
        float t0 = i / 8.0f, t1 = (i + 1) / 8.0f; float a0 = j * 6.2831853f / 24, a1 = (j + 1) * 6.2831853f / 24;
        Vec3 q[4]; float col[4][3];
        for (int k = 0; k < 4; k++) { float t = (k < 2) ? t0 : t1, a = (k & 1) ? a1 : a0; float el = -0.1f + t * 1.65f; q[k] = v3(cosf(el) * sinf(a) * 900, sinf(el) * 900, cosf(el) * cosf(a) * 900);
            float u = clampf(t * 1.4f, 0, 1); col[k][0] = 0.86f * (1 - u) + 0.36f * u; col[k][1] = 0.92f * (1 - u) + 0.62f * u; col[k][2] = 0.98f * (1 - u) + 0.96f * u; }
        tv_tri(q[0], q[1], q[2], v3(0, 1, 0), v3(0, 1, 0), v3(0, 1, 0), 0, 0, 0, 0, 0, 0, col[0][0], col[0][1], col[0][2]); tmpv[tmpn - 2].r = col[1][0]; tmpv[tmpn - 2].g = col[1][1]; tmpv[tmpn - 2].b = col[1][2]; tmpv[tmpn - 1].r = col[2][0]; tmpv[tmpn - 1].g = col[2][1]; tmpv[tmpn - 1].b = col[2][2];
        tv_tri(q[1], q[3], q[2], v3(0, 1, 0), v3(0, 1, 0), v3(0, 1, 0), 0, 0, 0, 0, 0, 0, col[1][0], col[1][1], col[1][2]); tmpv[tmpn - 2].r = col[3][0]; tmpv[tmpn - 2].g = col[3][1]; tmpv[tmpn - 2].b = col[3][2]; tmpv[tmpn - 1].r = col[2][0]; tmpv[tmpn - 1].g = col[2][1]; tmpv[tmpn - 1].b = col[2][2]; }
    n_sky = tmpn; m_sky = tv_mesh(); }

/* ---------- init ---------- */
int game_init(int gfx_status) {
    g_gfx_status = gfx_status;
    world_make(&g_world, 2026);
    dragon_init(&g_dragon, &g_world, 0, 0);
    if (gfx_status != GFX_OK) return 0;
    tx_grass = asset_texture("grass", 1, NULL, NULL); tx_dirt = asset_texture("dirt", 1, NULL, NULL); tx_bark = asset_texture("bark", 1, NULL, NULL);
    tx_leaves = asset_texture("leaves", 1, NULL, NULL); tx_fern = asset_texture("fern", 0, NULL, NULL); tx_berry = asset_texture("berrybush", 0, NULL, NULL); tx_bush = asset_texture("bush", 0, NULL, NULL);
    tx_rock = asset_texture("rock", 1, NULL, NULL); tx_water = asset_texture("water", 1, NULL, NULL); tx_mush = asset_texture("mushroom", 0, NULL, NULL);
    tx_grub = asset_texture("grub", 0, NULL, NULL); tx_fish = asset_texture("fish", 0, NULL, NULL); tx_spark = asset_texture("sparkle", 0, NULL, NULL);
    tx_font = asset_texture("font_hd", 0, NULL, NULL); if (tx_font) g_font_hd = 1;
    build_ground(); build_props();
    batch_init(&g_b, g_dynv, DYN_CAP); m_dyn = gfx_mesh_dynamic(DYN_CAP);
    static const char *names[ST_N] = {"forest_dragonet", "forest_dragon", "forest_beyonder"};
    for (int s = 0; s < ST_N; s++) g_model_ok[s] = anim3d_load(&g_model[s], names[s]);
    gfx_log("Dragons Beyond ready: %d props, %d food, models %d %d %d", g_world.nprop, g_world.nfood, g_model_ok[0], g_model_ok[1], g_model_ok[2]);
    snprintf(g_msg, sizeof g_msg, "WALK TO THE BERRIES. PRESS X TO EAT"); g_msg_t = 8;
    return 1; }
void game_shutdown(void) {}
const char *game_where(void) { static char s[96]; snprintf(s, sizeof s, "Everleaf Wilds, %s, growth %.0f, cam %d", dragon_stage_name(g_dragon.stage), g_dragon.growth, g_cam_mode); return s; }

/* ---------- camera ---------- */
static Mat4 dragon_matrix(const Dragon *d, float scale) {
    Mat4 m = m4_mul(m4_translate(d->x, d->y, d->z), m4_rotate_y(d->yaw));
    if (d->mode == DM_AIR) m = m4_mul(m, m4_rotate_x(-d->pitch * 0.8f));
    return m4_mul(m, m4_scale(scale, scale, scale)); }
static void camera_update(float dt) {
    const Dragon *d = &g_dragon; float L = dragon_length(d->stage);
    float k = 1 - expf(-dt * (d->mode == DM_AIR ? 2.2f : 3.5f));
    /* the camera turns to look where the dragon goes */
    float dy = d->yaw - g_cam_yaw; while (dy > 3.14159f) dy -= 6.2831853f; while (dy < -3.14159f) dy += 6.2831853f;
    g_cam_yaw += dy * k * (fabsf(d->speed) > 0.3f || d->mode == DM_AIR ? 1.0f : 0.35f);
    Vec3 want, at;
    if (g_cam_mode == 0) {        /* rider: just behind and above the dragon's back */
        float back = 1.6f * L + 1.2f, up = 0.75f * L + 0.9f; if (d->mode == DM_AIR) { back *= 1.15f; up *= 1.1f; }
        want = v3(d->x - sinf(g_cam_yaw) * back, d->y + up, d->z - cosf(g_cam_yaw) * back);
        at = v3(d->x + sinf(g_cam_yaw) * 1.5f * L, d->y + 0.45f * L + (d->mode == DM_AIR ? d->pitch * 2 : 0), d->z + cosf(g_cam_yaw) * 1.5f * L); }
    else if (g_cam_mode == 1) {   /* overhead: high up, looking down, still turning with the dragon */
        float up = 5.0f * L + 10; want = v3(d->x - sinf(g_cam_yaw) * up * 0.45f, d->y + up, d->z - cosf(g_cam_yaw) * up * 0.45f); at = v3(d->x, d->y, d->z); }
    else {                        /* the dragon's own eyes */
        float fwd = 0.42f * L, up = 0.55f * L; want = v3(d->x + sinf(d->yaw) * fwd, d->y + up, d->z + cosf(d->yaw) * fwd);
        at = v3(want.x + sinf(d->yaw) * 10, want.y - (d->mode == DM_AIR ? d->pitch * 8 : 0.8f), want.z + cosf(d->yaw) * 10); }
    /* keep the camera above the ground */
    float gnd = world_height(&g_world, want.x, want.z) + 0.6f; if (want.y < gnd) want.y = gnd;
    if (g_cam_mode == 2) { g_eye = want; g_at = at; }
    else { float kk = 1 - expf(-dt * 6); g_eye = v3(g_eye.x + (want.x - g_eye.x) * kk, g_eye.y + (want.y - g_eye.y) * kk, g_eye.z + (want.z - g_eye.z) * kk); g_at = v3(g_at.x + (at.x - g_at.x) * kk, g_at.y + (at.y - g_at.y) * kk, g_at.z + (at.z - g_at.z) * kk); } }

/* ---------- drawing ---------- */
static Vec3 sun_dir(void) { return v3_norm(v3(0.45f, 0.8f, 0.35f)); }
static void draw_food_kind(Vec3 eye, int kind, Texture *t) {
    batch_clear(&g_b);
    for (int i = 0; i < g_world.nfood; i++) { const Food *f = &g_world.food[i]; if (f->kind != kind) continue;
        float dx = f->x - eye.x, dz = f->z - eye.z; if (dx * dx + dz * dz > 150 * 150) continue;
        float ang = atan2f(dx, dz); Vec3 ax = v3(cosf(ang), 0, -sinf(ang)); Vec3 n = v3(0, 1, 0);
        if (kind == F_BERRY) { if (!f->alive && f->regrow > 20) continue; float s = 1.3f; batch_quad(&g_b, v3(f->x, f->y + s * 0.5f - 0.08f, f->z), v3(ax.x * s * 0.6f, 0, ax.z * s * 0.6f), v3(0, s * 0.5f, 0), n, 0, 0, 1, 1, 1, 1, 1, 1); }
        else if (kind == F_MUSHROOM) { if (!f->alive) continue; float s = 0.7f; batch_quad(&g_b, v3(f->x, f->y + s * 0.5f, f->z), v3(ax.x * s * 0.5f, 0, ax.z * s * 0.5f), v3(0, s * 0.5f, 0), n, 0, 0, 1, 1, 1, 1, 1, 1); }
        else if (kind == F_GRUB) { if (!f->alive) continue; float s = 0.6f; Vec3 fw = v3(sinf(f->yaw), 0, cosf(f->yaw)), rt = v3(cosf(f->yaw), 0, -sinf(f->yaw)); float wig = sinf(f->phase * 12) * 0.06f;
            batch_quad(&g_b, v3(f->x, f->y + 0.1f, f->z), v3(rt.x * s * 0.35f + fw.x * wig, 0, rt.z * s * 0.35f + fw.z * wig), v3(fw.x * s * 0.5f, 0.03f, fw.z * s * 0.5f), n, 0, 0, 1, 1, 1, 1, 1, 1); }
        else { if (!f->alive) continue; float s = 0.9f; Vec3 fw = v3(sinf(f->yaw), 0, cosf(f->yaw)); batch_quad(&g_b, v3(f->x, f->y, f->z), v3(fw.x * s * 0.5f, 0, fw.z * s * 0.5f), v3(0, s * 0.3f, 0), n, 0, 0, 1, 1, 1, 1, 1, 0.9f); } }
    if (!g_b.n) return; gfx_mesh_update(m_dyn, g_b.v, g_b.n); Mat4 id = m4_identity(); gfx_bind(t); gfx_state(ST_CUTOUT); gfx_draw_ex(m_dyn, &id, 1, 1, 1, 1, 0.05f, 0, 0, 0); }
static void draw_berry_bushes_eaten(Vec3 eye) {   /* the green bush stays when the berries are gone */
    batch_clear(&g_b); Vec3 n = v3(0, 1, 0);
    for (int i = 0; i < g_world.nfood; i++) { const Food *f = &g_world.food[i]; if (f->kind != F_BERRY || f->alive) continue;
        float dx = f->x - eye.x, dz = f->z - eye.z; if (dx * dx + dz * dz > 150 * 150) continue; float ang = atan2f(dx, dz); Vec3 ax = v3(cosf(ang), 0, -sinf(ang)); float s = 1.3f;
        batch_quad(&g_b, v3(f->x, f->y + s * 0.5f - 0.08f, f->z), v3(ax.x * s * 0.6f, 0, ax.z * s * 0.6f), v3(0, s * 0.5f, 0), n, 0, 0, 1, 1, 1, 1, 1, 1); }
    if (!g_b.n) return; gfx_mesh_update(m_dyn, g_b.v, g_b.n); Mat4 id = m4_identity(); gfx_bind(tx_bush); gfx_state(ST_CUTOUT); gfx_draw_ex(m_dyn, &id, 1, 1, 1, 1, 0.05f, 0, 0, 0); }

static void draw_dragon(const Dragon *d) {
    int st = d->stage; float sc = dragon_scale(st);
    if (d->evolve_t > 0) {   /* grows from the old size to the new in the first second, with a glow */
        float t = clampf(1 - d->evolve_t / 2.0f, 0, 1); float from = dragon_scale(st - 1 >= 0 ? st - 1 : 0); sc = from + (sc - from) * clampf(t * 1.6f, 0, 1); }
    Anim3D *m = &g_model[st]; if (!m->ok) m = g_model_ok[0] ? &g_model[0] : NULL; if (!m) return;
    Mat4 base = dragon_matrix(d, sc); int clip = anim3d_clip(m, dragon_anim_name(d->anim)); if (clip < 0) clip = 0;
    gfx_state(ST_SOLID);
    float glow = d->evolve_t > 0 ? 0.5f + 0.5f * sinf(g_time * 20) : 0;
    anim3d_draw(m, clip, d->anim_t, &base, 1 + glow, 1 + glow, 1 + 0.6f * glow, 1); gfx_bind(NULL); }

static void draw_sparkles(const Dragon *d, Vec3 eye) {
    if (d->evolve_t <= 0) return; batch_clear(&g_b); unsigned s = 9; float L = dragon_length(d->stage); float t = 2.0f - d->evolve_t;
    Vec3 right = v3_norm(v3_cross(v3(0, 1, 0), v3_sub(d->x < eye.x ? v3(d->x, d->y, d->z) : v3(d->x, d->y, d->z), eye))); Vec3 up = v3(0, 1, 0);
    for (int i = 0; i < 60; i++) { float a = frand01(&s) * 6.28f, r = frand01(&s) * L * 0.9f, h = frand01(&s) * L * 0.8f, ph = frand01(&s) * 6.28f;
        float rise = fmodf(t * (0.8f + frand01(&s)) + ph, 2.0f); float sz = 0.08f * L * (1 - rise / 2) + 0.05f;
        Vec3 c = v3(d->x + cosf(a + t) * r, d->y + h + rise * L * 0.6f, d->z + sinf(a + t) * r);
        batch_quad(&g_b, c, v3(right.x * sz, 0, right.z * sz), v3(up.x * sz, up.y * sz, up.z * sz), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 0.7f, 1 - rise / 2); }
    gfx_mesh_update(m_dyn, g_b.v, g_b.n); Mat4 id = m4_identity(); gfx_bind(tx_spark); gfx_state(ST_DEPTH | ST_BLEND | ST_ADD); gfx_draw_ex(m_dyn, &id, 1, 1, 1, 1, 0, 1, 0, 0); }

static void draw_scene(int drc) {
    float aspect = gfx_aspect(drc); Vec3 eye = g_eye, at = g_at;
    Mat4 proj = m4_perspective(g_cam_mode == 2 ? 1.25f : 1.05f, aspect, 0.2f, 1500.0f), view = m4_look_at(eye, at, v3(0, 1, 0)); Mat4 vp = m4_mul(proj, view);
    Vec3 L = sun_dir();
    gfx_light(v3(1.0f, 0.96f, 0.85f), v3(0.55f, 0.65f, 0.8f), v3(0.35f, 0.4f, 0.2f), eye);
    gfx_shadow_use(1); gfx_camera(&vp, L, 0.42f, 0.78f, 0.86f, 0.95f, 90, 420); gfx_shadow_use(0);
    Mat4 id = m4_identity();
    /* sky, around the camera, no depth */
    Mat4 skym = m4_translate(eye.x, eye.y - 60, eye.z); gfx_state(0); gfx_bind(NULL); gfx_draw_ex(m_sky, &skym, 1, 1, 1, 1, 0, 1, 0, 0);
    gfx_clear_depth(drc);
    gfx_state(ST_SOLID);
    gfx_bind(tx_grass); gfx_bump(2); gfx_draw(m_ground, &id, 1, 1, 1, 1); gfx_bump(0);
    gfx_bind(tx_bark); if (m_bark) gfx_draw(m_bark, &id, 1, 1, 1, 1);
    gfx_bind(tx_rock); if (m_rock) gfx_draw(m_rock, &id, 1, 1, 1, 1);
    gfx_state(ST_CUTOUT);
    gfx_bind(tx_leaves); if (m_leaves) gfx_draw(m_leaves, &id, 1, 1, 1, 1);
    gfx_bind(tx_fern); if (m_fern) gfx_draw(m_fern, &id, 1, 1, 1, 1);
    gfx_bind(tx_bush); if (m_bushes) gfx_draw(m_bushes, &id, 1, 1, 1, 1);
    draw_food_kind(eye, F_BERRY, tx_berry); draw_berry_bushes_eaten(eye); draw_food_kind(eye, F_MUSHROOM, tx_mush); draw_food_kind(eye, F_GRUB, tx_grub);
    if (g_cam_mode != 2) draw_dragon(&g_dragon);
    gfx_state(ST_TRANSPARENT); gfx_bind(tx_water); gfx_draw_ex(m_water, &id, 1, 1, 1, 0.8f, 0.9f, 0, g_time * 0.02f, g_time * 0.013f);
    draw_food_kind(eye, F_FISH, tx_fish);
    draw_sparkles(&g_dragon, eye);
    gfx_bind(NULL); }

static void shadow_pass(void) {
    const Dragon *d = &g_dragon; Vec3 L = sun_dir();
    Vec3 c = v3(d->x + sinf(g_cam_yaw) * 15, d->y, d->z + cosf(g_cam_yaw) * 15); const float half = 55.0f, dep = 260.0f;
    c.x = floorf(c.x / (2 * half / 1024)) * (2 * half / 1024); c.z = floorf(c.z / (2 * half / 1024)) * (2 * half / 1024);
    Mat4 view = m4_look_at(v3(c.x + L.x * dep * 0.5f, c.y + L.y * dep * 0.5f, c.z + L.z * dep * 0.5f), c, v3(0, 1, 0));
    Mat4 proj = m4_identity(); proj.m[0] = 1.0f / half; proj.m[5] = 1.0f / half; proj.m[10] = -2.0f / dep; proj.m[14] = -1.0f;
    Mat4 lvp = m4_mul(proj, view), id = m4_identity();
    if (!gfx_shadow_begin(&lvp)) return;
    gfx_state(ST_SOLID); if (m_bark) gfx_draw(m_bark, &id, 1, 1, 1, 1); if (m_rock) gfx_draw(m_rock, &id, 1, 1, 1, 1);
    gfx_state(ST_CUTOUT); gfx_bind(tx_leaves); if (m_leaves) gfx_draw(m_leaves, &id, 1, 1, 1, 1);
    draw_dragon(d);
    gfx_shadow_end(); }

static void text_c(const char *s, float y, float size, float r, float g, float b, float a) { batch_text(&g_b, s, 640 - text_width(s, size) / 2, y, size, r, g, b, a); }
static void draw_hud(int drc) {
    const Dragon *d = &g_dragon; batch_clear(&g_b);
    /* hearts */
    for (int i = 0; i < (int)d->max_hearts; i++) { float full = clampf(d->hearts - i, 0, 1); batch_rect(&g_b, 40 + i * 44, 36, 34, 30, 0.25f, 0.05f, 0.08f, 0.8f); if (full > 0) batch_rect(&g_b, 43 + i * 44, 39 + 24 * (1 - full), 28, 24 * full, 0.95f, 0.2f, 0.3f, 1); }
    /* growth bar */
    float need = dragon_need(d->stage); float gf = need > 1e8f ? 1 : clampf(d->growth / need, 0, 1);
    batch_rect(&g_b, 40, 80, 360, 22, 0.05f, 0.1f, 0.05f, 0.8f); batch_rect(&g_b, 43, 83, 354 * gf, 16, 0.45f, 0.9f, 0.3f, 1);
    batch_text(&g_b, dragon_stage_name(d->stage), 40, 108, 26, 0.9f, 1, 0.8f, 1);
    if (need < 1e8f) { char s[32]; snprintf(s, sizeof s, "GROW %d/%d", (int)d->growth, (int)need); batch_text(&g_b, s, 410, 80, 22, 0.9f, 1, 0.8f, 0.9f); }
    /* stamina while flying */
    if (d->mode == DM_AIR && d->stage >= ST_DRAGON) { batch_rect(&g_b, 40, 140, 200, 12, 0.05f, 0.08f, 0.15f, 0.8f); batch_rect(&g_b, 42, 142, 196 * d->stamina, 8, 0.5f, 0.8f, 1, 1); }
    /* prompts */
    float L = dragon_length(d->stage);
    int near = world_nearest_food(&g_world, d->x + sinf(d->yaw) * 0.35f * L, d->z + cosf(d->yaw) * 0.35f * L, 0.45f * L + 1.0f);
    if (near >= 0 && d->mode != DM_AIR && d->act_t <= 0) text_c(food_is_meat(g_world.food[near].kind) ? "X : EAT (MEAT, GROWS FAST)" : "X : EAT", 600, 30, 1, 1, 0.7f, 0.9f);
    if (g_msg_t > 0) text_c(g_msg, 640, 28, 1, 1, 1, clampf(g_msg_t, 0, 1));
    if (g_title_t > 0) { float a = clampf(g_title_t, 0, 1); text_c("DRAGONS BEYOND", 250, 72, 0.6f, 1, 0.5f, a); text_c("EVERLEAF WILDS", 330, 40, 0.9f, 1, 0.9f, a); }
    static const char *cams[3] = {"RIDER VIEW", "OVERHEAD VIEW", "DRAGON'S EYES"}; batch_text(&g_b, cams[g_cam_mode], 1000, 36, 20, 1, 1, 1, 0.6f);
    batch_text(&g_b, "D-PAD UP : VIEW   A : JUMP/FLY   ZR : RUN   B : TAIL   Y : ROAR", 300, 690, 18, 1, 1, 1, 0.5f);
    if (!g_b.n) return; gfx_camera_2d(); gfx_state(ST_HUD); gfx_bind(tx_font); gfx_mesh_update(m_dyn, g_b.v, g_b.n); Mat4 id = m4_identity(); gfx_draw_ex(m_dyn, &id, 1, 1, 1, 1, 0, 1, 0, 0); gfx_bind(NULL); (void)drc; }

/* ---------- the frame ---------- */
void game_frame(const Pad *pads, int npads, float dt) {
    g_time += dt; gfx_set_time(g_time);
    if (g_gfx_status != GFX_OK) { gfx_frame_begin(); gfx_view_begin(0, g_gfx_status == GFX_NO_COMPILER ? 1 : 1, g_gfx_status == GFX_NO_COMPILER ? 0.5f : 0, 0); gfx_view_end(0); gfx_view_begin(1, 1, 0.5f, 0); gfx_view_end(1); gfx_frame_end(); return; }
    /* player 1: the GamePad, or the first Wii controller if the GamePad stick is idle */
    const Pad *p = &pads[0]; const Pad *q = &g_prev[0];
    for (int i = 1; i < npads && i < KK_PADS; i++) if (pads[i].source && (fabsf(pads[i].sx) > 0.2f || fabsf(pads[i].sy) > 0.2f || pads[i].a || pads[i].x) && fabsf(p->sx) < 0.2f && fabsf(p->sy) < 0.2f && !p->a && !p->x) { p = &pads[i]; q = &g_prev[i]; }
    DragonInput in = {0}; in.sx = p->sx; in.sy = p->sy; in.a = p->a; in.a_press = p->a && !q->a; in.b_press = p->b && !q->b; in.x_press = p->x && !q->x; in.y_press = p->y && !q->y; in.zr = p->zr; in.zl = p->zl;
    if (p->up && !q->up) { g_cam_mode = (g_cam_mode + 1) % 3; }
    int ev = dragon_update(&g_dragon, &g_world, &in, g_cam_yaw, dt);
    world_update(&g_world, dt, g_dragon.x, g_dragon.z);
    if (ev & 1) { platform_rumble(0.3f, 0.1f); sfx_play(SFX_PICKUP, 0.5f, 0); }
    if (ev & 2) { snprintf(g_msg, sizeof g_msg, g_dragon.stage == ST_DRAGON ? "YOU GREW INTO A DRAGON! HOLD A TO FLY" : "BEYONDER! THE SKY IS YOURS"); g_msg_t = 6; platform_rumble(1, 0.8f); }
    if (ev & 16) platform_rumble(0.6f, 0.4f);
    if (g_msg_t > 0) g_msg_t -= dt;
    if (g_title_t > 0) g_title_t -= dt;
    memcpy(g_prev, pads, sizeof(Pad) * (npads < KK_PADS ? npads : KK_PADS));
    camera_update(dt);
    music_update(2);
    gfx_frame_begin();
    shadow_pass();
    for (int drc = 0; drc < 2; drc++) {
        gfx_view_begin(drc, 0.55f, 0.75f, 0.95f);
        draw_scene(drc);
        draw_hud(drc);
        gfx_view_end(drc); }
    gfx_frame_end(); }
