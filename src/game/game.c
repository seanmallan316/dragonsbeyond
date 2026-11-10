// game.c - Dragons Beyond, proof of concept: Everleaf Wilds with the forest dragonet. Walk, eat, grow, fly.
#include "game.h"
#include "world.h"
#include "dragon.h"
#include "anim3d.h"
#include "props.h"
#include "boss.h"
#include "../render/gfx.h"
#include "../render/batch.h"
#include "../render/assets.h"
#include "../render/ui_atlas.h"
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
#ifdef DB_TOUR
#define LD_HI 22.0f   /* the emulator renders in software: near things only in high detail */
#define LD_SKIP 120.0f
#else
#define LD_HI 70.0f
#define LD_SKIP 0.0f
#endif
enum { PM_OAK = 0, PM_PALM, PM_FERN, PM_BUSH, PM_MUSH, PM_IRON, PM_STORM, PM_N }; static PropModel g_pm[PM_N]; static SceneLayer g_lay_tree, g_lay_palm, g_lay_fern, g_lay_bush;
static const char *PM_NAMES[PM_N] = {"oak", "palm", "fern", "berrybush", "mushroom", "ironhorn", "stormtalon"};
static const char *BOSS_NAME[2] = {"IRONHORN", "STORMTALON"};
static const char *BOSS_GUARD[2] = {"IRONHORN GUARDS THE CAGE! DODGE ITS CHARGE SO IT HITS A TREE", "STORMTALON GUARDS THE CAGE! DODGE ITS SWOOP SO IT HITS A ROCK SPIRE"};
static const char *BOSS_STUCK[2] = {"ITS ANTLERS ARE STUCK! WHIP THE GLOWING CORE ON ITS BACK (B)", "ITS TALONS ARE STUCK IN THE ROCK! WHIP THE GLOWING CORE ON ITS CHEST (B)"};
/* Ironhorn guards the cage that holds the Sky dragonet; once freed, she follows the player everywhere */
static Boss g_boss;
/* the team: kind 0 forest (always), 1 sky (freed in Everleaf Wilds), 2 snow (freed in the Sky Kingdom) */
#define K_N 3
static int g_has[K_N] = {1, 0, 0}, g_wrescued[WK_N];   /* who is on the team; whether each world's captive is free */
#define g_rescued (g_wrescued[g_wk])
static const int CAPTIVE[WK_N] = {1, 2};   /* who is caged in each world */
static Anim3D g_km[K_N][ST_N]; static int g_km_ok[K_N][ST_N];   /* every dragon's models (forest uses g_model) */
#define g_skym g_km[1]
#define g_skym_ok g_km_ok[1]   /* the sky dragon's three stages (grown ones optional) */
static Anim3D *kind_model(int k, int st) { if (k == 0) { while (st > 0 && !g_model_ok[st]) st--; return g_model_ok[st] ? &g_model[st] : NULL; }
    while (st > 0 && !g_km_ok[k][st]) st--; return g_km_ok[k][st] ? &g_km[k][st] : NULL; }
static int kind_has_stage(int k, int st) { return k == 0 ? 1 : g_km_ok[k][st]; }
static int g_km_tried[K_N][ST_N];
static void ensure_kind(int k, int upto) {   /* load dragon k's models up to stage upto (once each) */
    static const char *kn[K_N] = {"forest", "sky", "snow"}, *sn[ST_N] = {"dragonet", "dragon", "beyonder"};
    if (k <= 0 || k >= K_N || g_gfx_status != GFX_OK) return;
    for (int st = 0; st <= upto && st < ST_N; st++) if (!g_km_tried[k][st]) { g_km_tried[k][st] = 1; char nm[40]; snprintf(nm, sizeof nm, "%s_%s", kn[k], sn[st]); g_km_ok[k][st] = anim3d_load(&g_km[k][st], nm); } }
static float kind_scale(int k, int st) { if (k == 0) return dragon_scale(st); while (st > 0 && !g_km_ok[k][st]) st--; return st == ST_DRAGONET ? dragon_scale(ST_DRAGONET) * 0.85f : dragon_scale(st); }
static struct { int on, mode, anim; float x, y, z, yaw, speed, anim_t; } g_friend;
static const char *SKY_NAMES[2] = {"SCARLET", "WINDSLY"}, *SNOW_NAMES[2] = {"SNOWFALL", "SNOWSTORM"};
static const char *kind_name(int k);
static const char *KIND_TITLE[3] = {"FOREST DRAGON", "SKY DRAGON", "SNOW DRAGON"};
/* Dragon Swap: g_kind is who the player is (0 the forest dragon, 1 the sky dragonet); g_other holds the other one's progress */
static int g_kind, g_swap_open, g_swap_sel; static Dragon g_team[K_N]; static int g_fk = 1;   /* g_team[k]: progress of each dragon not being played; g_fk: who follows as the friend */
#define g_other (g_team[g_fk])
static int team_size(void) { int n = 0; for (int k = 0; k < K_N; k++) n += g_has[k]; return n; }
/* co-op: a second controller (Wii Remote, Nunchuk, Classic or Pro) takes over the friend dragon. g_p2 = its pad slot, -1 = nobody */
static int g_p2 = -1; static float g_gp_last = -100, g_p2_gone;
/* the storyteller's cards after the rescue (the game waits while they show) */
static const char *TALE[WK_N][3] = {
  { "AND SO IRONHORN FELL, AND THE IRON CAGE SWUNG OPEN. %s, DRAGONET OF THE SKY, WAS FREE AT LAST.",
    "BUT FAR AWAY, IN HIS DARK TOWER, KAINE BLACKFEATHER CRACKED HIS WHIP. ONE DRAGON FREED, HE SNEERED. FIVE ARE STILL MINE.",
    "THE TWO FRIENDS LOOKED UP AT THE CLOUDS. A COLUMN OF WIND ROSE FROM THE CLEARING... THE SKY KINGDOM WAS WAITING." },
  { "STORMTALON CRASHED DOWN AMONG THE SPIRES, AND THE CAGE FELL OPEN. %s, DRAGONET OF THE SNOW, SHOOK THE FROST FROM TINY WINGS.",
    "IN HIS TOWER, KAINE BLACKFEATHER SNAPPED HIS WHIP SO HARD THE WINDOWS SHOOK. TWO DRAGONS FREED, HE ROARED. FOUR ARE STILL MINE!",
    "THREE FRIENDS NOW, AND GROWING STRONGER. FAR AWAY, PAST THE EDGE OF THE CLOUDS, THE WINTER PALACE WAS WAITING..." } };
static int g_tale_w;
static int g_tale_i = -1; static float g_tale_t;
static const char *active_name(void);
static float g_time, g_cam_yaw; static int g_cam_mode;   /* 0 rider, 1 overhead, 2 dragon's eyes */
static Vec3 g_eye, g_at; static float g_look_x, g_look_hold;
static Pad g_prev[KK_PADS];
static char g_save_path[256], g_save_base[256]; static float g_save_t;
/* three save slots and the difficulty: Easy halves the damage and Ironhorn needs 2 hits; Hard takes 1.5x damage and 4 hits */
static int g_slot = 0, g_diff = 1, g_slot_sel = 0, g_slot_erase = -1;
static const float DMG[3] = {0.5f, 1.0f, 1.5f}; static const int BOSS_HITS[3] = {2, 3, 4}; static const char *DIFF_NAMES[3] = {"EASY", "NORMAL", "HARD"};
typedef struct { int used, name, stage, rescued, diff, wk; } SlotInfo;   /* rescued: how many friends are on the team */ static SlotInfo g_slots[3];
static int load_progress(void); static void save_progress(void);
static char g_msg[128]; static float g_msg_t; static float g_title_t = 4.0f;
enum { G_TITLE = 0, G_NAME, G_STORY, G_PLAY, G_SLOT, G_DIFF }; static int g_state = G_TITLE; static int g_name = 0; static float g_state_t;   /* g_name: 0 Flo-Rida, 1 Leaf Blower */
static const char *NAMES[2] = {"FLO-RIDA", "LEAF BLOWER"};

/* textures and meshes */
static Texture *tx_grass, *tx_dirt, *tx_bark, *tx_leaves, *tx_fern, *tx_bush, *tx_rock, *tx_water, *tx_spark, *tx_font, *tx_tuft, *tx_flower, *tx_cloud;
static Mesh *m_ground, *m_ground_dirt, *m_ground_rock, *m_water, *m_bark, *m_leaves, *m_fern, *m_bushes, *m_rock, *m_sky, *m_tufts, *m_flowers, *m_clouds, *m_mountains, *m_apron, *m_sun, *m_cloudsea;
static int g_wk = WK_FOREST;   /* which world we are in */
static float g_safe_x, g_safe_z;   /* the last solid ground the player stood on (sky world) */
/* dragon powers (Y, once grown): forest Leafnado whirls round, sky breathes fire, snow breathes frost */
static float g_power_t, g_power_cd; static int g_power_kind;
/* Kaine Blackfeather sweeps over the Sky Kingdom on his black mech dragon the first time you arrive */
static float g_kaine_t = -1; static int g_kaine_done;
/* the wind column between worlds: in the forest it rises from the boss clearing once its captive is free; in the sky it waits on the first island */
static float g_travel_t = -1; static int g_travel_to; static float g_travel_cd;
static int column_at(float *cx, float *cz) {
    if (g_wk == WK_FOREST) { if (!g_wrescued[WK_FOREST]) return 0; *cx = g_world.arena_x; *cz = g_world.arena_z; return 1; }
    *cx = -12; *cz = 7; return 1; }
static Mesh *m_dynfood, *m_dynspark, *m_dynhud;   /* refilled per view (each at most twice a frame) */
#define DYN_CAP 48000
static Vertex g_dynv[DYN_CAP]; static Batch g_b;
#define UI_CAP 12000
static Texture *tx_ui; static Mesh *m_dynui, *m_dynuia; static Vertex g_uiv[UI_CAP], g_uav[UI_CAP]; static Batch g_ub, g_ua;
static uint32_t n_sky;

static Texture *tx_sky; static int g_sky_photo;
static Vec3 sun_dir(void) { return v3_norm(v3(0.55f, 0.62f, 0.4f)); }   /* late-afternoon sun: long shadows, shapes read as solid */
static float frand01(unsigned *s) { *s = *s * 1664525u + 1013904223u; return (float)((*s >> 8) & 0xFFFFFF) / 16777216.0f; }

/* ---------- building the world's meshes ---------- */
static Vertex *tmpv; static uint32_t tmpn, tmpcap;
static void tv_begin(uint32_t cap) { tmpcap = cap; tmpn = 0; tmpv = realloc(tmpv, sizeof(Vertex) * cap); }
static void tv_push(Vec3 p, Vec3 n, float u, float v, float r, float g, float b, float a) { if (tmpn < tmpcap) { Vertex q = {p.x, p.y, p.z, n.x, n.y, n.z, u, v, r, g, b, a}; tmpv[tmpn++] = q; } }
static void tv_tri(Vec3 a, Vec3 b, Vec3 c, Vec3 na, Vec3 nb, Vec3 nc, float ua, float va, float ub, float vb, float uc, float vc, float r, float g, float bl) { tv_push(a, na, ua, va, r, g, bl, 1); tv_push(b, nb, ub, vb, r, g, bl, 1); tv_push(c, nc, uc, vc, r, g, bl, 1); }
static Mesh *tv_mesh(void) { return tmpn ? gfx_mesh_static(tmpv, tmpn) : NULL; }

static float hashf2(int x, int y, unsigned sd) { unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + sd * 982451653u; h = (h ^ (h >> 13)) * 1274126177u; return (float)((h ^ (h >> 16)) & 0xFFFF) / 65535.0f; }
static float vnoise2(float x, float y, unsigned sd) { int xi = (int)floorf(x), yi = (int)floorf(y); float fx = x - xi, fy = y - yi; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    float a = hashf2(xi, yi, sd), b = hashf2(xi + 1, yi, sd), c = hashf2(xi, yi + 1, sd), d = hashf2(xi + 1, yi + 1, sd); return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy; }
/* how much dirt and bare rock show at a point: dirt in patches and along the meadow's worn ring, rock on steep ground */
static void ground_layers(float x, float z, float ny, float *dirt, float *rock) {
    float n = vnoise2(x * 0.045f + 11, z * 0.045f + 3, 77) * 0.6f + vnoise2(x * 0.15f, z * 0.15f, 78) * 0.4f;
    float d = clampf((n - 0.56f) * 5.0f, 0, 1); float ring = fabsf(sqrtf(x * x + z * z) - 9.0f); d = fmaxf(d, clampf(1 - ring / 2.2f, 0, 1) * 0.8f * clampf((vnoise2(x * 0.3f, z * 0.3f, 79) - 0.2f) * 2, 0, 1));
    float r = clampf((0.78f - ny) * 5.0f, 0, 1); *dirt = d * (1 - r); *rock = r; }
static void build_ground(void) {
    tv_begin((W_GRID - 1) * (W_GRID - 1) * 6);
    static float dirtw[W_GRID][W_GRID], rockw[W_GRID][W_GRID];
    for (int j = 0; j < W_GRID; j++) for (int i = 0; i < W_GRID; i++) { float x = -W_SIZE / 2 + W_SIZE * i / (W_GRID - 1), z = -W_SIZE / 2 + W_SIZE * j / (W_GRID - 1); Vec3 n = world_normal(&g_world, x, z); ground_layers(x, z, n.y, &dirtw[j][i], &rockw[j][i]); }
    for (int layer = 0; layer < 3; layer++) {
        tv_begin((W_GRID - 1) * (W_GRID - 1) * 6);
        for (int j = 0; j < W_GRID - 1; j++) for (int i = 0; i < W_GRID - 1; i++) {
            Vec3 p[4]; Vec3 n[4]; float c[4][4]; float wsum = 0;
            for (int k = 0; k < 4; k++) { int ii = i + (k & 1), jj = j + (k >> 1); float x = -W_SIZE / 2 + W_SIZE * ii / (W_GRID - 1), z = -W_SIZE / 2 + W_SIZE * jj / (W_GRID - 1);
                p[k] = v3(x, g_world.h[jj][ii] + (layer ? 0.03f * layer : 0), z); n[k] = world_normal(&g_world, x, z);
                float hgt = clampf((p[k].y + 4) / 30.0f, 0, 1); float shade = 0.9f + 0.2f * vnoise2(x * 0.08f, z * 0.08f, 5);
                float r = (0.7f + 0.2f * hgt) * shade, g = (0.9f - 0.05f * hgt) * shade, b = (0.62f + 0.15f * hgt) * shade;
                if (layer == 1) { r = g = b = 1.0f * shade; } if (layer == 2) { r = g = b = 1.0f; }
                float a = layer == 0 ? 1 : (layer == 1 ? dirtw[jj][ii] : rockw[jj][ii]); wsum += a;
                c[k][0] = r; c[k][1] = g; c[k][2] = b; c[k][3] = a; }
            if (layer && wsum < 0.02f) continue;
            if (p[0].y < W_CLOUDS - 6 && p[1].y < W_CLOUDS - 6 && p[2].y < W_CLOUDS - 6 && p[3].y < W_CLOUDS - 6) continue;   /* nothing to draw in the sky world's gaps */
            float s = layer == 2 ? 0.9f : 1.6f; float u0 = i * s, v0 = j * s, u1 = u0 + s, v1 = v0 + s;
            float hmin = fminf(fminf(p[0].y, p[1].y), fminf(p[2].y, p[3].y)), hmax = fmaxf(fmaxf(p[0].y, p[1].y), fmaxf(p[2].y, p[3].y));
            if (hmax - hmin > W_SIZE / (W_GRID - 1) * 1.5f) {   /* a cliff: texture runs along the wall and down it, not stretched from above */
                float cs = W_SIZE / (W_GRID - 1) / s; float k = 1.0f / cs; float uu[4], vv[4]; for (int c = 0; c < 4; c++) { uu[c] = (p[c].x + p[c].z) * k; vv[c] = -p[c].y * k; }
                tv_push(p[0], n[0], uu[0], vv[0], c[0][0], c[0][1], c[0][2], c[0][3]); tv_push(p[2], n[2], uu[2], vv[2], c[2][0], c[2][1], c[2][2], c[2][3]); tv_push(p[1], n[1], uu[1], vv[1], c[1][0], c[1][1], c[1][2], c[1][3]);
                tv_push(p[1], n[1], uu[1], vv[1], c[1][0], c[1][1], c[1][2], c[1][3]); tv_push(p[2], n[2], uu[2], vv[2], c[2][0], c[2][1], c[2][2], c[2][3]); tv_push(p[3], n[3], uu[3], vv[3], c[3][0], c[3][1], c[3][2], c[3][3]); continue; }
            tv_push(p[0], n[0], u0, v0, c[0][0], c[0][1], c[0][2], c[0][3]); tv_push(p[2], n[2], u0, v1, c[2][0], c[2][1], c[2][2], c[2][3]); tv_push(p[1], n[1], u1, v0, c[1][0], c[1][1], c[1][2], c[1][3]);
            tv_push(p[1], n[1], u1, v0, c[1][0], c[1][1], c[1][2], c[1][3]); tv_push(p[2], n[2], u0, v1, c[2][0], c[2][1], c[2][2], c[2][3]); tv_push(p[3], n[3], u1, v1, c[3][0], c[3][1], c[3][2], c[3][3]); }
        if (layer == 0) m_ground = tv_mesh(); else if (layer == 1) m_ground_dirt = tv_mesh(); else m_ground_rock = tv_mesh(); }

    /* the pond: a disc at water level */
    if (g_world.pond_r <= 0) { m_water = NULL; return; }
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
        if (p->kind == P_TREE && !g_pm[PM_OAK].ok) { float h = 7.5f * p->scale; tv_cone(b, v3(p->x + sinf(p->yaw) * 0.6f, y + h * 0.75f, p->z + cosf(p->yaw) * 0.6f), 0.55f * p->scale, 0.25f * p->scale, 8, 2, 0);
            for (int k = 0; k < 3; k++) { float a = p->yaw + k * 2.1f; Vec3 t = v3(p->x + sinf(a) * 2.2f * p->scale, y + h * (0.75f + 0.2f), p->z + cosf(a) * 2.2f * p->scale); tv_cone(v3(p->x + sinf(p->yaw) * 0.6f, y + h * 0.7f, p->z + cosf(p->yaw) * 0.6f), t, 0.22f * p->scale, 0.08f * p->scale, 6, 1, 0.1f); } }
        else if (p->kind == P_PALM && !g_pm[PM_PALM].ok) { float h = 9.0f * p->scale; tv_cone(b, v3(p->x + sinf(p->yaw) * 1.5f, y + h, p->z + cosf(p->yaw) * 1.5f), 0.4f * p->scale, 0.25f * p->scale, 7, 2, 0.15f); } }
    m_bark = tv_mesh();
    /* canopies: lumpy leaf balls plus fluff, and the palm crowns */
    tv_begin(W_MAXPROPS * 260 * 3 + W_MAXFOOD * 140 * 3);
    for (int i = 0; i < g_world.nprop; i++) { const Prop *p = &g_world.prop[i]; float y = world_height(&g_world, p->x, p->z);
        if (p->kind == P_TREE && !g_pm[PM_OAK].ok) { float h = 7.5f * p->scale; Vec3 c = v3(p->x + sinf(p->yaw) * 0.6f, y + h * 0.95f, p->z + cosf(p->yaw) * 0.6f);
            float tint = 0.85f + frand01(&s) * 0.3f; tv_ball(c, 3.9f * p->scale, 2.9f * p->scale, 3.9f * p->scale, i * 31 + 5, 0.18f, 1.6f, tint, tint, tint * 0.92f, 0.25f);
            for (int k = 0; k < 3; k++) { float a = p->yaw + k * 2.1f; Vec3 t = v3(p->x + sinf(a) * 2.2f * p->scale, y + h * 0.95f + 0.4f, p->z + cosf(a) * 2.2f * p->scale); tv_ball(t, 2.2f * p->scale, 1.7f * p->scale, 2.2f * p->scale, i * 31 + k, 0.2f, 1.1f, tint, tint, tint * 0.92f, 0.3f); } }
    }
    /* the berry bushes: round leafy balls (the berries themselves are drawn live, so they can be eaten) */
    for (int i = 0; i < g_world.nfood && !g_pm[PM_BUSH].ok; i++) { const Food *f = &g_world.food[i]; if (f->kind != F_BERRY) continue; tv_ball(v3(f->x, f->y + 0.35f, f->z), 0.6f, 0.45f, 0.6f, i * 13 + 1, 0.2f, 1.0f, 0.95f, 1.0f, 0.9f, 0.15f); }
    m_leaves = tv_mesh();
    /* ferns (splayed fronds) and the palm crowns share the frond texture */
    tv_begin(W_MAXPROPS * 60 + 64);
    for (int i = 0; i < g_world.nprop; i++) { const Prop *p = &g_world.prop[i]; float y = world_height(&g_world, p->x, p->z);
        if (p->kind == P_FERN && !g_pm[PM_FERN].ok) { for (int k = 0; k < 7; k++) { float a = p->yaw + k * 0.9f; float len = 1.6f * p->scale; Vec3 b = v3(p->x, y - 0.05f, p->z); Vec3 tip = v3(p->x + sinf(a) * len, y + 0.9f * p->scale, p->z + cosf(a) * len); Vec3 ax = v3(cosf(a) * 0.45f * p->scale, 0, -sinf(a) * 0.45f * p->scale); Vec3 n = v3(0, 1.0f, 0), ns = v3(0, 1.3f, 0);
            tv_tri(v3(b.x - ax.x, b.y, b.z - ax.z), v3(b.x + ax.x, b.y, b.z + ax.z), tip, n, n, ns, 0, 1, 1, 1, 0.5f, 0, 1, 1, 1); tv_tri(v3(b.x + ax.x, b.y, b.z + ax.z), v3(b.x - ax.x, b.y, b.z - ax.z), tip, n, n, ns, 1, 1, 0, 1, 0.5f, 0, 1, 1, 1); } }
        if (p->kind == P_PALM && !g_pm[PM_PALM].ok) { float h = 9.0f * p->scale; Vec3 top = v3(p->x + sinf(p->yaw) * 1.5f, y + h, p->z + cosf(p->yaw) * 1.5f);
            for (int k = 0; k < 7; k++) { float a = p->yaw + k * 0.9f; Vec3 ax = v3(cosf(a) * 1.1f, 0, -sinf(a) * 1.1f); Vec3 tip = v3(top.x + sinf(a) * 4.5f * p->scale, top.y - 1.6f * p->scale, top.z + cosf(a) * 4.5f * p->scale); Vec3 mid = v3(top.x + sinf(a) * 2.2f * p->scale, top.y + 0.6f * p->scale, top.z + cosf(a) * 2.2f * p->scale);
                Vec3 n = v3(0, 1.0f, 0), ns = v3(0, 1.5f, 0);
                tv_tri(v3(top.x - ax.x, top.y, top.z - ax.z), v3(top.x + ax.x, top.y, top.z + ax.z), mid, n, n, n, 0, 1, 1, 1, 0.5f, 0.5f, 0.9f, 1, 0.8f);
                tv_tri(v3(mid.x - ax.x * 0.8f, mid.y, mid.z - ax.z * 0.8f), v3(mid.x + ax.x * 0.8f, mid.y, mid.z + ax.z * 0.8f), tip, n, n, ns, 0, 0.5f, 1, 0.5f, 0.5f, 0, 0.9f, 1, 0.8f);
                tv_tri(v3(top.x + ax.x, top.y, top.z + ax.z), v3(top.x - ax.x, top.y, top.z - ax.z), mid, n, n, n, 1, 1, 0, 1, 0.5f, 0.5f, 0.9f, 1, 0.8f);
                tv_tri(v3(mid.x + ax.x * 0.8f, mid.y, mid.z + ax.z * 0.8f), v3(mid.x - ax.x * 0.8f, mid.y, mid.z - ax.z * 0.8f), tip, n, n, ns, 1, 0.5f, 0, 0.5f, 0.5f, 0, 0.9f, 1, 0.8f); } } }
    m_fern = tv_mesh();
    tv_begin(W_MAXPROPS * 36);
    for (int i = 0; i < g_world.nprop; i++) { const Prop *p = &g_world.prop[i]; float y = world_height(&g_world, p->x, p->z);
        if (p->kind == P_BUSH && !g_pm[PM_BUSH].ok) tv_cross(v3(p->x, y - 0.2f, p->z), 2.8f * p->scale, 2.0f * p->scale, p->yaw, 0.12f, 3, 1, 1, 1); }
    m_bushes = tv_mesh();
    /* rocks */
    tv_begin(W_MAXPROPS * 140 * 3);
    for (int i = 0; i < g_world.nprop; i++) { const Prop *p = &g_world.prop[i]; float y = world_height(&g_world, p->x, p->z);
        if (p->kind == P_ROCK) tv_ball(v3(p->x, y - 0.3f * p->scale, p->z), 1.4f * p->scale, 0.9f * p->scale, 1.1f * p->scale, i * 17 + 3, 0.3f, 1.0f, 1, 1, 1, 0);
        if (p->kind == P_SPIRE) { float H = 9.0f * p->scale; tv_ball(v3(p->x, y + H * 0.45f, p->z), 2.3f * p->scale, H, 2.0f * p->scale, i * 17 + 5, 0.35f, 1.0f, 0.95f, 0.95f, 1.0f, 0);   /* a rock needle */
            tv_ball(v3(p->x + 0.8f * p->scale, y + H * 1.15f, p->z - 0.4f * p->scale), 1.2f * p->scale, 2.6f * p->scale, 1.1f * p->scale, i * 17 + 9, 0.35f, 1.0f, 0.95f, 0.95f, 1.0f, 0); } }
    m_rock = tv_mesh();
    /* grass tufts and flowers: scattered everywhere the ground is not too steep, denser near the start */
    unsigned ts = 4242; tv_begin(6500 * 18);
    for (int i = 0; i < 6500; i++) { float x = (frand01(&ts) - 0.5f) * (W_SIZE - 20), z = (frand01(&ts) - 0.5f) * (W_SIZE - 20); if (world_in_pond(&g_world, x, z) || world_height(&g_world, x, z) < W_CLOUDS + 6) continue; Vec3 n = world_normal(&g_world, x, z); if (n.y < 0.8f) continue;
        float y = world_height(&g_world, x, z); float sc = 0.7f + frand01(&ts) * 0.8f; float tint = 0.85f + frand01(&ts) * 0.3f; tv_cross(v3(x, y - 0.05f, z), 1.1f * sc, 0.75f * sc, frand01(&ts) * 3.14f, 0.18f, 2, tint, tint, tint); }
    m_tufts = tv_mesh();
    tv_begin(700 * 12);
    for (int i = 0; i < 700; i++) { float x = (frand01(&ts) - 0.5f) * (W_SIZE - 40), z = (frand01(&ts) - 0.5f) * (W_SIZE - 40); if (world_in_pond(&g_world, x, z) || world_height(&g_world, x, z) < W_CLOUDS + 6) continue; Vec3 n = world_normal(&g_world, x, z); if (n.y < 0.85f) continue;
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
    m_mountains = m_apron = m_cloudsea = NULL;
    if (g_wk == WK_FOREST) {
    /* a ring of far mountains: a lit heightfield band (rock, snow on the high ground), hazed by the fog like the real thing */
    { const int M = 160, K = 7; static Vec3 P[7][161];
        for (int k = 0; k < K; k++) for (int i = 0; i <= M; i++) { int ii = i % M; float a = ii * 6.2831853f / M; float R = 300 + k * 36.0f;
            float ridge = 1.0f - fabsf((k - 3.2f) / 3.6f); ridge = ridge < 0 ? 0 : ridge; ridge = ridge * ridge * (3 - 2 * ridge);
            float n = world_fbm(cosf(a) * 4.0f + 50, sinf(a) * 4.0f + 50, 777) * 0.7f + world_fbm(cosf(a) * 11.0f + 9, sinf(a) * 11.0f + 9, 778) * 0.3f;
            float nn2 = world_fbm(cosf(a) * 1.6f + 20, sinf(a) * 1.6f + 20, 783); float peak = 30 + 300 * powf(n, 2.2f) * (0.35f + 0.65f * nn2 * nn2);   /* low passes between tall peaks */
            float h = -25 + (peak + 25) * ridge * (0.75f + 0.25f * world_fbm(a * 3 + k * 0.7f, k * 1.3f, 779));
            if (k == 0 || k == K - 1) h = -25; P[k][i] = v3(sinf(a) * (R + 25 * (world_fbm(a * 2, k * 0.9f, 780) - 0.5f)), h, cosf(a) * (R + 25 * (world_fbm(a * 2 + 5, k * 0.9f, 781) - 0.5f))); }
        tv_begin((uint32_t)((K - 1) * M * 6));
        for (int k = 0; k + 1 < K; k++) for (int i = 0; i < M; i++) {
            Vec3 q[4] = {P[k][i], P[k][i + 1], P[k + 1][i + 1], P[k + 1][i]};
            Vec3 e1 = v3(q[1].x - q[0].x, q[1].y - q[0].y, q[1].z - q[0].z), e2 = v3(q[3].x - q[0].x, q[3].y - q[0].y, q[3].z - q[0].z);
            Vec3 nn = v3_norm(v3(e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x)); if (nn.y < 0) nn = v3(-nn.x, -nn.y, -nn.z);
            float hm = 0.25f * (q[0].y + q[1].y + q[2].y + q[3].y); float snow = clampf((hm - 120) / 50.0f, 0, 1) * clampf((nn.y - 0.35f) * 2.5f, 0, 1);
            float r = 0.46f + 0.5f * snow, g = 0.44f + 0.52f * snow, bl = 0.42f + 0.56f * snow; float tone = 0.85f + 0.3f * world_fbm(i * 0.15f, k * 0.6f, 782); r *= tone; g *= tone; bl *= tone;
            float u0 = i * 0.5f, u1 = (i + 1) * 0.5f, v0 = k * 0.8f, v1 = (k + 1) * 0.8f;
            tv_tri(q[0], q[1], q[2], nn, nn, nn, u0, v0, u1, v0, u1, v1, r, g, bl); tv_tri(q[0], q[2], q[3], nn, nn, nn, u0, v0, u1, v1, u0, v1, r, g, bl); } }
    m_mountains = tv_mesh();
    /* the apron: rolling grassland from the edge of the map out to the mountains' feet */
    { const int M = 96, K = 5; float Rs[5] = {112, 150, 200, 255, 310}; tv_begin((uint32_t)((K - 1) * M * 6));
        for (int k = 0; k + 1 < K; k++) for (int i = 0; i < M; i++) { float a0 = i * 6.2831853f / M, a1 = (i + 1) * 6.2831853f / M; Vec3 q[4]; float ang[4] = {a0, a1, a1, a0}; float rr[4] = {Rs[k], Rs[k], Rs[k + 1], Rs[k + 1]};
            for (int c = 0; c < 4; c++) { float x = sinf(ang[c]) * rr[c], z = cosf(ang[c]) * rr[c]; float edge = clampf((rr[c] - 112) / 60.0f, 0, 1);
                float hy = (1 - edge) * world_height(&g_world, clampf(x, -118, 118), clampf(z, -118, 118)) + edge * (-4 + 22 * (world_fbm(x * 0.01f, z * 0.01f, 790) - 0.4f)); q[c] = v3(x, hy, z); }
            Vec3 e1 = v3(q[1].x - q[0].x, q[1].y - q[0].y, q[1].z - q[0].z), e2 = v3(q[3].x - q[0].x, q[3].y - q[0].y, q[3].z - q[0].z);
            Vec3 nn = v3_norm(v3(e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x)); if (nn.y < 0) nn = v3(-nn.x, -nn.y, -nn.z);
            float g = 0.88f + 0.12f * world_fbm(i * 0.2f, k * 0.5f, 791);
            tv_tri(q[0], q[1], q[2], nn, nn, nn, q[0].x * 0.1f, q[0].z * 0.1f, q[1].x * 0.1f, q[1].z * 0.1f, q[2].x * 0.1f, q[2].z * 0.1f, g * 0.9f, g, g * 0.85f);
            tv_tri(q[0], q[2], q[3], nn, nn, nn, q[0].x * 0.1f, q[0].z * 0.1f, q[2].x * 0.1f, q[2].z * 0.1f, q[3].x * 0.1f, q[3].z * 0.1f, g * 0.9f, g, g * 0.85f); } }
    m_apron = tv_mesh();
    } else {   /* the Sky Kingdom: a rolling sea of cloud below the islands, out to the horizon */
        const int N = 72; const float span = 1500; tv_begin((uint32_t)(N * N * 6));
        for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) { Vec3 q[4]; float col[4];
            for (int k = 0; k < 4; k++) { float x = -span / 2 + span * (i + (k & 1)) / N, z = -span / 2 + span * (j + (k >> 1)) / N;
                float n1 = world_fbm(x * 0.012f, z * 0.012f, 911), n2 = world_fbm(x * 0.05f, z * 0.05f, 912);
                q[k] = v3(x, W_CLOUDS - 2 + (n1 - 0.5f) * 7 + (n2 - 0.5f) * 2, z); col[k] = 0.8f + 0.25f * n1 + 0.1f * n2; }
            Vec3 nn = v3(0, 1, 0);
            tv_push(q[0], nn, WHITE_U, WHITE_V, col[0], col[0], col[0] * 1.02f, 1); tv_push(q[2], nn, WHITE_U, WHITE_V, col[2], col[2], col[2] * 1.02f, 1); tv_push(q[1], nn, WHITE_U, WHITE_V, col[1], col[1], col[1] * 1.02f, 1);
            tv_push(q[1], nn, WHITE_U, WHITE_V, col[1], col[1], col[1] * 1.02f, 1); tv_push(q[2], nn, WHITE_U, WHITE_V, col[2], col[2], col[2] * 1.02f, 1); tv_push(q[3], nn, WHITE_U, WHITE_V, col[3], col[3], col[3] * 1.02f, 1); }
        m_cloudsea = tv_mesh();
        /* far floating islands all round: rocky undersides hanging into the cloud (rock), grassy caps (grass) */
        unsigned fs = 5150; float FI[16][5]; int nf = 0;
        for (int i = 0; i < 16; i++) { float a = i * 6.2831853f / 16 + (frand01(&fs) - 0.5f) * 0.3f, r = 270 + frand01(&fs) * 230;
            FI[nf][0] = sinf(a) * r; FI[nf][1] = cosf(a) * r; FI[nf][2] = 15 + frand01(&fs) * 50; FI[nf][3] = 18 + frand01(&fs) * 34; FI[nf][4] = FI[nf][3] * (1.3f + frand01(&fs) * 1.2f); nf++; }
        tv_begin(16 * 2 * 2000);
        for (int i = 0; i < nf; i++) { float x = FI[i][0], z = FI[i][1], y = FI[i][2], R = FI[i][3], D = FI[i][4];
            tv_ball(v3(x, y - D * 0.5f, z), R, D * 0.5f, R * 0.92f, 300 + i * 7, 0.38f, 3.0f, 0.82f, 0.8f, 0.74f, 0);   /* a lumpy rock mass */
            tv_ball(v3(x + R * 0.25f, y - D * 0.85f, z - R * 0.15f), R * 0.45f, D * 0.35f, R * 0.4f, 301 + i * 7, 0.4f, 2.0f, 0.75f, 0.73f, 0.68f, 0); }   /* and a hanging chunk under it */
        m_mountains = tv_mesh();
        tv_begin(16 * 14 * 3 * 2);
        for (int i = 0; i < nf; i++) { float x = FI[i][0], z = FI[i][1], y = FI[i][2], R = FI[i][3]; Vec3 up = v3(0, 1, 0);
            for (int k = 0; k < 14; k++) { float a0 = k * 6.2831853f / 14, a1 = (k + 1) * 6.2831853f / 14; float bump = 1.5f + R * 0.06f;
                Vec3 c = v3(x, y + bump, z), p0 = v3(x + sinf(a0) * R, y, z + cosf(a0) * R), p1 = v3(x + sinf(a1) * R, y, z + cosf(a1) * R);
                tv_tri(c, p0, p1, up, up, up, x * 0.05f, z * 0.05f, p0.x * 0.05f, p0.z * 0.05f, p1.x * 0.05f, p1.z * 0.05f, 0.85f, 0.95f, 0.8f); } }
        m_apron = tv_mesh(); }
    /* sky dome: a photographed sky (content/img/sky.png, Poly Haven CC0) wrapped round the world, its sun turned to sit where the game's sun is.
       The picture's sun is higher (54 deg) than the game's (42 deg), so heights below the sun are stretched a little and above it squeezed. */
    if (tx_sky) {
        Vec3 L = sun_dir(); float sun_az = atan2f(L.x, L.z), sun_el = asinf(L.y);
        const float IMG_SUN_U = 0.6006f, IMG_SUN_EL = 0.944f, IMG_BOTTOM = -0.08f;   /* the picture's sun, and the height its bottom row shows */
        tv_begin(64 * 20 * 6);
        for (int i = 0; i < 20; i++) for (int j = 0; j < 64; j++) {
            Vec3 q[4]; float uv[4][2];
            for (int k = 0; k < 4; k++) { int ii = i + (k >> 1), jj = j + (k & 1); float el = -0.1f + (1.5708f + 0.1f) * ii / 20.0f, a = jj * 6.2831853f / 64;
                q[k] = v3(cosf(el) * sinf(a) * 900, sinf(el) * 900, cosf(el) * cosf(a) * 900);
                float ie = el < sun_el ? el * IMG_SUN_EL / sun_el : IMG_SUN_EL + (el - sun_el) * (1.5708f - IMG_SUN_EL) / (1.5708f - sun_el);
                uv[k][0] = IMG_SUN_U + (a - sun_az) / 6.2831853f; uv[k][1] = clampf((1.5708f - ie) / (1.5708f - IMG_BOTTOM), 0.002f, 0.998f); }
            Vec3 n = v3(0, 1, 0);
            tv_tri(q[0], q[2], q[1], n, n, n, uv[0][0], uv[0][1], uv[2][0], uv[2][1], uv[1][0], uv[1][1], 1, 1, 1);
            tv_tri(q[1], q[2], q[3], n, n, n, uv[1][0], uv[1][1], uv[2][0], uv[2][1], uv[3][0], uv[3][1], 1, 1, 1); }
        n_sky = tmpn; m_sky = tv_mesh(); g_sky_photo = 1; }
    else {
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
    /* the sun: a bright disc and a wide soft glow, built as fans facing the camera's dome centre, blended over the sky */
    { Vec3 L = sun_dir(); Vec3 C = v3(L.x * 860, L.y * 860, L.z * 860); Vec3 up = v3(0, 1, 0); Vec3 rt = v3_norm(v3(L.z, 0, -L.x)); Vec3 upv = v3_norm(v3(up.y * L.z - up.z * L.y, up.z * L.x - up.x * L.z, up.x * L.y - up.y * L.x)); upv = v3_norm(v3(rt.y * L.z - rt.z * L.y, rt.z * L.x - rt.x * L.z, rt.x * L.y - rt.y * L.x));
        tv_begin(3 * 24 * 3 * 3); Vec3 nn = v3(-L.x, -L.y, -L.z);
        float radii[3] = {38, 110, 320}; float alpha[3] = {1.0f, 0.55f, 0.22f}; float cr[3] = {1.0f, 1.0f, 1.0f}, cg[3] = {0.98f, 0.9f, 0.8f}, cb[3] = {0.9f, 0.7f, 0.55f};
        for (int ring = 2; ring >= 0; ring--) for (int j = 0; j < 24; j++) { float a0 = j * 6.2831853f / 24, a1 = (j + 1) * 6.2831853f / 24; float R = radii[ring];
            Vec3 p0 = v3(C.x + (rt.x * cosf(a0) + upv.x * sinf(a0)) * R, C.y + (rt.y * cosf(a0) + upv.y * sinf(a0)) * R, C.z + (rt.z * cosf(a0) + upv.z * sinf(a0)) * R);
            Vec3 p1 = v3(C.x + (rt.x * cosf(a1) + upv.x * sinf(a1)) * R, C.y + (rt.y * cosf(a1) + upv.y * sinf(a1)) * R, C.z + (rt.z * cosf(a1) + upv.z * sinf(a1)) * R);
            tv_push(C, nn, WHITE_U, WHITE_V, cr[ring], cg[ring], cb[ring], alpha[ring]); tv_push(p1, nn, WHITE_U, WHITE_V, cr[ring], cg[ring], cb[ring], 0); tv_push(p0, nn, WHITE_U, WHITE_V, cr[ring], cg[ring], cb[ring], 0); }
        m_sun = tv_mesh(); }
}

/* ---------- init ---------- */
int game_init(int gfx_status) {
    g_gfx_status = gfx_status;
    world_make(&g_world, 2026);
    dragon_init(&g_dragon, &g_world, 0, 0); boss_init(&g_boss, &g_world);
    g_boss.hp = BOSS_HITS[g_diff];
    if (gfx_status != GFX_OK) return 0;
    tx_grass = asset_texture("grass", 1, NULL, NULL); tx_dirt = asset_texture("dirt", 1, NULL, NULL); tx_bark = asset_texture("bark", 1, NULL, NULL);
    tx_leaves = asset_texture("leaves", 1, NULL, NULL); tx_fern = asset_texture("fern", 0, NULL, NULL); tx_bush = asset_texture("bush", 0, NULL, NULL);
    tx_rock = asset_texture("rock", 1, NULL, NULL); tx_water = asset_texture("water", 1, NULL, NULL); tx_spark = asset_texture("sparkle", 0, NULL, NULL);
    tx_tuft = asset_texture("grasstuft", 0, NULL, NULL); tx_flower = asset_texture("flowers", 0, NULL, NULL); tx_cloud = asset_texture("cloud", 0, NULL, NULL);
    tx_sky = asset_texture("sky", 1, NULL, NULL); tx_font = asset_texture("font_hd", 0, NULL, NULL); if (tx_font) g_font_hd = 1;
    for (int i = 0; i < PM_N; i++) propmodel_load(&g_pm[i], PM_NAMES[i]);
    build_ground(); build_props();
    scenelayer_build(&g_lay_tree, &g_pm[PM_OAK], &g_world, P_TREE, 1.0f, 0.2f); scenelayer_build(&g_lay_palm, &g_pm[PM_PALM], &g_world, P_PALM, 1.0f, 0.2f); scenelayer_build(&g_lay_fern, &g_pm[PM_FERN], &g_world, P_FERN, 1.0f, 0.05f); scenelayer_build(&g_lay_bush, &g_pm[PM_BUSH], &g_world, P_BUSH, 1.6f, 0.1f);   /* the packed models are already life-size; the prop's own scale varies them */
    batch_init(&g_b, g_dynv, DYN_CAP); m_dynfood = gfx_mesh_dynamic(DYN_CAP); m_dynspark = gfx_mesh_dynamic(6000); m_dynhud = gfx_mesh_dynamic(36000);
    sfx_load_samples(); tx_ui = asset_texture("ui", 0, NULL, NULL); batch_init(&g_ub, g_uiv, UI_CAP); batch_init(&g_ua, g_uav, UI_CAP); m_dynui = gfx_mesh_dynamic(UI_CAP); m_dynuia = gfx_mesh_dynamic(UI_CAP);
    static const char *names[ST_N] = {"forest_dragonet", "forest_dragon", "forest_beyonder"};
    for (int s = 0; s < ST_N; s++) g_model_ok[s] = anim3d_load(&g_model[s], names[s]);
    /* the other dragons load when they are needed (their world, their rescue, a save that has them): a quicker start */
    ensure_kind(CAPTIVE[WK_FOREST], ST_DRAGONET);   /* the forest cage holds the sky dragonet */
    gfx_log("Dragons Beyond ready: %d props, %d food, models %d %d %d", g_world.nprop, g_world.nfood, g_model_ok[0], g_model_ok[1], g_model_ok[2]);
    snprintf(g_msg, sizeof g_msg, "WALK TO THE BERRIES. PRESS X TO EAT"); g_msg_t = 8;
    return 1; }
void game_shutdown(void) {}
const char *game_where(void) { static char s[500]; static const char *st[] = {"title", "name pick", "story", "playing", "slots", "difficulty"}; static const char *md[] = {"ground", "air", "swim"}; snprintf(s, sizeof s, "%s: %s, %s, growth %.0f, cam %d, %s at %.0f,%.0f,%.0f anim %d act %.1f evo %.1f hearts %.1f | boss %d hp %d at %.0f,%.0f kind %d swap %d p2 %d at %.0f,%.0f camyaw %.3f msg %s", st[g_state], g_wk == WK_SKY ? "Sky Kingdom" : "Everleaf Wilds", dragon_stage_name(g_dragon.stage), g_dragon.growth, g_cam_mode, md[g_dragon.mode], g_dragon.x, g_dragon.y, g_dragon.z, g_dragon.anim, g_dragon.act_t, g_dragon.evolve_t, g_dragon.hearts, g_boss.state, g_boss.hp, g_boss.x, g_boss.z, g_kind, g_swap_open, g_p2, g_other.x, g_other.z, g_cam_yaw, g_msg_t > 0 ? g_msg : "-"); return s; }


/* ---------- progress: one small text line on the SD card ---------- */
static void slot_path(int k, char *out, size_t n) {   /* base ".../dragonsbeyond_save.txt" -> ".../dragonsbeyond_save1.txt" */
    const char *dot = strrchr(g_save_base, '.'); size_t stem = dot ? (size_t)(dot - g_save_base) : strlen(g_save_base);
    snprintf(out, n, "%.*s%d%s", (int)stem, g_save_base, k + 1, dot ? dot : ""); }
static void slot_summary(FILE *f, SlotInfo *si);
static void scan_slots(void) {
    for (int k = 0; k < 3; k++) { SlotInfo *si = &g_slots[k]; memset(si, 0, sizeof *si); si->diff = 1; if (!g_save_base[0]) continue;
        char path[300]; slot_path(k, path, sizeof path); FILE *f = fopen(path, "r"); if (!f) continue;
        slot_summary(f, si); fclose(f); } }
void game_set_save_path(const char *path) {
    snprintf(g_save_base, sizeof g_save_base, "%s", path); g_save_path[0] = 0;
    char p1[300]; slot_path(0, p1, sizeof p1); FILE *t = fopen(p1, "r");
    if (t) fclose(t); else { FILE *old = fopen(path, "r"); if (old) {   /* an older single save becomes slot 1 */
        char line[256] = {0}; if (fgets(line, sizeof line, old)) { FILE *nw = fopen(p1, "w"); if (nw) { fputs(line, nw); fclose(nw); } } fclose(old); } }
    scan_slots(); }
static void use_slot(int k) { g_slot = k; if (g_save_base[0]) slot_path(k, g_save_path, sizeof g_save_path); }
static int g_wcrystal[WK_N];   /* each world's egg crystal found */
static void enter_world(int wk, float x, float z, float yaw);
static void save_progress(void) {
    if (!g_save_path[0]) return; FILE *f = fopen(g_save_path, "w"); if (!f) return;
    g_wcrystal[g_wk] = g_world.crystal_found;
    fprintf(f, "DB2 %d %d %d %d %d %.2f %.2f %.3f %d %d %d %d", g_name, g_diff, g_wk, g_kind, g_fk, g_dragon.x, g_dragon.z, g_dragon.yaw, g_wrescued[0], g_wrescued[1], g_wcrystal[0], g_wcrystal[1]);
    for (int k = 0; k < K_N; k++) { const Dragon *d = k == g_kind ? &g_dragon : &g_team[k]; fprintf(f, " %d %d %.1f %.1f", g_has[k], g_has[k] ? d->stage : 0, g_has[k] ? d->growth : 0, g_has[k] ? d->hearts : 3); }
    fprintf(f, "\n"); fclose(f); }
/* a save line, either format, into plain numbers */
typedef struct { int name, diff, wk, kind, fk, wres[WK_N], wcry[WK_N], has[K_N], stage[K_N]; float x, z, yaw, growth[K_N], hearts[K_N]; } SaveData;
static int parse_save(FILE *f, SaveData *sd) {
    memset(sd, 0, sizeof *sd); sd->diff = 1; sd->fk = 1; sd->has[0] = 1; for (int k = 0; k < K_N; k++) sd->hearts[k] = 3;
    char line[400]; if (!fgets(line, sizeof line, f)) return 0;
    if (!strncmp(line, "DB2", 3)) {
        int n = sscanf(line + 3, "%d %d %d %d %d %f %f %f %d %d %d %d %d %d %f %f %d %d %f %f %d %d %f %f", &sd->name, &sd->diff, &sd->wk, &sd->kind, &sd->fk, &sd->x, &sd->z, &sd->yaw,
            &sd->wres[0], &sd->wres[1], &sd->wcry[0], &sd->wcry[1], &sd->has[0], &sd->stage[0], &sd->growth[0], &sd->hearts[0], &sd->has[1], &sd->stage[1], &sd->growth[1], &sd->hearts[1], &sd->has[2], &sd->stage[2], &sd->growth[2], &sd->hearts[2]);
        if (n < 16) return 0; }
    else if (!strncmp(line, "DB1", 3)) {   /* the first builds: forest + sky only */
        int stage, cr = 0, resc = 0, kind = 0, sstage = 0, diff = 1; float growth, x, z, yaw, hearts, sgrowth = 0, shearts = 3;
        int n = sscanf(line + 3, "%d %d %f %f %f %f %f %d %d %d %f %f %d %d", &sd->name, &stage, &growth, &x, &z, &yaw, &hearts, &cr, &resc, &kind, &sgrowth, &shearts, &sstage, &diff);
        if (n < 7) return 0;
        sd->diff = diff; sd->x = x; sd->z = z; sd->yaw = yaw; sd->wcry[0] = cr; sd->wres[0] = resc; sd->kind = resc ? (kind & 1) : 0; sd->fk = 1;
        sd->stage[0] = stage; sd->growth[0] = growth; sd->hearts[0] = hearts; sd->has[1] = resc; sd->stage[1] = sstage; sd->growth[1] = sgrowth; sd->hearts[1] = shearts; }
    else return 0;
    sd->name &= 1; if (sd->diff < 0 || sd->diff > 2) sd->diff = 1; if (sd->wk < 0 || sd->wk >= WK_N) sd->wk = 0; sd->has[0] = 1;
    for (int k = 0; k < K_N; k++) { if (sd->stage[k] < 0 || sd->stage[k] >= ST_N) sd->stage[k] = 0; }
    if (sd->kind < 0 || sd->kind >= K_N || !sd->has[sd->kind]) sd->kind = 0;
    return 1; }
static void slot_summary(FILE *f, SlotInfo *si) { SaveData sd; if (!parse_save(f, &sd)) return; si->used = 1; si->name = sd.name; si->stage = sd.stage[0]; si->diff = sd.diff; si->wk = sd.wk; si->rescued = sd.has[1] + sd.has[2]; }
static void new_game(void) {   /* a fresh adventure: just the forest dragonet, in Everleaf Wilds */
    for (int k = 0; k < K_N; k++) g_has[k] = k == 0; for (int w = 0; w < WK_N; w++) { g_wrescued[w] = 0; g_wcrystal[w] = 0; }
    g_kind = 0; g_fk = 1; g_p2 = -1; g_friend.on = 0; dragon_init(&g_dragon, &g_world, 0, 0);
    if (g_wk != WK_FOREST || g_world.kind != WK_FOREST) enter_world(WK_FOREST, 0, 0, 0); else { boss_init(&g_boss, &g_world); g_boss.hp = BOSS_HITS[g_diff]; }
    g_dragon.x = 0; g_dragon.z = 0; g_dragon.y = world_height(&g_world, 0, 0); }
static int load_progress(void) {
    if (!g_save_path[0]) return 0; FILE *f = fopen(g_save_path, "r"); if (!f) return 0;
    SaveData sd; int ok = parse_save(f, &sd); fclose(f); if (!ok) return 0;
    g_name = sd.name; g_diff = sd.diff; g_p2 = -1;
    for (int w = 0; w < WK_N; w++) { g_wrescued[w] = sd.wres[w]; g_wcrystal[w] = sd.wcry[w]; }
    for (int k = 0; k < K_N; k++) { g_has[k] = sd.has[k]; dragon_init(&g_team[k], &g_world, 0, 0); if (!sd.has[k]) continue;
        Dragon *d = &g_team[k]; d->stage = sd.stage[k]; d->max_hearts = 3 + 2 * d->stage; d->growth = clampf(sd.growth[k], 0, dragon_need(d->stage) - 1); d->hearts = clampf(sd.hearts[k], 1, d->max_hearts); }
    g_kind = sd.kind; g_fk = sd.fk;
    if (g_fk == g_kind || g_fk < 0 || g_fk >= K_N || !g_has[g_fk]) { g_fk = -1; for (int k = K_N - 1; k >= 0; k--) if (k != g_kind && g_has[k]) { g_fk = k; break; } if (g_fk < 0) g_fk = 1; }
    g_dragon = g_team[g_kind];
    enter_world(sd.wk, sd.x, sd.z, sd.yaw);
    gfx_log("progress loaded: %s the %s, world %d, team %d", kind_name(g_kind), dragon_stage_name(g_dragon.stage), g_wk, team_size()); return 1; }
static const char *kind_name(int k) { return k == 0 ? NAMES[g_name] : (k == 1 ? SKY_NAMES[g_name] : SNOW_NAMES[g_name]); }
static const char *active_name(void) { return kind_name(g_kind); }
/* swap the player into the other dragon: it takes over where the friend is; the old one becomes the friend */
static void do_swap(int k) {   /* become dragon k; the one you were becomes the friend who follows you */
    if (k == g_kind || !g_has[k]) return;
    Dragon old = g_dragon; Dragon nw = g_team[k]; int was = g_kind;
    float sx = k == g_fk ? g_friend.x : g_dragon.x - sinf(g_dragon.yaw) * 3, sz = k == g_fk ? g_friend.z : g_dragon.z - cosf(g_dragon.yaw) * 3; float sy = k == g_fk ? g_friend.y : g_dragon.y;
    nw.x = sx; nw.z = sz; nw.y = sy; nw.yaw = k == g_fk ? g_friend.yaw : g_dragon.yaw; nw.speed = 0; nw.act_t = 0; nw.evolve_t = 0; nw.anim = DA_IDLE; nw.anim_t = 0;
    float gy = world_height(&g_world, nw.x, nw.z); if (nw.y > gy + 0.5f) { nw.mode = DM_AIR; nw.vy = 0; } else { nw.mode = DM_GROUND; nw.y = gy; }
    g_friend.x = old.x; g_friend.z = old.z; g_friend.y = old.y; g_friend.yaw = old.yaw; g_friend.anim = DA_IDLE; g_friend.anim_t = 0; g_friend.speed = 0; g_friend.on = 1;
    g_team[was] = old; g_dragon = nw; g_kind = k; g_fk = was; g_cam_yaw = g_dragon.yaw;
    snprintf(g_msg, sizeof g_msg, "YOU ARE NOW %s!", active_name()); g_msg_t = 3; sfx_play(g_dragon.stage == ST_DRAGONET ? SFX_CHIRP : SFX_ROAR, 0.8f, 0); save_progress(); }

/* ---------- camera ---------- */
static float g_turn, g_bank, g_bodyph;   /* smoothed turn rate (rad/s), bank angle, phase of the body wave */
static Mat4 dragon_matrix(const Dragon *d, float scale) {
    Mat4 m = m4_mul(m4_translate(d->x, d->y, d->z), m4_rotate_y(d->yaw));
    if (d->mode == DM_AIR) m = m4_mul(m4_mul(m, m4_rotate_x(-d->pitch * 0.8f)), m4_rotate_z(g_bank));
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
        if (g_p2 >= 0) { float px = g_other.x - d->x, pz = g_other.z - d->z; float extra = clampf(sqrtf(px * px + pz * pz) - 6, 0, 30) * 0.45f; back += extra; up += extra * 0.5f; }   /* keep player 2 in the picture */
        want = v3(d->x - sinf(g_cam_yaw) * back, d->y + up, d->z - cosf(g_cam_yaw) * back);
        at = v3(d->x + sinf(g_cam_yaw) * 1.5f * L, d->y + 0.45f * L + (d->mode == DM_AIR ? d->pitch * 2 : 0), d->z + cosf(g_cam_yaw) * 1.5f * L); }
    else if (g_cam_mode == 1) {   /* overhead: high up, looking down, still turning with the dragon */
        float up = 5.0f * L + 10; want = v3(d->x - sinf(g_cam_yaw) * up * 0.45f, d->y + up, d->z - cosf(g_cam_yaw) * up * 0.45f); at = v3(d->x, d->y, d->z); }
    else {                        /* the dragon's own eyes */
        float fwd = 0.42f * L, up = 0.55f * L; want = v3(d->x + sinf(d->yaw) * fwd, d->y + up, d->z + cosf(d->yaw) * fwd);
        at = v3(want.x + sinf(d->yaw) * 10, want.y - (d->mode == DM_AIR ? d->pitch * 8 : 0.8f), want.z + cosf(d->yaw) * 10); }
    /* keep the camera above the ground and out of the trees */
    float gnd = world_height(&g_world, want.x, want.z) + 0.6f; if (want.y < gnd) want.y = gnd;
    if (g_cam_mode != 2) for (int i = 0; i < g_world.nprop; i++) { const Prop *p = &g_world.prop[i];
        if (p->kind == P_BUSH) {   /* bushes: lift the camera over them rather than look through the leaves */
            float bx = want.x - p->x, bz = want.z - p->z, R = 1.6f * p->scale + 0.4f; if (bx * bx + bz * bz < R * R) { float top = world_height(&g_world, p->x, p->z) + 2.0f * p->scale + 0.3f; if (want.y < top) want.y = top; } continue; }
        if (p->kind != P_TREE && p->kind != P_PALM) continue;
        float dx = want.x - p->x, dz = want.z - p->z, d2 = dx * dx + dz * dz; if (d2 > 36) continue;
        float py = world_height(&g_world, p->x, p->z); float h = (p->kind == P_TREE ? (g_pm[PM_OAK].ok ? 9.0f : 7.5f) : 9.0f) * p->scale;
        float trunk_r = 1.0f; if (d2 < trunk_r * trunk_r && want.y < py + h) { float dd = sqrtf(d2) + 1e-4f; want.x = p->x + dx / dd * trunk_r; want.z = p->z + dz / dd * trunk_r; }
        if (p->kind == P_TREE) { Vec3 c = v3(p->x + (g_pm[PM_OAK].ok ? 0 : sinf(p->yaw) * 0.6f), py + h * (g_pm[PM_OAK].ok ? 0.7f : 0.95f), p->z + (g_pm[PM_OAK].ok ? 0 : cosf(p->yaw) * 0.6f)); float R = (g_pm[PM_OAK].ok ? 5.0f : 4.3f) * p->scale; Vec3 off = v3_sub(want, c); float od = sqrtf(v3_dot(off, off));
            if (od < R) { float push = R - od; if (off.y < -R * 0.3f || od < 0.01f) want.y = c.y - R; else { want.x += off.x / od * push; want.y += off.y / od * push; want.z += off.z / od * push; } } } }
    if (g_cam_mode == 2) { g_eye = want; g_at = at; }
    else { float kk = 1 - expf(-dt * 6); g_eye = v3(g_eye.x + (want.x - g_eye.x) * kk, g_eye.y + (want.y - g_eye.y) * kk, g_eye.z + (want.z - g_eye.z) * kk); g_at = v3(g_at.x + (at.x - g_at.x) * kk, g_at.y + (at.y - g_at.y) * kk, g_at.z + (at.z - g_at.z) * kk); } }

/* ---------- drawing ---------- */
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
    /* real models: bushes (duller when the berries are eaten) and mushrooms, one draw each */
    gfx_state(ST_SOLID);
    for (int i = 0; i < g_world.nfood; i++) { const Food *f = &g_world.food[i]; float dx = f->x - eye.x, dz = f->z - eye.z; float d2 = dx * dx + dz * dz; if (d2 > 130 * 130) continue; int lod = d2 > 40 * 40;
        if (f->kind == F_BERRY && g_pm[PM_BUSH].ok) { float t = f->alive ? 1.0f : 0.72f; propmodel_draw(&g_pm[PM_BUSH], lod, f->x, f->y - 0.05f, f->z, f->yaw, 1.0f, t, t * 1.05f, t); }
        if (f->kind == F_MUSHROOM && g_pm[PM_MUSH].ok && f->alive) propmodel_draw(&g_pm[PM_MUSH], lod, f->x, f->y - 0.02f, f->z, f->yaw, 1.0f + 0.06f * sinf(f->phase), 1, 1, 1); }
    for (int i = 0; i < g_world.nfood; i++) { const Food *f = &g_world.food[i]; if (!f->alive) continue;
        float dx = f->x - eye.x, dz = f->z - eye.z; float d2 = dx * dx + dz * dz; if (d2 > 120 * 120) continue; int lod = d2 > 35 * 35;
        if (f->kind == F_BERRY && !g_pm[PM_BUSH].ok) {   /* red berries sitting on the (static) bush */
            unsigned s = (unsigned)i * 97 + 13; for (int k = 0; k < 6; k++) { float a = frand01(&s) * 6.28f, el = 0.3f + frand01(&s) * 1.1f, rr = 0.55f; Vec3 c = v3(f->x + cosf(el) * sinf(a) * rr, f->y + 0.4f + sinf(el) * 0.45f, f->z + cosf(el) * cosf(a) * rr);
                batch_ball(c, 0.1f, 0.1f, 0.1f, lod ? 2 : 3, lod ? 4 : 5, 0.9f, 0.12f, 0.2f, 1); } }
        else if (f->kind == F_MUSHROOM && !g_pm[PM_MUSH].ok) { float h = 0.42f + 0.05f * sinf(f->phase); batch_cyl(v3(f->x, f->y - 0.05f, f->z), v3(f->x, f->y + h, f->z), 0.09f, 0.08f, 6, 0.93f, 0.88f, 0.75f);
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
        batch_ball(v3(dr->x, dr->y, dr->z), 0.28f, 0.22f, 0.5f, 3, 6, 0.3f, 0.32f, 0.36f, 1);                                    /* gunmetal body */
        batch_ball(v3(dr->x + fw.x * 0.55f, dr->y + 0.08f, dr->z + fw.z * 0.55f), 0.16f, 0.14f, 0.2f, 3, 5, 0.26f, 0.28f, 0.32f, 1);   /* head */
        batch_ball(v3(dr->x + fw.x * 0.68f, dr->y + 0.12f, dr->z + fw.z * 0.68f), 0.06f, 0.06f, 0.06f, 2, 4, 1.0f, 0.1f, 0.05f, 1);  /* the red eye */
        for (int sgn = -1; sgn <= 1; sgn += 2) { Vec3 a = v3(dr->x + rt.x * sgn * 0.25f, dr->y + 0.05f, dr->z + rt.z * sgn * 0.25f), b = v3(dr->x + rt.x * sgn * 1.3f, dr->y + 0.05f + fl * 0.9f, dr->z + rt.z * sgn * 1.3f);
            Vec3 a2 = v3(a.x - fw.x * 0.35f, a.y, a.z - fw.z * 0.35f), b2 = v3(b.x - fw.x * 0.5f, b.y, b.z - fw.z * 0.5f); Vec3 n = v3(0, 1, 0);
            bq_push(a, n, 0.22f, 0.23f, 0.27f); bq_push(b, n, 0.34f, 0.36f, 0.42f); bq_push(b2, n, 0.34f, 0.36f, 0.42f); bq_push(a, n, 0.22f, 0.23f, 0.27f); bq_push(b2, n, 0.34f, 0.36f, 0.42f); bq_push(a2, n, 0.22f, 0.23f, 0.27f);
            n = v3(0, -1, 0); bq_push(a, n, 0.2f, 0.21f, 0.25f); bq_push(b2, n, 0.3f, 0.32f, 0.38f); bq_push(b, n, 0.3f, 0.32f, 0.38f); bq_push(a, n, 0.2f, 0.21f, 0.25f); bq_push(a2, n, 0.2f, 0.21f, 0.25f); bq_push(b2, n, 0.3f, 0.32f, 0.38f); } }
    for (int i = 0; i < g_world.nthief; i++) { const Drone *t = &g_world.thief[i]; if (t->state == 4) continue; float dx = t->x - eye.x, dz = t->z - eye.z; if (dx * dx + dz * dz > 120 * 120) continue;
        int ko = t->state == 3; float bob = ko ? 0 : fabsf(sinf(g_time * (t->state ? 14 : 6) + i)) * 0.08f; Vec3 fw = v3(sinf(t->yaw), 0, cosf(t->yaw)), rt = v3(fw.z, 0, -fw.x);
        float y = t->y + bob; float lean = ko ? 0.0f : (t->state ? 0.25f : 0.08f);
        if (ko) { batch_ball(v3(t->x, t->y + 0.15f, t->z), 0.45f, 0.15f, 0.3f, 3, 8, 0.12f, 0.08f, 0.16f, 0); continue; }   /* flat on the ground */
        batch_cyl(v3(t->x, y, t->z), v3(t->x + fw.x * lean, y + 0.85f, t->z + fw.z * lean), 0.34f, 0.07f, 8, 0.13f, 0.09f, 0.17f);   /* black-feather cloak */
        for (int k = 0; k < 6; k++) { float a = k * 1.047f + t->yaw; batch_cyl(v3(t->x + sinf(a) * 0.3f, y + 0.05f, t->z + cosf(a) * 0.3f), v3(t->x + sinf(a) * 0.42f, y - 0.02f, t->z + cosf(a) * 0.42f), 0.05f, 0.01f, 4, 0.07f, 0.06f, 0.09f); }   /* ragged hem */
        Vec3 hd = v3(t->x + fw.x * (lean + 0.08f), y + 0.95f, t->z + fw.z * (lean + 0.08f));
        batch_ball(hd, 0.2f, 0.22f, 0.2f, 4, 8, 0.11f, 0.08f, 0.14f, 0);   /* hood */
        for (int sgn = -1; sgn <= 1; sgn += 2) batch_ball(v3(hd.x + fw.x * 0.17f + rt.x * 0.07f * sgn, hd.y, hd.z + fw.z * 0.17f + rt.z * 0.07f * sgn), 0.045f, 0.035f, 0.03f, 2, 5, 1.6f, 1.3f, 0.2f, 0);   /* glowing eyes */
        batch_cyl(v3(hd.x, hd.y + 0.15f, hd.z), v3(hd.x - fw.x * 0.25f, hd.y + 0.45f, hd.z - fw.z * 0.25f), 0.06f, 0.0f, 4, 0.1f, 0.07f, 0.12f);   /* the hood's point */
        if (t->flap > 0) batch_ball(v3(t->x - fw.x * 0.35f, y + 0.75f, t->z - fw.z * 0.35f), 0.26f, 0.3f, 0.24f, 4, 8, 0.55f, 0.38f, 0.2f, 0); }   /* the stolen food sack */
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
    { /* the cage: iron bars hanging on a chain from the big tree; lowered and opened once Ironhorn falls */
      float open = g_boss.cage_open, ground = g_world.arena_y + 0.1f; float cy = g_world.cage_y - open * (g_world.cage_y - ground);
      float dx = g_world.cage_x - eye.x, dz = g_world.cage_z - eye.z;
      if (dx * dx + dz * dz < 140 * 140) {
        Vec3 c = v3(g_world.cage_x, cy, g_world.cage_z); float R = 1.0f, H = 1.7f; float iron = 0.28f;
        batch_cyl(v3(c.x, cy + H, c.z), v3(c.x, g_world.cage_y + 4.0f, c.z), 0.05f, 0.05f, 5, 0.35f, 0.33f, 0.3f);   /* chain */
        batch_ball(v3(c.x, cy, c.z), R + 0.1f, 0.07f, R + 0.1f, 3, 12, iron, iron, iron * 1.1f, 0);             /* floor */
        batch_ball(v3(c.x, cy + H, c.z), R + 0.1f, 0.25f, R + 0.1f, 4, 12, iron, iron, iron * 1.1f, 0.5f);      /* domed top */
        float facing = atan2f(g_world.arena_x - c.x, g_world.arena_z - c.z);
        for (int i = 0; i < 12; i++) { float a = i * 6.2831853f / 12; float off = fabsf(fmodf(a - facing + 9.42478f, 6.2831853f) - 3.14159f);
            if (open > 0.6f && off < 0.6f) continue;   /* the door bars are gone */
            batch_cyl(v3(c.x + sinf(a) * R, cy, c.z + cosf(a) * R), v3(c.x + sinf(a) * R, cy + H, c.z + cosf(a) * R), 0.045f, 0.045f, 4, iron, iron, iron * 1.1f); } } }
    if (g_boss.state != B_DOWN || g_boss.t < 1.0f) {   /* Ironhorn's core: pulses, flares bright when it is open to a hit */
      Vec3 c = boss_core(&g_boss); int vul = boss_vulnerable(&g_boss); float p = 0.5f + 0.5f * sinf(g_time * (vul ? 14 : 5));
      float r = vul ? 0.42f + 0.1f * p : 0.3f + 0.04f * p;
      batch_ball(c, r, r, r, 5, 8, vul ? 1.6f : 1.3f, vul ? 1.2f : 0.55f + 0.25f * p, vul ? 0.5f : 0.12f, 0); }
    if (!g_b.n) return; gfx_mesh_update(m_dynfood, g_b.v, g_b.n); Mat4 id = m4_identity(); gfx_bind(tx_font); gfx_state(ST_SOLID); gfx_draw_ex(m_dynfood, &id, 1, 1, 1, 1, 0.3f, 0, 0, 0); }

static void draw_dragon(const Dragon *d) {
    int st = d->stage; float sc = dragon_scale(st);
    if (d->evolve_t > 0) {   /* grows from the old size to the new in the first second, with a glow */
        float t = clampf(1 - d->evolve_t / 2.0f, 0, 1); float from = dragon_scale(st - 1 >= 0 ? st - 1 : 0); sc = from + (sc - from) * clampf(t * 1.6f, 0, 1); }
    Anim3D *m = &g_model[st]; if (!m->ok) m = g_model_ok[0] ? &g_model[0] : NULL;
    if (g_kind != 0) { m = kind_model(g_kind, st); sc = kind_scale(g_kind, st); if (d->evolve_t > 0) { float t = clampf(1 - d->evolve_t / 2.0f, 0, 1); float from = kind_scale(g_kind, st > 0 ? st - 1 : 0); sc = from + (sc - from) * clampf(t * 1.6f, 0, 1); } }
    if (!m) return;
    Mat4 base = dragon_matrix(d, sc); int clip = anim3d_clip(m, dragon_anim_name(d->anim)); if (clip < 0) clip = 0;
    if (d->mode == DM_AIR && d->anim == DA_GLIDE && d->pitch < -0.3f) { int dv = anim3d_clip(m, "dive"); if (dv >= 0) clip = dv; }   /* nose down: wings swept back in a dive */
    gfx_state(ST_SOLID);
    float glow = d->evolve_t > 0 ? 0.5f + 0.5f * sinf(g_time * 20) : 0;
    { int air = d->mode == DM_AIR; float arc = clampf(g_turn * (air ? 0.55f : 0.35f), -1, 1);
      float climb = air ? clampf(d->vy / 5.0f, 0, 1) : 0; float side = air ? 0.35f + 0.25f * fabsf(arc) : (d->speed > 0.5f ? 0.25f : 0.0f);
      anim3d_set_bend(arc, side, air ? 0.3f + 0.9f * climb : 0, g_bodyph); }
    gfx_bump(2); anim3d_draw(m, clip, d->anim_t, &base, 1.12f + glow, 1.12f + glow, 1.1f + 0.6f * glow, 1); gfx_bump(0); gfx_bind(NULL); }

static void draw_sparkles(const Dragon *d, Vec3 eye) {
    float ccx, ccz; int col = column_at(&ccx, &ccz) && (ccx - eye.x) * (ccx - eye.x) + (ccz - eye.z) * (ccz - eye.z) < 170 * 170;
    if (d->evolve_t <= 0 && !col && g_power_t <= 0) return; batch_clear(&g_b); unsigned s = 9; float L = dragon_length(d->stage); float t = 2.0f - d->evolve_t;
    Vec3 right = v3_norm(v3_cross(v3(0, 1, 0), v3_sub(d->x < eye.x ? v3(d->x, d->y, d->z) : v3(d->x, d->y, d->z), eye))); Vec3 up = v3(0, 1, 0);
    for (int i = 0; i < 60; i++) { float a = frand01(&s) * 6.28f, r = frand01(&s) * L * 0.9f, h = frand01(&s) * L * 0.8f, ph = frand01(&s) * 6.28f;
        float rise = fmodf(t * (0.8f + frand01(&s)) + ph, 2.0f); float sz = 0.08f * L * (1 - rise / 2) + 0.05f;
        Vec3 c = v3(d->x + cosf(a + t) * r, d->y + h + rise * L * 0.6f, d->z + sinf(a + t) * r);
        if (d->evolve_t > 0) batch_quad(&g_b, c, v3(right.x * sz, 0, right.z * sz), v3(up.x * sz, up.y * sz, up.z * sz), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 0.7f, 1 - rise / 2); }
    if (col) { float gy = world_height(&g_world, ccx, ccz); Vec3 rt = v3_norm(v3_cross(v3(0, 1, 0), v3_sub(v3(ccx, gy, ccz), eye)));   /* a rising spiral of light */
        for (int i = 0; i < 220; i++) { float u = fmodf(i * 0.6180339f + g_time * 0.18f, 1.0f); float a = i * 2.39996f + g_time * 1.6f; float r = 1.2f + 1.8f * u + 0.4f * sinf(i * 1.3f);
            float h = u * 34; float sz = 0.35f + 0.25f * sinf(i * 0.7f + g_time * 3); float al = (1 - u) * fminf(1, u * 8);
            Vec3 c = v3(ccx + cosf(a) * r, gy + h, ccz + sinf(a) * r); float bl = g_wk == WK_FOREST ? 1.0f : 0.85f;
            batch_quad(&g_b, c, v3(rt.x * sz, 0, rt.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.75f, 0.9f * bl, 1.0f, al); } }
    if (g_power_t > 0) { float L2 = dragon_length(d->stage); float fx = sinf(d->yaw), fz = cosf(d->yaw); float age = 1.4f - g_power_t; Vec3 hd = v3(d->x + fx * 0.55f * L2, d->y + 0.55f * L2, d->z + fz * 0.55f * L2);
        Vec3 rt2 = v3_norm(v3_cross(v3(0, 1, 0), v3_sub(hd, eye)));
        for (int i = 0; i < 150; i++) { float u = fmodf(i * 0.618034f + age * 1.7f, 1.0f); float a = i * 2.39996f; Vec3 c; float r, g, b, sz;
            if (g_power_kind == 0) { float rad = 1.0f + u * (6 + 2 * L2); float ang = a + age * 9 * (1.2f - u); c = v3(d->x + cosf(ang) * rad, d->y + u * 5 + 0.4f * sinf(a * 3), d->z + sinf(ang) * rad); r = 0.35f + 0.3f * (i % 3 == 0); g = 0.9f; b = 0.25f; sz = 0.35f; }
            else { float dist = u * (10 + 3 * L2); float spread = 0.08f + u * 0.45f; float sx = cosf(a) * spread * dist, sy = sinf(a) * spread * dist * 0.6f;
                c = v3(hd.x + fx * dist + fz * sx, hd.y + sy - u * u * 1.5f, hd.z + fz * dist - fx * sx);
                if (g_power_kind == 1) { r = 1.0f; g = 0.75f - 0.45f * u; b = 0.2f - 0.15f * u; } else { r = 0.75f + 0.2f * u; g = 0.9f + 0.1f * u; b = 1.0f; } sz = 0.25f + u * 0.9f; }
            float al = clampf(g_power_t / 0.4f, 0, 1) * (1 - u * 0.6f);
            batch_quad(&g_b, c, v3(rt2.x * sz, 0, rt2.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0, 0, 1, 1, r, g, b, al); } }
    gfx_mesh_update(m_dynspark, g_b.v, g_b.n); Mat4 id = m4_identity(); gfx_bind(tx_spark); gfx_state(ST_DEPTH | ST_BLEND | ST_ADD); gfx_draw_ex(m_dynspark, &id, 1, 1, 1, 1, 0, 1, 0, 0); }

static void draw_ironhorn(Vec3 eye) {
    const PropModel *m = &g_pm[g_wk == WK_SKY ? PM_STORM : PM_IRON]; if (!m->ok) return; const Boss *b = &g_boss;
    float dx = b->x - eye.x, dz = b->z - eye.z; int lod = dx * dx + dz * dz > 45 * 45;
    float pitch = 0, roll = 0, yaw = b->yaw, y = b->y;
    if (g_wk == WK_SKY && b->state != B_DOWN) y += 0.6f + 0.25f * sinf(g_time * 3);   /* the eagle hops on its talons, wings half open */
    if (b->state == B_CHARGE) { pitch = g_wk == WK_SKY ? 0.45f : 0.12f; y += g_wk == WK_SKY ? 1.0f : 0.15f * fabsf(sinf(g_time * 14)); }
    else if (b->state == B_STALK) { pitch = 0.04f * sinf(g_time * 6); if (b->speed < 0) y += 0.05f * fabsf(sinf(g_time * 8)); }
    else if (b->state == B_STUCK) { pitch = 0.18f; yaw += 0.06f * sinf(g_time * 30); }
    else if (b->state == B_STUNNED || b->state == B_HURT) { roll = 0.12f * sinf(g_time * 9); pitch = -0.05f; }
    else if (b->state == B_DOWN) { float k = fminf(1, b->t / 0.8f); roll = 1.35f * k; y -= 0.6f * k; }
    Mat4 mm = m4_mul(m4_mul(m4_mul(m4_translate(b->x, y, b->z), m4_rotate_y(yaw)), m4_rotate_x(pitch)), m4_rotate_z(roll));
    gfx_state(ST_SOLID); gfx_bind(m->tex); gfx_draw(m->mesh[lod], &mm, 1, 1, 1, 1); gfx_bind(NULL); }
static void draw_kind_dragon(int k, int st, float x, float y, float z, float yaw, int anim, float anim_t, int air) {
    Anim3D *m = kind_model(k, st); if (!m) return; float scale = kind_scale(k, st); int clip = anim3d_clip(m, dragon_anim_name(anim)); if (clip < 0) clip = 0;
    Mat4 mm = m4_mul(m4_mul(m4_translate(x, y, z), m4_rotate_y(yaw)), m4_scale(scale, scale, scale));
    anim3d_set_bend(0, air ? 0.35f : 0, air ? 0.4f : 0, g_time * 5.5f + 1.3f);
    gfx_state(ST_SOLID); gfx_bump(2); anim3d_draw(m, clip, anim_t, &mm, 1.1f, 1.1f, 1.1f, 1); gfx_bump(0); gfx_bind(NULL); }
static void draw_kaine(void) {
    if (g_kaine_t < 1.5f || g_kaine_t > 13 || !g_model_ok[ST_BEYONDER]) return;
    if ((g_kind == 0 && g_dragon.stage == ST_BEYONDER) || (g_friend.on && g_fk == 0 && g_other.stage == ST_BEYONDER)) return;   /* that model is busy being a player's dragon */ float u = (g_kaine_t - 1.5f) / 11.5f;
    float a = -2.2f + u * 3.4f; float r = 70 - 30 * sinf(u * 3.14159f); Vec3 c = v3(g_dragon.x + sinf(a) * r, g_dragon.y + 16 + 10 * sinf(u * 3.14159f), g_dragon.z + cosf(a) * r);
    float yaw = a + 1.5708f; float sc = dragon_scale(ST_BEYONDER) * 1.25f;
    Mat4 mm = m4_mul(m4_mul(m4_mul(m4_translate(c.x, c.y, c.z), m4_rotate_y(yaw)), m4_rotate_z(-0.35f)), m4_scale(sc, sc, sc));
    Anim3D *m = &g_model[ST_BEYONDER]; int clip = anim3d_clip(m, "fly"); anim3d_set_bend(0.3f, 0.35f, 0.3f, g_time * 5.5f);
    gfx_state(ST_SOLID); anim3d_draw(m, clip < 0 ? 0 : clip, g_time * 1.3f, &mm, 0.16f, 0.15f, 0.2f, 1); gfx_bind(NULL); }
static void draw_arena(Vec3 eye) {
    draw_ironhorn(eye); draw_kaine();
    if (!g_rescued) { float open = g_boss.cage_open, ground = g_world.arena_y + 0.1f; float cy = g_world.cage_y - open * (g_world.cage_y - ground);
        float dx = g_world.cage_x - eye.x, dz = g_world.cage_z - eye.z;
        if (dx * dx + dz * dz < 140 * 140) draw_kind_dragon(CAPTIVE[g_wk], ST_DRAGONET, g_world.cage_x, cy + 0.08f, g_world.cage_z, atan2f(g_world.arena_x - g_world.cage_x, g_world.arena_z - g_world.cage_z) + 0.4f * sinf(g_time * 0.7f), DA_IDLE, g_time, 0); }
    if (g_friend.on) draw_kind_dragon(g_fk, g_other.stage, g_friend.x, g_friend.y, g_friend.z, g_friend.yaw, g_friend.anim, g_friend.anim_t, g_friend.mode); }
static void draw_scene(int drc) {
    float aspect = gfx_aspect(drc); Vec3 eye = g_eye, at = g_at;
    float cr = g_dragon.mode == DM_AIR && g_cam_mode != 1 ? g_bank * (g_cam_mode == 2 ? 0.6f : 0.3f) : 0;   /* the camera leans into a banked turn with the dragon */
    Vec3 up = v3(-sinf(cr) * cosf(g_cam_yaw), cosf(cr), sinf(cr) * sinf(g_cam_yaw));
    Mat4 proj = m4_perspective(g_cam_mode == 2 ? 1.25f : 1.05f, aspect, 0.2f, 1500.0f), view = m4_look_at(eye, at, up); Mat4 vp = m4_mul(proj, view);
    Vec3 L = sun_dir();
    gfx_grade(0.9f, 0.3f, 0.22f, 1.06f); gfx_light(v3(1.0f, 0.92f, 0.78f), v3(0.5f, 0.62f, 0.82f), v3(0.3f, 0.33f, 0.2f), eye);
    gfx_shadow_use(1); gfx_camera(&vp, L, 0.34f, g_sky_photo ? 0.66f : 0.74f, g_sky_photo ? 0.70f : 0.82f, g_sky_photo ? 0.80f : 0.92f, 140, 620);   /* fog fades into the sky picture's horizon */ gfx_shadow_use(0);
    Mat4 id = m4_identity();
    /* sky, around the camera, no depth */
    Mat4 skym = m4_translate(eye.x, eye.y - 60, eye.z); gfx_state(0); gfx_bind(g_sky_photo ? tx_sky : NULL); gfx_draw_ex(m_sky, &skym, 1, 1, 1, 1, 0, 1, 0, 0); if (m_sun) { gfx_state(ST_BLEND); gfx_bind(tx_font); gfx_draw_ex(m_sun, &skym, 1, 1, 1, 1, 0, 1, 0, 0); gfx_state(0); gfx_bind(NULL); }
    gfx_state(ST_DEPTH | ST_DEPTH_WRITE); if (m_mountains) { gfx_bind(tx_rock); gfx_draw(m_mountains, &id, 1, 1, 1, 1); } if (m_apron) { gfx_bind(tx_grass); gfx_draw(m_apron, &id, 1, 1, 1, 1); }
    if (m_cloudsea) { gfx_bind(tx_font); gfx_draw_ex(m_cloudsea, &id, 1, 1, 1, 1, 0, 1, 0, 0); }
    gfx_clear_depth(drc);
    gfx_state(ST_SOLID);
    gfx_bind(tx_grass); gfx_bump(3); gfx_draw(m_ground, &id, 1, 1, 1, 1);
    gfx_state(ST_TRANSPARENT); if (m_ground_dirt) { gfx_bind(tx_dirt); gfx_bump(3); gfx_draw(m_ground_dirt, &id, 1, 1, 1, 1); } if (m_ground_rock) { gfx_bind(tx_rock); gfx_bump(4); gfx_draw(m_ground_rock, &id, 1, 1, 1, 1); } gfx_bump(0); gfx_state(ST_SOLID);
    gfx_bind(tx_bark); if (m_bark) gfx_draw(m_bark, &id, 1, 1, 1, 1);
    gfx_bind(tx_rock); if (m_rock) gfx_draw(m_rock, &id, 1, 1, 1, 1);
    scenelayer_view(at.x - eye.x, at.z - eye.z); scenelayer_draw(&g_lay_tree, eye, LD_HI, LD_SKIP); scenelayer_draw(&g_lay_palm, eye, LD_HI, LD_SKIP); scenelayer_draw(&g_lay_fern, eye, LD_HI * 0.6f, 150); scenelayer_draw(&g_lay_bush, eye, LD_HI * 0.7f, 170); scenelayer_view(0, 0);
    gfx_state(ST_CUTOUT);
    gfx_bind(tx_leaves); if (m_leaves) gfx_draw(m_leaves, &id, 1, 1, 1, 1);
    gfx_bind(tx_fern); if (m_fern) gfx_draw(m_fern, &id, 1, 1, 1, 1);
    gfx_bind(tx_bush); if (m_bushes) gfx_draw(m_bushes, &id, 1, 1, 1, 1);
    gfx_bind(tx_tuft); if (m_tufts) gfx_draw(m_tufts, &id, 1, 1, 1, 1);
    gfx_bind(tx_flower); if (m_flowers) gfx_draw(m_flowers, &id, 1, 1, 1, 1);
    draw_food3d(eye);
    draw_arena(eye);
    if (g_cam_mode != 2) draw_dragon(&g_dragon);
    if (m_water) { gfx_state(ST_TRANSPARENT); gfx_bind(tx_water); gfx_draw_ex(m_water, &id, 1, 1, 1, 0.8f, 0.9f, 0, g_time * 0.02f, g_time * 0.013f); }
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
    scenelayer_draw(&g_lay_tree, v3(d->x, d->y, d->z), 0, 95); scenelayer_draw(&g_lay_palm, v3(d->x, d->y, d->z), 0, 95);   /* the trees' shadows (low detail is plenty for a shadow) */
    draw_arena(v3(d->x, d->y, d->z));
    draw_dragon(d);
    gfx_shadow_end(); }

static void text_wrap_c(const char *txt, float y, float size, float maxw, float r, float g, float b, float a);
static int text_wrap_n(const char *txt, float y, float size, float maxw, float r, float g, float b, float a, int draw);
static void text_c(const char *s, float y, float size, float r, float g, float b, float a) { batch_text(&g_b, s, 640 - text_width(s, size) / 2, y, size, r, g, b, a); }
static void text_cg(const char *s, float y, float size, float a) { batch_text_g(&g_b, s, 640 - text_width(s, size) / 2, y, size, 1, 1, 1, a, 1); }

/* ---------- the menu art: carved stone, old gold and jewels (content/img/ui.png, made by tools/make_ui.py) ----------
   three layers each frame: g_ub (panels, under the words), the words (g_b), then g_ua (light: glints, embers, shine) added on top */
static void ui_part(Batch *b, int id, float fx0, float fy0, float fx1, float fy1, float x, float y, float w, float h, float r, float g, float bl, float a) {
    const UiPic *p = &UI_PICS[id]; float u0 = (p->x + fx0 * p->w) / 1024.0f, v0 = (p->y + fy0 * p->h) / 1024.0f, u1 = (p->x + fx1 * p->w) / 1024.0f, v1 = (p->y + fy1 * p->h) / 1024.0f;
    batch_rect_uv(b, x, y, w, h, u0, v0, u1, v1, r, g, bl, a); }
static void ui_pic(Batch *b, int id, float x, float y, float w, float h, float r, float g, float bl, float a) { ui_part(b, id, 0, 0, 1, 1, x, y, w, h, r, g, bl, a); }
static void ui_pic_c(Batch *b, int id, float cx, float cy, float w, float h, float r, float g, float bl, float a) { ui_pic(b, id, cx - w / 2, cy - h / 2, w, h, r, g, bl, a); }
static void ui_pic_rot(Batch *b, int id, float cx, float cy, float w, float h, float ang, float r, float g, float bl, float a) {
    const UiPic *p = &UI_PICS[id]; float c = cosf(ang), s = sinf(ang), hx = w / 2, hy = h / 2; Vec3 q[4]; static const float K[4][2] = {{-1, 1}, {1, 1}, {1, -1}, {-1, -1}};   /* screen y grows down: bottom-left first */
    for (int i = 0; i < 4; i++) { float x = K[i][0] * hx, y = K[i][1] * hy; q[i] = scr(cx + x * c - y * s, cy + x * s + y * c); }
    batch_quad4(b, q[0], q[1], q[2], q[3], v3(0, 0, 1), p->x / 1024.0f, p->y / 1024.0f, (p->x + p->w) / 1024.0f, (p->y + p->h) / 1024.0f, r, g, bl, a); }
static void ui_rect(Batch *b, float x, float y, float w, float h, float r, float g, float bl, float a) { ui_pic(b, UI_WHITE, x, y, w, h, r, g, bl, a); }
/* a light running round the edge of a box */
static void ui_glint(float x, float y, float w, float h, float t, float size, float a) {
    float per = 2 * (w + h), d = fmodf(t, 1.0f) * per, px, py;
    if (d < w) { px = x + d; py = y; } else if (d < w + h) { px = x + w; py = y + d - w; } else if (d < 2 * w + h) { px = x + w - (d - w - h); py = y + h; } else { px = x; py = y + h - (d - 2 * w - h); }
    ui_pic_c(&g_ua, UI_GLOW, px, py, size, size, 1, 0.85f, 0.5f, a); ui_pic_c(&g_ua, UI_GLOW, px, py, size * 0.35f, size * 0.35f, 1, 1, 0.9f, a); }
/* the stone slab with its gold rim, cut in nine so it stretches to any size without bending the corners */
static void ui_panel(float x, float y, float w, float h, float a, int sel) {
    const float F = 104.0f / 512.0f; float c = fminf(fminf(76, w / 2), h / 2); float xs[4] = {x, x + c, x + w - c, x + w}, ys[4] = {y, y + c, y + h - c, y + h}, fs[4] = {0, F, 1 - F, 1};
    float k = sel ? 1.0f : 0.78f;
    for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) ui_part(&g_ub, UI_PANEL, fs[i], fs[j], fs[i + 1], fs[j + 1], xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j], k, k, k * (sel ? 0.95f : 1), a);
    if (sel) { float pulse = 0.5f + 0.5f * sinf(g_time * 4);
        ui_glint(x + c * 0.25f, y + c * 0.25f, w - c * 0.5f, h - c * 0.5f, g_time * 0.25f, 70, 0.8f * a); ui_glint(x + c * 0.25f, y + c * 0.25f, w - c * 0.5f, h - c * 0.5f, g_time * 0.25f + 0.5f, 70, 0.8f * a);
        ui_pic_c(&g_ub, UI_GEM, x - 8, y + h / 2, 56, 56, 1, 1, 1, a); ui_pic_c(&g_ua, UI_GLOW, x - 8, y + h / 2, 110, 110, 0.2f, 1, 0.5f, (0.35f + 0.35f * pulse) * a); } }
/* a gold bar in a little gold frame, filled with a jewel colour that shimmers */
static void ui_bar(float x, float y, float w, float h, float f, float r, float g, float bl) {
    ui_rect(&g_ub, x - 4, y - 4, w + 8, h + 8, 0.45f, 0.3f, 0.1f, 0.95f); ui_rect(&g_ub, x - 2, y - 2, w + 4, h + 4, 1, 0.78f, 0.35f, 1); ui_rect(&g_ub, x, y, w, h, 0.03f, 0.03f, 0.03f, 0.9f);
    if (f <= 0) return; ui_rect(&g_ub, x, y, w * f, h, r, g, bl, 1); ui_rect(&g_ub, x, y, w * f, h * 0.4f, fminf(r + 0.35f, 1), fminf(g + 0.35f, 1), fminf(bl + 0.35f, 1), 0.6f);
    float sx = x + fmodf(g_time * 160, w * f + 60) - 30; if (sx > x && sx < x + w * f) ui_pic_c(&g_ua, UI_GLOW, sx, y + h / 2, h * 3, h * 1.6f, r, g, bl, 0.6f); }
/* hearts are rubies: dark when lost, filling from the bottom */
static void ui_heart(float cx, float cy, float s, float full) {
    ui_pic_c(&g_ub, UI_RUBY, cx, cy, s, s, 0.3f, 0.25f, 0.25f, 0.85f);
    if (full > 0) { ui_part(&g_ub, UI_RUBY, 0, 1 - full, 1, 1, cx - s / 2, cy - s / 2 + s * (1 - full), s, s * full, 1, 1, 1, 1);
        if (full >= 1) ui_pic_c(&g_ua, UI_GLOW, cx, cy, s * 1.5f, s * 1.5f, 1, 0.15f, 0.2f, 0.18f + 0.1f * sinf(g_time * 2.5f + cx)); } }
static float ui_hash(int i) { unsigned h = (unsigned)i * 2654435761u; h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; return (h & 0xFFFF) / 65535.0f; }
/* embers drifting up out of the dark */
static void ui_embers(int n, float a) {
    for (int i = 0; i < n; i++) { float sp = 30 + 50 * ui_hash(i * 3 + 1), life = 900 / sp, t = fmodf(g_time + ui_hash(i * 3 + 2) * life, life), y = 760 - t * sp;
        float x = ui_hash(i * 3) * 1280 + 30 * sinf(g_time * (0.6f + ui_hash(i + 77)) + i), s = 10 + 18 * ui_hash(i + 33), fade = sinf(3.14159f * t / life) * (0.6f + 0.4f * sinf(g_time * 7 + i));
        ui_pic_c(&g_ua, UI_EMBER, x, y, s, s, 1, 0.55f + 0.3f * ui_hash(i + 5), 0.2f, a * fade); } }
/* darken the picture, darkest at the edges */
static void ui_shade(float a) { ui_rect(&g_ub, 0, 0, 1280, 720, 0.02f, 0.015f, 0.01f, 0.45f * a); ui_pic(&g_ub, UI_VIGNETTE, -80, -60, 1440, 840, 1, 1, 1, a); }
static void ui_divider(float cx, float y, float w, float a) { ui_pic_c(&g_ub, UI_DIVIDER, cx, y, w, w / 8, 1, 1, 1, a); ui_pic_c(&g_ua, UI_GLOW, cx + sinf(g_time * 0.9f) * w * 0.4f, y, 60, 18, 1, 0.8f, 0.4f, 0.5f * a); }
/* the gold title, with a band of light sweeping across the letters every few seconds */
static void ui_logo(float cx, float y, float w, float a) {
    const UiPic *p = &UI_PICS[UI_LOGO]; float h = w * p->h / p->w, x = cx - w / 2; ui_pic(&g_ub, UI_LOGO, x + 6, y + 8, w, h, 0, 0, 0, 0.5f * a); ui_pic(&g_ub, UI_LOGO, x, y, w, h, 1, 1, 1, a);
    float sweep = fmodf(g_time * 0.45f, 1.6f) * 1.4f - 0.2f; const int N = 48;
    for (int i = 0; i < N; i++) { float f0 = (float)i / N, f1 = (float)(i + 1) / N, d = (f0 + f1) / 2 - sweep, k = expf(-d * d * 120);
        if (k > 0.02f) ui_part(&g_ua, UI_LOGO, f0, 0, f1, 1, x + f0 * w, y, (f1 - f0) * w, h, 1, 0.9f, 0.7f, 0.9f * k * a); } }
/* just the gold rim of a panel, nothing in the middle: a frame round a picture (the GamePad map) */
static void ui_frame(float x, float y, float w, float h, float a) {
    const float F = 104.0f / 512.0f; float c = fminf(fminf(76, w / 2), h / 2); float xs[4] = {x, x + c, x + w - c, x + w}, ys[4] = {y, y + c, y + h - c, y + h}, fs[4] = {0, F, 1 - F, 1};
    for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) if (i != 1 || j != 1) ui_part(&g_ub, UI_PANEL, fs[i], fs[j], fs[i + 1], fs[j + 1], xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j], 1, 1, 1, a); }
static void ui_begin(void) { batch_clear(&g_ub); batch_clear(&g_ua); }
/* draw the three layers (call once per view, after the words are in g_b) */
static void ui_flush(void) {
    Mat4 id = m4_identity(); gfx_camera_2d();
    if (tx_ui && g_ub.n) { gfx_state(ST_HUD); gfx_bind(tx_ui); gfx_mesh_update(m_dynui, g_ub.v, g_ub.n); gfx_draw_ex(m_dynui, &id, 1, 1, 1, 1, 0, 1, 0, 0); }
    if (g_b.n) { gfx_state(ST_HUD); gfx_bind(tx_font); gfx_mesh_update(m_dynhud, g_b.v, g_b.n); gfx_draw_ex(m_dynhud, &id, 1, 1, 1, 1, 0, 1, 0, 0); }
    if (tx_ui && g_ua.n) { gfx_state(ST_BLEND | ST_ADD); gfx_bind(tx_ui); gfx_mesh_update(m_dynuia, g_ua.v, g_ua.n); gfx_draw_ex(m_dynuia, &id, 1, 1, 1, 1, 0, 1, 0, 0); }
    gfx_bind(NULL); }
#ifdef DB_TOUR
static float g_tour_real_dt = 0.1f; static int g_tour_presses, g_tour_takeoffs, g_tour_a;
#endif
static void draw_hud(int drc) {
    const Dragon *d = &g_dragon; batch_clear(&g_b); ui_begin();
#ifdef DB_TOUR
    { char tb[200]; snprintf(tb, sizeof tb, "T %.0f FPS %.1f %s ANIM %d ACT %.1f EVO %.1f STAM %.2f VY %.1f A %d PRESSES %d TAKEOFFS %d", g_time, 1.0f / fmaxf(g_tour_real_dt, 0.001f), d->mode == DM_AIR ? "AIR" : d->mode == DM_SWIM ? "SWIM" : "GROUND", d->anim, d->act_t, d->evolve_t, d->stamina, d->vy, g_tour_a, g_tour_presses, g_tour_takeoffs); batch_text(&g_b, tb, 40, 690, 14, 1, 1, 0.6f, 0.9f); }
#endif
    if (!g_swap_open && g_tale_i < 0) {   /* the play HUD hides under the menus and story cards */
    /* hearts */
    for (int i = 0; i < (int)d->max_hearts; i++) ui_heart(58 + i * 42, 50, 44, clampf(d->hearts - i, 0, 1));
    if (g_p2 >= 0) {   /* player 2's dragon, top right */
        const Dragon *o = &g_other; const char *nm = kind_name(g_fk); char lab[48]; snprintf(lab, sizeof lab, "P2  %s", nm);
        batch_text(&g_b, lab, 1240 - text_width(lab, 22), 78, 22, 1, 0.8f, 0.8f, 1);
        for (int i = 0; i < (int)o->max_hearts; i++) { float full = clampf(o->hearts - i, 0, 1); float hx = 1240 - (o->max_hearts - i) * 30; ui_heart(hx + 12, 122, 30, full); }
        if (o->stage < ST_BEYONDER) { float k = clampf(o->growth / dragon_need(o->stage), 0, 1); ui_bar(1040, 144, 200, 10, k, 0.2f, 0.85f, 0.35f); } }
    else if (team_size() > 1 && g_friend.on && g_time - g_gp_last < 20 && fmodf(g_time, 30.0f) < 4) {   /* invite a second player now and then */
        const char *inv = "PLAYER 2: PRESS A ON A WII REMOTE TO JOIN"; batch_text(&g_b, inv, 1240 - text_width(inv, 12), 64, 12, 1, 1, 1, 0.75f); }
    /* Ironhorn's health, while the fight is on and the player is near */
    if (g_boss.woke && g_boss.state != B_DOWN) { float dx = d->x - g_boss.x, dz = d->z - g_boss.z;
        if (dx * dx + dz * dz < 60 * 60) { ui_panel(440, 132, 400, 90, 0.9f, 0); text_cg(BOSS_NAME[g_wk], 150, 26, 1);
            for (int i = 0; i < BOSS_HITS[g_diff]; i++) { float x = 640 - BOSS_HITS[g_diff] * 34 + i * 68; ui_bar(x + 8, 188, 52, 10, i < g_boss.hp, 1.0f, 0.4f, 0.08f); } } }
    /* growth bar */
    float need = dragon_need(d->stage); float gf = need > 1e8f ? 1 : clampf(d->growth / need, 0, 1);
    ui_bar(42, 82, 356, 16, gf, 0.25f, 0.8f, 0.2f);
    { char nm[48]; snprintf(nm, sizeof nm, "%s THE %s", active_name(), dragon_stage_name(d->stage)); batch_text_g(&g_b, nm, 42, 112, 24, 1, 1, 1, 1, 1); }
    if (need < 1e8f) { char s[32]; snprintf(s, sizeof s, "GROW %d/%d", (int)d->growth, (int)need); batch_text(&g_b, s, 414, 78, 20, 0.9f, 1, 0.8f, 0.9f); }
    /* stamina while flying */
    if (d->mode == DM_AIR && d->stage >= ST_DRAGON) { ui_bar(42, 156, 196, 9, d->stamina, 0.35f, 0.65f, 1.0f); }
    /* prompts */
    float L = dragon_length(d->stage);
    int near = world_nearest_food(&g_world, d->x + sinf(d->yaw) * 0.45f * L, d->z + cosf(d->yaw) * 0.45f * L, 0.5f * L + 1.2f);
    if (near >= 0 && d->mode != DM_AIR && d->act_t <= 0) text_c(food_is_meat(g_world.food[near].kind) ? "X : EAT (MEAT, GROWS FAST)" : "X : EAT", 555, 30, 1, 1, 0.7f, 0.9f);
    }
    if (g_msg_t > 0 && !g_swap_open && g_tale_i < 0) { float ma = clampf(g_msg_t, 0, 1), mw = fminf(text_width(g_msg, 26), 1060); int nl = text_wrap_n(g_msg, 0, 26, 1060, 0, 0, 0, 0, 0); float my = 644 - (nl - 1) * 39;
        ui_panel(640 - mw / 2 - 70, my - 36, mw + 140, 100 + (nl - 1) * 39, 0.9f * ma, 0); text_wrap_c(g_msg, my, 26, 1060, 1, 0.96f, 0.86f, ma); }
    if (g_title_t > 0) { float a = clampf(g_title_t, 0, 1); ui_pic_c(&g_ua, UI_GLOW, 640, 300, 1200, 260, 1, 0.6f, 0.25f, 0.15f * a); ui_logo(640, 220, 980, a); ui_divider(640, 330, 560, a); text_cg(g_wk == WK_SKY ? "SKY KINGDOM" : "EVERLEAF WILDS", 352, 34, a); }
    static const char *cams[3] = {"RIDER VIEW", "OVERHEAD VIEW", "DRAGON EYES"}; batch_text(&g_b, cams[g_cam_mode], 1240 - text_width(cams[g_cam_mode], 20), 36, 20, 1, 1, 1, 0.6f);
    { static const char *PW[K_N] = {"LEAFNADO", "FIRE", "FROST"}; char cl[120]; snprintf(cl, sizeof cl, "D-PAD UP : VIEW   A : JUMP/FLY   ZR : RUN   B : TAIL   Y : %s   + : SWAP", d->stage >= ST_DRAGON ? PW[g_kind] : "ROAR");
      batch_text(&g_b, cl, 640 - text_width(cl, 16) / 2, 692, 16, 1, 1, 1, 0.5f); }
    if (g_travel_t >= 0) { float a = 1 - fabsf(g_travel_t - 0.9f) / 0.9f; batch_rect(&g_b, 0, 0, 1280, 720, 1, 1, 1, clampf(a * 1.2f, 0, 1)); }
    if (g_tale_i >= 0) { char line[200]; snprintf(line, sizeof line, TALE[g_tale_w][g_tale_i], kind_name(CAPTIVE[g_tale_w])); float fade = clampf(g_tale_t * 2, 0, 1);
        ui_shade(fade); ui_embers(40, fade); ui_panel(90, 150, 1100, 440, 0.95f * fade, 0); ui_divider(640, 200, 520, fade); text_wrap_c(line, 250, 30, 960, 1, 0.95f, 0.82f, fade);
        char pg[16]; snprintf(pg, sizeof pg, "%d / 3", g_tale_i + 1); text_cg(pg, 556, 18, 0.7f * fade); text_c("A : NEXT", 620, 24, 1, 1, 1, 0.6f + 0.4f * sinf(g_time * 3)); }
    else if (g_swap_open) {   /* Dragon Swap menu */
        ui_shade(1); ui_embers(40, 0.8f);
        text_cg("DRAGON SWAP", 110, 52, 1); ui_divider(640, 172, 560, 1); text_c("THE GAME IS PAUSED", 182, 20, 1, 1, 1, 0.7f);
        { int n = team_size(), slot = 0; float cw = n >= 3 ? 330 : 352, gap = 24, x0 = 640 - (n * cw + (n - 1) * gap) / 2;
          static const char *QUIRK[K_N] = {"STRONG TAIL, FLIES WHEN GROWN", "LIGHT AS AIR: LONG GLIDES", "ICE CLAWS: RUNS FASTER"};
          static const float CARD[K_N][3] = {{0.08f, 0.22f, 0.1f}, {0.25f, 0.06f, 0.06f}, {0.12f, 0.18f, 0.28f}};
          for (int k = 0; k < K_N; k++) { if (!g_has[k]) continue; const Dragon *dd = k == g_kind ? &g_dragon : &g_team[k]; float x = x0 + slot++ * (cw + gap), cx = x + cw / 2 - 6; int sel = k == g_swap_sel;
            ui_panel(x - 6, 212, cw, 330, 1, sel); ui_rect(&g_ub, x + 20, 240, cw - 64, 274, CARD[k][0], CARD[k][1], CARD[k][2], 0.35f);
            const char *nm = kind_name(k); float ns = 30; while (ns > 18 && text_width(nm, ns) > cw - 90) ns -= 2; batch_text(&g_b, nm, cx - text_width(nm, ns) / 2, 262, ns, 1, 1, 1, 1);
            batch_text(&g_b, KIND_TITLE[k], cx - text_width(KIND_TITLE[k], 18) / 2, 310, 18, 0.8f, 0.9f, 1, 0.9f);
            const char *st = dragon_stage_name(dd->stage); batch_text(&g_b, st, cx - text_width(st, 24) / 2, 346, 24, 1, 0.9f, 0.6f, 1);
            for (int i = 0; i < (int)dd->max_hearts; i++) { float full = clampf(dd->hearts - i, 0, 1); float hx = cx - dd->max_hearts * 15 + i * 30; ui_heart(hx + 12, 402, 28, full); }
            char gb[64]; snprintf(gb, sizeof gb, "FOOD %.0f / %.0f", dd->growth, dragon_need(dd->stage)); if (dd->stage < ST_BEYONDER) batch_text(&g_b, gb, cx - text_width(gb, 16) / 2, 434, 16, 0.7f, 1, 0.7f, 0.9f);
            float qs = 15; while (qs > 10 && text_width(QUIRK[k], qs) > cw - 80) qs -= 1; batch_text(&g_b, QUIRK[k], cx - text_width(QUIRK[k], qs) / 2, 462, qs, 1, 1, 1, 0.75f);
            if (k == g_kind) batch_text(&g_b, "PLAYING NOW", cx - text_width("PLAYING NOW", 16) / 2, 492, 16, 0.6f, 1, 0.6f, 1);
            else if (k == g_fk) batch_text(&g_b, "FOLLOWING YOU", cx - text_width("FOLLOWING YOU", 16) / 2, 492, 16, 0.8f, 0.8f, 1, 1); } }
        text_c("LEFT / RIGHT: CHOOSE    A: SWAP    X: SHARE FOOD    B OR +: BACK", 560, 22, 1, 1, 1, 0.9f);
        if (g_msg_t > 0) text_c(g_msg, 610, 22, 1, 0.95f, 0.5f, 1); }
    ui_flush(); (void)drc; }


#ifdef DB_TOUR
/* the tour build plays itself (for the emulator screenshots in CI): walks to food and eats until it grows, then flies, changing views as it goes */
static float g_tour_rescue_t = 1e9f;
static void tour_input_base(Pad *p, float t);
static void tour_input(Pad *p, float t) {   /* the base script, plus the dragon's power on diving drones and now and then to show it off */
    tour_input_base(p, t); const Dragon *d = &g_dragon; if (g_state != G_PLAY || g_tale_i >= 0 || g_swap_open) return;
    int diving = 0; for (int i = 0; i < g_world.ndrone; i++) if (g_world.drone[i].state == 1) { float dx = g_world.drone[i].x - d->x, dz = g_world.drone[i].z - d->z; if (dx * dx + dz * dz < 14 * 14) diving = 1; }
    static float last; if ((diving || t - last > 12) && d->stage >= ST_DRAGON && g_power_cd <= 0 && !p->b) { p->y = 1; last = t; } }
static void tour_input_base(Pad *p, float t) {
    static float last_eat = -9, last_whip = -9; static int prev_up = 0; if (g_rescued && g_tale_i < 0 && g_tour_rescue_t > 1e8f) g_tour_rescue_t = t;
    memset(p, 0, sizeof *p); p->source = 1;
    const Dragon *d = &g_dragon;
    if (t < 4) return; if (g_state != G_PLAY || g_tale_i >= 0) { p->a = fmodf(t, 0.9f) < 0.12f && (g_state != G_NAME || t > 6); return; }
    int wantcam = 0; int press = wantcam != g_cam_mode && !prev_up; p->up = press; prev_up = press;
    float ax = g_world.arena_x - d->x, az = g_world.arena_z - d->z, adist = sqrtf(ax * ax + az * az);
    if (g_wk == WK_SKY && g_wrescued[WK_SKY] && g_kind == 2) {   /* after the swap: walk home over the bridge and ride the wind column down */
        static int reached; float tx, tz;
        if (!reached) { float ax = g_world.arena_x, az = g_world.arena_z, ex = -ax, ez = -az, L2 = ex * ex + ez * ez;   /* along the bridge from the boss island to the first one */
            float u = clampf(((d->x - ax) * ex + (d->z - az) * ez) / L2 + 0.1f, 0, 1); tx = ax + ex * u; tz = az + ez * u; if (d->x * d->x + d->z * d->z < 25) reached = 1; }
        else { tx = -12; tz = 7; }
        float dx = tx - d->x, dz = tz - d->z;
        float ang = atan2f(dx, dz) - g_cam_yaw; p->sx = sinf(ang); p->sy = cosf(ang); p->zr = 1; }
    else if (d->stage == ST_DRAGONET) {   /* eat until grown */
        int near = world_nearest_food(&g_world, d->x, d->z, 400);
        if (near >= 0) { const Food *f = &g_world.food[near]; float dx = f->x - d->x, dz = f->z - d->z, dist = sqrtf(dx * dx + dz * dz);
            float ang = atan2f(dx, dz) - g_cam_yaw; if (dist > 1.0f) { p->sx = sinf(ang); p->sy = cosf(ang); p->zr = dist > 6; } else if (d->act_t <= 0 && t - last_eat > 0.5f) { p->x = 1; last_eat = t; } } }
    else if (!g_rescued && g_boss.state != B_DOWN) {
        if (d->mode == DM_AIR) {   /* fly to the clearing, then glide down into it */
            if (adist > 14) { float ang = atan2f(ax, az) - g_cam_yaw; p->sx = 0.35f * sinf(ang); p->sy = 0.35f * cosf(ang); p->a = d->y - world_height(&g_world, d->x, d->z) < 14 || fmodf(t, 2.0f) < 1.0f; } }
        else if (adist > 40) { if (d->evolve_t <= 0 && d->act_t <= 0) { p->a = fmodf(t, 1.0f) < 0.5f; p->sy = 1; } }   /* take off toward it */
        else {   /* the fight: lure a charge in front of a tree, sidestep, whip the core when it is stuck */
            const Boss *b = &g_boss; float tx, tz;
            if (b->state == B_CHARGE) { float sx = -b->cz, sz = b->cx; float mx = g_world.arena_x - d->x, mz = g_world.arena_z - d->z; if (sx * mx + sz * mz < 0) { sx = -sx; sz = -sz; } tx = d->x + sx * 5; tz = d->z + sz * 5; p->zr = 1; }   /* sidestep, toward the middle */
            else if (boss_vulnerable(b)) { Vec3 c = boss_core(b); tx = c.x; tz = c.z; float dx = c.x - d->x, dz = c.z - d->z; if (dx * dx + dz * dz < 2.6f * 2.6f && d->act_t <= 0 && t - last_whip > 0.9f) { p->b = 1; last_whip = t; } }
            else { const Prop *tr = &g_world.prop[g_world.arena_tree[2]]; tx = tr->x + (g_world.arena_x - tr->x) * 0.18f; tz = tr->z + (g_world.arena_z - tr->z) * 0.18f; }
            float dx = tx - d->x, dz = tz - d->z, dist = sqrtf(dx * dx + dz * dz); if (dist > 0.6f && !p->b) { float ang = atan2f(dx, dz) - g_cam_yaw; p->sx = sinf(ang); p->sy = cosf(ang); p->zr |= dist > 4; } } }
    else if (!g_rescued || g_friend.anim_t < 0) { float dx = g_world.cage_x - d->x, dz = g_world.cage_z - d->z, dist = sqrtf(dx * dx + dz * dz);   /* watch the cage open */
        if (dist > 5) { float ang = atan2f(dx, dz) - g_cam_yaw; p->sx = sinf(ang); p->sy = cosf(ang); } }
    else if (t - g_tour_rescue_t < 4) { if (fmodf(t, 2.0f) < 0.1f) p->y = 1; }   /* a happy roar */
    else if (t - g_tour_rescue_t < 8) { static int step; float u = t - g_tour_rescue_t;   /* show the Dragon Swap menu and become the friend */
        if (step == 0 && u > 4.5f) { p->plus = 1; step = 1; } else if (step == 1 && u > 5.8f) { p->left = 1; step = 2; }   /* left wraps round to the newest friend */ else if (step == 2 && u > 7.0f) { p->a = 1; step = 3; } }
    else {   /* after the rescue: take off and fly around with the friend */
        if (d->mode != DM_AIR) { if (d->evolve_t <= 0 && d->act_t <= 0) { p->a = fmodf(t, 1.0f) < 0.5f; p->sy = 1; } }
        else { p->a = fmodf(t, 6.0f) < 3.5f; p->sy = 0.4f; p->sx = 0.5f * sinf(t * 0.4f); } }
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
    if (g_wk == WK_SKY) {   /* the Sky Kingdom: white cloud, green islands */
        batch_rect(&g_b, MX(-W_SIZE / 2), MY(W_SIZE / 2), W_SIZE * S, W_SIZE * S, 0.86f, 0.9f, 0.96f, 1);
        float cs = W_SIZE / (W_GRID - 1);
        for (int j = 0; j < W_GRID; j++) for (int i = 0; i < W_GRID; i++) { float h = g_world.h[j][i]; if (h < W_CLOUDS + 4) continue; float x = -W_SIZE / 2 + cs * i, z = -W_SIZE / 2 + cs * j; float t = clampf(h / 22.0f, 0, 1);
            batch_rect(&g_b, MX(x - cs / 2), MY(z + cs / 2), cs * S + 1, cs * S + 1, 0.3f - 0.1f * t, 0.55f - 0.12f * t, 0.25f - 0.05f * t, 1); } }
    else batch_rect(&g_b, MX(-W_SIZE / 2), MY(W_SIZE / 2), W_SIZE * S, W_SIZE * S, 0.3f, 0.55f, 0.25f, 1);
    { float ccx, ccz; if (column_at(&ccx, &ccz)) { batch_rect(&g_b, MX(ccx) - 7, MY(ccz) - 7, 14, 14, 0.7f, 0.9f, 1, 1); batch_rect(&g_b, MX(ccx) - 3, MY(ccz) - 20, 6, 40, 0.9f, 1, 1, 0.8f); } }   /* the wind column */
    /* hills: darker where the ground is high */
    for (int j = 0; j < W_GRID - 1 && g_wk == WK_FOREST; j += 2) for (int i = 0; i < W_GRID - 1; i += 2) { float h = g_world.h[j][i]; if (h < 6) continue; float t = clampf((h - 6) / 20.0f, 0, 1); float x = -W_SIZE / 2 + W_SIZE * i / (W_GRID - 1), z = -W_SIZE / 2 + W_SIZE * j / (W_GRID - 1); batch_rect(&g_b, MX(x), MY(z) - 2 * S, 2 * S * W_SIZE / (W_GRID - 1) / 2.5f + 1, 2 * S * W_SIZE / (W_GRID - 1) / 2.5f + 1, 0.3f + 0.3f * t, 0.45f + 0.1f * t, 0.2f + 0.1f * t, 0.9f); }
    map_disc(MX(g_world.pond_x), MY(g_world.pond_z), g_world.pond_r * 1.25f * S, 24, 0.25f, 0.55f, 0.85f, 1);
    for (int i = 0; i < g_world.nprop; i++) { const Prop *p = &g_world.prop[i]; float r = p->kind == P_TREE || p->kind == P_PALM ? 4.5f : 2.5f; float cr = p->kind == P_ROCK ? 0.5f : 0.12f, cg = p->kind == P_ROCK ? 0.5f : 0.35f, cb = p->kind == P_ROCK ? 0.48f : 0.1f; map_disc(MX(p->x), MY(p->z), r, 6, cr, cg, cb, 1); }
    for (int i = 0; i < g_world.nfood; i++) { const Food *f = &g_world.food[i]; if (!f->alive) continue; float cr = 1, cg = 1, cb = 1;
        if (f->kind == F_BERRY) { cr = 0.95f; cg = 0.15f; cb = 0.25f; } else if (f->kind == F_MUSHROOM) { cr = 1; cg = 0.95f; cb = 0.8f; } else if (f->kind == F_GRUB) { cr = 1; cg = 0.85f; cb = 0.3f; } else { cr = 0.5f; cg = 0.8f; cb = 1; }
        map_disc(MX(f->x), MY(f->z), 3, 6, cr, cg, cb, 1); }
    for (int i = 0; i < g_world.nhound; i++) if (g_world.hound[i].state != 3) map_disc(MX(g_world.hound[i].x), MY(g_world.hound[i].z), 4, 4, 0.9f, 0.3f, 0.1f, 1);
    if (g_boss.state != B_DOWN) map_disc(MX(g_boss.x), MY(g_boss.z), 7, 8, 1.0f, 0.45f, 0.1f, 1);
    if (!g_rescued) map_disc(MX(g_world.cage_x), MY(g_world.cage_z), 5, 8, 0.4f, 0.7f, 1.0f, 1);
    if (g_friend.on) map_disc(MX(g_friend.x), MY(g_friend.z), 4, 6, 0.9f, 0.2f, 0.2f, 1);
    for (int i = 0; i < g_world.nthief; i++) if (g_world.thief[i].state < 3) map_disc(MX(g_world.thief[i].x), MY(g_world.thief[i].z), 3, 6, 0.6f, 0.2f, 0.7f, 1);
    for (int i = 0; i < g_world.ndrone; i++) if (g_world.drone[i].state != 3) map_disc(MX(g_world.drone[i].x), MY(g_world.drone[i].z), 4, 6, 0.9f, 0.2f, 0.1f, 1);
    if (!g_world.crystal_found && d->stage >= ST_DRAGON) map_disc(MX(g_world.crystal_x), MY(g_world.crystal_z), 5 + 2 * sinf(g_time * 3), 8, 0.6f, 0.95f, 1, 0.8f);
    /* the dragon: a bright arrow pointing where it faces, with a pulse ring */
    float ax = MX(d->x), ay = MY(d->z), sy = sinf(d->yaw), cy = cosf(d->yaw); float pulse = 10 + 4 * sinf(g_time * 4);
    map_disc(ax, ay, pulse, 16, 0.7f, 1, 0.5f, 0.35f);
    map_tri2d(ax + sy * 14, ay - cy * 14, ax - cy * 8 - sy * 8, ay - sy * 8 + cy * 8, ax + cy * 8 - sy * 8, ay + sy * 8 + cy * 8, 0.85f, 1, 0.6f, 1);
    /* words */
    batch_text_g(&g_b, g_wk == WK_SKY ? "SKY KINGDOM" : "EVERLEAF WILDS", 90, 70, 34, 1, 1, 1, 1, 1);
    char line[64]; snprintf(line, sizeof line, "%s", dragon_stage_name(d->stage)); batch_text(&g_b, line, 90, 116, 26, 1, 0.95f, 0.85f, 0.9f);
    float need = dragon_need(d->stage); if (need < 1e8f) { snprintf(line, sizeof line, "GROW %d/%d", (int)d->growth, (int)need); batch_text(&g_b, line, 90, 148, 22, 0.8f, 1, 0.7f, 0.9f); }
    batch_text(&g_b, "RED: BERRIES  WHITE: MUSHROOMS", 90, 612, 18, 1, 1, 1, 0.8f); batch_text(&g_b, "YELLOW: GRUBS  BLUE: FISH (MEAT)  DARK RED: DRONES", 90, 640, 18, 1, 1, 1, 0.8f);
    batch_text(&g_b, "D-PAD UP: CHANGE VIEW", 1190 - text_width("D-PAD UP: CHANGE VIEW", 18), 640, 18, 1, 1, 1, 0.8f);
    gfx_camera_2d(); gfx_state(ST_HUD); gfx_bind(tx_font); gfx_mesh_update(m_dynhud, g_b.v, g_b.n); Mat4 id = m4_identity(); gfx_draw_ex(m_dynhud, &id, 1, 1, 1, 1, 0, 1, 0, 0); gfx_bind(NULL);
    batch_clear(&g_b); ui_begin(); ui_frame(0, 0, 1280, 720, 1); ui_glint(30, 30, 1220, 660, g_time * 0.05f, 90, 0.7f); ui_flush();   /* the gold frame on top */
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
/* centred words wrapped to maxw; returns how many lines (draw = 0 only counts them) */
static int text_wrap_n(const char *txt, float y, float size, float maxw, float r, float g, float b, float a, int draw) {
    char line[160]; int ln = 0, n = 0; const char *w = txt;
    while (*w) { const char *e = w; while (*e && *e != ' ') e++; int wl = (int)(e - w); char cand[160]; int cl = ln; memcpy(cand, line, (size_t)ln);
        if (cl) cand[cl++] = ' '; memcpy(cand + cl, w, (size_t)wl); cl += wl; cand[cl] = 0;
        if (ln && text_width(cand, size) > maxw) { line[ln] = 0; if (draw) text_c(line, y, size, r, g, b, a); n++; y += size * 1.5f; ln = 0; memcpy(line, w, wl); ln = wl; }
        else { memcpy(line, cand, cl + 1); ln = cl; }
        w = *e ? e + 1 : e; }
    if (ln) { line[ln] = 0; if (draw) text_c(line, y, size, r, g, b, a); n++; } return n; }
static void text_wrap_c(const char *txt, float y, float size, float maxw, float r, float g, float b, float a) { text_wrap_n(txt, y, size, maxw, r, g, b, a, 1); }

/* ---------- title and name screens: dragons flying everywhere over the meadow ---------- */
static void draw_title_dragons(void) {
    /* three dragons (one of each size) circling at different heights, plus the dragonet looping low */
    for (int k = 0; k < 4; k++) { int st = k % 3; Anim3D *m = &g_model[st]; if (!m->ok) continue; float sc = dragon_scale(st) * (k == 3 ? 1 : 1.0f);
        float R = 15 + k * 9, w = (0.5f - k * 0.07f) * (k & 1 ? -1 : 1), a = g_time * w + k * 1.7f, h = 7 + k * 5 + 2.5f * sinf(g_time * 0.8f + k);
        Vec3 pos = v3(cosf(a) * R, h, sinf(a) * R); float yaw = atan2f(-sinf(a) * w, cosf(a) * w) ;   /* face along the circle */
        Mat4 base = m4_mul(m4_mul(m4_translate(pos.x, pos.y, pos.z), m4_rotate_y(yaw)), m4_mul(m4_rotate_z(w > 0 ? -0.35f : 0.35f), m4_scale(sc, sc, sc)));
        int clip = anim3d_clip(m, "fly"); gfx_state(ST_SOLID); anim3d_set_bend(0.35f * sinf(g_time * 0.4f + k), 0.45f, 0.6f, g_time * 5.5f + k); anim3d_draw(m, clip < 0 ? 0 : clip, g_time + k * 0.3f, &base, 1.12f, 1.12f, 1.1f, 1); }
    gfx_bind(NULL); }
static void title_camera(void) {
    float a = g_time * 0.07f; g_eye = v3(cosf(a) * 9, world_height(&g_world, cosf(a) * 9, sinf(a) * 9) + 3.0f, sinf(a) * 9); g_at = v3(-cosf(a) * 10, 15, -sinf(a) * 10); }   /* inside the 14 m clearing, clear of the trees */
static void draw_title_hud(void) {
    batch_clear(&g_b); ui_begin();
    float pulse = 0.75f + 0.25f * sinf(g_time * 3), in = fmaxf(clampf(g_state_t * 1.2f, 0, 1), g_state == G_TITLE ? clampf(g_time - 1.0f, 0, 1) : 0);
    if (g_state == G_TITLE) {
        ui_shade(0.55f); ui_embers(60, 1);
        ui_pic_rot(&g_ub, UI_RING, 640, 215, 560, 560, g_time * 0.08f, 1, 1, 1, 0.28f * in); ui_pic_rot(&g_ua, UI_RING, 640, 215, 470, 470, -g_time * 0.13f, 1, 0.7f, 0.3f, 0.07f * in);
        ui_pic_c(&g_ua, UI_GLOW, 640, 205, 1300, 300, 1, 0.6f, 0.25f, 0.05f * in);
        { float sc = 1.15f - 0.15f * in; ui_logo(640, 205 - 60 * sc, 1120 * sc, in); }
        ui_divider(640, 300, 640, in);
        text_cg("EVERLEAF WILDS", 322, 34, in);
        ui_panel(470, 560, 340, 92, in, 1); text_cg("PRESS A", 588, 38, (0.6f + 0.4f * pulse) * in);
        text_c("A GAME DESIGNED BY A 10 YEAR OLD DRAGON EXPERT", 684, 18, 1, 0.92f, 0.78f, 0.75f * in);
    } else if (g_state == G_NAME) {
        ui_shade(1); ui_embers(30, 0.8f); ui_panel(120, 90, 1040, 540, 0.97f, 0);
        text_cg("WHO IS YOUR FOREST DRAGON?", 140, 44, 1); ui_divider(640, 210, 560, 1);
        for (int i = 0; i < 2; i++) { float x = 240 + i * 440, y = 250; int sel = g_name == i;
            ui_panel(x, y, 360, 240, 1, sel);
            { float ns = 40; while (ns > 20 && text_width(NAMES[i], ns) > 300) ns -= 2; batch_text_g(&g_b, NAMES[i], x + 180 - text_width(NAMES[i], ns) / 2, y + 48, ns, 1, 1, 1, sel ? 1 : 0.75f, 1); }
            batch_text(&g_b, i == 0 ? "GIRL" : "BOY", x + 180 - text_width(i == 0 ? "GIRL" : "BOY", 26) / 2, y + 116, 26, 0.85f, 1, 0.8f, 0.9f);
            batch_text(&g_b, "LEAFNADO + LEAF TAIL", x + 180 - text_width("LEAFNADO + LEAF TAIL", 18) / 2, y + 172, 18, 1, 0.95f, 0.85f, 0.75f); }
        text_c("LEFT / RIGHT : CHOOSE     A : START", 560, 22, 1, 0.95f, 0.85f, 0.85f); }
    if (g_state == G_SLOT) {
        ui_shade(1); ui_embers(30, 0.8f); ui_panel(200, 50, 880, 640, 0.97f, 0); text_cg("CHOOSE A SAVE", 100, 44, 1); ui_divider(640, 172, 520, 1);
        for (int k = 0; k < 3; k++) { const SlotInfo *si = &g_slots[k]; float y = 196 + k * 132; int sel = k == g_slot_sel;
            ui_panel(290, y, 700, 124, 1, sel);
            char a[64], b[96]; snprintf(a, sizeof a, "SAVE %d", k + 1); batch_text_g(&g_b, a, 340, y + 36, 24, 1, 1, 1, sel ? 1 : 0.75f, 1);
            if (!si->used) snprintf(b, sizeof b, "EMPTY - START A NEW ADVENTURE");
            else { static const char *WN[WK_N] = {"EVERLEAF WILDS", "SKY KINGDOM"}; snprintf(b, sizeof b, "%s THE %s  -  %s  -  %s  -  %d FRIEND%s", NAMES[si->name], dragon_stage_name(si->stage), DIFF_NAMES[si->diff], WN[si->wk], si->rescued, si->rescued == 1 ? "" : "S"); }
            { float bs = 17; while (bs > 11 && text_width(b, bs) > 600) bs -= 1; batch_text(&g_b, b, 340, y + 74, bs, 1, 0.95f, 0.85f, 0.9f); }
            if (g_slot_erase == k) batch_text(&g_b, "PRESS X AGAIN TO ERASE", 940 - text_width("PRESS X AGAIN TO ERASE", 18), y + 38, 18, 1, 0.4f, 0.3f, 1); }
        text_c("UP / DOWN : CHOOSE    A : PLAY    X : ERASE    B : BACK", 606, 18, 1, 0.95f, 0.85f, 0.85f); }
    if (g_state == G_DIFF) {
        ui_shade(1); ui_embers(30, 0.8f); ui_panel(320, 90, 640, 530, 0.97f, 0); text_cg("HOW HARD?", 136, 44, 1); ui_divider(640, 200, 440, 1);
        static const char *desc[3] = {"FEWER OUCHES, IRONHORN NEEDS 2 HITS", "THE WAY IT WAS DESIGNED", "BIGGER OUCHES, IRONHORN NEEDS 4 HITS"};
        for (int k = 0; k < 3; k++) { float y = 222 + k * 114; int sel = k == g_diff;
            ui_panel(390, y, 500, 106, 1, sel);
            batch_text_g(&g_b, DIFF_NAMES[k], 640 - text_width(DIFF_NAMES[k], 28) / 2, y + 28, 28, 1, 1, 1, sel ? 1 : 0.7f, 1);
            batch_text(&g_b, desc[k], 640 - text_width(desc[k], 15) / 2, y + 66, 15, 1, 0.95f, 0.85f, 0.85f); }
        text_c("UP / DOWN : CHOOSE    A : OK    B : BACK", 574, 20, 1, 0.95f, 0.85f, 0.85f); }
    if (g_state == G_STORY) { float fade = clampf(g_state_t * 2, 0, 1);
        ui_shade(1); ui_embers(50, 1); ui_panel(90, 150, 1100, 440, 0.95f * fade, 0); ui_divider(640, 200, 520, fade);
        text_wrap_c(STORY[g_story_i], 250, 30, 960, 1, 0.95f, 0.82f, fade); char pg[16]; snprintf(pg, sizeof pg, "%d / %d", g_story_i + 1, STORY_N); text_cg(pg, 556, 18, 0.7f * fade); text_cg("A : NEXT", 620, 26, pulse); }
    ui_flush(); }

/* ---------- the frame ---------- */

/* go to a world: make it, rebuild its meshes (waiting for the GPU first), set up its boss and cage, put the team at (x, z) */
static void enter_world(int wk, float x, float z, float yaw) {
    g_wk = wk; world_make_kind(&g_world, 2026 + wk * 101, wk); g_world.crystal_found = g_wcrystal[wk];
    if (g_gfx_status == GFX_OK) {
        gfx_sync();
        Mesh **ms[] = {&m_ground, &m_ground_dirt, &m_ground_rock, &m_water, &m_bark, &m_leaves, &m_fern, &m_bushes, &m_rock, &m_tufts, &m_flowers, &m_clouds, &m_mountains, &m_apron, &m_cloudsea, &m_sky, &m_sun};
        for (int i = 0; i < (int)(sizeof ms / sizeof ms[0]); i++) if (*ms[i]) { gfx_mesh_free(*ms[i]); *ms[i] = NULL; }
        build_ground(); build_props();
        scenelayer_build(&g_lay_tree, &g_pm[PM_OAK], &g_world, P_TREE, 1.0f, 0.2f); scenelayer_build(&g_lay_palm, &g_pm[PM_PALM], &g_world, P_PALM, 1.0f, 0.2f);
        scenelayer_build(&g_lay_fern, &g_pm[PM_FERN], &g_world, P_FERN, 1.0f, 0.05f); scenelayer_build(&g_lay_bush, &g_pm[PM_BUSH], &g_world, P_BUSH, 1.6f, 0.1f); }
    ensure_kind(CAPTIVE[wk], ST_DRAGONET); for (int k = 1; k < K_N; k++) if (g_has[k]) ensure_kind(k, ST_BEYONDER);
    boss_init(&g_boss, &g_world); g_boss.hp = BOSS_HITS[g_diff]; if (wk == WK_SKY) { g_boss.core_fwd = 0.55f; g_boss.core_h = 3.1f; }
    if (g_wrescued[wk]) { g_boss.state = B_DOWN; g_boss.hp = 0; g_boss.cage_open = 1; g_boss.t = 10; }
    float sx, sz; world_safe_spot(&g_world, clampf(x, -110, 110), clampf(z, -110, 110), &sx, &sz);
    g_dragon.x = sx; g_dragon.z = sz; g_dragon.y = world_height(&g_world, sx, sz); g_dragon.yaw = yaw; g_dragon.mode = DM_GROUND; g_dragon.vy = 0; g_dragon.speed = 0; g_dragon.act_t = 0; g_dragon.anim = DA_IDLE;
    g_cam_yaw = yaw; g_safe_x = sx; g_safe_z = sz; g_friend.on = team_size() > 1 && g_fk >= 0 && g_has[g_fk];
    if (g_friend.on) { float fx = sx - sinf(yaw) * 3 + cosf(yaw) * 2, fz = sz - cosf(yaw) * 3 - sinf(yaw) * 2; world_safe_spot(&g_world, fx, fz, &fx, &fz);
        g_friend.x = fx; g_friend.z = fz; g_friend.y = world_height(&g_world, fx, fz); g_friend.yaw = yaw; g_friend.anim = DA_IDLE; g_friend.anim_t = 0; g_friend.speed = 0;
        g_other.x = fx; g_other.z = fz; g_other.y = g_friend.y; g_other.mode = DM_GROUND; }
    g_eye = v3(sx - sinf(yaw) * 8, g_dragon.y + 4, sz - cosf(yaw) * 8); g_at = v3(sx, g_dragon.y + 1, sz); }
/* what makes each dragon different, and holding back growth a dragon has no model for yet */
static void kind_quirks(Dragon *d, int kind, const DragonInput *in, float dt, int *ev) {
    if (kind == 1 && d->mode == DM_AIR && in->a && d->vy < 0) d->vy += 6.0f * dt;   /* sky: rides the air, much longer glides */
    if (kind == 2 && d->mode == DM_GROUND && in->zr && d->speed > 1) {           /* snow: ice claws, runs faster */
        float nx = d->x + sinf(d->yaw) * d->speed * 0.3f * dt, nz = d->z + cosf(d->yaw) * d->speed * 0.3f * dt;
        if (!(g_world.kind == WK_SKY && world_is_void(&g_world, nx, nz))) { d->x = nx; d->z = nz; d->y = world_height(&g_world, d->x, d->z); } }
    if (d->stage > ST_DRAGONET && !kind_has_stage(kind, d->stage)) {
        d->stage--; d->growth = dragon_need(d->stage) - 1; d->evolve_t = 0; d->max_hearts = 3 + 2 * d->stage; d->hearts = fminf(d->hearts, d->max_hearts); *ev &= ~2;
        static int told[K_N]; if (!told[kind]) { told[kind] = 1; snprintf(g_msg, sizeof g_msg, "%s IS READY TO GROW! THE GROWN-UP LOOK COMES IN A LATER UPDATE", kind_name(kind)); g_msg_t = 6; } } }

/* the Sky Kingdom has edges: off one, or down into the clouds, and the dragon is put back on the last solid ground */
static int sky_fall(Dragon *d, float *sx, float *sz) {
    if (g_world.kind != WK_SKY) return 0;
    int over_void = world_is_void(&g_world, d->x, d->z);
    if (!over_void && d->mode != DM_AIR) { if (world_height(&g_world, d->x, d->z) > W_CLOUDS + 18) { *sx = d->x; *sz = d->z; } return 0; }   /* remember only real island tops, not the cliff faces */
    if ((d->mode != DM_AIR && over_void) || d->y < W_CLOUDS) {
        d->x = *sx; d->z = *sz; d->y = world_height(&g_world, d->x, d->z); d->mode = DM_GROUND; d->vy = 0; d->speed = 0; return 1; }
    return 0; }
/* ---------- Ironhorn, the cage and the rescued friend ---------- */
static void hurt_dragon(float amount, const char *msg) {
    if (g_dragon.evolve_t > 0) return;
    g_dragon.hearts -= amount * DMG[g_diff]; g_dragon.act_t = 0.77f; g_dragon.anim = DA_HIT; g_dragon.anim_t = 0; sfx_play(SFX_HIT, 0.7f, 0); platform_rumble(0.9f, 0.4f);
    snprintf(g_msg, sizeof g_msg, "%s", msg); g_msg_t = 3;
    if (g_dragon.hearts <= 0) { g_dragon.hearts = 1; g_dragon.x = 0; g_dragon.z = 0; g_dragon.y = world_height(&g_world, 0, 0); g_dragon.mode = DM_GROUND; snprintf(g_msg, sizeof g_msg, "OUCH! BACK TO THE START. EAT TO GET HEARTS BACK"); g_msg_t = 5; } }
static void boss_frame(float dt) {
    const Dragon *d = &g_dragon; float L = dragon_length(d->stage);
    int whip = d->anim == DA_TAILWHIP && d->act_t > 0.35f && d->act_t < 0.6f; int roar = (d->anim == DA_ROAR && d->act_t > 1.0f) || g_power_t > 0.3f;
    int bev = boss_update(&g_boss, &g_world, dt, d->x, d->y, d->z, 0.6f * L + 0.8f, whip, roar);
    if (bev & 1) { snprintf(g_msg, sizeof g_msg, "%s", BOSS_GUARD[g_wk]); g_msg_t = 6; sfx_play(SFX_ROAR, 0.9f, 0); platform_rumble(0.6f, 0.5f); }
    if (bev & 64) { sfx_play(SFX_BUMP, 0.8f, 0); platform_rumble(0.3f, 0.3f); }
    if (bev & 2) { hurt_dragon(1.0f, g_wk == WK_SKY ? "STORMTALON HIT YOU! RUN (ZR) TO THE SIDE WHEN IT SWOOPS" : "IRONHORN RAMMED YOU! RUN (ZR) TO THE SIDE WHEN IT CHARGES");
        g_dragon.x += g_boss.cx * 3; g_dragon.z += g_boss.cz * 3; }
    if (bev & 16) { snprintf(g_msg, sizeof g_msg, "%s", BOSS_STUCK[g_wk]); g_msg_t = 4; sfx_play(SFX_CRUMBLE, 0.8f, 0); platform_rumble(1, 0.5f); }
    if (bev & 32) { snprintf(g_msg, sizeof g_msg, "STUNNED! QUICK, WHIP ITS CORE (B)"); g_msg_t = 3; }
    if ((bev & 4) && !(bev & 8)) { snprintf(g_msg, sizeof g_msg, g_boss.hp == 1 ? "HIT! ONE MORE" : "HIT! %d MORE", g_boss.hp); g_msg_t = 3; sfx_play(SFX_BLOCK, 0.9f, 0); platform_rumble(0.8f, 0.3f); }
    if (bev & 8) { snprintf(g_msg, sizeof g_msg, "%s IS DOWN! THE CAGE IS OPENING", BOSS_NAME[g_wk]); g_msg_t = 5; sfx_play(SFX_WIN, 0.9f, 0); platform_rumble(1, 0.8f); g_dragon.growth += 20; }
    if (g_boss.state == B_DOWN && g_boss.cage_open >= 1 && !g_rescued) {
        int c = CAPTIVE[g_wk]; ensure_kind(c, ST_BEYONDER); g_rescued = 1; g_has[c] = 1; g_fk = c; g_friend.on = 1; g_friend.x = g_world.cage_x; g_friend.y = g_world.arena_y + 1.2f; g_friend.z = g_world.cage_z; g_friend.yaw = 0;
        dragon_init(&g_other, &g_world, g_friend.x, g_friend.z);
        snprintf(g_msg, sizeof g_msg, "%s IS FREE AND JOINS YOUR TEAM! PRESS + TO SWAP DRAGONS", kind_name(c)); g_msg_t = 8; g_tale_i = 0; g_tale_t = 0; g_tale_w = g_wk; sfx_play(SFX_CHIRP, 1.0f, 0); save_progress(); }
    /* the friend: keeps to the player's side, a little behind; flies when the player flies (unless player 2 is steering it) */
    if (g_friend.on && g_p2 < 0) {
        float fx = sinf(d->yaw), fz = cosf(d->yaw); float back = 2.5f + L, side = 1.8f + 0.5f * L;
        float tx = d->x - fx * back + fz * side, tz = d->z - fz * back - fx * side; float gy = world_height(&g_world, tx, tz);
        int air = d->mode == DM_AIR || g_friend.y > world_height(&g_world, g_friend.x, g_friend.z) + 0.6f || (g_world.kind == WK_SKY && (world_is_void(&g_world, g_friend.x, g_friend.z) || world_is_void(&g_world, tx, tz)));
        float ty = d->mode == DM_AIR ? d->y + 0.8f : (g_world.kind == WK_SKY && gy < W_CLOUDS + 4 ? d->y + 1.5f : gy);
        float dx = tx - g_friend.x, dz = tz - g_friend.z, dy = ty - g_friend.y, dist = sqrtf(dx * dx + dz * dz);
        float want = dist > 0.3f ? fminf(dist * 1.6f, d->mode == DM_AIR ? 22.0f : 11.0f) : 0; g_friend.speed += (want - g_friend.speed) * (1 - expf(-dt * 4));
        if (dist > 40) { g_friend.x = tx; g_friend.z = tz; g_friend.y = ty; }   /* fell far behind: catch up */
        else if (dist > 0.01f) { g_friend.x += dx / dist * g_friend.speed * dt; g_friend.z += dz / dist * g_friend.speed * dt; }
        g_friend.y += dy * (1 - expf(-dt * (air ? 3 : 10))); if (!air) g_friend.y = fmaxf(g_friend.y, world_height(&g_world, g_friend.x, g_friend.z));
        float fyaw = g_friend.speed > 0.5f ? atan2f(dx, dz) : d->yaw; float dyaw = fyaw - g_friend.yaw; while (dyaw > 3.14159f) dyaw -= 6.28318f; while (dyaw < -3.14159f) dyaw += 6.28318f; g_friend.yaw += dyaw * (1 - expf(-dt * 5));
        int anim = air ? (dy > 0.2f || g_friend.speed > 6 ? DA_FLY : DA_GLIDE) : (g_friend.speed > 6 ? DA_RUN : (g_friend.speed > 0.6f ? DA_WALK : DA_IDLE));
        static float chirp_t; chirp_t -= dt;
        if (fmodf(g_time, 9.0f) < dt && !air) { sfx_play(SFX_CHIRP, 0.25f, 0.3f); if (anim == DA_IDLE) chirp_t = 0.8f; }   /* the friend chirps, and its mouth moves with it */
        if (chirp_t > 0 && anim == DA_IDLE) anim = DA_CHIRP;
        if (anim != g_friend.anim) { g_friend.anim = anim; g_friend.anim_t = 0; } g_friend.anim_t += dt; g_friend.mode = air;
        g_other.x = g_friend.x; g_other.z = g_friend.z; g_other.y = g_friend.y; g_other.yaw = g_friend.yaw; g_other.mode = air ? DM_AIR : DM_GROUND; } }

/* co-op: join with A on a Wii controller while player 1 is on the GamePad; + on that controller (or putting it down for a while) leaves */
static void coop_frame(const Pad *pads, int npads, float dt) {
    if (team_size() < 2 || !g_friend.on) return;
    if (g_p2 < 0) {
        if (g_time - g_gp_last > 20) return;   /* player 1 is on a Wii controller, so that controller is not a second player */
        for (int i = 1; i < npads && i < KK_PADS; i++) if (pads[i].source && pads[i].a && !g_prev[i].a) {
            g_p2 = i; g_p2_gone = 0; g_swap_open = 0; g_other.speed = 0; g_other.act_t = 0; g_other.anim = DA_IDLE;
            float gy = world_height(&g_world, g_other.x, g_other.z); if (g_other.y <= gy + 0.3f) { g_other.y = gy; g_other.mode = DM_GROUND; } else { g_other.mode = DM_AIR; g_other.vy = 0; }
            snprintf(g_msg, sizeof g_msg, "PLAYER 2 JOINED AS %s! PRESS + ON THAT CONTROLLER TO LEAVE", kind_name(g_fk)); g_msg_t = 6;
            sfx_play(SFX_CHIRP, 0.9f, 0); return; }
        return; }
    const Pad *p = &pads[g_p2], *q = &g_prev[g_p2];
    if (!p->source) { g_p2_gone += dt; if (g_p2_gone > 3) { g_p2 = -1; snprintf(g_msg, sizeof g_msg, "PLAYER 2 LEFT. YOUR FRIEND FOLLOWS YOU AGAIN"); g_msg_t = 4; } return; }
    g_p2_gone = 0;
    if (p->plus && !q->plus) { g_p2 = -1; snprintf(g_msg, sizeof g_msg, "PLAYER 2 LEFT. YOUR FRIEND FOLLOWS YOU AGAIN"); g_msg_t = 4; sfx_play(SFX_MENU_BACK, 0.6f, 0); return; }
    DragonInput in = {0}; in.sx = p->sx; in.sy = p->sy; in.a = p->a; in.a_press = p->a && !q->a; in.b_press = p->b && !q->b; in.x_press = p->x && !q->x; in.y_press = p->y && !q->y; in.zr = p->zr; in.zl = p->zl;
    int ev = dragon_update(&g_other, &g_world, &in, g_cam_yaw, dt);
    kind_quirks(&g_other, g_fk, &in, dt, &ev);   /* player 2 plays the friend */
    { static float p2x, p2z; static int init; if (!init) { init = 1; p2x = g_dragon.x; p2z = g_dragon.z; } if (sky_fall(&g_other, &p2x, &p2z)) { snprintf(g_msg, sizeof g_msg, "PLAYER 2: LOST IN THE CLOUDS!"); g_msg_t = 3; } }
    float dx = g_other.x - g_dragon.x, dz = g_other.z - g_dragon.z;
    if (dx * dx + dz * dz > 55 * 55) {   /* too far from player 1: pop back beside them */
        g_other.x = g_dragon.x - sinf(g_dragon.yaw) * 4 + cosf(g_dragon.yaw) * 3; g_other.z = g_dragon.z - cosf(g_dragon.yaw) * 4 - sinf(g_dragon.yaw) * 3;
        g_other.y = world_height(&g_world, g_other.x, g_other.z); g_other.mode = DM_GROUND; g_other.vy = 0;
        snprintf(g_msg, sizeof g_msg, "PLAYER 2: STAY CLOSE TO YOUR FRIEND!"); g_msg_t = 3; }
    if (ev & 1) sfx_play(SFX_MUNCH, 0.5f, 0.4f);
    if (ev & 2) { snprintf(g_msg, sizeof g_msg, "PLAYER 2 GREW INTO A %s!", dragon_stage_name(g_other.stage)); g_msg_t = 5; sfx_play(SFX_GROW, 0.8f, 0); save_progress(); }
    if (ev & 4) sfx_play(SFX_FLAP, 0.5f, 0.4f);
    if (ev & 16) sfx_play(g_other.stage == ST_DRAGONET ? SFX_CHIRP : SFX_ROAR, 0.8f, 0.4f);
    /* the friend drawing follows player 2's dragon */
    g_friend.x = g_other.x; g_friend.y = g_other.y; g_friend.z = g_other.z; g_friend.yaw = g_other.yaw; g_friend.anim = g_other.anim; g_friend.anim_t = g_other.anim_t; g_friend.mode = g_other.mode == DM_AIR; g_friend.speed = g_other.speed; }

void game_frame(const Pad *pads, int npads, float dt) {
    g_time += dt; gfx_set_time(g_time);
    if (g_gfx_status != GFX_OK) { gfx_frame_begin(); gfx_view_begin(0, g_gfx_status == GFX_NO_COMPILER ? 1 : 1, g_gfx_status == GFX_NO_COMPILER ? 0.5f : 0, 0); gfx_view_end(0); gfx_view_begin(1, 1, 0.5f, 0); gfx_view_end(1); gfx_frame_end(); return; }
    /* player 1: the GamePad, or the first Wii controller if the GamePad stick is idle */
#ifdef DB_TOUR
    static Pad tourpad; static int tour_once; if (!tour_once && g_state == G_PLAY) { tour_once = 1;   /* the emulator is slow: start grown, in the Sky Kingdom, with Scarlet */
        g_dragon.stage = ST_DRAGON; g_dragon.max_hearts = g_dragon.hearts = 5; g_dragon.growth = 0;
        g_wrescued[WK_FOREST] = 1; g_has[1] = 1; g_fk = 1; g_diff = 0; dragon_init(&g_team[1], &g_world, 0, 0); enter_world(WK_SKY, 4, -4, 2.3f); g_kaine_done = 1; g_kaine_t = 0; }
    g_tour_real_dt = g_tour_real_dt * 0.9f + dt * 0.1f;
    if (dt < 0.08f) dt = 0.08f;   /* the cloud emulator is slow: let game time run ahead so the clip covers more */
    tour_input(&tourpad, g_time); pads = &tourpad; npads = 1;
#endif
    const Pad *p = &pads[0]; const Pad *q = &g_prev[0];
    if (pads[0].source && (fabsf(pads[0].sx) > 0.2f || fabsf(pads[0].sy) > 0.2f || pads[0].a || pads[0].b || pads[0].x || pads[0].y || pads[0].zr)) g_gp_last = g_time;
    for (int i = 1; i < npads && i < KK_PADS; i++) if (g_p2 < 0 && pads[i].source && (fabsf(pads[i].sx) > 0.2f || fabsf(pads[i].sy) > 0.2f || pads[i].a || pads[i].x) && fabsf(p->sx) < 0.2f && fabsf(p->sy) < 0.2f && !p->a && !p->x) { p = &pads[i]; q = &g_prev[i]; }
    DragonInput in = {0}; in.sx = p->sx; in.sy = p->sy; in.a = p->a; in.a_press = p->a && !q->a; in.b_press = p->b && !q->b; in.x_press = p->x && !q->x; in.y_press = p->y && !q->y; in.zr = p->zr; in.zl = p->zl;
    g_state_t += dt;
    if (g_state != G_PLAY) {
        int upk = (p->up && !q->up) || (p->sy > 0.6f && q->sy <= 0.6f), downk = (p->down && !q->down) || (p->sy < -0.6f && q->sy >= -0.6f);
        if (g_state == G_TITLE && in.a_press && g_state_t > 0.5f) { scan_slots(); g_state = G_SLOT; g_state_t = 0; g_slot_erase = -1; sfx_play(SFX_MENU_OK, 0.5f, 0); }
        else if (g_state == G_SLOT) {
            if (upk) { g_slot_sel = (g_slot_sel + 2) % 3; g_slot_erase = -1; sfx_play(SFX_MENU_MOVE, 0.5f, 0); }
            if (downk) { g_slot_sel = (g_slot_sel + 1) % 3; g_slot_erase = -1; sfx_play(SFX_MENU_MOVE, 0.5f, 0); }
            if (in.b_press) { g_state = G_TITLE; g_state_t = 0; sfx_play(SFX_MENU_BACK, 0.5f, 0); }
            else if (in.x_press && g_slots[g_slot_sel].used) {   /* erase: X twice */
                if (g_slot_erase == g_slot_sel) { char path[300]; slot_path(g_slot_sel, path, sizeof path); remove(path); scan_slots(); g_slot_erase = -1; sfx_play(SFX_CRUMBLE, 0.6f, 0); }
                else { g_slot_erase = g_slot_sel; sfx_play(SFX_MENU_MOVE, 0.5f, 0); } }
            else if (in.a_press && g_state_t > 0.3f) { use_slot(g_slot_sel); sfx_play(SFX_MENU_OK, 0.5f, 0); g_state_t = 0;
                if (g_slots[g_slot_sel].used && load_progress()) { g_state = G_PLAY; g_title_t = 4.0f; if (!g_rescued) g_boss.hp = BOSS_HITS[g_diff];
                    snprintf(g_msg, sizeof g_msg, "WELCOME BACK, %s!", active_name()); g_msg_t = 4; }
                else { g_state = G_DIFF; g_diff = 1; } } }
        else if (g_state == G_DIFF) {
            if (upk) { g_diff = (g_diff + 2) % 3; sfx_play(SFX_MENU_MOVE, 0.5f, 0); }
            if (downk) { g_diff = (g_diff + 1) % 3; sfx_play(SFX_MENU_MOVE, 0.5f, 0); }
            if (in.b_press) { g_state = G_SLOT; g_state_t = 0; sfx_play(SFX_MENU_BACK, 0.5f, 0); }
            else if (in.a_press && g_state_t > 0.3f) { new_game(); g_state = G_NAME; g_state_t = 0; sfx_play(SFX_MENU_OK, 0.5f, 0); } }
        else if (g_state == G_NAME) { if ((p->left && !q->left) || (p->right && !q->right) || (fabsf(p->sx) > 0.6f && fabsf(q->sx) <= 0.6f)) { g_name ^= 1; sfx_play(SFX_MENU_MOVE, 0.5f, 0); } if (in.a_press && g_state_t > 0.3f) { g_state = G_STORY; g_state_t = 0; g_story_i = 0; } }
        else if (g_state == G_STORY) { if ((in.a_press && g_state_t > 0.4f) || g_state_t > 12) { g_state_t = 0; if (++g_story_i >= STORY_N) { g_state = G_PLAY; g_title_t = 4.0f; snprintf(g_msg, sizeof g_msg, "%s IS HUNGRY. WALK TO A BUSH AND PRESS X", NAMES[g_name]); g_msg_t = 9; } } }
        memcpy(g_prev, pads, sizeof(Pad) * (npads < KK_PADS ? npads : KK_PADS));
        music_want("title"); music_update(2);
        g_dragon.anim_t += dt; title_camera();
        gfx_frame_begin(); gfx_shadow_none();
        gfx_view_begin(0, 0.55f, 0.75f, 0.95f); { float aspect = gfx_aspect(0); Mat4 proj = m4_perspective(1.1f, aspect, 0.2f, 1500.0f), view = m4_look_at(g_eye, g_at, v3(0, 1, 0)); Mat4 vp = m4_mul(proj, view); Mat4 id = m4_identity();
            gfx_grade(0.9f, 0.3f, 0.22f, 1.06f); gfx_light(v3(1.0f, 0.92f, 0.78f), v3(0.5f, 0.62f, 0.82f), v3(0.3f, 0.33f, 0.2f), g_eye); gfx_camera(&vp, sun_dir(), 0.34f, g_sky_photo ? 0.66f : 0.74f, g_sky_photo ? 0.70f : 0.82f, g_sky_photo ? 0.80f : 0.92f, 140, 620);
            Mat4 skym = m4_translate(g_eye.x, g_eye.y - 60, g_eye.z); gfx_state(0); gfx_bind(g_sky_photo ? tx_sky : NULL); gfx_draw_ex(m_sky, &skym, 1, 1, 1, 1, 0, 1, 0, 0); if (m_sun) { gfx_state(ST_BLEND); gfx_bind(tx_font); gfx_draw_ex(m_sun, &skym, 1, 1, 1, 1, 0, 1, 0, 0); gfx_state(0); gfx_bind(NULL); } gfx_state(ST_DEPTH | ST_DEPTH_WRITE); if (m_mountains) { gfx_bind(tx_rock); gfx_draw(m_mountains, &id, 1, 1, 1, 1); } if (m_apron) { gfx_bind(tx_grass); gfx_draw(m_apron, &id, 1, 1, 1, 1); } if (m_cloudsea) { gfx_bind(tx_font); gfx_draw_ex(m_cloudsea, &id, 1, 1, 1, 1, 0, 1, 0, 0); } gfx_clear_depth(0);
            gfx_state(ST_SOLID); gfx_bind(tx_grass); gfx_bump(3); gfx_draw(m_ground, &id, 1, 1, 1, 1); gfx_state(ST_TRANSPARENT); if (m_ground_dirt) { gfx_bind(tx_dirt); gfx_draw(m_ground_dirt, &id, 1, 1, 1, 1); } if (m_ground_rock) { gfx_bind(tx_rock); gfx_draw(m_ground_rock, &id, 1, 1, 1, 1); } gfx_bump(0); gfx_state(ST_SOLID); gfx_bind(tx_bark); if (m_bark) gfx_draw(m_bark, &id, 1, 1, 1, 1); scenelayer_draw(&g_lay_tree, g_eye, LD_HI, LD_SKIP); scenelayer_draw(&g_lay_palm, g_eye, LD_HI, LD_SKIP); scenelayer_draw(&g_lay_fern, g_eye, LD_HI * 0.6f, 150); scenelayer_draw(&g_lay_bush, g_eye, LD_HI * 0.7f, 170);
            gfx_state(ST_CUTOUT); gfx_bind(tx_leaves); if (m_leaves) gfx_draw(m_leaves, &id, 1, 1, 1, 1); gfx_bind(tx_fern); if (m_fern) gfx_draw(m_fern, &id, 1, 1, 1, 1); gfx_bind(tx_tuft); if (m_tufts) gfx_draw(m_tufts, &id, 1, 1, 1, 1);
            draw_title_dragons();
            { Mat4 cm = m4_translate(fmodf(g_time * 1.5f, 900.0f) - 450.0f, 0, 0); gfx_state(ST_DEPTH | ST_BLEND); gfx_bind(tx_cloud); if (m_clouds) { gfx_draw_ex(m_clouds, &cm, 1, 1, 1, 0.9f, 0, 1, 0, 0); Mat4 cm2 = m4_translate(cm.m[12] + 900.0f, 0, 0); gfx_draw_ex(m_clouds, &cm2, 1, 1, 1, 0.9f, 0, 1, 0, 0); } }
            gfx_post(0, 0.9f, 0.35f, 0.35f); draw_title_hud(); }
        gfx_view_end(0);
        gfx_view_begin(1, 0.08f, 0.14f, 0.08f); draw_title_hud(); gfx_view_end(1);
        gfx_frame_end(); return; }
    if (g_tale_i >= 0) {}   /* + waits until the storyteller is done */
    else if (p->plus && !q->plus && g_p2 >= 0) { snprintf(g_msg, sizeof g_msg, "TWO PLAYERS: EACH OF YOU HAS A DRAGON. PLAYER 2 PRESSES + TO LEAVE"); g_msg_t = 3; }
    else if (p->plus && !q->plus && team_size() > 1) { g_swap_open ^= 1; g_swap_sel = g_kind; sfx_play(g_swap_open ? SFX_MENU_OK : SFX_MENU_BACK, 0.6f, 0); }
    else if (p->plus && !q->plus) { snprintf(g_msg, sizeof g_msg, "RESCUE A DRAGON FRIEND FIRST TO SWAP"); g_msg_t = 3; }
    if (g_tale_i >= 0) { g_tale_t += dt; if ((in.a_press && g_tale_t > 0.6f) || g_tale_t > 14) { g_tale_t = 0; sfx_play(SFX_MENU_MOVE, 0.5f, 0); if (++g_tale_i >= 3) { g_tale_i = -1; g_msg_t = 6; } } goto paused; }
    if (g_swap_open) {   /* Dragon Swap: the game is paused while the menu is up */
        { int dir = ((p->right && !q->right) || (p->sx > 0.6f && q->sx <= 0.6f)) ? 1 : (((p->left && !q->left) || (p->sx < -0.6f && q->sx >= -0.6f)) ? -1 : 0);
          if (dir) { do g_swap_sel = (g_swap_sel + dir + K_N) % K_N; while (!g_has[g_swap_sel]); sfx_play(SFX_MENU_MOVE, 0.5f, 0); } }
        if (in.a_press) { if (g_swap_sel != g_kind) do_swap(g_swap_sel); g_swap_open = 0; }
        if (in.b_press) { g_swap_open = 0; sfx_play(SFX_MENU_BACK, 0.5f, 0); }
        if (in.x_press) {   /* share food: hand 10 growth from the dragon you are playing to the other one */
            int tk = g_swap_sel != g_kind ? g_swap_sel : g_fk; Dragon *o = &g_team[tk]; float room = dragon_need(o->stage) - o->growth;
            if (g_dragon.growth < 10) { snprintf(g_msg, sizeof g_msg, "EAT MORE FIRST: YOU NEED 10 FOOD TO SHARE"); g_msg_t = 3; sfx_play(SFX_MENU_BACK, 0.5f, 0); }
            else if (o->stage >= ST_BEYONDER || room <= 0) { snprintf(g_msg, sizeof g_msg, "YOUR FRIEND IS FULL!"); g_msg_t = 3; }
            else { float give = fminf(10, room); g_dragon.growth -= give; o->growth += give; sfx_play(SFX_MUNCH, 0.7f, 0); sfx_play(SFX_CHIRP, 0.5f, 0);
                if (o->growth >= dragon_need(o->stage) && o->stage < ST_BEYONDER && kind_has_stage(tk, o->stage + 1)) { o->stage++; o->growth = 0; o->max_hearts += 2; o->hearts = o->max_hearts;
                    snprintf(g_msg, sizeof g_msg, "SHARED! YOUR FRIEND GREW INTO A %s!", dragon_stage_name(o->stage)); }
                else snprintf(g_msg, sizeof g_msg, "SHARED FOOD WITH YOUR FRIEND");
                g_msg_t = 3; save_progress(); } }
        goto paused; }
    if (p->up && !q->up) { g_cam_mode = (g_cam_mode + 1) % 3; sfx_play(SFX_MENU_MOVE, 0.4f, 0); }
    g_look_x = p->rx;
    float yaw_before = g_dragon.yaw;
    int ev = dragon_update(&g_dragon, &g_world, &in, g_cam_yaw, dt);
    kind_quirks(&g_dragon, g_kind, &in, dt, &ev);
    if (g_power_cd > 0) g_power_cd -= dt; if (g_power_t > 0) g_power_t -= dt;
    if (in.y_press && g_dragon.stage >= ST_DRAGON && g_power_cd <= 0) { g_power_t = 1.4f; g_power_cd = 3.0f; g_power_kind = g_kind; sfx_play(g_kind == 1 ? SFX_FIRE : SFX_ROAR, 0.9f, 0); platform_rumble(0.7f, 0.6f);
        if (g_dragon.mode == DM_AIR) { g_dragon.act_t = 0; } }
    if (g_power_t > 0) { float L = dragon_length(g_dragon.stage); float fx = sinf(g_dragon.yaw), fz = cosf(g_dragon.yaw);
        int hit = g_power_kind == 0 ? world_blast(&g_world, g_dragon.x, g_dragon.y + 0.5f * L, g_dragon.z, fx, fz, 7 + 2 * L, -1)
                                    : world_blast(&g_world, g_dragon.x + fx * L, g_dragon.y + 0.5f * L, g_dragon.z + fz * L, fx, fz, 10 + 3 * L, 0.75f);
        if (hit) { g_dragon.growth += 5 * hit; sfx_play(SFX_BLOCK, 0.6f, 0); snprintf(g_msg, sizeof g_msg, g_power_kind == 2 ? "FROZEN SOLID!" : (g_power_kind == 1 ? "TOASTED!" : "BLOWN AWAY!")); g_msg_t = 2; } }
    if (sky_fall(&g_dragon, &g_safe_x, &g_safe_z)) { g_dragon.hearts -= 0.5f * DMG[g_diff]; if (g_dragon.hearts < 1) g_dragon.hearts = 1; snprintf(g_msg, sizeof g_msg, "LOST IN THE CLOUDS! BACK TO SOLID GROUND"); g_msg_t = 3; sfx_play(SFX_SPLASH, 0.6f, 0); platform_rumble(0.6f, 0.3f); }
    { float ccx, ccz; if (g_travel_cd > 0) g_travel_cd -= dt;
      if (g_travel_t < 0 && g_travel_cd <= 0 && column_at(&ccx, &ccz)) { float dx = g_dragon.x - ccx, dz = g_dragon.z - ccz; float gy = world_height(&g_world, ccx, ccz);
          if (dx * dx + dz * dz < 3.5f * 3.5f && g_dragon.y < gy + 30) { g_travel_t = 0; g_travel_to = g_wk == WK_FOREST ? WK_SKY : WK_FOREST; sfx_play(SFX_FLAP, 1.0f, 0); sfx_play(SFX_GROW, 0.7f, 0); platform_rumble(0.5f, 1.0f);
              snprintf(g_msg, sizeof g_msg, g_travel_to == WK_SKY ? "THE WIND CARRIES YOU UP TO THE SKY KINGDOM..." : "THE WIND CARRIES YOU DOWN TO EVERLEAF WILDS..."); g_msg_t = 4; } } }
    if (g_kaine_t >= 0) { float before = g_kaine_t; g_kaine_t += dt;
        if (before < 2.5f && g_kaine_t >= 2.5f) { snprintf(g_msg, sizeof g_msg, "KAINE BLACKFEATHER! HE SWOOPS PAST ON HIS MECH DRAGON: YOU WILL NEVER FREE THEM ALL!"); g_msg_t = 6; sfx_play(SFX_ROAR, 1.0f, 0); platform_rumble(0.8f, 0.8f); }
        if (g_kaine_t > 13) g_kaine_t = -1; }
    if (g_travel_t >= 0) { float before = g_travel_t; g_travel_t += dt; g_dragon.y += 12 * dt; g_dragon.mode = DM_AIR; g_dragon.vy = 0;
        if (before < 0.9f && g_travel_t >= 0.9f) { float ax = g_travel_to == WK_SKY ? 4 : g_world.arena_x, az = g_travel_to == WK_SKY ? -4 : g_world.arena_z;
            if (g_travel_to == WK_FOREST) { ax = -62 + 8; az = 60 - 8; }   /* beside the forest column */
            enter_world(g_travel_to, ax, az, g_travel_to == WK_SKY ? 2.3f : 0.8f); g_travel_cd = 4; save_progress();
            snprintf(g_msg, sizeof g_msg, g_wk == WK_SKY ? (g_wrescued[WK_SKY] ? "THE SKY KINGDOM" : "THE SKY KINGDOM! A DRAGONET IS CAGED ON THE BIG ISLAND") : "EVERLEAF WILDS"); g_msg_t = 5;
            if (g_wk == WK_SKY && !g_wrescued[WK_SKY] && !g_kaine_done) { g_kaine_done = 1; g_kaine_t = 0; } }
        if (g_travel_t > 1.8f) g_travel_t = -1; }
    { float dy = g_dragon.yaw - yaw_before; while (dy > 3.14159f) dy -= 6.28318f; while (dy < -3.14159f) dy += 6.28318f;
      float rate = dt > 0 ? dy / dt : 0; g_turn += (rate - g_turn) * (1 - expf(-dt * 4));
      float want_bank = g_dragon.mode == DM_AIR ? clampf(-g_turn * 0.6f, -0.95f, 0.95f) : 0; g_bank += (want_bank - g_bank) * (1 - expf(-dt * 3));
      float spd = g_dragon.mode == DM_AIR ? 5.5f : 2.0f + g_dragon.speed * 1.2f; g_bodyph = fmodf(g_bodyph + dt * spd, 6.28318f * 100); }
#ifdef DB_TOUR
    g_tour_a = in.a; g_tour_presses += in.a_press; g_tour_takeoffs += (ev & 4) != 0;
#endif
    world_update(&g_world, dt, g_dragon.x, g_dragon.z);
    { int whip = g_dragon.anim == DA_TAILWHIP && g_dragon.act_t > 0.35f && g_dragon.act_t < 0.6f; int roar = g_dragon.anim == DA_ROAR && g_dragon.act_t > 1.0f;
      int dev = world_drones(&g_world, dt, g_dragon.x, g_dragon.y + 0.4f * dragon_length(g_dragon.stage), g_dragon.z, 0.8f * dragon_length(g_dragon.stage) + 1.0f, whip, roar);
      if ((dev & 1) && g_dragon.evolve_t <= 0) { g_dragon.hearts -= 0.5f * DMG[g_diff]; g_dragon.act_t = 0.77f; g_dragon.anim = DA_HIT; g_dragon.anim_t = 0; sfx_play(SFX_HIT, 0.6f, 0); platform_rumble(0.7f, 0.3f); snprintf(g_msg, sizeof g_msg, g_dragon.stage >= ST_DRAGON ? "A FEATHER DRONE! B TO SWAT IT, Y FOR YOUR POWER" : "A FEATHER DRONE! PRESS B TO SWAT IT, Y TO SCARE IT"); g_msg_t = 4;
          if (g_dragon.hearts <= 0) { g_dragon.hearts = 1; g_dragon.x = 0; g_dragon.z = 0; g_dragon.y = world_height(&g_world, 0, 0); g_dragon.mode = DM_GROUND; snprintf(g_msg, sizeof g_msg, "OUCH! BACK TO THE START. EAT TO GET HEARTS BACK"); g_msg_t = 5; } }
      if (dev & 2) { sfx_play(SFX_BLOCK, 0.6f, 0); platform_rumble(0.5f, 0.2f); g_dragon.growth += 5; snprintf(g_msg, sizeof g_msg, "DRONE DOWN!"); g_msg_t = 2; }
      int hev = world_hounds(&g_world, dt, g_dragon.x, g_dragon.y, g_dragon.z, 0.7f * dragon_length(g_dragon.stage) + 0.9f, whip, roar, g_dragon.mode == DM_AIR);
      if ((hev & 1) && g_dragon.evolve_t <= 0) { g_dragon.hearts -= 0.5f * DMG[g_diff]; g_dragon.act_t = 0.77f; g_dragon.anim = DA_HIT; g_dragon.anim_t = 0; sfx_play(SFX_HIT, 0.6f, 0); platform_rumble(0.7f, 0.3f); snprintf(g_msg, sizeof g_msg, "A SHOCK HOUND! RUN (ZR), HOP (A), OR SWAT IT (B)"); g_msg_t = 4;
          if (g_dragon.hearts <= 0) { g_dragon.hearts = 1; g_dragon.x = 0; g_dragon.z = 0; g_dragon.y = world_height(&g_world, 0, 0); g_dragon.mode = DM_GROUND; snprintf(g_msg, sizeof g_msg, "OUCH! BACK TO THE START. EAT TO GET HEARTS BACK"); g_msg_t = 5; } }
      if (hev & 2) { sfx_play(SFX_BLOCK, 0.6f, 0); platform_rumble(0.5f, 0.2f); g_dragon.growth += 5; snprintf(g_msg, sizeof g_msg, "HOUND DOWN!"); g_msg_t = 2; }
      { float L = dragon_length(g_dragon.stage), stolen, rec; int pw = g_power_t > 0.3f;
        int tev = world_thieves(&g_world, dt, g_dragon.x, g_dragon.y, g_dragon.z, pw ? 8 + 2 * L : 0.7f * L + 0.9f, whip || pw, g_dragon.growth, &stolen, &rec);
        if (tev & 1) { g_dragon.growth = fmaxf(0, g_dragon.growth - stolen); sfx_play(SFX_HIT, 0.5f, 0); platform_rumble(0.4f, 0.2f); snprintf(g_msg, sizeof g_msg, "A SCALE THIEF STOLE YOUR FOOD! CATCH IT WITH YOUR TAIL (B)"); g_msg_t = 4; }
        if (tev & 2) { g_dragon.growth += rec + 3; sfx_play(SFX_PICKUP, 0.7f, 0); platform_rumble(0.4f, 0.2f); snprintf(g_msg, sizeof g_msg, rec > 0 ? "CAUGHT! YOU GOT YOUR FOOD BACK" : "SCALE THIEF CAUGHT!"); g_msg_t = 3; } } }
    boss_frame(dt);
    coop_frame(pads, npads, dt);
    if (ev & 1) { platform_rumble(0.3f, 0.1f); sfx_play(SFX_MUNCH, 0.6f, 0); sfx_play(SFX_PICKUP, 0.3f, 0); }
    if (ev & 2) { snprintf(g_msg, sizeof g_msg, g_dragon.stage == ST_DRAGON ? "YOU GREW INTO A DRAGON! HOLD A TO FLY" : "BEYONDER! THE SKY IS YOURS"); g_msg_t = 6; platform_rumble(1, 0.8f); }
    if (ev & 16) { platform_rumble(0.6f, 0.4f); sfx_play(g_dragon.stage == ST_DRAGONET ? SFX_CHIRP : SFX_ROAR, 0.8f, 0); }
    if (ev & 2) sfx_play(SFX_GROW, 0.8f, 0);
    if (ev & 4) sfx_play(SFX_FLAP, 0.6f, 0);
    if (ev & 8) sfx_play(SFX_LAND, 0.5f, 0);
    if (in.b_press && g_dragon.anim == DA_TAILWHIP && g_dragon.act_t > 0.9f) sfx_play(SFX_TAIL, 0.6f, 0);
    if (g_dragon.mode == DM_AIR && g_dragon.anim == DA_FLY) {   /* the whoosh lands on each downstroke of the 0.8 s wingbeat */
        float ph = fmodf(g_dragon.anim_t, 0.8f), was = fmodf(fmaxf(g_dragon.anim_t - dt, 0), 0.8f); if (was < 0.26f && ph >= 0.26f) sfx_play(SFX_FLAP, 0.45f, 0); }
    if (g_state == G_PLAY && g_dragon.stage == ST_DRAGONET && fmodf(g_time, 7.0f) < dt && g_dragon.anim == DA_IDLE) sfx_play(SFX_CHIRP, 0.3f, 0);
    if (ev & 2) save_progress();
    if (!g_world.crystal_found) { float dx = g_dragon.x - g_world.crystal_x, dz = g_dragon.z - g_world.crystal_z, dy = g_dragon.y - g_world.crystal_y; if (dx * dx + dz * dz + dy * dy < (1.2f + 0.5f * dragon_length(g_dragon.stage)) * (1.2f + 0.5f * dragon_length(g_dragon.stage))) { g_world.crystal_found = 1; snprintf(g_msg, sizeof g_msg, "SECRET EGG CRYSTAL FOUND! 1 OF 10"); g_msg_t = 6; sfx_play(SFX_UNLOCK, 0.8f, 0); platform_rumble(0.8f, 0.5f); save_progress(); } }
    g_save_t += dt; if (g_save_t > 20) { g_save_t = 0; save_progress(); }
    if (!g_rescued && g_dragon.stage >= ST_DRAGON && !g_boss.woke) {   /* point the way to the cage */
        static float hint_t = 4; hint_t -= dt;
        if (hint_t <= 0 && g_msg_t <= 0) { hint_t = 25; snprintf(g_msg, sizeof g_msg, "A DRAGONET IS CRYING FOR HELP! FLY TO THE BLUE DOT ON THE GAMEPAD MAP"); g_msg_t = 6; } }
    if (!g_rescued) { float dx = g_world.cage_x - g_dragon.x, dz = g_world.cage_z - g_dragon.z; static float chirp_t;
        chirp_t -= dt; if (chirp_t <= 0 && dx * dx + dz * dz < 55 * 55) { chirp_t = 3.5f; float pan = clampf((dx * cosf(g_cam_yaw) - dz * sinf(g_cam_yaw)) / 30.0f, -1, 1); sfx_play(SFX_CHIRP, 0.5f, pan); } }
    if (g_msg_t > 0) g_msg_t -= dt;
    if (g_title_t > 0) g_title_t -= dt;
    camera_update(dt);
paused:
    memcpy(g_prev, pads, sizeof(Pad) * (npads < KK_PADS ? npads : KK_PADS));
    { int chased = 0; for (int i = 0; i < g_world.ndrone; i++) if (g_world.drone[i].state == 1) chased = 1; for (int i = 0; i < g_world.nhound; i++) if (g_world.hound[i].state == 1) chased = 1;
      static float danger_t; if (chased) danger_t = 4; else danger_t -= dt;
      int bossfight = g_boss.woke && g_boss.state != B_DOWN;
      music_want(bossfight && music_available("boss") ? "boss" : danger_t > 0 && music_available("danger") ? "danger" : (g_wk == WK_SKY && music_available("sky") ? "sky" : "everleaf")); }
    music_update(2);
    gfx_frame_begin();
    shadow_pass();
    gfx_view_begin(0, 0.55f, 0.75f, 0.95f); draw_scene(0); gfx_post(0, 0.9f, 0.35f, 0.35f); draw_hud(0); gfx_view_end(0);   /* the TV: the game */
    gfx_view_begin(1, 0.08f, 0.14f, 0.08f); draw_map(); gfx_view_end(1);                   /* the GamePad: the map */
    gfx_frame_end(); }
