/* worldfx.inc - what makes each world its own place: the light and fog (a dark crystal cave, the blue under the sea,
   the red glow of the volcano), the air (snow, embers, bubbles, fireflies...), and one special thing to play with in
   every world (sky rings, dust devils, lava geysers, sea animals, lightning). Included by game.c. */

typedef struct { float sun, amb, tint[3], fog[3], f0, f1, sky[3]; } Env;
static Env g_env = {1, 1, {1, 1, 1}, {0.74f, 0.82f, 0.92f}, 140, 620, {1, 1, 1}};
static int g_uw;   /* the camera is under the sea */
static void env_compute(Vec3 eye) {
    Env e = {1, 1, {1, 1, 1}, {0, 0, 0}, 140, 620, {TH->sky[0], TH->sky[1], TH->sky[2]}};
    for (int i = 0; i < 3; i++) e.fog[i] = (g_sky_photo ? (0.66f + 0.04f * i + (i == 2 ? 0.06f : 0)) / (0.74f + 0.08f * i + (i == 2 ? 0.02f : 0)) : 1.0f) * TH->fog[i];
    g_uw = g_world.sea_on && eye.y < g_world.sea - 0.05f;
    switch (g_wk) {
    case WK_CRYSTAL: e.sun = 0.35f; e.amb = 0.75f; e.tint[0] = 0.8f; e.tint[1] = 0.65f; e.tint[2] = 1.15f; e.fog[0] = 0.05f; e.fog[1] = 0.03f; e.fog[2] = 0.1f; e.f0 = 22; e.f1 = 200; e.sky[0] = 0.05f; e.sky[1] = 0.03f; e.sky[2] = 0.1f; break;
    case WK_LAVA: e.sun = 0.75f; e.amb = 0.85f; e.tint[0] = 1.15f; e.tint[1] = 0.8f; e.tint[2] = 0.7f; e.f0 = 60; e.f1 = 420; break;
    case WK_KAINE: e.sun = 0.5f; e.amb = 0.7f; e.tint[0] = 0.8f; e.tint[1] = 0.82f; e.tint[2] = 1.0f; e.f0 = 40; e.f1 = 330; break;
    case WK_SNOW: e.f0 = 90; e.f1 = 520; break;
    case WK_SAND: e.f0 = 110; e.f1 = 520; break;
    default: break; }
    if (g_uw) { e.sun = 0.7f; e.amb = 0.95f; e.tint[0] = 0.45f; e.tint[1] = 0.9f; e.tint[2] = 1.1f; e.fog[0] = 0.02f; e.fog[1] = 0.25f; e.fog[2] = 0.4f; e.f0 = 3; e.f1 = 100; e.sky[0] = 0.08f; e.sky[1] = 0.4f; e.sky[2] = 0.55f; }
    g_env = e; }
static void env_light3(Vec3 sun, Vec3 sky, Vec3 ground, Vec3 eye) {
    float s = g_env.sun, a = g_env.amb; if (sun.x > 1.02f) { s = fmaxf(s, 0.8f); a = fmaxf(a, 0.9f); }   /* the player's own dragon keeps its character light, even in the dark */
    gfx_light(v3(sun.x * s * g_env.tint[0], sun.y * s * g_env.tint[1], sun.z * s * g_env.tint[2]), v3(sky.x * a * g_env.tint[0], sky.y * a * g_env.tint[1], sky.z * a * g_env.tint[2]),
              v3(ground.x * a * g_env.tint[0], ground.y * a * g_env.tint[1], ground.z * a * g_env.tint[2]), eye); }

static Mesh *m_sea, *m_cave;
static float wfx_h(int i, int k) { unsigned h = (unsigned)i * 374761393u + (unsigned)k * 668265263u + (unsigned)g_wk * 2246822519u; h = (h ^ (h >> 13)) * 1274126177u; return (float)((h ^ (h >> 16)) & 0xFFFFFF) / 16777216.0f; }
static float cave_top(float x, float z) { float r2 = x * x + z * z; float R = 275; if (r2 > R * R) return -30; return -32 + 0.36f * sqrtf(R * R - r2) - 9.0f * world_fbm(x * 0.02f, z * 0.02f, 4411); }
static void wfx_build(void) {
    if (m_sea) { gfx_mesh_free(m_sea); m_sea = NULL; } if (m_cave) { gfx_mesh_free(m_cave); m_cave = NULL; }
    if (g_world.sea_on) { const int N = 24; float S = 720, y = g_world.sea; tv_begin(N * N * 6);
        for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) { float x0 = -S + 2 * S * i / N, x1 = -S + 2 * S * (i + 1) / N, z0 = -S + 2 * S * j / N, z1 = -S + 2 * S * (j + 1) / N; Vec3 n = v3(0, 1, 0);
            tv_tri(v3(x0, y, z0), v3(x1, y, z0), v3(x1, y, z1), n, n, n, x0 * 0.06f, z0 * 0.06f, x1 * 0.06f, z0 * 0.06f, x1 * 0.06f, z1 * 0.06f, 1, 1, 1);
            tv_tri(v3(x0, y, z0), v3(x1, y, z1), v3(x0, y, z1), n, n, n, x0 * 0.06f, z0 * 0.06f, x1 * 0.06f, z1 * 0.06f, x0 * 0.06f, z1 * 0.06f, 1, 1, 1); }
        m_sea = tv_mesh(); }
    if (g_wk == WK_CRYSTAL && !g_side) {   /* the cave roof: a great dome of dark rock over the whole world */
        const int M = 48, K = 14; float R = 275; tv_begin(M * K * 6);
        for (int k = 0; k < K; k++) for (int i = 0; i < M; i++) { Vec3 q[4]; float rr[4] = {R * k / K, R * k / K, R * (k + 1) / K, R * (k + 1) / K}; float aa[4] = {i * 6.2831853f / M, (i + 1) * 6.2831853f / M, (i + 1) * 6.2831853f / M, i * 6.2831853f / M};
            for (int c = 0; c < 4; c++) { float x = sinf(aa[c]) * rr[c], z = cosf(aa[c]) * rr[c]; q[c] = v3(x, cave_top(x, z), z); }
            Vec3 e1 = v3_sub(q[1], q[0]), e2 = v3_sub(q[3], q[0]); if (k == 0) e1 = v3_sub(q[2], q[0]); Vec3 n = v3_norm(v3_cross(e1, e2)); if (n.y > 0) n = v3(-n.x, -n.y, -n.z);
            float g = 0.5f + 0.5f * world_fbm(i * 0.4f, k * 0.6f, 771);
            tv_tri(q[0], q[1], q[2], n, n, n, q[0].x * 0.04f, q[0].z * 0.04f, q[1].x * 0.04f, q[1].z * 0.04f, q[2].x * 0.04f, q[2].z * 0.04f, 0.5f * g, 0.42f * g, 0.62f * g);
            tv_tri(q[0], q[2], q[3], n, n, n, q[0].x * 0.04f, q[0].z * 0.04f, q[2].x * 0.04f, q[2].z * 0.04f, q[3].x * 0.04f, q[3].z * 0.04f, 0.5f * g, 0.42f * g, 0.62f * g); }
        m_cave = tv_mesh(); } }

/* ---- things in each world (their places are worked out from a number, the same every visit) ---- */
#define WFX_N 12
static float g_ring_t[WFX_N];   /* sky rings: > 0 = just flown through, comes back when it runs out */
static float g_zapw_t, g_zapw_x, g_zapw_z, g_zapw_next = 4;   /* Kaine's keep: lightning strikes */
static Vec3 wfx_spot(int i, float rmin, float rmax) { float a = wfx_h(i, 1) * 6.2831853f, r = rmin + wfx_h(i, 2) * (rmax - rmin); return v3(cosf(a) * r, 0, sinf(a) * r); }
static Vec3 ring_pos(int i) { Vec3 p = wfx_spot(i, 22, 105); p.y = 16 + 16 * wfx_h(i, 3); return p; }
static Vec3 geyser_pos(int i) { Vec3 p = wfx_spot(i, 28, 150); p.y = world_height(&g_world, p.x, p.z); return p; }
static float geyser_phase(int i) { return fmodf(g_time * 0.16f + wfx_h(i, 5), 1.0f); }   /* 0..0.72 quiet, ..0.84 rumbling, ..1 erupting */
static Vec3 devil_pos(int i) { float t = g_time * 0.05f + i * 1.7f; Vec3 p = v3(cosf(t * (1 + 0.2f * i)) * (40 + 25 * i), 0, sinf(t * 1.3f) * (35 + 22 * i)); p.y = world_height(&g_world, p.x, p.z); return p; }
typedef struct { int pm; float x, y, z, yaw, s, roll; } Critter;
static int critters(Critter *c, int max) {   /* the sea's animals, each swimming its own slow loop */
    int n = 0; if (!g_world.sea_on) return 0;
    for (int i = 0; i < 16 && n < max; i++) { int kind = i < 5 ? 0 : i < 8 ? 1 : i < 15 ? 2 : 3;   /* turtles, manta rays, jellyfish, one whale */
        Vec3 o = wfx_spot(i + 40, 55, 150); float R = kind == 3 ? 95 : 14 + 22 * wfx_h(i, 7), sp = (kind == 3 ? 0.035f : kind == 2 ? 0.02f : 0.12f) * (wfx_h(i, 8) > 0.5f ? 1 : -1), a = g_time * sp + i * 2.1f;
        if (kind == 3) o = v3(0, 0, 0);
        Critter *q = &c[n++]; q->x = o.x + cosf(a) * R; q->z = o.z + sinf(a) * R; float bed = world_height(&g_world, q->x, q->z);
        float depth = kind == 2 ? 3 + 5 * wfx_h(i, 9) + 1.2f * sinf(g_time * 0.8f + i) : kind == 3 ? 9 : 4 + 5 * wfx_h(i, 9) + sinf(g_time * 0.4f + i);
        q->y = fminf(bed + depth, g_world.sea - 2.5f); if (q->y < bed + 1.5f) { n--; continue; }
        q->yaw = (sp > 0 ? -a : -a + 3.14159f); q->roll = kind == 1 ? 0.25f * sinf(g_time * 1.5f + i) : 0;
        q->pm = kind == 0 ? PM_TURTLE : kind == 1 ? PM_MANTA : kind == 2 ? PM_JELLY : PM_WHALE; q->s = kind == 3 ? 1.0f : 0.8f + 0.5f * wfx_h(i, 10); }
    return n; }

static void wfx_update(float dt) {
    Dragon *d = &g_dragon; if (g_side) return;
    if (g_wk == WK_SKY) for (int i = 0; i < WFX_N; i++) { if (g_ring_t[i] > 0) { g_ring_t[i] -= dt; continue; } Vec3 p = ring_pos(i);
        float dx = d->x - p.x, dy = d->y + 0.6f - p.y, dz = d->z - p.z; if (dx * dx + dy * dy + dz * dz < 3.4f * 3.4f) { g_ring_t[i] = 40; d->growth += 35; d->hearts = fminf(d->max_hearts, d->hearts + 0.5f); d->stamina = 1; d->speed += 6;
            sfx_play(SFX_COIN, 0.9f, 0); sfx_play(SFX_GROW, 0.4f, 0); burst(p, 30, 7, 0.3f, 1, 0.9f, 0.4f, 0.7f); snprintf(g_msg, sizeof g_msg, "SKY RING! +35 FOOD AND A BURST OF SPEED"); g_msg_t = 2.5f; } }
    if (g_wk == WK_LAVA || g_wk == WK_KAINE) for (int i = 0; i < WFX_N; i++) { float ph = geyser_phase(i); Vec3 p = geyser_pos(i); if (world_in_pond(&g_world, p.x, p.z)) continue;
        float dx = d->x - p.x, dz = d->z - p.z, d2 = dx * dx + dz * dz;
        if (ph > 0.84f && ph - dt * 0.16f <= 0.84f && d2 < 60 * 60) { sfx_play(SFX_FIRE, 0.5f * (1 - sqrtf(d2) / 60), 0); }
        if (ph > 0.84f && d2 < 2.4f * 2.4f && d->y < p.y + 13 && d->invuln_t <= 0) { d->invuln_t = 1.2f; hurt_dragon(d, 0.75f, "WHOOSH! A FIRE GEYSER! WATCH FOR THE GLOWING CRACKS"); if (d->mode != DM_AIR) { d->mode = DM_AIR; } d->vy = 9; } }
    if (g_wk == WK_SAND) for (int i = 0; i < 4; i++) { Vec3 p = devil_pos(i); float dx = d->x - p.x, dz = d->z - p.z; static float cd; if (cd > 0) cd -= dt * 0.25f;
        if (dx * dx + dz * dz < 3.0f * 3.0f && d->y < p.y + 10 && cd <= 0) { cd = 1.5f; d->mode = DM_AIR; d->vy = 15; d->vx = sinf(d->yaw) * 6; d->vz = cosf(d->yaw) * 6; d->stamina = 1; sfx_play(SFX_WIND, 0.9f, 0); platform_rumble(0.5f, 0.4f);
            snprintf(g_msg, sizeof g_msg, "WHEEE! THE DUST DEVIL THROWS YOU SKY HIGH. HOLD A TO GLIDE"); g_msg_t = 3; } }
    if (g_wk == WK_KAINE) { g_zapw_next -= dt; if (g_zapw_t > 0) g_zapw_t -= dt;
        if (g_zapw_next <= 0) { g_zapw_next = 3.5f + 5 * wfx_h((int)(g_time * 7), 11); float a = wfx_h((int)(g_time * 13), 12) * 6.2831853f, r = 12 + 60 * wfx_h((int)(g_time * 17), 13); g_zapw_x = d->x + cosf(a) * r; g_zapw_z = d->z + sinf(a) * r; g_zapw_t = 0.35f;
            sfx_play(SFX_CRUMBLE, clampf(1.1f - r / 70, 0.25f, 1), 0); if (r < 30) g_shake_t = fmaxf(g_shake_t, 0.15f); } }
    if (g_world.sea_on) { static int was; int in = d->y < g_world.sea - 0.2f && world_height(&g_world, d->x, d->z) < g_world.sea - 0.5f;   /* a splash going in and out */
        if (in != was) { was = in; sfx_play(SFX_SPLASH, 0.8f, 0); burst(v3(d->x, g_world.sea + 0.2f, d->z), 30, 6, 0.3f, 0.7f, 0.9f, 1, 0.6f); if (in) { static int told; if (!told) { told = 1; snprintf(g_msg, sizeof g_msg, "UNDER THE SEA! HOLD A TO SWIM UP, STICK DOWN TO SWIM DOWN. X SNAPS UP FISH"); g_msg_t = 6; } } } } }

/* solid things, drawn with the scenery */
static void wfx_draw_solid(Vec3 eye) {
    Mat4 id = m4_identity();
    if (m_cave) { gfx_state(ST_DEPTH | ST_DEPTH_WRITE); gfx_bind(tx_rock); gfx_draw(m_cave, &id, 1, 1, 1, 1); gfx_state(ST_SOLID);
        if (g_pm[PM_STALAC].ok) for (int i = 0; i < 70; i++) { Vec3 p = wfx_spot(i + 100, 10, 190); float dx = p.x - eye.x, dz = p.z - eye.z, d2 = dx * dx + dz * dz; if (d2 > 170 * 170) continue;
            float s = 0.7f + 1.3f * wfx_h(i, 20); float top = cave_top(p.x, p.z) + 2.0f; const PropModel *m = &g_pm[PM_STALAC]; int lod = d2 > 60 * 60;   /* hung upside down from the roof */
            Mat4 mm = m4_mul(m4_mul(m4_mul(m4_translate(p.x, top, p.z), m4_rotate_y(wfx_h(i, 21) * 6.28f)), m4_rotate_x(3.14159265f)), m4_scale(s, s, s)); gfx_bind(m->tex); gfx_draw_ex(m->mesh[lod], &mm, 1.7f, 1.7f, 2.0f, 1, 0, 0, 0, 0); } gfx_bind(NULL); }
    if (g_world.sea_on) { Critter c[16]; int n = critters(c, 16);
        for (int i = 0; i < n; i++) { const PropModel *m = &g_pm[c[i].pm]; if (!m->ok) continue; float dx = c[i].x - eye.x, dz = c[i].z - eye.z, d2 = dx * dx + dz * dz; if (d2 > 130 * 130) continue;
            propmodel_draw(m, d2 > 45 * 45, c[i].x, c[i].y, c[i].z, c[i].yaw + CRIT_YAW[c[i].pm - PM_TURTLE], c[i].s, 1.15f, 1.15f, 1.15f); } }
    if (g_wk == WK_KAINE && g_pm[PM_TOWER].ok && !g_side) for (int i = 0; i < 5; i++) { float a = i * 1.2566f + 0.9f, r = g_world.arena_r + 16; float x = g_world.arena_x + cosf(a) * r, z = g_world.arena_z + sinf(a) * r;
        propmodel_draw(&g_pm[PM_TOWER], 0, x, world_height(&g_world, x, z) - 1.5f, z, -a, 1.0f + 0.25f * (i % 2), 1, 1, 1); } }
static void wfx_draw_sea(void) {   /* the sea's surface, seen from above or from below */
    if (!m_sea) return; Mat4 id = m4_identity(); gfx_state(ST_TRANSPARENT); gfx_bind(tx_water);
    if (g_uw) gfx_draw_ex(m_sea, &id, 0.6f, 1.2f, 1.4f, 0.55f, 0, 1, g_time * 0.015f, g_time * 0.01f);
    else gfx_draw_ex(m_sea, &id, 0.55f, 0.95f, 1.15f, 0.78f, 0.9f, 0, g_time * 0.02f, g_time * 0.013f);
    gfx_state(ST_SOLID); }

/* glowing things, added into the sparkle batch */
static float wrapc(float v, float b) { return v - b * floorf(v / b + 0.5f); }
static void air_bits(Vec3 eye, Vec3 rt, int n, float box, float boxy, float vx, float vy, float sway, float size, float r, float g, float b, float a, int seed) {
    for (int i = 0; i < n; i++) { float hx = wfx_h(i, seed), hy = wfx_h(i, seed + 1), hz = wfx_h(i, seed + 2);
        float x = eye.x + wrapc(hx * box + g_time * vx + sway * sinf(g_time * 0.9f + i * 1.3f) - eye.x, box), y = eye.y + wrapc(hy * boxy + g_time * vy - eye.y, boxy), z = eye.z + wrapc(hz * box + sway * cosf(g_time * 0.7f + i * 2.1f) - eye.z, box);
        float tw = 0.6f + 0.4f * sinf(g_time * 3 + i * 1.9f), sz = size * (0.6f + 0.8f * wfx_h(i, seed + 3));
        batch_quad(&g_b, v3(x, y, z), v3(rt.x * sz, 0, rt.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0, 0, 1, 1, r, g, b, a * tw); } }
static void wfx_draw_add(Vec3 eye) {
    Vec3 fw = v3_norm(v3(g_at.x - eye.x, 0, g_at.z - eye.z)), rt = v3(fw.z, 0, -fw.x);
    if (g_uw) { air_bits(eye, rt, 150, 46, 22, 0.2f, 1.6f, 0.6f, 0.13f, 0.7f, 0.95f, 1.0f, 0.55f, 31);   /* bubbles rising, and shafts of sunlight coming down through the water */
        for (int i = 0; i < 26; i++) { float x = eye.x + wrapc(wfx_h(i, 40) * 90 + 3 * sinf(g_time * 0.2f + i) - eye.x, 90), z = eye.z + wrapc(wfx_h(i, 41) * 90 - eye.z, 90); float w = 1.2f + 2.5f * wfx_h(i, 42), al = 0.07f + 0.06f * sinf(g_time * 0.8f + i * 2);
            batch_quad(&g_b, v3(x - 4, g_world.sea - 14, z), v3(rt.x * w, 0, rt.z * w), v3(4, 14, 0), v3(0, 1, 0), 0.45f, 0.1f, 0.55f, 0.9f, 0.7f, 1.0f, 0.9f, al); } }
    else switch (g_wk) {
    case WK_FOREST: air_bits(eye, rt, 70, 50, 9, 0.3f, 0.15f, 1.6f, 0.16f, 0.85f, 1.0f, 0.4f, 0.8f, 51); break;                 /* fireflies and drifting pollen */
    case WK_SKY: air_bits(eye, rt, 60, 70, 30, 2.5f, 0.2f, 1.0f, 0.14f, 1.0f, 1.0f, 1.0f, 0.5f, 52); break;                      /* wisps on the wind */
    case WK_SNOW: air_bits(eye, rt, 260, 44, 26, 1.2f, -2.6f, 1.2f, 0.11f, 1.0f, 1.0f, 1.0f, 0.85f, 53); break;                 /* falling snow */
    case WK_CRYSTAL: air_bits(eye, rt, 120, 50, 20, 0.1f, 0.25f, 1.0f, 0.15f, 0.8f, 0.6f, 1.0f, 0.9f, 54); air_bits(eye, rt, 60, 50, 20, -0.1f, 0.2f, 1.0f, 0.13f, 0.4f, 0.95f, 1.0f, 0.9f, 58); break;   /* glowing motes */
    case WK_SAND: air_bits(eye, rt, 170, 50, 8, 9.0f, 0.1f, 0.5f, 0.14f, 1.0f, 0.85f, 0.55f, 0.45f, 55); break;                 /* sand blowing low across the dunes */
    case WK_LAVA: air_bits(eye, rt, 170, 46, 24, 0.5f, 2.2f, 1.2f, 0.12f, 1.0f, 0.5f, 0.12f, 0.9f, 56); break;                   /* embers rising */
    case WK_KAINE: air_bits(eye, rt, 120, 46, 24, 3.0f, -1.0f, 1.5f, 0.1f, 1.0f, 0.2f, 0.15f, 0.8f, 57); break;                  /* red sparks on the storm wind */
    default: break; }
    if (g_side) return;
    if (g_wk == WK_SNOW) for (int k = 0; k < 3; k++) for (int i = 0; i < 40; i++) {   /* the northern lights: three slow green and violet curtains */
        float u = i / 40.0f, a = u * 3.0f + k * 2.1f + 0.3f, R = 330 + 40 * k, x = cosf(a) * R, z = sinf(a) * R, y = 120 + 30 * k + 16 * sinf(u * 9 + g_time * 0.5f + k), h = 50 + 18 * sinf(u * 13 - g_time * 0.4f);
        float al = (0.1f + 0.08f * sinf(u * 21 + g_time * 0.9f + k)) * sinf(u * 3.14159f);
        batch_quad(&g_b, v3(x, y, z), v3(-sinf(a) * 14, 0, cosf(a) * 14), v3(0, h, 0), v3(0, 1, 0), 0.4f, 0.4f, 0.6f, 0.6f, k == 1 ? 0.6f : 0.2f, k == 1 ? 0.3f : 1.0f, k == 1 ? 1.0f : 0.5f, al); }
    if (g_wk == WK_CRYSTAL) { int shown = 0; for (int i = 0; i < g_world.nprop && shown < 46; i++) { const Prop *p = &g_world.prop[i]; if (p->kind != P_TREE && p->kind != P_SPIRE && p->kind != P_PALM) continue;   /* every crystal lights the dark round it */
            float dx = p->x - eye.x, dz = p->z - eye.z; if (dx * dx + dz * dz > 75 * 75) continue; shown++; float gy = world_height(&g_world, p->x, p->z), pu = 0.7f + 0.3f * sinf(g_time * 1.7f + i), s = 4.5f * p->scale;
            int c = i % 3; batch_quad(&g_b, v3(p->x, gy + 2.2f * p->scale, p->z), v3(rt.x * s, 0, rt.z * s), v3(0, s, 0), v3(0, 1, 0), 0, 0, 1, 1, c == 0 ? 1.0f : 0.35f, c == 1 ? 0.9f : 0.4f, 1.0f, 0.42f * pu); } }
    if (g_wk == WK_SKY) for (int i = 0; i < WFX_N; i++) { if (g_ring_t[i] > 0) continue; Vec3 p = ring_pos(i); float dx = p.x - eye.x, dz = p.z - eye.z; if (dx * dx + dz * dz > 160 * 160) continue;   /* golden rings to fly through */
        float ya = wfx_h(i, 4) * 3.14159f; Vec3 ax = v3(cosf(ya), 0, sinf(ya));
        for (int k = 0; k < 20; k++) { float a = k * 0.314159f + g_time * 0.8f; Vec3 c = v3(p.x + ax.x * cosf(a) * 3.0f, p.y + sinf(a) * 3.0f, p.z + ax.z * cosf(a) * 3.0f); float sz = 0.55f + 0.15f * sinf(g_time * 5 + k);
            batch_quad(&g_b, c, v3(rt.x * sz, 0, rt.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0, 0, 1, 1, 1.0f, 0.85f, 0.3f, 1); }
        batch_quad(&g_b, p, v3(rt.x * 3.4f, 0, rt.z * 3.4f), v3(0, 3.4f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1.0f, 0.9f, 0.5f, 0.16f); }
    if (g_wk == WK_LAVA || g_wk == WK_KAINE) for (int i = 0; i < WFX_N; i++) { Vec3 p = geyser_pos(i); float dx = p.x - eye.x, dz = p.z - eye.z; if (dx * dx + dz * dz > 120 * 120 || world_in_pond(&g_world, p.x, p.z)) continue; float ph = geyser_phase(i);
        float glow = ph > 0.72f ? 1 : 0.35f + 0.15f * sinf(g_time * 2 + i); batch_quad(&g_b, v3(p.x, p.y + 0.25f, p.z), v3(1.6f * glow + 0.6f, 0, 0), v3(0, 0, 1.6f * glow + 0.6f), v3(0, 1, 0), 0, 0, 1, 1, 1.0f, 0.4f, 0.08f, 0.8f * glow);   /* the glowing crack */
        if (ph > 0.72f && ph <= 0.84f) for (int k = 0; k < 8; k++) { float u = fmodf(g_time * 2 + k * 0.125f, 1.0f); batch_quad(&g_b, v3(p.x + 0.8f * sinf(k * 2.4f), p.y + u * 1.5f, p.z + 0.8f * cosf(k * 2.4f)), v3(rt.x * 0.3f, 0, rt.z * 0.3f), v3(0, 0.3f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 0.6f, 0.2f, 1 - u); }
        if (ph > 0.84f) { float e = (ph - 0.84f) / 0.16f, hgt = 13 * sinf(fminf(1, e * 4) * 1.5708f) * (e > 0.8f ? (1 - e) * 5 : 1);
            for (int k = 0; k < 34; k++) { float u = fmodf(k * 0.618f + g_time * 2.5f, 1.0f), wob = 0.5f + 1.2f * u; float sz = 1.3f - 0.7f * u;
                batch_quad(&g_b, v3(p.x + wob * sinf(k * 2.4f + g_time * 3), p.y + u * hgt, p.z + wob * cosf(k * 2.4f + g_time * 3)), v3(rt.x * sz, 0, rt.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0, 0, 1, 1, 1.0f, 0.75f - 0.55f * u, 0.25f - 0.2f * u, 1 - u * 0.7f); } } }
    if (g_wk == WK_SAND) for (int i = 0; i < 4; i++) { Vec3 p = devil_pos(i); float dx = p.x - eye.x, dz = p.z - eye.z; if (dx * dx + dz * dz > 150 * 150) continue;   /* dust devils wander the dunes */
        for (int k = 0; k < 70; k++) { float u = fmodf(k * 0.618f + g_time * 0.5f, 1.0f), a = k * 2.4f + g_time * (7 - 4 * u), r = 0.5f + 2.6f * u * u + 0.3f * sinf(k); float sz = 0.5f + 0.7f * u;
            batch_quad(&g_b, v3(p.x + cosf(a) * r + 0.8f * sinf(g_time * 2 + u * 5), p.y + u * 11, p.z + sinf(a) * r), v3(rt.x * sz, 0, rt.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0, 0, 1, 1, 1.0f, 0.82f, 0.5f, 0.5f * (1 - u * 0.6f)); } }
    if (g_wk == WK_KAINE && g_zapw_t > 0) { float gy = world_height(&g_world, g_zapw_x, g_zapw_z); Vec3 a = v3(g_zapw_x + 6, gy + 90, g_zapw_z); float al = g_zapw_t / 0.35f;   /* a fork of lightning */
        for (int k = 0; k < 9; k++) { Vec3 b = v3(g_zapw_x + (k < 8 ? 5 * (wfx_h(k + (int)(g_time * 30), 60) - 0.5f) * (8 - k) * 0.4f : 0), gy + 90 - 10 * (k + 1), g_zapw_z + (k < 8 ? 4 * (wfx_h(k, 61) - 0.5f) : 0)); Vec3 mid = v3((a.x + b.x) / 2, (a.y + b.y) / 2, (a.z + b.z) / 2);
            batch_quad(&g_b, mid, v3(rt.x * 0.5f, 0, rt.z * 0.5f), v3((b.x - a.x) / 2, (b.y - a.y) / 2, (b.z - a.z) / 2), v3(0, 1, 0), 0.45f, 0.1f, 0.55f, 0.9f, 3.0f, 2.6f, 3.0f, al); a = b; }
        batch_quad(&g_b, v3(g_zapw_x, gy + 1.5f, g_zapw_z), v3(rt.x * 7, 0, rt.z * 7), v3(0, 7, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 0.9f, 1, al); } }
