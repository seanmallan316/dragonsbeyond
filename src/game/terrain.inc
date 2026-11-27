/* terrain.inc - the ground of a 3.6 km world, streamed in 100 m chunks round the camera (included by game.c).
   Near chunks: every height sample, with the dirt and rock layers blended on top (as the old single ground mesh was).
   Far chunks, out to the fog: every 4th sample, the dirt and rock mixed into the colour, and the trees as low solid
   shapes, so far forests and ridges still read. Each chunk has a skirt hanging down its edges, so where a near chunk
   meets a far one there is never a crack. Chunks are built a few per frame, nearest first, and freed a few frames
   after they fall out of range (the GPU may still be drawing last frame's). */
#if defined(DB_TOUR) && !defined(DB_LOOK)   /* DB_LOOK: the tour with the real view, for look checks on a PC */
#define TR_NEAR 140.0f   /* the emulator renders in software: a smaller world round the camera */
#define TR_FAR 380.0f
#else
#define TR_NEAR 190.0f
#define TR_FAR 840.0f
#endif
#define TR_CELLS 32                 /* height cells per chunk side (W_GRID - 1 = 36 x 32) */
#define TR_FSTEP 4                  /* far chunks: every 4th sample (12.5 m) */
typedef struct { Mesh *g, *dirt, *rock, *far; } TChunk;
static TChunk g_tc[W_NCH];
static unsigned char g_tc_near[W_NCH];   /* chunks drawn in full (the scenery layers only draw props in these) */
static int g_tc_nnear, g_tc_nfar, g_tc_builds;   /* live counts, and builds this frame (the world test reads these through game_where) */
static float g_tc_ex = 1e9f, g_tc_ez = 1e9f;
#define TR_FREEQ 512
static Mesh *g_tfree[TR_FREEQ]; static int g_tfree_at[TR_FREEQ], g_tfree_n, g_tframe;
static void tr_free_later(Mesh *m) { if (!m) return; if (g_tfree_n >= TR_FREEQ) { gfx_sync(); for (int i = 0; i < g_tfree_n; i++) gfx_mesh_free(g_tfree[i]); g_tfree_n = 0; }
    g_tfree[g_tfree_n] = m; g_tfree_at[g_tfree_n] = g_tframe; g_tfree_n++; }
static void tr_free_old(void) { int k = 0; for (int i = 0; i < g_tfree_n; i++) { if (g_tframe - g_tfree_at[i] > 3) gfx_mesh_free(g_tfree[i]); else { g_tfree[k] = g_tfree[i]; g_tfree_at[k] = g_tfree_at[i]; k++; } } g_tfree_n = k; }
static float tr_far_r(void) { return fminf(TR_FAR, g_env.f1 + 80); }

/* the ground colour at a sample, as the old ground mesh made it */
static void tr_base_colour(float x, float z, float y, float *c) {
    float hgt = clampf((y + 4) / 30.0f, 0, 1); float shade = 0.9f + 0.2f * vnoise2(x * 0.08f, z * 0.08f, 5);
    float r = (0.7f + 0.2f * hgt) * shade, g = (0.9f - 0.05f * hgt) * shade, b = (0.62f + 0.15f * hgt) * shade;
    if (strcmp(TH->gtex0, "grass")) { r = g = b = (0.92f + 0.12f * hgt) * shade; } c[0] = r * TH->gtint[0]; c[1] = g * TH->gtint[1]; c[2] = b * TH->gtint[2]; }

static void tr_skirt(Vec3 a, Vec3 b, float drop, float u0, float u1, float s, const float *ca, const float *cb) {
    Vec3 a2 = v3(a.x, a.y - drop, a.z), b2 = v3(b.x, b.y - drop, b.z), n = v3(0, 1, 0);
    tv_push(a, n, u0, 0, ca[0], ca[1], ca[2], 1); tv_push(a2, n, u0, drop / s, ca[0], ca[1], ca[2], 1); tv_push(b, n, u1, 0, cb[0], cb[1], cb[2], 1);   /* both faces: seen from either side */
    tv_push(b, n, u1, 0, cb[0], cb[1], cb[2], 1); tv_push(a2, n, u0, drop / s, ca[0], ca[1], ca[2], 1); tv_push(b2, n, u1, drop / s, cb[0], cb[1], cb[2], 1);
    tv_push(a, n, u0, 0, ca[0], ca[1], ca[2], 1); tv_push(b, n, u1, 0, cb[0], cb[1], cb[2], 1); tv_push(a2, n, u0, drop / s, ca[0], ca[1], ca[2], 1);
    tv_push(b, n, u1, 0, cb[0], cb[1], cb[2], 1); tv_push(b2, n, u1, drop / s, cb[0], cb[1], cb[2], 1); tv_push(a2, n, u0, drop / s, ca[0], ca[1], ca[2], 1); }

static void tr_build_near(int c) {
    TChunk *T = &g_tc[c]; int i0 = (c % W_CHN) * TR_CELLS, j0 = (c / W_CHN) * TR_CELLS; const int N = TR_CELLS + 1;
    static float dirtw[TR_CELLS + 1][TR_CELLS + 1], rockw[TR_CELLS + 1][TR_CELLS + 1]; static Vec3 nrm[TR_CELLS + 1][TR_CELLS + 1]; static float base[TR_CELLS + 1][TR_CELLS + 1][3];
    int any = 0;
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) { int ii = i0 + i, jj = j0 + j; float x = -W_SIZE / 2 + W_CELL * ii, z = -W_SIZE / 2 + W_CELL * jj;
        nrm[j][i] = world_normal(&g_world, x, z); ground_layers(x, z, nrm[j][i].y, &dirtw[j][i], &rockw[j][i]); tr_base_colour(x, z, g_world.h[jj][ii], base[j][i]); if (g_world.h[jj][ii] > W_CLOUDS - 6) any = 1; }
    if (!any) return;   /* all cloud gap */
    for (int layer = 0; layer < 3; layer++) {
        tv_begin(TR_CELLS * TR_CELLS * 6 + (layer == 0 ? 4 * TR_CELLS * 12 : 0));
        for (int j = 0; j < TR_CELLS; j++) for (int i = 0; i < TR_CELLS; i++) {
            Vec3 p[4]; Vec3 n[4]; float cl[4][4]; float wsum = 0;
            for (int k = 0; k < 4; k++) { int li = i + (k & 1), lj = j + (k >> 1), ii = i0 + li, jj = j0 + lj; float x = -W_SIZE / 2 + W_CELL * ii, z = -W_SIZE / 2 + W_CELL * jj;
                p[k] = v3(x, g_world.h[jj][ii] + (layer ? 0.03f * layer : 0), z); n[k] = nrm[lj][li];
                float r = base[lj][li][0], g = base[lj][li][1], b = base[lj][li][2]; float shade = r / fmaxf(1e-3f, (0.7f + 0.2f * clampf((p[k].y + 4) / 30.0f, 0, 1)) * TH->gtint[0]);
                if (layer == 1) { r = g = b = fminf(1.2f, shade); } if (layer == 2) { r = g = b = 1.0f; }
                float a = layer == 0 ? 1 : (layer == 1 ? dirtw[lj][li] : rockw[lj][li]); wsum += a;
                cl[k][0] = r; cl[k][1] = g; cl[k][2] = b; cl[k][3] = a; }
            if (layer && wsum < 0.02f) continue;
            if (p[0].y < W_CLOUDS - 6 && p[1].y < W_CLOUDS - 6 && p[2].y < W_CLOUDS - 6 && p[3].y < W_CLOUDS - 6) continue;   /* nothing to draw in the sky world's gaps */
            float s = layer == 2 ? 0.9f : 1.6f; float u0 = (i0 + i) * s, v0 = (j0 + j) * s, u1 = u0 + s, v1 = v0 + s;
            float hmin = fminf(fminf(p[0].y, p[1].y), fminf(p[2].y, p[3].y)), hmax = fmaxf(fmaxf(p[0].y, p[1].y), fmaxf(p[2].y, p[3].y));
            if (hmax - hmin > W_CELL * 1.5f) {   /* a cliff: texture runs along the wall and down it, not stretched from above */
                float k = s / W_CELL; float uu[4], vv[4]; for (int q = 0; q < 4; q++) { uu[q] = (p[q].x + p[q].z) * k; vv[q] = -p[q].y * k; }
                tv_push(p[0], n[0], uu[0], vv[0], cl[0][0], cl[0][1], cl[0][2], cl[0][3]); tv_push(p[2], n[2], uu[2], vv[2], cl[2][0], cl[2][1], cl[2][2], cl[2][3]); tv_push(p[1], n[1], uu[1], vv[1], cl[1][0], cl[1][1], cl[1][2], cl[1][3]);
                tv_push(p[1], n[1], uu[1], vv[1], cl[1][0], cl[1][1], cl[1][2], cl[1][3]); tv_push(p[2], n[2], uu[2], vv[2], cl[2][0], cl[2][1], cl[2][2], cl[2][3]); tv_push(p[3], n[3], uu[3], vv[3], cl[3][0], cl[3][1], cl[3][2], cl[3][3]); continue; }
            tv_push(p[0], n[0], u0, v0, cl[0][0], cl[0][1], cl[0][2], cl[0][3]); tv_push(p[2], n[2], u0, v1, cl[2][0], cl[2][1], cl[2][2], cl[2][3]); tv_push(p[1], n[1], u1, v0, cl[1][0], cl[1][1], cl[1][2], cl[1][3]);
            tv_push(p[1], n[1], u1, v0, cl[1][0], cl[1][1], cl[1][2], cl[1][3]); tv_push(p[2], n[2], u0, v1, cl[2][0], cl[2][1], cl[2][2], cl[2][3]); tv_push(p[3], n[3], u1, v1, cl[3][0], cl[3][1], cl[3][2], cl[3][3]); }
        if (layer == 0 && !WK_ISLES(g_wk))   /* the skirt round the chunk's edge (the islands' cliffs make their own edges) */
            for (int e = 0; e < 4; e++) for (int t = 0; t < TR_CELLS; t++) {
                int ai, aj, bi, bj; if (e == 0) { ai = t; aj = 0; bi = t + 1; bj = 0; } else if (e == 1) { ai = t; aj = TR_CELLS; bi = t + 1; bj = TR_CELLS; } else if (e == 2) { ai = 0; aj = t; bi = 0; bj = t + 1; } else { ai = TR_CELLS; aj = t; bi = TR_CELLS; bj = t + 1; }
                Vec3 a = v3(-W_SIZE / 2 + W_CELL * (i0 + ai), g_world.h[j0 + aj][i0 + ai], -W_SIZE / 2 + W_CELL * (j0 + aj)), b = v3(-W_SIZE / 2 + W_CELL * (i0 + bi), g_world.h[j0 + bj][i0 + bi], -W_SIZE / 2 + W_CELL * (j0 + bj));
                tr_skirt(a, b, 3.0f, (a.x + a.z) * 0.5f, (b.x + b.z) * 0.5f, 2.0f, base[aj][ai], base[bj][bi]); }
        Mesh *m = tv_mesh(); if (layer == 0) T->g = m; else if (layer == 1) T->dirt = m; else T->rock = m; }
    g_tc_nnear++; g_tc_near[c] = 1; }

/* a low solid shape for a far tree, crystal or rock needle: a lit dome on short sides (no dark underside to catch the eye),
   each one its own size and shade so a far forest reads as one rolling canopy */
static void tr_far_prop(const Prop *p, float gy) {
    float s = p->scale, cx = p->x, cz = p->z, r, y0, y1; float col[3];
    unsigned hh = (unsigned)(int)(cx * 13.0f) * 2654435761u ^ (unsigned)(int)(cz * 7.0f) * 40503u; float v = ((hh >> 9) & 255) / 255.0f, v2 = ((hh >> 17) & 255) / 255.0f;
    switch (g_wk) { case WK_SNOW: col[0] = 0.62f; col[1] = 0.72f; col[2] = 0.7f; break; case WK_CRYSTAL: col[0] = 0.85f; col[1] = 0.7f; col[2] = 1.15f; break;
        case WK_SAND: col[0] = 0.62f; col[1] = 0.74f; col[2] = 0.45f; break; case WK_LAVA: case WK_KAINE: col[0] = 0.42f; col[1] = 0.38f; col[2] = 0.36f; break;
        default: col[0] = 0.5f; col[1] = 0.72f; col[2] = 0.38f; break; }
    float tint = 0.82f + 0.3f * v; col[0] *= tint * (0.95f + 0.1f * v2); col[1] *= tint; col[2] *= tint * (1.05f - 0.1f * v2);
    if (p->kind == P_SPIRE) { r = 2.0f * s; y0 = gy - 0.5f; y1 = gy + 10.0f * s;   /* rock needles in the world's own stone */
        static const float SP[WK_N][3] = {{0.78f, 0.78f, 0.76f}, {0.8f, 0.8f, 0.8f}, {0.85f, 0.93f, 1.05f}, {0.9f, 0.72f, 1.2f}, {0.98f, 0.8f, 0.58f}, {0.95f, 0.6f, 0.55f}, {0.4f, 0.33f, 0.31f}, {0.36f, 0.34f, 0.4f}};
        for (int q = 0; q < 3; q++) col[q] = SP[g_wk][q] * tint; }
    else if (p->kind == P_PALM) { r = 2.6f * s; y0 = gy + 4.5f * s; y1 = gy + 8.0f * s; }
    else { r = (3.8f + 1.4f * v2) * s; y0 = gy + 2.0f * s; y1 = gy + (8.0f + 2.0f * v) * s; if (g_wk == WK_SNOW) { r = 2.3f * s; y0 = gy + 0.5f; y1 = gy + 9.0f * s; } }
    int spire = p->kind == P_SPIRE || g_wk == WK_SNOW; float ym = spire ? y0 + (y1 - y0) * 0.2f : y0 + (y1 - y0) * 0.55f; Vec3 top = v3(cx, y1, cz); float tu = 0.5f, tv = 0.5f;
    for (int k = 0; k < 6; k++) { float a0 = p->yaw + k * 1.0472f, a1 = a0 + 1.0472f; Vec3 e0 = v3(cx + sinf(a0) * r, ym, cz + cosf(a0) * r), e1 = v3(cx + sinf(a1) * r, ym, cz + cosf(a1) * r);
        Vec3 b0 = v3(cx + sinf(a0) * r * 0.8f, y0, cz + cosf(a0) * r * 0.8f), b1 = v3(cx + sinf(a1) * r * 0.8f, y0, cz + cosf(a1) * r * 0.8f);
        Vec3 nu = v3_norm(v3(sinf(a0 + 0.52f), spire ? 0.5f : 1.1f, cosf(a0 + 0.52f))), ns = v3_norm(v3(sinf(a0 + 0.52f), 0.35f, cosf(a0 + 0.52f)));
        tv_push(top, v3(0, 1, 0), tu, tv, col[0] * 1.12f, col[1] * 1.12f, col[2] * 1.12f, 1); tv_push(e1, nu, tu, tv, col[0], col[1], col[2], 1); tv_push(e0, nu, tu, tv, col[0], col[1], col[2], 1);
        if (spire) continue;
        tv_push(e0, ns, tu, tv, col[0] * 0.9f, col[1] * 0.9f, col[2] * 0.9f, 1); tv_push(e1, ns, tu, tv, col[0] * 0.9f, col[1] * 0.9f, col[2] * 0.9f, 1); tv_push(b1, ns, tu, tv, col[0] * 0.72f, col[1] * 0.72f, col[2] * 0.72f, 1);
        tv_push(e0, ns, tu, tv, col[0] * 0.9f, col[1] * 0.9f, col[2] * 0.9f, 1); tv_push(b1, ns, tu, tv, col[0] * 0.72f, col[1] * 0.72f, col[2] * 0.72f, 1); tv_push(b0, ns, tu, tv, col[0] * 0.72f, col[1] * 0.72f, col[2] * 0.72f, 1); }
    if (spire) { Vec3 bot = v3(cx, y0, cz); for (int k = 0; k < 6; k++) { float a0 = p->yaw + k * 1.0472f, a1 = a0 + 1.0472f; Vec3 e0 = v3(cx + sinf(a0) * r, ym, cz + cosf(a0) * r), e1 = v3(cx + sinf(a1) * r, ym, cz + cosf(a1) * r); Vec3 nd = v3_norm(v3(sinf(a0 + 0.52f), 0.1f, cosf(a0 + 0.52f)));
        tv_push(bot, nd, tu, tv, col[0] * 0.8f, col[1] * 0.8f, col[2] * 0.8f, 1); tv_push(e0, nd, tu, tv, col[0] * 0.85f, col[1] * 0.85f, col[2] * 0.85f, 1); tv_push(e1, nd, tu, tv, col[0] * 0.85f, col[1] * 0.85f, col[2] * 0.85f, 1); } } }

static void tr_build_far(int c) {
    TChunk *T = &g_tc[c]; int i0 = (c % W_CHN) * TR_CELLS, j0 = (c / W_CHN) * TR_CELLS; const int Q = TR_CELLS / TR_FSTEP;
    int any = 0; for (int j = 0; j <= Q && !any; j++) for (int i = 0; i <= Q; i++) if (g_world.h[j0 + j * TR_FSTEP][i0 + i * TR_FSTEP] > W_CLOUDS - 6) { any = 1; break; }
    int np = 0; for (int k = g_world.pch[c]; k < g_world.pch[c + 1]; k++) { int kd = g_world.prop[k].kind; np += kd == P_TREE || kd == P_PALM || kd == P_SPIRE; }
    if (!any && !np) { T->far = NULL; g_tc_nfar++; return; }
    tv_begin((uint32_t)(Q * Q * 6 + 4 * Q * 12 + np * 60));
    static Vec3 P[TR_CELLS / TR_FSTEP + 1][TR_CELLS / TR_FSTEP + 1], Nn[TR_CELLS / TR_FSTEP + 1][TR_CELLS / TR_FSTEP + 1]; static float C[TR_CELLS / TR_FSTEP + 1][TR_CELLS / TR_FSTEP + 1][3];
    for (int j = 0; j <= Q; j++) for (int i = 0; i <= Q; i++) { int ii = i0 + i * TR_FSTEP, jj = j0 + j * TR_FSTEP; float x = -W_SIZE / 2 + W_CELL * ii, z = -W_SIZE / 2 + W_CELL * jj;
        P[j][i] = v3(x, g_world.h[jj][ii], z); Nn[j][i] = world_normal(&g_world, x, z); tr_base_colour(x, z, P[j][i].y, C[j][i]);
        float dw, rw; ground_layers(x, z, Nn[j][i].y, &dw, &rw);   /* the dirt and rock layers, mixed into the colour */
        static const float DIRT[3] = {0.78f, 0.66f, 0.5f}, ROCK[3] = {0.68f, 0.66f, 0.64f};
        for (int q = 0; q < 3; q++) C[j][i][q] = C[j][i][q] * (1 - dw * 0.7f - rw * 0.8f) + DIRT[q] * dw * 0.7f + ROCK[q] * rw * 0.8f; }
    for (int j = 0; j < Q; j++) for (int i = 0; i < Q; i++) {
        Vec3 p0 = P[j][i], p1 = P[j][i + 1], p2 = P[j + 1][i], p3 = P[j + 1][i + 1]; if (p0.y < W_CLOUDS - 6 && p1.y < W_CLOUDS - 6 && p2.y < W_CLOUDS - 6 && p3.y < W_CLOUDS - 6) continue;
        float s = 1.6f * TR_FSTEP, u0 = (i0 / TR_FSTEP + i) * s, v0 = (j0 / TR_FSTEP + j) * s, u1 = u0 + s, v1 = v0 + s;
        tv_push(p0, Nn[j][i], u0, v0, C[j][i][0], C[j][i][1], C[j][i][2], 1); tv_push(p2, Nn[j + 1][i], u0, v1, C[j + 1][i][0], C[j + 1][i][1], C[j + 1][i][2], 1); tv_push(p1, Nn[j][i + 1], u1, v0, C[j][i + 1][0], C[j][i + 1][1], C[j][i + 1][2], 1);
        tv_push(p1, Nn[j][i + 1], u1, v0, C[j][i + 1][0], C[j][i + 1][1], C[j][i + 1][2], 1); tv_push(p2, Nn[j + 1][i], u0, v1, C[j + 1][i][0], C[j + 1][i][1], C[j + 1][i][2], 1); tv_push(p3, Nn[j + 1][i + 1], u1, v1, C[j + 1][i + 1][0], C[j + 1][i + 1][1], C[j + 1][i + 1][2], 1); }
    if (!WK_ISLES(g_wk)) for (int e = 0; e < 4; e++) for (int t = 0; t < Q; t++) {   /* a deep skirt: far chunks meet near ones and each other without cracks */
        int ai, aj, bi, bj; if (e == 0) { ai = t; aj = 0; bi = t + 1; bj = 0; } else if (e == 1) { ai = t; aj = Q; bi = t + 1; bj = Q; } else if (e == 2) { ai = 0; aj = t; bi = 0; bj = t + 1; } else { ai = Q; aj = t; bi = Q; bj = t + 1; }
        tr_skirt(P[aj][ai], P[bj][bi], 9.0f, (P[aj][ai].x + P[aj][ai].z) * 0.13f, (P[bj][bi].x + P[bj][bi].z) * 0.13f, 8.0f, C[aj][ai], C[bj][bi]); }
    for (int k = g_world.pch[c]; k < g_world.pch[c + 1]; k++) { const Prop *p = &g_world.prop[k]; if (p->kind != P_TREE && p->kind != P_PALM && p->kind != P_SPIRE) continue;
        if (g_wk == WK_WATER && p->kind == P_TREE) continue;   /* kelp under the sea: nothing to see from far */
        tr_far_prop(p, world_height(&g_world, p->x, p->z)); }
    T->far = tv_mesh(); g_tc_nfar++; }

static void tr_drop_near(int c) { TChunk *T = &g_tc[c]; if (!g_tc_near[c]) return; tr_free_later(T->g); tr_free_later(T->dirt); tr_free_later(T->rock); T->g = T->dirt = T->rock = NULL; g_tc_near[c] = 0; g_tc_nnear--; }
static unsigned char g_tc_farok[W_NCH];
static void tr_drop_far(int c) { TChunk *T = &g_tc[c]; if (!g_tc_farok[c]) return; tr_free_later(T->far); T->far = NULL; g_tc_farok[c] = 0; g_tc_nfar--; }
static void terrain_reset(void) {   /* a new world: everything goes (the caller has waited for the GPU) */
    for (int c = 0; c < W_NCH; c++) { TChunk *T = &g_tc[c]; Mesh *ms[4] = {T->g, T->dirt, T->rock, T->far}; for (int k = 0; k < 4; k++) if (ms[k]) gfx_mesh_free(ms[k]); memset(T, 0, sizeof *T); }
    for (int i = 0; i < g_tfree_n; i++) gfx_mesh_free(g_tfree[i]); g_tfree_n = 0;
    memset(g_tc_near, 0, sizeof g_tc_near); memset(g_tc_farok, 0, sizeof g_tc_farok); g_tc_nnear = g_tc_nfar = 0; g_tc_ex = g_tc_ez = 1e9f; }
/* keep the chunks round (ex, ez) built: near ones in full, the rest out to the fog. budget = how many chunks may be built now
   (a big number right after arriving in a world, a few per frame while playing) */
static void terrain_update(float ex, float ez, int budget) {
    g_tframe++; tr_free_old(); g_tc_builds = 0; if (g_gfx_status != GFX_OK) return;
    g_tc_ex = ex; g_tc_ez = ez; float RF = tr_far_r();
    for (int pass = 0; pass < 2; pass++) {   /* pass 0: near chunks (they matter most), pass 1: far ones */
        int built = 0;
        while (built < budget) {
            int best = -1; float bd = 1e18f;
            for (int c = 0; c < W_NCH; c++) { float cx = -W_SIZE / 2 + (c % W_CHN + 0.5f) * W_CHS - ex, cz = -W_SIZE / 2 + (c / W_CHN + 0.5f) * W_CHS - ez, d2 = cx * cx + cz * cz;
                int wnear = d2 < TR_NEAR * TR_NEAR, wfar = d2 < RF * RF;
                if (pass == 0 && wnear && !g_tc_near[c] && d2 < bd) { bd = d2; best = c; }
                if (pass == 1 && wfar && !g_tc_farok[c] && d2 < bd) { bd = d2; best = c; } }
            if (best < 0) break;
            if (pass == 0) tr_build_near(best); else { tr_build_far(best); g_tc_farok[best] = 1; }
            if (pass == 0 && !g_tc_near[best]) g_tc_near[best] = 2;   /* nothing there (cloud gap): count it done */
            built++; g_tc_builds++; }
        budget -= built; if (budget <= 0) budget = pass == 0 ? 1 : 0; }   /* always at least one far chunk a frame */
    for (int c = 0; c < W_NCH; c++) { float cx = -W_SIZE / 2 + (c % W_CHN + 0.5f) * W_CHS - ex, cz = -W_SIZE / 2 + (c / W_CHN + 0.5f) * W_CHS - ez, d2 = cx * cx + cz * cz;
        if (g_tc_near[c] && d2 > (TR_NEAR + 70) * (TR_NEAR + 70)) { if (g_tc_near[c] == 2) g_tc_near[c] = 0; else tr_drop_near(c); }
        if (g_tc_farok[c] && d2 > (RF + 120) * (RF + 120)) tr_drop_far(c); } }
static int tr_visible(int c, Vec3 eye, float fx, float fz, float fl, float RF) {
    float cx = -W_SIZE / 2 + (c % W_CHN + 0.5f) * W_CHS - eye.x, cz = -W_SIZE / 2 + (c / W_CHN + 0.5f) * W_CHS - eye.z;
    return cx * cx + cz * cz < RF * RF && (fl < 1e-4f || cx * fx + cz * fz > -W_CHS * 0.75f); }   /* in range, and not behind the camera */
/* draw the ground: near chunks with their layers, far chunks where there is no near one. fx, fz: the camera's flat forward direction */
static void terrain_draw(Vec3 eye, float fx, float fz) {
    Mat4 id = m4_identity(); float fl = sqrtf(fx * fx + fz * fz); if (fl > 1e-4f) { fx /= fl; fz /= fl; }
    float RF = tr_far_r() + 120;
#define TR_VISIBLE(c) tr_visible(c, eye, fx, fz, fl, RF)
    gfx_state(ST_SOLID); gfx_bind(tx_grass); gfx_bump(3);
    WORLD_FOR_CHUNKS(eye.x, eye.z, RF, c) if (g_tc_near[c] == 1 && g_tc[c].g && TR_VISIBLE(c)) gfx_draw(g_tc[c].g, &id, 1, 1, 1, 1);
    gfx_bump(0);
    WORLD_FOR_CHUNKS(eye.x, eye.z, RF, c) if (g_tc_near[c] != 1 && g_tc[c].far && TR_VISIBLE(c)) gfx_draw(g_tc[c].far, &id, 1, 1, 1, 1);
    gfx_state(ST_TRANSPARENT);
    gfx_bind(tx_dirt); gfx_bump(3); WORLD_FOR_CHUNKS(eye.x, eye.z, TR_NEAR + 80, c) if (g_tc_near[c] == 1 && g_tc[c].dirt && TR_VISIBLE(c)) gfx_draw(g_tc[c].dirt, &id, 1, 1, 1, 1);
    gfx_bind(tx_rock); gfx_bump(4); WORLD_FOR_CHUNKS(eye.x, eye.z, TR_NEAR + 80, c) if (g_tc_near[c] == 1 && g_tc[c].rock && TR_VISIBLE(c)) gfx_draw(g_tc[c].rock, &id, 1, 1, 1, 1);
    gfx_bump(0); gfx_state(ST_SOLID);
#undef TR_VISIBLE
}
