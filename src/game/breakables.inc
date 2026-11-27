/* ================= smashing things: rocks, spires and crystals break into tumbling pieces; mega blasts leave craters ================= */
#define SH_N 260
typedef struct { float x, y, z, vx, vy, vz, ax, ay, az, sx, sy, sz, spin, t, life, r, g, b, glow; int rest; } Shard;
static Shard g_sh[SH_N]; static int g_shn; static Mesh *m_shards; static Vertex g_shv[SH_N * 24];
static float g_crater_cd;
static void shard_colour(int kind, float *r, float *g, float *b, float *glow) {   /* what the broken thing is made of, by world */
    *glow = 0;
    switch (g_wk) {
    case WK_CRYSTAL: { const float *c = TH->portal; *r = 0.55f + 0.45f * c[0]; *g = 0.55f + 0.45f * c[1]; *b = 0.6f + 0.4f * c[2]; *glow = 0.6f; } break;
    case WK_SNOW: if (kind == P_SPIRE) { *r = 0.78f; *g = 0.88f; *b = 0.98f; *glow = 0.15f; } else { *r = 0.62f; *g = 0.64f; *b = 0.7f; } break;
    case WK_SAND: *r = 0.86f; *g = 0.68f; *b = 0.44f; break;
    case WK_LAVA: case WK_KAINE: *r = 0.26f; *g = 0.22f; *b = 0.21f; *glow = kind == P_SPIRE ? 0.35f : 0.1f; break;
    case WK_WATER: *r = 0.74f; *g = 0.62f; *b = 0.58f; break;   /* coral rock */
    default: *r = 0.52f; *g = 0.49f; *b = 0.44f; break; } }
/* one smashed prop: big chunks that tumble and bounce, small chips, a puff of dust, a crack of sound */
static void shatter(int pi) {
    const Prop *p = &g_world.prop[pi]; float gy = world_height(&g_world, p->x, p->z), H = (p->kind == P_SPIRE ? 7.0f : p->kind == P_ROCK ? 1.6f : 4.0f) * p->scale;
    float r, g, b, glow; shard_colour(p->kind, &r, &g, &b, &glow); unsigned s = (unsigned)pi * 2654435761u + (unsigned)(g_time * 1000);
    int n = p->kind == P_SPIRE ? 26 : p->kind == P_ROCK ? 14 : 20;
    for (int k = 0; k < n; k++) { Shard *q = &g_sh[g_shn]; g_shn = (g_shn + 1) % SH_N; float u = frand01(&s), a = frand01(&s) * 6.2831853f, big = k < n / 3 ? 1.0f : 0.45f;
        float sz = (0.25f + 0.45f * frand01(&s)) * big * fminf(2.2f, p->scale) * (p->kind == P_ROCK ? 0.9f : 1.1f);
        q->x = p->x + cosf(a) * 0.6f * p->scale; q->z = p->z + sinf(a) * 0.6f * p->scale; q->y = gy + 0.2f + u * H;
        float out = 3.0f + 6.0f * frand01(&s); q->vx = cosf(a) * out; q->vz = sinf(a) * out; q->vy = 3.0f + 7.0f * frand01(&s);
        q->ax = frand01(&s) * 6.28f; q->ay = frand01(&s) * 6.28f; q->az = frand01(&s) * 6.28f; q->spin = 4 + 10 * frand01(&s);
        q->sx = sz * (0.7f + 0.6f * frand01(&s)); q->sy = sz * (0.6f + 0.9f * frand01(&s)) * (p->kind == P_SPIRE || g_wk == WK_CRYSTAL ? 1.6f : 1.0f); q->sz = sz * (0.7f + 0.6f * frand01(&s));
        float sh = 0.85f + 0.3f * frand01(&s); q->r = r * sh; q->g = g * sh; q->b = b * sh; q->glow = glow; q->t = 0; q->life = 5.5f + 3 * frand01(&s); q->rest = 0; }
    Vec3 c = v3(p->x, gy + H * 0.4f, p->z); fx_shatter(c, fminf(2.5f, p->scale) * (p->kind == P_SPIRE ? 1.6f : 1.0f), r, g, b); burst(c, 22, 5, 0.9f, r * 0.9f + 0.1f, g * 0.9f + 0.1f, b * 0.9f + 0.1f, 1.0f);
    if (glow > 0.3f) burst(c, 26, 8, 0.4f, r, g, b, 0.8f);   /* crystals and lava rock: glittering bits */
    sfx_play(SFX_CRUMBLE, 0.8f, 0); g_shake_t = fmaxf(g_shake_t, 0.25f); platform_rumble(0.5f, 0.18f); }
static int smash(float x, float y, float z, float dx, float dz, float range, float cone, float max_scale) {
    int idx[24]; int n = world_break(&g_world, x, y, z, dx, dz, range, cone, max_scale, idx, 24);
    for (int i = 0; i < n && i < 24; i++) shatter(idx[i]);
    if (n) { g_dragon.growth += 2 * n; }
#ifndef __wiiu__
    if (n && getenv("DB_SMASH")) fprintf(stderr, "smash: %d broke\n", n);
#endif
    return n; }
/* a mega blast on the ground punches a crater into it: the ground sinks in a bowl with a raised rim, and everything on it drops in */
static void crater(float x, float z, float R, float depth) {
    if (g_side || WK_ISLES(g_world.kind) || g_crater_cd > 0) return;
    if (g_world.sea_on && world_height(&g_world, x, z) < g_world.sea + 0.5f) return;
    float dxp = x - g_world.pond_x, dzp = z - g_world.pond_z; if (g_world.pond_r > 0 && dxp * dxp + dzp * dzp < (g_world.pond_r + R + 4) * (g_world.pond_r + R + 4)) return;
    float ax = x - g_world.arena_x, az = z - g_world.arena_z; if (ax * ax + az * az < (g_world.arena_r + 2) * (g_world.arena_r + 2)) R *= 0.7f;
    g_crater_cd = 1.5f; const float cs = W_SIZE / (W_GRID - 1);
    int i0 = (int)floorf((x - R * 1.4f + W_SIZE / 2) / cs), i1 = (int)ceilf((x + R * 1.4f + W_SIZE / 2) / cs), j0 = (int)floorf((z - R * 1.4f + W_SIZE / 2) / cs), j1 = (int)ceilf((z + R * 1.4f + W_SIZE / 2) / cs);
    for (int j = j0; j <= j1; j++) for (int i = i0; i <= i1; i++) { if (i < 1 || j < 1 || i >= W_GRID - 1 || j >= W_GRID - 1) continue;
        float px = -W_SIZE / 2 + cs * i - x, pz = -W_SIZE / 2 + cs * j - z, d = sqrtf(px * px + pz * pz) / R;
        float bowl = d < 1 ? -(1 - d * d) * (1 - d * d) : 0, rim = d > 0.75f && d < 1.4f ? 0.22f * sinf((d - 0.75f) / 0.65f * 3.14159f) : 0;
        g_world.h[j][i] += depth * (bowl + rim); }
    if (g_gfx_status == GFX_OK) { gfx_sync(); Mesh **ms[] = {&m_ground, &m_ground_dirt, &m_ground_rock}; for (int i = 0; i < 3; i++) if (*ms[i]) { gfx_mesh_free(*ms[i]); *ms[i] = NULL; } build_ground(); }
    SceneLayer *L[] = {&g_lay_tree, &g_lay_palm, &g_lay_fern, &g_lay_bush, &g_lay_rock, &g_lay_spire};   /* scenery and food settle onto the new ground */
    for (int l = 0; l < 6; l++) for (int i = 0; i < L[l]->n; i++) { SceneInst *I = &L[l]->inst[i]; float ex = I->x - x, ez = I->z - z; if (ex * ex + ez * ez > R * R * 2.2f) continue; float gy = world_height(&g_world, I->x, I->z); if (I->p) { float old = I->y; I->y = gy - (old > gy ? 0 : 0); I->y = fminf(old, gy); } }
    for (int i = 0; i < g_world.nfood; i++) { Food *f = &g_world.food[i]; float ex = f->x - x, ez = f->z - z; if (ex * ex + ez * ez < R * R * 2.2f) f->y = world_height(&g_world, f->x, f->z); }
    g_ng_wk = -1;   /* re-lay the grass */
    float gy = world_height(&g_world, x, z); burst(v3(x, gy + 0.5f, z), 60, 10, 1.1f, 0.5f, 0.45f, 0.38f, 1.4f); }
static void shards_update(float dt) {
    if (g_crater_cd > 0) g_crater_cd -= dt;
    for (int i = 0; i < SH_N; i++) { Shard *q = &g_sh[i]; if (q->life <= 0) continue; q->t += dt; if (q->t > q->life) { q->life = 0; continue; }
        if (!q->rest) { q->vy -= 17 * dt; q->x += q->vx * dt; q->y += q->vy * dt; q->z += q->vz * dt; q->ax += q->spin * dt; q->ay += q->spin * 0.7f * dt;
            float gy = world_height(&g_world, q->x, q->z) + q->sy * 0.35f;
            if (q->y < gy) { q->y = gy; if (q->vy < -2.5f) { q->vy = -q->vy * 0.35f; q->vx *= 0.55f; q->vz *= 0.55f; q->spin *= 0.5f; } else { q->vy = 0; q->vx *= 0.8f; q->vz *= 0.8f; q->spin *= 0.7f; if (fabsf(q->vx) + fabsf(q->vz) < 0.3f) q->rest = 1; } }
            float dx = q->x - g_dragon.x, dz = q->z - g_dragon.z; if (dx * dx + dz * dz < 1.2f && q->rest) q->rest = 0; } } }   /* kicked about by the dragon */
static void shards_draw(void) {
    if (!m_shards) { m_shards = gfx_mesh_dynamic(SH_N * 24); if (!m_shards) return; }
    int n = 0;
    for (int i = 0; i < SH_N; i++) { const Shard *q = &g_sh[i]; if (q->life <= 0) continue; float left = q->life - q->t, k = left < 1 ? left : 1;   /* shrink away at the end */
        float ca = cosf(q->ax), sa = sinf(q->ax), cb = cosf(q->ay), sb = sinf(q->ay);
        Vec3 ex = v3(cb * q->sx * k, 0, -sb * q->sx * k), ey = v3(sb * sa * q->sy * k, ca * q->sy * k, cb * sa * q->sy * k), ez = v3(sb * ca * q->sz * k, -sa * q->sz * k, cb * ca * q->sz * k);
        Vec3 P[6] = { v3(q->x + ex.x, q->y + ex.y, q->z + ex.z), v3(q->x - ex.x * 0.8f, q->y - ex.y * 0.8f, q->z - ex.z * 0.8f), v3(q->x + ey.x, q->y + ey.y, q->z + ey.z),
                      v3(q->x - ey.x * 0.7f, q->y - ey.y * 0.7f, q->z - ey.z * 0.7f), v3(q->x + ez.x, q->y + ez.y, q->z + ez.z), v3(q->x - ez.x * 0.9f, q->y - ez.y * 0.9f, q->z - ez.z * 0.9f) };   /* a lumpy octahedron */
        static const int F[8][3] = {{0,2,4},{2,1,4},{1,3,4},{3,0,4},{2,0,5},{1,2,5},{3,1,5},{0,3,5}};
        for (int f = 0; f < 8 && n + 3 <= SH_N * 24; f++) { Vec3 a = P[F[f][0]], b = P[F[f][1]], c = P[F[f][2]]; Vec3 nn = v3_norm(v3_cross(v3_sub(b, a), v3_sub(c, a)));
            float l = 1 + q->glow * 0.8f; float fr = q->r * l, fg = q->g * l, fb = q->b * l;
            Vertex va = {a.x, a.y, a.z, nn.x, nn.y, nn.z, 0, 0, fr, fg, fb, 1}, vb = {b.x, b.y, b.z, nn.x, nn.y, nn.z, 1, 0, fr, fg, fb, 1}, vc = {c.x, c.y, c.z, nn.x, nn.y, nn.z, 0, 1, fr, fg, fb, 1};
            g_shv[n++] = va; g_shv[n++] = vb; g_shv[n++] = vc; } }
    if (!n) return; gfx_mesh_update(m_shards, g_shv, (uint32_t)n); Mat4 id = m4_identity(); gfx_state(ST_SOLID); gfx_bind(NULL); gfx_draw(m_shards, &id, 1, 1, 1, 1); }
