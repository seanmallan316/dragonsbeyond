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
static Vec3 g_eye, g_at; static float g_look_x, g_look_hold;
static Pad g_prev[KK_PADS];
static char g_save_path[256]; static float g_save_t;
static int load_progress(void); static void save_progress(void);
static char g_msg[64]; static float g_msg_t; static float g_title_t = 4.0f;
enum { G_TITLE = 0, G_NAME, G_STORY, G_PLAY }; static int g_state = G_TITLE; static int g_name = 0; static float g_state_t;   /* g_name: 0 Flo-Rida, 1 Leaf Blower */
static const char *NAMES[2] = {"FLO-RIDA", "LEAF BLOWER"};

/* textures and meshes */
static Texture *tx_grass, *tx_dirt, *tx_bark, *tx_leaves, *tx_fern, *tx_bush, *tx_rock, *tx_water, *tx_spark, *tx_font, *tx_tuft, *tx_flower, *tx_cloud;
static Mesh *m_ground, *m_water, *m_bark, *m_leaves, *m_fern, *m_bushes, *m_rock, *m_sky, *m_tufts, *m_flowers, *m_clouds, *m_mountains;
static Mesh *m_dynfood, *m_dynspark, *m_dynhud;   /* refilled per view (each at most twice a frame) */
#define DYN_CAP 48000
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
            float r = 0.62f + 0.3f * hgt, g = 0.84f - 0.1f * hgt, b = 0.55f + 0.2f * hgt;
            float rk = clampf((slope - 0.35f) * 4, 0, 1); r = r * (1 - rk) + 0.75f * rk; g = g * (1 - rk) + 0.72f * rk; b = b * (1 - rk) + 0.7f * rk;
            if (world_in_pond(&g_world, x, z)) { r *= 0.8f; g *= 0.85f; b *= 0.8f; }
            c[k][0] = r; c[k][1] = g; c[k][2] = b; }
        float s = 1.25f; float u0 = i * s, v0 = j * s, u1 = u0 + s, v1 = v0 + s;
        /* each corner keeps its own colour, so the shading is smooth across the ground */
        tv_push(p[0], n[0], u0, v0, c[0][0], c[0][1], c[0][2], 1); tv_push(p[2], n[2], u0, v1, c[2][0], c[2][1], c[2][2], 1); tv_push(p[1], n[1], u1, v0, c[1][0], c[1][1], c[1][2], 1);
        tv_push(p[1], n[1], u1, v0, c[1][0], c[1][1], c[1][2], 1); tv_push(p[2], n[2], u0, v1, c[2][0], c[2][1], c[2][2], 1); tv_push(p[3], n[3], u1, v1, c[3][0], c[3][1], c[3][2], 1); }
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
    tv_begin(W_MAXPROPS * 260 * 3 + W_MAXFOOD * 140 * 3);
    for (int i = 0; i < g_world.nprop; i++) { const Prop *p = &g_world.prop[i]; float y = world_height(&g_world, p->x, p->z);
        if (p->kind == P_TREE) { float h = 7.5f * p->scale; Vec3 c = v3(p->x + sinf(p->yaw) * 0.6f, y + h * 0.95f, p->z + cosf(p->yaw) * 0.6f);
            float tint = 0.85f + frand01(&s) * 0.3f; tv_ball(c, 3.9f * p->scale, 2.9f * p->scale, 3.9f * p->scale, i * 31 + 5, 0.18f, 1.6f, tint, tint, tint * 0.92f, 0.25f);
            for (int k = 0; k < 3; k++) { float a = p->yaw + k * 2.1f; Vec3 t = v3(p->x + sinf(a) * 2.2f * p->scale, y + h * 0.95f + 0.4f, p->z + cosf(a) * 2.2f * p->scale); tv_ball(t, 2.2f * p->scale, 1.7f * p->scale, 2.2f * p->scale, i * 31 + k, 0.2f, 1.1f, tint, tint, tint * 0.92f, 0.3f); } }
    }
    /* the berry bushes: round leafy balls (the berries themselves are drawn live, so they can be eaten) */
    for (int i = 0; i < g_world.nfood; i++) { const Food *f = &g_world.food[i]; if (f->kind != F_BERRY) continue; tv_ball(v3(f->x, f->y + 0.35f, f->z), 0.6f, 0.45f, 0.6f, i * 13 + 1, 0.2f, 1.0f, 0.95f, 1.0f, 0.9f, 0.15f); }
    m_leaves = tv_mesh();
    /* ferns (splayed fronds) and the palm crowns share the frond texture */
    tv_begin(W_MAXPROPS * 60 + 64);
    for (int i = 0; i < g_world.nprop; i++) { const Prop *p = &g_world.prop[i]; float y = world_height(&g_world, p->x, p->z);
        if (p->kind == P_FERN) { for (int k = 0; k < 7; k++) { float a = p->yaw + k * 0.9f; float len = 1.6f * p->scale; Vec3 b = v3(p->x, y - 0.05f, p->z); Vec3 tip = v3(p->x + sinf(a) * len, y + 0.9f * p->scale, p->z + cosf(a) * len); Vec3 ax = v3(cosf(a) * 0.45f * p->scale, 0, -sinf(a) * 0.45f * p->scale); Vec3 n = v3(0, 1.0f, 0), ns = v3(0, 1.3f, 0);
            tv_tri(v3(b.x - ax.x, b.y, b.z - ax.z), v3(b.x + ax.x, b.y, b.z + ax.z), tip, n, n, ns, 0, 1, 1, 1, 0.5f, 0, 1, 1, 1); tv_tri(v3(b.x + ax.x, b.y, b.z + ax.z), v3(b.x - ax.x, b.y, b.z - ax.z), tip, n, n, ns, 1, 1, 0, 1, 0.5f, 0, 1, 1, 1); } }
        if (p->kind == P_PALM) { float h = 9.0f * p->scale; Vec3 top = v3(p->x + sinf(p->yaw) * 1.5f, y + h, p->z + cosf(p->yaw) * 1.5f);
            for (int k = 0; k < 7; k++) { float a = p->yaw + k * 0.9f; Vec3 ax = v3(cosf(a) * 1.1f, 0, -sinf(a) * 1.1f); Vec3 tip = v3(top.x + sinf(a) * 4.5f * p->scale, top.y - 1.6f * p->scale, top.z + cosf(a) * 4.5f * p->scale); Vec3 mid = v3(top.x + sinf(a) * 2.2f * p->scale, top.y + 0.6f * p->scale, top.z + cosf(a) * 2.2f * p->scale);
                Vec3 n = v3(0, 1.0f, 0), ns = v3(0, 1.5f, 0);
                tv_tri(v3(top.x - ax.x, top.y, top.z - ax.z), v3(top.x + ax.x, top.y, top.z + ax.z), mid, n, n, n, 0, 1, 1, 1, 0.5f, 0.5f, 0.9f, 1, 0.8f);
                tv_tri(v3(mid.x - ax.x * 0.8f, mid.y, mid.z - ax.z * 0.8f), v3(mid.x + ax.x * 0.8f, mid.y, mid.z + ax.z * 0.8f), tip, n, n, ns, 0, 0.5f, 1, 0.5f, 0.5f, 0, 0.9f, 1, 0.8f);
                tv_tri(v3(top.x + ax.x, top.y, top.z + ax.z), v3(top.x - ax.x, top.y, top.z - ax.z), mid, n, n, n, 1, 1, 0, 1, 0.5f, 0.5f, 0.9f, 1, 0.8f);
                tv_tri(v3(mid.x + ax.x * 0.8f, mid.y, mid.z + ax.z * 0.8f), v3(mid.x - ax.x * 0.8f, mid.y, mid.z - ax.z * 0.8f), tip, n, n, ns, 1, 0.5f, 0, 0.5f, 0.5f, 0, 0.9f, 1, 0.8f); } } }
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
    /* grass tufts and flowers: scattered everywhere the ground is not too steep, denser near the start */
    unsigned ts = 4242; tv_begin(2600 * 12);
    for (int i = 0; i < 2600; i++) { float x = (frand01(&ts) - 0.5f) * (W_SIZE - 20), z = (frand01(&ts) - 0.5f) * (W_SIZE - 20); if (world_in_pond(&g_world, x, z)) continue; Vec3 n = world_normal(&g_world, x, z); if (n.y < 0.8f) continue;
        float y = world_height(&g_world, x, z); float sc = 0.7f + frand01(&ts) * 0.8f; float tint = 0.85f + frand01(&ts) * 0.3f; tv_cross(v3(x, y - 0.05f, z), 1.1f * sc, 0.75f * sc, frand01(&ts) * 3.14f, 0.18f, 2, tint, tint, tint); }
    m_tufts = tv_mesh();
    tv_begin(700 * 12);
    for (int i = 0; i < 700; i++) { float x = (frand01(&ts) - 0.5f) * (W_SIZE - 40), z = (frand01(&ts) - 0.5f) * (W_SIZE - 40); if (world_in_pond(&g_world, x, z)) continue; Vec3 n = world_normal(&g_world, x, z); if (n.y < 0.85f) continue;
        float y = world_height(&g_world, x, z); float sc = 0.6f + frand01(&ts) * 0.5f; tv_cross(v3(x, y - 0.02f, z), 1.2f * sc, 0.7f * sc, frand01(&ts) * 3.14f, 0.1f, 2, 1, 1, 1); }
    m_flowers = tv_mesh();
    /* clouds: big soft quads high up, facing down-ish so they read from below and from the air */
    tv_begin(40 * 12);
    for (int i = 0; i < 40; i++) { float x = (frand01(&ts) - 0.5f) * 900, z = (frand01(&ts) - 0.5f) * 900, y = 110 + frand01(&ts) * 80, w = 50 + frand01(&ts) * 70, h = w * 0.45f; float a = frand01(&ts) * 3.14f; Vec3 ax = v3(cosf(a) * w / 2, 0, -sinf(a) * w / 2), az = v3(sinf(a) * h / 2, 0, cosf(a) * h / 2); Vec3 nn = v3(0, 1, 0);
        Vec3 p0 = v3(x - ax.x - az.x, y, z - ax.z - az.z), p1 = v3(x + ax.x - az.x, y, z + ax.z - az.z), p2 = v3(x + ax.x + az.x, y, z + ax.z + az.z), p3 = v3(x - ax.x + az.x, y, z - ax.z + az.z);
        tv_tri(p0, p1, p2, nn, nn, nn, 0, 1, 1, 1, 1, 0, 1, 1, 1); tv_tri(p0, p2, p3, nn, nn, nn, 0, 1, 1, 0, 0, 0, 1, 1, 1); tv_tri(p0, p2, p1, nn, nn, nn, 0, 1, 1, 0, 1, 1, 1, 1, 1); tv_tri(p0, p3, p2, nn, nn, nn, 0, 1, 0, 0, 1, 0, 1, 1, 1);
        /* an upright copy so they have some body from the side */
        Vec3 q0 = v3(x - ax.x, y - h * 0.3f, z - ax.z), q1 = v3(x + ax.x, y - h * 0.3f, z + ax.z), q2 = v3(x + ax.x, y + h * 0.3f, z + ax.z), q3 = v3(x - ax.x, y + h * 0.3f, z - ax.z);
        tv_tri(q0, q1, q2, nn, nn, nn, 0, 1, 1, 1, 1, 0, 1, 1, 1); tv_tri(q0, q2, q3, nn, nn, nn, 0, 1, 1, 0, 0, 0, 1, 1, 1); tv_tri(q0, q2, q1, nn, nn, nn, 0, 1, 1, 0, 1, 1, 1, 1, 1); tv_tri(q0, q3, q2, nn, nn, nn, 0, 1, 0, 0, 1, 0, 1, 1, 1); }
    m_clouds = tv_mesh();
    /* a ring of far mountains, in the fog's colours, so the world never shows an edge */
    tv_begin(96 * 6); { const int M = 96; float hs[97]; unsigned ms = 99; for (int i = 0; i < M; i++) hs[i] = 60 + frand01(&ms) * 110; hs[M] = hs[0];
        for (int i = 0; i < M; i++) { float a0 = i * 6.2831853f / M, a1 = (i + 1) * 6.2831853f / M; float R0 = 420 + 40 * sinf(i * 1.3f), R1 = 420 + 40 * sinf((i + 1) * 1.3f);
            Vec3 b0 = v3(sinf(a0) * R0, -20, cosf(a0) * R0), b1 = v3(sinf(a1) * R1, -20, cosf(a1) * R1), t0 = v3(sinf(a0) * R0, hs[i], cosf(a0) * R0), t1 = v3(sinf(a1) * R1, hs[i + 1], cosf(a1) * R1);
            float c0 = 0.42f + 0.1f * (hs[i] / 170), c1 = 0.42f + 0.1f * (hs[i + 1] / 170); Vec3 nn = v3(0, 1, 0);
            tv_tri(b0, t0, b1, nn, nn, nn, 0, 0, 0, 0, 0, 0, c0, c0 + 0.1f, c0 + 0.2f); tmpv[tmpn - 3].r = 0.5f; tmpv[tmpn - 3].g = 0.62f; tmpv[tmpn - 3].b = 0.78f; tmpv[tmpn - 1].r = 0.5f; tmpv[tmpn - 1].g = 0.62f; tmpv[tmpn - 1].b = 0.78f;
            tv_tri(b1, t0, t1, nn, nn, nn, 0, 0, 0, 0, 0, 0, c1, c1 + 0.1f, c1 + 0.2f); tmpv[tmpn - 3].r = 0.5f; tmpv[tmpn - 3].g = 0.62f; tmpv[tmpn - 3].b = 0.78f; } }
    m_mountains = tv_mesh();
    /* sky dome: unlit, coloured by height */
    tv_begin(16 * 24 * 6);
    for (int i = 0; i < 8; i++) for (int j = 0; j < 24; j++) {
        float t0 = i / 8.0f, t1 = (i + 1) / 8.0f; float a0 = j * 6.2831853f / 24, a1 = (j + 1) * 6.2831853f / 24;
        Vec3 q[4]; float col[4][3];
        for (int k = 0; k < 4; k++) { float t = (k < 2) ? t0 : t1, a = (k & 1) ? a1 : a0; float el = -0.15f + t * 1.9f; q[k] = v3(cosf(el) * sinf(a) * 900, sinf(el) * 900, cosf(el) * cosf(a) * 900);
            float u = clampf(t * 1.4f, 0, 1); col[k][0] = 0.86f * (1 - u) + 0.36f * u; col[k][1] = 0.92f * (1 - u) + 0.62f * u; col[k][2] = 0.98f * (1 - u) + 0.96f * u; }
        tv_tri(q[0], q[1], q[2], v3(0, 1, 0), v3(0, 1, 0), v3(0, 1, 0), 0, 0, 0, 0, 0, 0, col[0][0], col[0][1], col[0][2]); tmpv[tmpn - 2].r = col[1][0]; tmpv[tmpn - 2].g = col[1][1]; tmpv[tmpn - 2].b = col[1][2]; tmpv[tmpn - 1].r = col[2][0]; tmpv[tmpn - 1].g = col[2][1]; tmpv[tmpn - 1].b = col[2][2];
        tv_tri(q[1], q[3], q[2], v3(0, 1, 0), v3(0, 1, 0), v3(0, 1, 0), 0, 0, 0, 0, 0, 0, col[1][0], col[1][1], col[1][2]); tmpv[tmpn - 2].r = col[3][0]; tmpv[tmpn - 2].g = col[3][1]; tmpv[tmpn - 2].b = col[3][2]; tmpv[tmpn - 1].r = col[2][0]; tmpv[tmpn - 1].g = col[2][1]; tmpv[tmpn - 1].b = col[2][2]; }
    n_sky = tmpn; m_sky = tv_mesh(); }

/* ---------- init ---------- */
int game_init(int gfx_status) {
    g_gfx_status = gfx_status;
    world_make(&g_world, 2026);
    dragon_init(&g_dragon, &g_world, 0, 0);
    if (load_progress()) g_state_t = 0;
    if (gfx_status != GFX_OK) return 0;
    tx_grass = asset_texture("grass", 1, NULL, NULL); tx_dirt = asset_texture("dirt", 1, NULL, NULL); tx_bark = asset_texture("bark", 1, NULL, NULL);
    tx_leaves = asset_texture("leaves", 1, NULL, NULL); tx_fern = asset_texture("fern", 0, NULL, NULL); tx_bush = asset_texture("bush", 0, NULL, NULL);
    tx_rock = asset_texture("rock", 1, NULL, NULL); tx_water = asset_texture("water", 1, NULL, NULL); tx_spark = asset_texture("sparkle", 0, NULL, NULL);
    tx_tuft = asset_texture("grasstuft", 0, NULL, NULL); tx_flower = asset_texture("flowers", 0, NULL, NULL); tx_cloud = asset_texture("cloud", 0, NULL, NULL);
    tx_font = asset_texture("font_hd", 0, NULL, NULL); if (tx_font) g_font_hd = 1;
    build_ground(); build_props();
    batch_init(&g_b, g_dynv, DYN_CAP); m_dynfood = gfx_mesh_dynamic(DYN_CAP); m_dynspark = gfx_mesh_dynamic(2000); m_dynhud = gfx_mesh_dynamic(36000);
    static const char *names[ST_N] = {"forest_dragonet", "forest_dragon", "forest_beyonder"};
    for (int s = 0; s < ST_N; s++) g_model_ok[s] = anim3d_load(&g_model[s], names[s]);
    gfx_log("Dragons Beyond ready: %d props, %d food, models %d %d %d", g_world.nprop, g_world.nfood, g_model_ok[0], g_model_ok[1], g_model_ok[2]);
    snprintf(g_msg, sizeof g_msg, "WALK TO THE BERRIES. PRESS X TO EAT"); g_msg_t = 8;
    return 1; }
void game_shutdown(void) {}
const char *game_where(void) { static char s[96]; static const char *st[] = {"title", "name pick", "story", "playing"}; snprintf(s, sizeof s, "%s: Everleaf Wilds, %s, growth %.0f, cam %d", st[g_state], dragon_stage_name(g_dragon.stage), g_dragon.growth, g_cam_mode); return s; }


/* ---------- progress: one small text line on the SD card ---------- */
void game_set_save_path(const char *path) { snprintf(g_save_path, sizeof g_save_path, "%s", path); }
static void save_progress(void) {
    if (!g_save_path[0]) return; FILE *f = fopen(g_save_path, "w"); if (!f) return;
    fprintf(f, "DB1 %d %d %.1f %.2f %.2f %.3f %.1f %d\n", g_name, g_dragon.stage, g_dragon.growth, g_dragon.x, g_dragon.z, g_dragon.yaw, g_dragon.hearts, g_world.crystal_found); fclose(f); }
static int load_progress(void) {
    if (!g_save_path[0]) return 0; FILE *f = fopen(g_save_path, "r"); if (!f) return 0;
    int name, stage, cr = 0; float growth, x, z, yaw, hearts; int n = fscanf(f, "DB1 %d %d %f %f %f %f %f %d", &name, &stage, &growth, &x, &z, &yaw, &hearts, &cr); fclose(f);
    if (n < 7 || stage < 0 || stage >= ST_N) return 0; g_world.crystal_found = cr;
    g_name = name & 1; g_dragon.stage = stage; g_dragon.growth = growth; g_dragon.x = clampf(x, -100, 100); g_dragon.z = clampf(z, -100, 100); g_dragon.yaw = yaw;
    g_dragon.max_hearts = 3 + 2 * stage; g_dragon.hearts = clampf(hearts, 1, g_dragon.max_hearts); g_dragon.y = world_height(&g_world, g_dragon.x, g_dragon.z);
    gfx_log("progress loaded: %s the %s, growth %.0f", NAMES[g_name], dragon_stage_name(stage), growth); return 1; }

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
    if (fabsf(g_look_x) > 0.15f) { g_cam_yaw += g_look_x * 2.6f * dt; g_look_hold = 1.2f; }
    else { g_look_hold -= dt; if (g_look_hold <= 0) g_cam_yaw += dy * k * (fabsf(d->speed) > 0.3f || d->mode == DM_AIR ? 1.0f : 0.35f); }
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
/* small solid shapes for the food, lit by their normals, flat-coloured (they use the white block in the font texture) */
static void bq_push(Vec3 p, Vec3 n, float r, float g, float b) { if (g_b.n < g_b.cap) { Vertex q = {p.x, p.y, p.z, n.x, n.y, n.z, WHITE_U, WHITE_V, r, g, b, 1}; g_b.v[g_b.n++] = q; } }
static void batch_ball(Vec3 c, float rx, float ry, float rz, int la, int lo, float r, float g, float b, float ybias) {
    for (int i = 0; i < la; i++) for (int j = 0; j < lo; j++) {
        float t0 = 3.14159265f * i / la, t1 = 3.14159265f * (i + 1) / la, p0 = 6.2831853f * j / lo, p1 = 6.2831853f * (j + 1) / lo;
        Vec3 n00 = v3(sinf(t0) * sinf(p0), cosf(t0), sinf(t0) * cosf(p0)), n01 = v3(sinf(t0) * sinf(p1), cosf(t0), sinf(t0) * cosf(p1)), n10 = v3(sinf(t1) * sinf(p0), cosf(t1), sinf(t1) * cosf(p0)), n11 = v3(sinf(t1) * sinf(p1), cosf(t1), sinf(t1) * cosf(p1));
        Vec3 q00 = v3(c.x + n00.x * rx, c.y + n00.y * ry * (n00.y > 0 ? 1 : ybias), c.z + n00.z * rz), q01 = v3(c.x + n01.x * rx, c.y + n01.y * ry * (n01.y > 0 ? 1 : ybias), c.z + n01.z * rz);
        Vec3 q10 = v3(c.x + n10.x * rx, c.y + n10.y * ry * (n10.y > 0 ? 1 : ybias), c.z + n10.z * rz), q11 = v3(c.x + n11.x * rx, c.y + n11.y * ry * (n11.y > 0 ? 1 : ybias), c.z + n11.z * rz);
        if (i > 0) { bq_push(q00, n00, r, g, b); bq_push(q10, n10, r, g, b); bq_push(q01, n01, r, g, b); }
        if (i < la - 1) { bq_push(q01, n01, r, g, b); bq_push(q10, n10, r, g, b); bq_push(q11, n11, r, g, b); } } }
static void batch_cyl(Vec3 base, Vec3 top, float r0, float r1, int seg, float r, float g, float b) {
    for (int i = 0; i < seg; i++) { float a0 = i * 6.2831853f / seg, a1 = (i + 1) * 6.2831853f / seg; Vec3 n0 = v3(sinf(a0), 0, cosf(a0)), n1 = v3(sinf(a1), 0, cosf(a1));
        Vec3 b0 = v3(base.x + n0.x * r0, base.y, base.z + n0.z * r0), b1 = v3(base.x + n1.x * r0, base.y, base.z + n1.z * r0), t0 = v3(top.x + n0.x * r1, top.y, top.z + n0.z * r1), t1 = v3(top.x + n1.x * r1, top.y, top.z + n1.z * r1);
        bq_push(b0, n0, r, g, b); bq_push(t0, n0, r, g, b); bq_push(b1, n1, r, g, b); bq_push(b1, n1, r, g, b); bq_push(t0, n0, r, g, b); bq_push(t1, n1, r, g, b); } }
static void draw_food3d(Vec3 eye) {
    batch_clear(&g_b);
    for (int i = 0; i < g_world.nfood; i++) { const Food *f = &g_world.food[i]; if (!f->alive) continue;
        float dx = f->x - eye.x, dz = f->z - eye.z; float d2 = dx * dx + dz * dz; if (d2 > 120 * 120) continue; int lod = d2 > 35 * 35;
        if (f->kind == F_BERRY) {   /* red berries sitting on the (static) bush */
            unsigned s = (unsigned)i * 97 + 13; for (int k = 0; k < 6; k++) { float a = frand01(&s) * 6.28f, el = 0.3f + frand01(&s) * 1.1f, rr = 0.55f; Vec3 c = v3(f->x + cosf(el) * sinf(a) * rr, f->y + 0.4f + sinf(el) * 0.45f, f->z + cosf(el) * cosf(a) * rr);
                batch_ball(c, 0.1f, 0.1f, 0.1f, lod ? 2 : 3, lod ? 4 : 5, 0.9f, 0.12f, 0.2f, 1); } }
        else if (f->kind == F_MUSHROOM) { float h = 0.42f + 0.05f * sinf(f->phase); batch_cyl(v3(f->x, f->y - 0.05f, f->z), v3(f->x, f->y + h, f->z), 0.09f, 0.08f, 6, 0.93f, 0.88f, 0.75f);
            batch_ball(v3(f->x, f->y + h, f->z), 0.32f, 0.2f, 0.32f, 3, lod ? 6 : 8, 0.85f, 0.2f, 0.18f, 0.25f);
            if (!lod) { unsigned s = (unsigned)i * 31; for (int k = 0; k < 4; k++) { float a = frand01(&s) * 6.28f, rr = 0.1f + frand01(&s) * 0.15f; batch_ball(v3(f->x + sinf(a) * rr, f->y + h + 0.2f * sqrtf(fmaxf(0, 1 - (rr / 0.32f) * (rr / 0.32f))) , f->z + cosf(a) * rr), 0.045f, 0.03f, 0.045f, 2, 4, 1, 0.98f, 0.9f, 1); } } }
        else if (f->kind == F_GRUB) { Vec3 fw = v3(sinf(f->yaw), 0, cosf(f->yaw)); float wig = sinf(f->phase * 10) * 0.05f; Vec3 rt = v3(fw.z, 0, -fw.x);
            for (int k = 0; k < 3; k++) { float o = 0.14f - k * 0.14f; float w = sinf(f->phase * 10 + k * 1.5f) * 0.04f; batch_ball(v3(f->x + fw.x * o + rt.x * (wig + w), f->y + 0.11f, f->z + fw.z * o + rt.z * (wig + w)), 0.13f, 0.11f, 0.15f, 3, lod ? 5 : 6, 0.9f, 0.78f, 0.45f, 1); }
            batch_ball(v3(f->x + fw.x * 0.3f + rt.x * wig, f->y + 0.12f, f->z + fw.z * 0.3f + rt.z * wig), 0.1f, 0.1f, 0.1f, 3, 5, 0.45f, 0.28f, 0.15f, 1); }
        else { Vec3 fw = v3(sinf(f->yaw), 0, cosf(f->yaw)); Vec3 rt = v3(fw.z, 0, -fw.x); float sw = sinf(f->phase * 6) * 0.08f;
            batch_ball(v3(f->x, f->y, f->z), 0.1f, 0.14f, 0.1f, 3, 6, 0.4f, 0.6f, 0.9f, 1); batch_ball(v3(f->x + fw.x * 0.18f, f->y, f->z + fw.z * 0.18f), 0.08f, 0.1f, 0.08f, 2, 5, 0.4f, 0.6f, 0.9f, 1); batch_ball(v3(f->x - fw.x * 0.2f + rt.x * sw, f->y, f->z - fw.z * 0.2f + rt.z * sw), 0.06f, 0.1f, 0.06f, 2, 4, 0.3f, 0.5f, 0.8f, 1);
            Vec3 t0 = v3(f->x - fw.x * 0.26f + rt.x * sw, f->y, f->z - fw.z * 0.26f + rt.z * sw), t1 = v3(f->x - fw.x * 0.42f + rt.x * sw * 2, f->y + 0.12f, f->z - fw.z * 0.42f + rt.z * sw * 2), t2 = v3(f->x - fw.x * 0.42f + rt.x * sw * 2, f->y - 0.12f, f->z - fw.z * 0.42f + rt.z * sw * 2);
            bq_push(t0, rt, 0.3f, 0.5f, 0.8f); bq_push(t1, rt, 0.3f, 0.5f, 0.8f); bq_push(t2, rt, 0.3f, 0.5f, 0.8f); bq_push(t0, v3(-rt.x, 0, -rt.z), 0.3f, 0.5f, 0.8f); bq_push(t2, v3(-rt.x, 0, -rt.z), 0.3f, 0.5f, 0.8f); bq_push(t1, v3(-rt.x, 0, -rt.z), 0.3f, 0.5f, 0.8f); } }
    for (int i = 0; i < g_world.ndrone; i++) { const Drone *dr = &g_world.drone[i]; float dx = dr->x - eye.x, dz = dr->z - eye.z; if (dx * dx + dz * dz > 160 * 160) continue;
        Vec3 fw = v3(sinf(dr->yaw), 0, cosf(dr->yaw)), rt = v3(fw.z, 0, -fw.x); float fl = sinf(dr->flap) * (dr->state == 3 ? 0.1f : 0.6f);
        batch_ball(v3(dr->x, dr->y, dr->z), 0.28f, 0.22f, 0.5f, 3, 6, 0.08f, 0.08f, 0.1f, 1);                                    /* body */
        batch_ball(v3(dr->x + fw.x * 0.55f, dr->y + 0.08f, dr->z + fw.z * 0.55f), 0.16f, 0.14f, 0.2f, 3, 5, 0.1f, 0.1f, 0.12f, 1);   /* head */
        batch_ball(v3(dr->x + fw.x * 0.68f, dr->y + 0.12f, dr->z + fw.z * 0.68f), 0.06f, 0.06f, 0.06f, 2, 4, 1.0f, 0.1f, 0.05f, 1);  /* the red eye */
        for (int sgn = -1; sgn <= 1; sgn += 2) { Vec3 a = v3(dr->x + rt.x * sgn * 0.25f, dr->y + 0.05f, dr->z + rt.z * sgn * 0.25f), b = v3(dr->x + rt.x * sgn * 1.3f, dr->y + 0.05f + fl * 0.9f, dr->z + rt.z * sgn * 1.3f);
            Vec3 a2 = v3(a.x - fw.x * 0.35f, a.y, a.z - fw.z * 0.35f), b2 = v3(b.x - fw.x * 0.5f, b.y, b.z - fw.z * 0.5f); Vec3 n = v3(0, 1, 0);
            bq_push(a, n, 0.12f, 0.12f, 0.14f); bq_push(b, n, 0.12f, 0.12f, 0.14f); bq_push(b2, n, 0.12f, 0.12f, 0.14f); bq_push(a, n, 0.12f, 0.12f, 0.14f); bq_push(b2, n, 0.12f, 0.12f, 0.14f); bq_push(a2, n, 0.12f, 0.12f, 0.14f);
            n = v3(0, -1, 0); bq_push(a, n, 0.1f, 0.1f, 0.12f); bq_push(b2, n, 0.1f, 0.1f, 0.12f); bq_push(b, n, 0.1f, 0.1f, 0.12f); bq_push(a, n, 0.1f, 0.1f, 0.12f); bq_push(a2, n, 0.1f, 0.1f, 0.12f); bq_push(b2, n, 0.1f, 0.1f, 0.12f); } }
    for (int i = 0; i < g_world.nhound; i++) { const Drone *h = &g_world.hound[i]; float dx = h->x - eye.x, dz = h->z - eye.z; if (dx * dx + dz * dz > 160 * 160) continue;
        Vec3 fw = v3(sinf(h->yaw), 0, cosf(h->yaw)), rt = v3(fw.z, 0, -fw.x); int ko = h->state == 3; float gait = sinf(h->flap);
        float by = h->y + (ko ? 0.25f : 0.55f); float gl = 0.6f + 0.4f * sinf(g_time * 8);
        batch_ball(v3(h->x, by, h->z), 0.3f, 0.3f, 0.6f, 4, 7, 0.1f, 0.1f, 0.12f, 1);                                                     /* body */
        batch_ball(v3(h->x + fw.x * 0.75f, by + 0.15f, h->z + fw.z * 0.75f), 0.22f, 0.2f, 0.3f, 3, 6, 0.12f, 0.12f, 0.14f, 1);              /* head */
        batch_ball(v3(h->x + fw.x * 0.5f, by + 0.1f, h->z + fw.z * 0.5f), 0.26f, 0.08f, 0.26f, 2, 8, ko ? 0.3f : gl, ko ? 0.05f : 0.15f * gl, 0.05f, 1);   /* the zapping collar */
        for (int k = 0; k < 2; k++) { int sgn = k ? 1 : -1; batch_ball(v3(h->x + fw.x * 0.85f + rt.x * sgn * 0.1f, by + 0.2f, h->z + fw.z * 0.85f + rt.z * sgn * 0.1f), 0.04f, 0.04f, 0.04f, 2, 4, 1, 0.1f, 0.05f, 1); }   /* eyes */
        for (int k = 0; k < 4; k++) { float ox = (k & 1 ? 0.22f : -0.22f), oz = (k < 2 ? 0.35f : -0.35f); float sw = ko ? 0 : (k == 0 || k == 3 ? gait : -gait) * 0.25f;
            Vec3 top = v3(h->x + rt.x * ox + fw.x * oz, by - 0.1f, h->z + rt.z * ox + fw.z * oz), bot = v3(top.x + fw.x * sw, h->y + (ko ? 0.05f : 0), top.z + fw.z * sw); batch_cyl(bot, top, 0.06f, 0.08f, 5, 0.15f, 0.15f, 0.17f); } }
    if (!g_world.crystal_found) { float cx = g_world.crystal_x, cz = g_world.crystal_z, cy = g_world.crystal_y + 0.9f + 0.15f * sinf(g_time * 2); float gl = 0.8f + 0.2f * sinf(g_time * 5);
        /* an egg-shaped crystal, slowly turning, and a halo of six small shards */
        float rot = g_time * 0.8f; batch_ball(v3(cx, cy, cz), 0.32f, 0.5f, 0.32f, 4, 6, 0.55f * gl, 0.95f * gl, 1.0f * gl, 1.0f);
        for (int k = 0; k < 6; k++) { float a = rot + k * 1.047f; batch_ball(v3(cx + cosf(a) * 0.75f, cy + 0.25f * sinf(g_time * 3 + k), cz + sinf(a) * 0.75f), 0.07f, 0.16f, 0.07f, 3, 4, 0.8f * gl, 1.0f * gl, 1.0f, 1.0f); } }
    if (!g_b.n) return; gfx_mesh_update(m_dynfood, g_b.v, g_b.n); Mat4 id = m4_identity(); gfx_bind(tx_font); gfx_state(ST_SOLID); gfx_draw_ex(m_dynfood, &id, 1, 1, 1, 1, 0.3f, 0, 0, 0); }

static void draw_dragon(const Dragon *d) {
    int st = d->stage; float sc = dragon_scale(st);
    if (d->evolve_t > 0) {   /* grows from the old size to the new in the first second, with a glow */
        float t = clampf(1 - d->evolve_t / 2.0f, 0, 1); float from = dragon_scale(st - 1 >= 0 ? st - 1 : 0); sc = from + (sc - from) * clampf(t * 1.6f, 0, 1); }
    Anim3D *m = &g_model[st]; if (!m->ok) m = g_model_ok[0] ? &g_model[0] : NULL; if (!m) return;
    Mat4 base = dragon_matrix(d, sc); int clip = anim3d_clip(m, dragon_anim_name(d->anim)); if (clip < 0) clip = 0;
    gfx_state(ST_SOLID);
    float glow = d->evolve_t > 0 ? 0.5f + 0.5f * sinf(g_time * 20) : 0;
    anim3d_draw(m, clip, d->anim_t, &base, 1.12f + glow, 1.12f + glow, 1.1f + 0.6f * glow, 1); gfx_bind(NULL); }

static void draw_sparkles(const Dragon *d, Vec3 eye) {
    if (d->evolve_t <= 0) return; batch_clear(&g_b); unsigned s = 9; float L = dragon_length(d->stage); float t = 2.0f - d->evolve_t;
    Vec3 right = v3_norm(v3_cross(v3(0, 1, 0), v3_sub(d->x < eye.x ? v3(d->x, d->y, d->z) : v3(d->x, d->y, d->z), eye))); Vec3 up = v3(0, 1, 0);
    for (int i = 0; i < 60; i++) { float a = frand01(&s) * 6.28f, r = frand01(&s) * L * 0.9f, h = frand01(&s) * L * 0.8f, ph = frand01(&s) * 6.28f;
        float rise = fmodf(t * (0.8f + frand01(&s)) + ph, 2.0f); float sz = 0.08f * L * (1 - rise / 2) + 0.05f;
        Vec3 c = v3(d->x + cosf(a + t) * r, d->y + h + rise * L * 0.6f, d->z + sinf(a + t) * r);
        batch_quad(&g_b, c, v3(right.x * sz, 0, right.z * sz), v3(up.x * sz, up.y * sz, up.z * sz), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 0.7f, 1 - rise / 2); }
    gfx_mesh_update(m_dynspark, g_b.v, g_b.n); Mat4 id = m4_identity(); gfx_bind(tx_spark); gfx_state(ST_DEPTH | ST_BLEND | ST_ADD); gfx_draw_ex(m_dynspark, &id, 1, 1, 1, 1, 0, 1, 0, 0); }

static void draw_scene(int drc) {
    float aspect = gfx_aspect(drc); Vec3 eye = g_eye, at = g_at;
    Mat4 proj = m4_perspective(g_cam_mode == 2 ? 1.25f : 1.05f, aspect, 0.2f, 1500.0f), view = m4_look_at(eye, at, v3(0, 1, 0)); Mat4 vp = m4_mul(proj, view);
    Vec3 L = sun_dir();
    gfx_grade(0.8f, 0.45f, 0.6f, 0.95f); gfx_light(v3(1.0f, 0.96f, 0.85f), v3(0.6f, 0.7f, 0.85f), v3(0.38f, 0.42f, 0.25f), eye);
    gfx_shadow_use(1); gfx_camera(&vp, L, 0.45f, 0.74f, 0.82f, 0.92f, 120, 480); gfx_shadow_use(0);
    Mat4 id = m4_identity();
    /* sky, around the camera, no depth */
    Mat4 skym = m4_translate(eye.x, eye.y - 60, eye.z); gfx_state(0); gfx_bind(NULL); gfx_draw_ex(m_sky, &skym, 1, 1, 1, 1, 0, 1, 0, 0);
    if (m_mountains) gfx_draw_ex(m_mountains, &id, 1, 1, 1, 1, 0, 1, 0, 0);
    gfx_clear_depth(drc);
    gfx_state(ST_SOLID);
    gfx_bind(tx_grass); gfx_bump(2); gfx_draw(m_ground, &id, 1, 1, 1, 1); gfx_bump(0);
    gfx_bind(tx_bark); if (m_bark) gfx_draw(m_bark, &id, 1, 1, 1, 1);
    gfx_bind(tx_rock); if (m_rock) gfx_draw(m_rock, &id, 1, 1, 1, 1);
    gfx_state(ST_CUTOUT);
    gfx_bind(tx_leaves); if (m_leaves) gfx_draw(m_leaves, &id, 1, 1, 1, 1);
    gfx_bind(tx_fern); if (m_fern) gfx_draw(m_fern, &id, 1, 1, 1, 1);
    gfx_bind(tx_bush); if (m_bushes) gfx_draw(m_bushes, &id, 1, 1, 1, 1);
    gfx_bind(tx_tuft); if (m_tufts) gfx_draw(m_tufts, &id, 1, 1, 1, 1);
    gfx_bind(tx_flower); if (m_flowers) gfx_draw(m_flowers, &id, 1, 1, 1, 1);
    draw_food3d(eye);
    if (g_cam_mode != 2) draw_dragon(&g_dragon);
    gfx_state(ST_TRANSPARENT); gfx_bind(tx_water); gfx_draw_ex(m_water, &id, 1, 1, 1, 0.8f, 0.9f, 0, g_time * 0.02f, g_time * 0.013f);
    draw_sparkles(&g_dragon, eye);
    { Mat4 cm = m4_translate(fmodf(g_time * 1.5f, 900.0f) - 450.0f, 0, 0); gfx_state(ST_DEPTH | ST_BLEND); gfx_bind(tx_cloud); if (m_clouds) { gfx_draw_ex(m_clouds, &cm, 1, 1, 1, 0.9f, 0, 1, 0, 0); Mat4 cm2 = m4_translate(cm.m[12] + 900.0f, 0, 0); gfx_draw_ex(m_clouds, &cm2, 1, 1, 1, 0.9f, 0, 1, 0, 0); } }
    gfx_bind(NULL); }

static void shadow_pass(void) {
    const Dragon *d = &g_dragon; Vec3 L = sun_dir();
    Vec3 c = v3(d->x + sinf(g_cam_yaw) * 25, d->y, d->z + cosf(g_cam_yaw) * 25); const float half = 60.0f, dep = 300.0f;
    c.x = floorf(c.x / (2 * half / 1024)) * (2 * half / 1024); c.z = floorf(c.z / (2 * half / 1024)) * (2 * half / 1024);
    Mat4 view = m4_look_at(v3(c.x + L.x * dep * 0.5f, c.y + L.y * dep * 0.5f, c.z + L.z * dep * 0.5f), c, v3(0, 1, 0));
    Mat4 proj = m4_identity(); proj.m[0] = 1.0f / half; proj.m[5] = 1.0f / half; proj.m[10] = -2.0f / dep; proj.m[14] = -1.0f;
    Mat4 lvp = m4_mul(proj, view), id = m4_identity();
    if (!gfx_shadow_begin(&lvp)) return;
    gfx_state(ST_SOLID); if (m_bark) gfx_draw(m_bark, &id, 1, 1, 1, 1); if (m_rock) gfx_draw(m_rock, &id, 1, 1, 1, 1);
    draw_dragon(d);
    gfx_shadow_end(); }

static void text_wrap_c(const char *txt, float y, float size, float maxw, float r, float g, float b, float a);
static void text_c(const char *s, float y, float size, float r, float g, float b, float a) { batch_text(&g_b, s, 640 - text_width(s, size) / 2, y, size, r, g, b, a); }
static void draw_hud(int drc) {
    const Dragon *d = &g_dragon; batch_clear(&g_b);
    /* hearts */
    for (int i = 0; i < (int)d->max_hearts; i++) { float full = clampf(d->hearts - i, 0, 1); batch_rect(&g_b, 40 + i * 44, 36, 34, 30, 0.25f, 0.05f, 0.08f, 0.8f); if (full > 0) batch_rect(&g_b, 43 + i * 44, 39 + 24 * (1 - full), 28, 24 * full, 0.95f, 0.2f, 0.3f, 1); }
    /* growth bar */
    float need = dragon_need(d->stage); float gf = need > 1e8f ? 1 : clampf(d->growth / need, 0, 1);
    batch_rect(&g_b, 40, 80, 360, 22, 0.05f, 0.1f, 0.05f, 0.8f); batch_rect(&g_b, 43, 83, 354 * gf, 16, 0.45f, 0.9f, 0.3f, 1);
    { char nm[48]; snprintf(nm, sizeof nm, "%s THE %s", NAMES[g_name], dragon_stage_name(d->stage)); batch_text(&g_b, nm, 40, 108, 26, 0.9f, 1, 0.8f, 1); }
    if (need < 1e8f) { char s[32]; snprintf(s, sizeof s, "GROW %d/%d", (int)d->growth, (int)need); batch_text(&g_b, s, 410, 80, 22, 0.9f, 1, 0.8f, 0.9f); }
    /* stamina while flying */
    if (d->mode == DM_AIR && d->stage >= ST_DRAGON) { batch_rect(&g_b, 40, 140, 200, 12, 0.05f, 0.08f, 0.15f, 0.8f); batch_rect(&g_b, 42, 142, 196 * d->stamina, 8, 0.5f, 0.8f, 1, 1); }
    /* prompts */
    float L = dragon_length(d->stage);
    int near = world_nearest_food(&g_world, d->x + sinf(d->yaw) * 0.45f * L, d->z + cosf(d->yaw) * 0.45f * L, 0.5f * L + 1.2f);
    if (near >= 0 && d->mode != DM_AIR && d->act_t <= 0) text_c(food_is_meat(g_world.food[near].kind) ? "X : EAT (MEAT, GROWS FAST)" : "X : EAT", 555, 30, 1, 1, 0.7f, 0.9f);
    if (g_msg_t > 0) text_wrap_c(g_msg, 630, 28, 1180, 1, 1, 1, clampf(g_msg_t, 0, 1));
    if (g_title_t > 0) { float a = clampf(g_title_t, 0, 1); text_c("DRAGONS BEYOND", 250, 72, 0.6f, 1, 0.5f, a); text_c("EVERLEAF WILDS", 330, 40, 0.9f, 1, 0.9f, a); }
    static const char *cams[3] = {"RIDER VIEW", "OVERHEAD VIEW", "DRAGON EYES"}; batch_text(&g_b, cams[g_cam_mode], 1240 - text_width(cams[g_cam_mode], 20), 36, 20, 1, 1, 1, 0.6f);
    batch_text(&g_b, "D-PAD UP : VIEW   A : JUMP/FLY   ZR : RUN   B : TAIL   Y : ROAR", 300, 690, 18, 1, 1, 1, 0.5f);
    if (!g_b.n) return; gfx_camera_2d(); gfx_state(ST_HUD); gfx_bind(tx_font); gfx_mesh_update(m_dynhud, g_b.v, g_b.n); Mat4 id = m4_identity(); gfx_draw_ex(m_dynhud, &id, 1, 1, 1, 1, 0, 1, 0, 0); gfx_bind(NULL); (void)drc; }


#ifdef DB_TOUR
/* the tour build plays itself (for the emulator screenshots in CI): walks to food and eats until it grows, then flies, changing views as it goes */
static void tour_input(Pad *p, float t) {
    static float last_eat = -9; static int prev_up = 0;
    memset(p, 0, sizeof *p); p->source = 1;
    const Dragon *d = &g_dragon;
    if (t < 4) return; if (g_state != G_PLAY) { p->a = fmodf(t, 0.9f) < 0.12f && (g_state != G_NAME || t > 6); return; }
    int wantcam = t < 45 ? 0 : (t < 58 ? 1 : (t < 70 ? 2 : 0)); int press = wantcam != g_cam_mode && !prev_up; p->up = press; prev_up = press;
    if (d->stage == ST_DRAGONET || d->mode != DM_AIR) {
        int near = world_nearest_food(&g_world, d->x, d->z, 400);
        if (near >= 0 && d->stage == ST_DRAGONET) { const Food *f = &g_world.food[near]; float dx = f->x - d->x, dz = f->z - d->z, dist = sqrtf(dx * dx + dz * dz);
            float ang = atan2f(dx, dz) - g_cam_yaw; if (dist > 1.0f) { p->sx = sinf(ang); p->sy = cosf(ang); p->zr = dist > 6; } else if (d->act_t <= 0 && t - last_eat > 0.5f) { p->x = 1; last_eat = t; } }
        else if (d->stage >= ST_DRAGON && d->evolve_t <= 0 && d->act_t <= 0) { p->a = fmodf(t, 1.0f) < 0.5f; p->sy = 1; }   /* take off (tap A) */
    } else { p->a = fmodf(t, 6.0f) < 3.5f; p->sy = 0.4f; p->sx = 0.5f * sinf(t * 0.4f); }   /* flap in bursts, circle */
    if (fmodf(t, 30.0f) < 0.05f && t > 25 && d->mode != DM_AIR) p->y = 1;   /* a roar now and then */
}
#endif


/* ---------- the GamePad: a map of Everleaf Wilds ---------- */
static void map_tri2d(float x0, float y0, float x1, float y1, float x2, float y2, float r, float g, float b, float a) {
    if (g_b.n + 3 > g_b.cap) return; Vec3 p0 = scr(x0, y0), p1 = scr(x1, y1), p2 = scr(x2, y2);
    Vertex q0 = {p0.x, p0.y, 0, 0, 0, 1, WHITE_U, WHITE_V, r, g, b, a}, q1 = {p1.x, p1.y, 0, 0, 0, 1, WHITE_U, WHITE_V, r, g, b, a}, q2 = {p2.x, p2.y, 0, 0, 0, 1, WHITE_U, WHITE_V, r, g, b, a};
    g_b.v[g_b.n++] = q0; g_b.v[g_b.n++] = q1; g_b.v[g_b.n++] = q2; }
static void map_disc(float cx, float cy, float rad, int seg, float r, float g, float b, float a) { for (int i = 0; i < seg; i++) { float a0 = i * 6.2831853f / seg, a1 = (i + 1) * 6.2831853f / seg; map_tri2d(cx, cy, cx + cosf(a1) * rad, cy + sinf(a1) * rad, cx + cosf(a0) * rad, cy + sinf(a0) * rad, r, g, b, a); } }
static void draw_map(void) {
    const Dragon *d = &g_dragon; const float S = 2.6f, CX = 640, CY = 360;   /* pixels per metre; world x -> right, world z -> up the screen */
#define MX(x) (CX + (x) * S)
#define MY(z) (CY - (z) * S)
    batch_clear(&g_b);
    batch_rect(&g_b, 0, 0, 1280, 720, 0.08f, 0.14f, 0.08f, 1);
    batch_rect(&g_b, MX(-W_SIZE / 2), MY(W_SIZE / 2), W_SIZE * S, W_SIZE * S, 0.3f, 0.55f, 0.25f, 1);
    /* hills: darker where the ground is high */
    for (int j = 0; j < W_GRID - 1; j += 2) for (int i = 0; i < W_GRID - 1; i += 2) { float h = g_world.h[j][i]; if (h < 6) continue; float t = clampf((h - 6) / 20.0f, 0, 1); float x = -W_SIZE / 2 + W_SIZE * i / (W_GRID - 1), z = -W_SIZE / 2 + W_SIZE * j / (W_GRID - 1); batch_rect(&g_b, MX(x), MY(z) - 2 * S, 2 * S * W_SIZE / (W_GRID - 1) / 2.5f + 1, 2 * S * W_SIZE / (W_GRID - 1) / 2.5f + 1, 0.3f + 0.3f * t, 0.45f + 0.1f * t, 0.2f + 0.1f * t, 0.9f); }
    map_disc(MX(g_world.pond_x), MY(g_world.pond_z), g_world.pond_r * 1.25f * S, 24, 0.25f, 0.55f, 0.85f, 1);
    for (int i = 0; i < g_world.nprop; i++) { const Prop *p = &g_world.prop[i]; float r = p->kind == P_TREE || p->kind == P_PALM ? 4.5f : 2.5f; float cr = p->kind == P_ROCK ? 0.5f : 0.12f, cg = p->kind == P_ROCK ? 0.5f : 0.35f, cb = p->kind == P_ROCK ? 0.48f : 0.1f; map_disc(MX(p->x), MY(p->z), r, 6, cr, cg, cb, 1); }
    for (int i = 0; i < g_world.nfood; i++) { const Food *f = &g_world.food[i]; if (!f->alive) continue; float cr = 1, cg = 1, cb = 1;
        if (f->kind == F_BERRY) { cr = 0.95f; cg = 0.15f; cb = 0.25f; } else if (f->kind == F_MUSHROOM) { cr = 1; cg = 0.95f; cb = 0.8f; } else if (f->kind == F_GRUB) { cr = 1; cg = 0.85f; cb = 0.3f; } else { cr = 0.5f; cg = 0.8f; cb = 1; }
        map_disc(MX(f->x), MY(f->z), 3, 6, cr, cg, cb, 1); }
    for (int i = 0; i < g_world.nhound; i++) if (g_world.hound[i].state != 3) map_disc(MX(g_world.hound[i].x), MY(g_world.hound[i].z), 4, 4, 0.9f, 0.3f, 0.1f, 1);
    for (int i = 0; i < g_world.ndrone; i++) if (g_world.drone[i].state != 3) map_disc(MX(g_world.drone[i].x), MY(g_world.drone[i].z), 4, 6, 0.9f, 0.2f, 0.1f, 1);
    if (!g_world.crystal_found && d->stage >= ST_DRAGON) map_disc(MX(g_world.crystal_x), MY(g_world.crystal_z), 5 + 2 * sinf(g_time * 3), 8, 0.6f, 0.95f, 1, 0.8f);
    /* the dragon: a bright arrow pointing where it faces, with a pulse ring */
    float ax = MX(d->x), ay = MY(d->z), sy = sinf(d->yaw), cy = cosf(d->yaw); float pulse = 10 + 4 * sinf(g_time * 4);
    map_disc(ax, ay, pulse, 16, 0.7f, 1, 0.5f, 0.35f);
    map_tri2d(ax + sy * 14, ay - cy * 14, ax - cy * 8 - sy * 8, ay - sy * 8 + cy * 8, ax + cy * 8 - sy * 8, ay + sy * 8 + cy * 8, 0.85f, 1, 0.6f, 1);
    /* words */
    batch_text(&g_b, "EVERLEAF WILDS", 40, 30, 34, 0.9f, 1, 0.8f, 1);
    char line[64]; snprintf(line, sizeof line, "%s", dragon_stage_name(d->stage)); batch_text(&g_b, line, 40, 80, 26, 1, 1, 1, 0.9f);
    float need = dragon_need(d->stage); if (need < 1e8f) { snprintf(line, sizeof line, "GROW %d/%d", (int)d->growth, (int)need); batch_text(&g_b, line, 40, 112, 22, 0.8f, 1, 0.7f, 0.9f); }
    batch_text(&g_b, "RED: BERRIES  WHITE: MUSHROOMS", 40, 640, 18, 1, 1, 1, 0.7f); batch_text(&g_b, "YELLOW: GRUBS  BLUE: FISH (MEAT)  DARK RED: DRONES", 40, 668, 18, 1, 1, 1, 0.7f);
    batch_text(&g_b, "D-PAD UP: CHANGE VIEW", 900, 668, 18, 1, 1, 1, 0.7f);
    gfx_camera_2d(); gfx_state(ST_HUD); gfx_bind(tx_font); gfx_mesh_update(m_dynhud, g_b.v, g_b.n); Mat4 id = m4_identity(); gfx_draw_ex(m_dynhud, &id, 1, 1, 1, 1, 0, 1, 0, 0); gfx_bind(NULL);
#undef MX
#undef MY
}



static const char *STORY[] = {
    "ONE NIGHT, SEVEN DRAGON EGGS HATCHED ACROSS THE WORLD. IN THE DESERT, THE CRYSTAL PALACE, THE VOLCANIC SWIRL, THE SEA, THE WINTER PALACE, THE SKY, AND THE FOREST.",
    "THE LITTLE DRAGONETS BLINKED UP AT THE STARS AND WAITED FOR THEIR PARENTS. NO ONE CAME.",
    "THAT SAME NIGHT, A SHADOW CREPT FROM NEST TO NEST. ONE BY ONE, THE DRAGONETS WERE SNATCHED AWAY AND LOCKED IN A CAVE.",
    "A HUMAN TOSSED THEM MEAT AND LAUGHED. HIS NAME WAS KAINE BLACKFEATHER, AND HE HAD PLANS FOR THEM.",
    "BUT HE MISSED ONE. DEEP IN EVERLEAF WILDS, A FOREST DRAGONET WOKE UP ALONE, SMALL, AND VERY HUNGRY.",
    "SOMEDAY THIS LITTLE DRAGON WOULD HAVE TO SAVE THEM ALL. BUT FIRST... BREAKFAST." };
#define STORY_N 6
static int g_story_i;
/* words wrapped into lines no wider than maxw, centred */
static void text_wrap_c(const char *txt, float y, float size, float maxw, float r, float g, float b, float a) {
    char line[160]; int ln = 0; const char *w = txt;
    while (*w) { const char *e = w; while (*e && *e != ' ') e++; int wl = (int)(e - w); char cand[160]; int cl = ln; memcpy(cand, line, (size_t)ln);
        if (cl) cand[cl++] = ' '; memcpy(cand + cl, w, (size_t)wl); cl += wl; cand[cl] = 0;
        if (ln && text_width(cand, size) > maxw) { line[ln] = 0; text_c(line, y, size, r, g, b, a); y += size * 1.25f; ln = 0; memcpy(line, w, wl); ln = wl; }
        else { memcpy(line, cand, cl + 1); ln = cl; }
        w = *e ? e + 1 : e; }
    if (ln) { line[ln] = 0; text_c(line, y, size, r, g, b, a); } }

/* ---------- title and name screens: dragons flying everywhere over the meadow ---------- */
static void draw_title_dragons(void) {
    /* three dragons (one of each size) circling at different heights, plus the dragonet looping low */
    for (int k = 0; k < 4; k++) { int st = k % 3; Anim3D *m = &g_model[st]; if (!m->ok) continue; float sc = dragon_scale(st) * (k == 3 ? 1 : 1.0f);
        float R = 15 + k * 9, w = (0.5f - k * 0.07f) * (k & 1 ? -1 : 1), a = g_time * w + k * 1.7f, h = 7 + k * 5 + 2.5f * sinf(g_time * 0.8f + k);
        Vec3 pos = v3(cosf(a) * R, h, sinf(a) * R); float yaw = atan2f(-sinf(a) * w, cosf(a) * w) ;   /* face along the circle */
        Mat4 base = m4_mul(m4_mul(m4_translate(pos.x, pos.y, pos.z), m4_rotate_y(yaw)), m4_mul(m4_rotate_z(w > 0 ? -0.35f : 0.35f), m4_scale(sc, sc, sc)));
        int clip = anim3d_clip(m, "fly"); gfx_state(ST_SOLID); anim3d_draw(m, clip < 0 ? 0 : clip, g_time + k * 0.3f, &base, 1.12f, 1.12f, 1.1f, 1); }
    gfx_bind(NULL); }
static void title_camera(void) {
    float a = g_time * 0.07f; g_eye = v3(cosf(a) * 14, world_height(&g_world, cosf(a) * 14, sinf(a) * 14) + 3.5f, sinf(a) * 14); g_at = v3(-cosf(a) * 10, 16, -sinf(a) * 10); }
static void draw_title_hud(void) {
    batch_clear(&g_b);
    float pulse = 0.75f + 0.25f * sinf(g_time * 3);
    if (g_state == G_TITLE) {
        batch_rect(&g_b, 0, 150, 1280, 190, 0, 0.08f, 0, 0.35f);
        text_c("DRAGONS BEYOND", 174, 74, 0.35f, 0.7f, 0.3f, 1); text_c("DRAGONS BEYOND", 170, 74, 0.9f, 1, 0.8f, 1);
        text_c("EVERLEAF WILDS", 272, 34, 0.9f, 1, 0.9f, 0.9f);
        text_c(g_dragon.stage > 0 || g_dragon.growth > 0 ? "PRESS A TO CONTINUE" : "PRESS A", 600, 40, 1, 1, 1, pulse);
        text_c("A GAME DESIGNED BY A 10 YEAR OLD DRAGON EXPERT", 690, 18, 1, 1, 1, 0.6f);
    } else if (g_state == G_NAME) {
        batch_rect(&g_b, 140, 120, 1000, 480, 0, 0.06f, 0, 0.6f);
        text_c("WHO IS YOUR FOREST DRAGON?", 150, 44, 0.9f, 1, 0.8f, 1);
        for (int i = 0; i < 2; i++) { float x = 240 + i * 440, y = 260; int sel = g_name == i;
            batch_rect(&g_b, x, y, 360, 230, sel ? 0.25f : 0.1f, sel ? 0.5f : 0.2f, sel ? 0.2f : 0.1f, 0.9f);
            if (sel) { batch_rect(&g_b, x - 6, y - 6, 372, 6, 1, 1, 0.6f, pulse); batch_rect(&g_b, x - 6, y + 230, 372, 6, 1, 1, 0.6f, pulse); batch_rect(&g_b, x - 6, y, 6, 230, 1, 1, 0.6f, pulse); batch_rect(&g_b, x + 360, y, 6, 230, 1, 1, 0.6f, pulse); }
            batch_text(&g_b, NAMES[i], x + 180 - text_width(NAMES[i], 40) / 2, y + 40, 40, 1, 1, 1, 1);
            batch_text(&g_b, i == 0 ? "GIRL" : "BOY", x + 180 - text_width(i == 0 ? "GIRL" : "BOY", 26) / 2, y + 110, 26, 0.9f, 1, 0.8f, 0.9f);
            batch_text(&g_b, "LEAFNADO + LEAF TAIL", x + 180 - text_width("LEAFNADO + LEAF TAIL", 18) / 2, y + 170, 18, 1, 1, 1, 0.7f); }
        text_c("LEFT / RIGHT : CHOOSE     A : START", 540, 24, 1, 1, 1, 0.8f); }
    if (g_state == G_STORY) { batch_rect(&g_b, 0, 0, 1280, 720, 0, 0, 0, 0.55f); float fade = clampf(g_state_t * 2, 0, 1);
        text_wrap_c(STORY[g_story_i], 250, 34, 1040, 1, 1, 0.9f, fade); char pg[16]; snprintf(pg, sizeof pg, "%d / %d", g_story_i + 1, STORY_N); text_c(pg, 560, 20, 1, 1, 1, 0.5f); text_c("A : NEXT", 620, 24, 1, 1, 1, pulse); }
    if (!g_b.n) return; gfx_camera_2d(); gfx_state(ST_HUD); gfx_bind(tx_font); gfx_mesh_update(m_dynhud, g_b.v, g_b.n); Mat4 id = m4_identity(); gfx_draw_ex(m_dynhud, &id, 1, 1, 1, 1, 0, 1, 0, 0); gfx_bind(NULL); }

/* ---------- the frame ---------- */
void game_frame(const Pad *pads, int npads, float dt) {
    g_time += dt; gfx_set_time(g_time);
    if (g_gfx_status != GFX_OK) { gfx_frame_begin(); gfx_view_begin(0, g_gfx_status == GFX_NO_COMPILER ? 1 : 1, g_gfx_status == GFX_NO_COMPILER ? 0.5f : 0, 0); gfx_view_end(0); gfx_view_begin(1, 1, 0.5f, 0); gfx_view_end(1); gfx_frame_end(); return; }
    /* player 1: the GamePad, or the first Wii controller if the GamePad stick is idle */
#ifdef DB_TOUR
    static Pad tourpad; static int tour_once; if (!tour_once) { tour_once = 1; g_dragon.growth = 40; }   /* one meal from growing up, so the clip reaches the flying */
    if (dt < 0.08f) dt = 0.08f;   /* the cloud emulator is slow: let game time run ahead so the clip covers more */
    tour_input(&tourpad, g_time); pads = &tourpad; npads = 1;
#endif
    const Pad *p = &pads[0]; const Pad *q = &g_prev[0];
    for (int i = 1; i < npads && i < KK_PADS; i++) if (pads[i].source && (fabsf(pads[i].sx) > 0.2f || fabsf(pads[i].sy) > 0.2f || pads[i].a || pads[i].x) && fabsf(p->sx) < 0.2f && fabsf(p->sy) < 0.2f && !p->a && !p->x) { p = &pads[i]; q = &g_prev[i]; }
    DragonInput in = {0}; in.sx = p->sx; in.sy = p->sy; in.a = p->a; in.a_press = p->a && !q->a; in.b_press = p->b && !q->b; in.x_press = p->x && !q->x; in.y_press = p->y && !q->y; in.zr = p->zr; in.zl = p->zl;
    g_state_t += dt;
    if (g_state != G_PLAY) {
        if (g_state == G_TITLE && in.a_press && g_state_t > 0.5f) { g_state = G_NAME; g_state_t = 0; sfx_play(SFX_MENU_OK, 0.5f, 0); }
        else if (g_state == G_NAME) { if ((p->left && !q->left) || (p->right && !q->right) || (fabsf(p->sx) > 0.6f && fabsf(q->sx) <= 0.6f)) { g_name ^= 1; sfx_play(SFX_MENU_MOVE, 0.5f, 0); } if (in.a_press && g_state_t > 0.3f) { g_state = G_STORY; g_state_t = 0; g_story_i = 0; } }
        else if (g_state == G_STORY) { if ((in.a_press && g_state_t > 0.4f) || g_state_t > 12) { g_state_t = 0; if (++g_story_i >= STORY_N) { g_state = G_PLAY; g_title_t = 4.0f; snprintf(g_msg, sizeof g_msg, "%s IS HUNGRY. WALK TO A BUSH AND PRESS X", NAMES[g_name]); g_msg_t = 9; } } }
        memcpy(g_prev, pads, sizeof(Pad) * (npads < KK_PADS ? npads : KK_PADS));
        music_want("title"); music_update(2);
        g_dragon.anim_t += dt; title_camera();
        gfx_frame_begin(); gfx_shadow_none();
        gfx_view_begin(0, 0.55f, 0.75f, 0.95f); { float aspect = gfx_aspect(0); Mat4 proj = m4_perspective(1.1f, aspect, 0.2f, 1500.0f), view = m4_look_at(g_eye, g_at, v3(0, 1, 0)); Mat4 vp = m4_mul(proj, view); Mat4 id = m4_identity();
            gfx_grade(0.8f, 0.45f, 0.6f, 0.95f); gfx_light(v3(1.0f, 0.96f, 0.85f), v3(0.6f, 0.7f, 0.85f), v3(0.38f, 0.42f, 0.25f), g_eye); gfx_camera(&vp, sun_dir(), 0.45f, 0.74f, 0.82f, 0.92f, 120, 480);
            Mat4 skym = m4_translate(g_eye.x, g_eye.y - 60, g_eye.z); gfx_state(0); gfx_bind(NULL); gfx_draw_ex(m_sky, &skym, 1, 1, 1, 1, 0, 1, 0, 0); if (m_mountains) gfx_draw_ex(m_mountains, &id, 1, 1, 1, 1, 0, 1, 0, 0); gfx_clear_depth(0);
            gfx_state(ST_SOLID); gfx_bind(tx_grass); gfx_bump(2); gfx_draw(m_ground, &id, 1, 1, 1, 1); gfx_bump(0); gfx_bind(tx_bark); if (m_bark) gfx_draw(m_bark, &id, 1, 1, 1, 1);
            gfx_state(ST_CUTOUT); gfx_bind(tx_leaves); if (m_leaves) gfx_draw(m_leaves, &id, 1, 1, 1, 1); gfx_bind(tx_fern); if (m_fern) gfx_draw(m_fern, &id, 1, 1, 1, 1); gfx_bind(tx_tuft); if (m_tufts) gfx_draw(m_tufts, &id, 1, 1, 1, 1);
            draw_title_dragons();
            { Mat4 cm = m4_translate(fmodf(g_time * 1.5f, 900.0f) - 450.0f, 0, 0); gfx_state(ST_DEPTH | ST_BLEND); gfx_bind(tx_cloud); if (m_clouds) { gfx_draw_ex(m_clouds, &cm, 1, 1, 1, 0.9f, 0, 1, 0, 0); Mat4 cm2 = m4_translate(cm.m[12] + 900.0f, 0, 0); gfx_draw_ex(m_clouds, &cm2, 1, 1, 1, 0.9f, 0, 1, 0, 0); } }
            draw_title_hud(); }
        gfx_view_end(0);
        gfx_view_begin(1, 0.08f, 0.14f, 0.08f); draw_title_hud(); gfx_view_end(1);
        gfx_frame_end(); return; }
    if (p->up && !q->up) { g_cam_mode = (g_cam_mode + 1) % 3; sfx_play(SFX_MENU_MOVE, 0.4f, 0); }
    g_look_x = p->rx;
    int ev = dragon_update(&g_dragon, &g_world, &in, g_cam_yaw, dt);
    world_update(&g_world, dt, g_dragon.x, g_dragon.z);
    { int whip = g_dragon.anim == DA_TAILWHIP && g_dragon.act_t > 0.35f && g_dragon.act_t < 0.6f; int roar = g_dragon.anim == DA_ROAR && g_dragon.act_t > 1.0f;
      int dev = world_drones(&g_world, dt, g_dragon.x, g_dragon.y + 0.4f * dragon_length(g_dragon.stage), g_dragon.z, 0.8f * dragon_length(g_dragon.stage) + 1.0f, whip, roar);
      if ((dev & 1) && g_dragon.evolve_t <= 0) { g_dragon.hearts -= 0.5f; g_dragon.act_t = 0.77f; g_dragon.anim = DA_HIT; g_dragon.anim_t = 0; sfx_play(SFX_HIT, 0.6f, 0); platform_rumble(0.7f, 0.3f); snprintf(g_msg, sizeof g_msg, "A FEATHER DRONE! PRESS B TO SWAT IT, Y TO SCARE IT"); g_msg_t = 4;
          if (g_dragon.hearts <= 0) { g_dragon.hearts = 1; g_dragon.x = 0; g_dragon.z = 0; g_dragon.y = world_height(&g_world, 0, 0); g_dragon.mode = DM_GROUND; snprintf(g_msg, sizeof g_msg, "OUCH! BACK TO THE MEADOW. EAT TO GET HEARTS BACK"); g_msg_t = 5; } }
      if (dev & 2) { sfx_play(SFX_BLOCK, 0.6f, 0); platform_rumble(0.5f, 0.2f); g_dragon.growth += 5; snprintf(g_msg, sizeof g_msg, "DRONE DOWN!"); g_msg_t = 2; }
      int hev = world_hounds(&g_world, dt, g_dragon.x, g_dragon.y, g_dragon.z, 0.7f * dragon_length(g_dragon.stage) + 0.9f, whip, roar, g_dragon.mode == DM_AIR);
      if ((hev & 1) && g_dragon.evolve_t <= 0) { g_dragon.hearts -= 0.5f; g_dragon.act_t = 0.77f; g_dragon.anim = DA_HIT; g_dragon.anim_t = 0; sfx_play(SFX_HIT, 0.6f, 0); platform_rumble(0.7f, 0.3f); snprintf(g_msg, sizeof g_msg, "A SHOCK HOUND! RUN (ZR), HOP (A), OR SWAT IT (B)"); g_msg_t = 4;
          if (g_dragon.hearts <= 0) { g_dragon.hearts = 1; g_dragon.x = 0; g_dragon.z = 0; g_dragon.y = world_height(&g_world, 0, 0); g_dragon.mode = DM_GROUND; snprintf(g_msg, sizeof g_msg, "OUCH! BACK TO THE MEADOW. EAT TO GET HEARTS BACK"); g_msg_t = 5; } }
      if (hev & 2) { sfx_play(SFX_BLOCK, 0.6f, 0); platform_rumble(0.5f, 0.2f); g_dragon.growth += 5; snprintf(g_msg, sizeof g_msg, "HOUND DOWN!"); g_msg_t = 2; } }
    if (ev & 1) { platform_rumble(0.3f, 0.1f); sfx_play(SFX_MUNCH, 0.6f, 0); sfx_play(SFX_PICKUP, 0.3f, 0); }
    if (ev & 2) { snprintf(g_msg, sizeof g_msg, g_dragon.stage == ST_DRAGON ? "YOU GREW INTO A DRAGON! HOLD A TO FLY" : "BEYONDER! THE SKY IS YOURS"); g_msg_t = 6; platform_rumble(1, 0.8f); }
    if (ev & 16) { platform_rumble(0.6f, 0.4f); sfx_play(g_dragon.stage == ST_DRAGONET ? SFX_CHIRP : SFX_ROAR, 0.8f, 0); }
    if (ev & 2) sfx_play(SFX_GROW, 0.8f, 0);
    if (ev & 4) sfx_play(SFX_FLAP, 0.6f, 0);
    if (ev & 8) sfx_play(SFX_LAND, 0.5f, 0);
    if (in.b_press && g_dragon.anim == DA_TAILWHIP && g_dragon.act_t > 0.9f) sfx_play(SFX_TAIL, 0.6f, 0);
    { static float flap_t; if (g_dragon.mode == DM_AIR && g_dragon.anim == DA_FLY) { flap_t += dt; if (flap_t > 0.8f) { flap_t = 0; sfx_play(SFX_FLAP, 0.35f, 0); } } else flap_t = 0.6f; }
    if (g_state == G_PLAY && g_dragon.stage == ST_DRAGONET && fmodf(g_time, 7.0f) < dt && g_dragon.anim == DA_IDLE) sfx_play(SFX_CHIRP, 0.3f, 0);
    if (ev & 2) save_progress();
    if (!g_world.crystal_found) { float dx = g_dragon.x - g_world.crystal_x, dz = g_dragon.z - g_world.crystal_z, dy = g_dragon.y - g_world.crystal_y; if (dx * dx + dz * dz + dy * dy < (1.2f + 0.5f * dragon_length(g_dragon.stage)) * (1.2f + 0.5f * dragon_length(g_dragon.stage))) { g_world.crystal_found = 1; snprintf(g_msg, sizeof g_msg, "SECRET EGG CRYSTAL FOUND! 1 OF 10"); g_msg_t = 6; sfx_play(SFX_UNLOCK, 0.8f, 0); platform_rumble(0.8f, 0.5f); save_progress(); } }
    g_save_t += dt; if (g_save_t > 20) { g_save_t = 0; save_progress(); }
    if (g_msg_t > 0) g_msg_t -= dt;
    if (g_title_t > 0) g_title_t -= dt;
    memcpy(g_prev, pads, sizeof(Pad) * (npads < KK_PADS ? npads : KK_PADS));
    camera_update(dt);
    { int chased = 0; for (int i = 0; i < g_world.ndrone; i++) if (g_world.drone[i].state == 1) chased = 1; for (int i = 0; i < g_world.nhound; i++) if (g_world.hound[i].state == 1) chased = 1;
      static float danger_t; if (chased) danger_t = 4; else danger_t -= dt;
      music_want(danger_t > 0 && music_available("danger") ? "danger" : "everleaf"); }
    music_update(2);
    gfx_frame_begin();
    shadow_pass();
    gfx_view_begin(0, 0.55f, 0.75f, 0.95f); draw_scene(0); draw_hud(0); gfx_view_end(0);   /* the TV: the game */
    gfx_view_begin(1, 0.08f, 0.14f, 0.08f); draw_map(); gfx_view_end(1);                   /* the GamePad: the map */
    gfx_frame_end(); }
