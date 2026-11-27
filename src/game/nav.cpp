/* Recast & Detour baked into the game in one file, so every build (Wii U, Android, PC tests) only adds this one C++ file. */
#include "../third_party/recast/RecastAlloc.cpp"
#include "../third_party/recast/RecastAssert.cpp"
#include "../third_party/recast/Recast.cpp"
#include "../third_party/recast/RecastArea.cpp"
#include "../third_party/recast/RecastContour.cpp"
#include "../third_party/recast/RecastFilter.cpp"
#include "../third_party/recast/RecastLayers.cpp"
#include "../third_party/recast/RecastRasterization.cpp"
#include "../third_party/recast/RecastRegion.cpp"
#include "../third_party/recast/DetourAlloc.cpp"
#include "../third_party/recast/DetourAssert.cpp"
#include "../third_party/recast/DetourCommon.cpp"
#include "../third_party/recast/DetourNavMesh.cpp"
#include "../third_party/recast/DetourNavMeshBuilder.cpp"
#include "../third_party/recast/DetourNavMeshQuery.cpp"
#include "../third_party/recast/DetourNode.cpp"
#include <stdio.h>
#include <math.h>
extern "C" {
#include "world.h"
}
#include "nav.h"

static dtNavMesh *g_nm; static dtNavMeshQuery *g_q; static dtQueryFilter g_filter;
#define NAV_IDS 32
static struct { int age; float tx, tz, cx, cz; int ok; } g_cache[NAV_IDS];

extern "C" void nav_clear(void) { if (g_q) dtFreeNavMeshQuery(g_q); if (g_nm) dtFreeNavMesh(g_nm); g_q = 0; g_nm = 0; memset(g_cache, 0, sizeof g_cache); }
extern "C" int nav_ready(void) { return g_q != 0; }

extern "C" int nav_build(const World *w) {
    nav_clear();
    const int G = W_GRID; const float S = W_SIZE, step = S / (G - 1);
    /* the ground as triangles; leave out the cloud sea under the sky islands and everything under water */
    float water = w->sea_on ? w->sea - 0.4f : -1e9f;
    float *verts = (float *)malloc(sizeof(float) * 3 * G * G); int *tris = (int *)malloc(sizeof(int) * 6 * (G - 1) * (G - 1)); int nt = 0;
    if (!verts || !tris) { free(verts); free(tris); return 0; }
    for (int j = 0; j < G; j++) for (int i = 0; i < G; i++) { float *v = verts + 3 * (j * G + i); v[0] = -S / 2 + step * i; v[1] = w->h[j][i]; v[2] = -S / 2 + step * j; }
    for (int j = 0; j < G - 1; j++) for (int i = 0; i < G - 1; i++) {
        int a = j * G + i, b = a + 1, c = a + G, d = c + 1; float hs[4] = { w->h[j][i], w->h[j][i + 1], w->h[j + 1][i], w->h[j + 1][i + 1] };
        float mn = fminf(fminf(hs[0], hs[1]), fminf(hs[2], hs[3])), mx = fmaxf(fmaxf(hs[0], hs[1]), fmaxf(hs[2], hs[3]));
        if (mn < W_CLOUDS + 2 || mx < water) continue;
        if (w->pond_r > 0) { float cx = -S / 2 + step * (i + 0.5f) - w->pond_x, cz = -S / 2 + step * (j + 0.5f) - w->pond_z; if (cx * cx + cz * cz < w->pond_r * w->pond_r * 1.1f && mx < w->pond_y + 0.2f) continue; }
        tris[nt * 3] = a; tris[nt * 3 + 1] = c; tris[nt * 3 + 2] = b; nt++; tris[nt * 3] = b; tris[nt * 3 + 1] = c; tris[nt * 3 + 2] = d; nt++; }
    rcConfig cfg; memset(&cfg, 0, sizeof cfg);
    cfg.cs = 0.8f; cfg.ch = 0.3f; cfg.walkableSlopeAngle = 42; cfg.walkableHeight = 5; cfg.walkableClimb = 3; cfg.walkableRadius = 1;
    cfg.maxEdgeLen = 30; cfg.maxSimplificationError = 1.6f; cfg.minRegionArea = 64; cfg.mergeRegionArea = 400; cfg.maxVertsPerPoly = 6;
    cfg.detailSampleDist = 0; cfg.detailSampleMaxError = 1;
    rcCalcBounds(verts, G * G, cfg.bmin, cfg.bmax); cfg.bmin[1] -= 2; cfg.bmax[1] += 12;
    rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);
    rcContext ctx(false); int ok = 0;
    rcHeightfield *hf = rcAllocHeightfield(); rcCompactHeightfield *chf = 0; rcContourSet *cset = 0; rcPolyMesh *pm = 0;
    unsigned char *areas = (unsigned char *)calloc(nt ? nt : 1, 1); unsigned char *data = 0; int dsize = 0;
    do {
        if (!hf || !areas || !rcCreateHeightfield(&ctx, *hf, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch)) break;
        rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, verts, G * G, tris, nt, areas);
        if (!rcRasterizeTriangles(&ctx, verts, G * G, tris, areas, nt, *hf, cfg.walkableClimb)) break;
        rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *hf); rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf); rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *hf);
        chf = rcAllocCompactHeightfield(); if (!chf || !rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf, *chf)) break;
        rcFreeHeightField(hf); hf = 0;
        if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf)) break;
        /* the scenery: every solid tree, rock, crystal and spire cuts a round hole (plus room for a body) */
        for (int i = 0; i < w->nprop; i++) { float r = world_prop_radius(w, &w->prop[i]); if (r <= 0) continue;
            float p[3] = { w->prop[i].x, world_height(w, w->prop[i].x, w->prop[i].z) - 2, w->prop[i].z };
            rcMarkCylinderArea(&ctx, p, r + 0.7f, 14, RC_NULL_AREA, *chf); }
        if (w->arena_r > 0 && w->cage_x != 0) { float p[3] = { w->cage_x, w->cage_y - 2, w->cage_z }; rcMarkCylinderArea(&ctx, p, 3.2f, 12, RC_NULL_AREA, *chf); }
        if (!rcBuildDistanceField(&ctx, *chf) || !rcBuildRegions(&ctx, *chf, 0, cfg.minRegionArea, cfg.mergeRegionArea)) break;
        cset = rcAllocContourSet(); if (!cset || !rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset)) break;
        pm = rcAllocPolyMesh(); if (!pm || !rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *pm)) break;
        /* (no detail mesh: Detour uses the polygons' own corners for heights, which is plenty for steering) */
        if (pm->nverts >= 0xffff || pm->npolys == 0) break;
        for (int i = 0; i < pm->npolys; i++) pm->flags[i] = pm->areas[i] == RC_WALKABLE_AREA ? 1 : 0;
        dtNavMeshCreateParams p; memset(&p, 0, sizeof p);
        p.verts = pm->verts; p.vertCount = pm->nverts; p.polys = pm->polys; p.polyAreas = pm->areas; p.polyFlags = pm->flags; p.polyCount = pm->npolys; p.nvp = pm->nvp;
        p.walkableHeight = cfg.walkableHeight * cfg.ch; p.walkableRadius = cfg.walkableRadius * cfg.cs; p.walkableClimb = cfg.walkableClimb * cfg.ch;
        rcVcopy(p.bmin, pm->bmin); rcVcopy(p.bmax, pm->bmax); p.cs = cfg.cs; p.ch = cfg.ch; p.buildBvTree = true;
        if (!dtCreateNavMeshData(&p, &data, &dsize)) break;
        g_nm = dtAllocNavMesh(); if (!g_nm || dtStatusFailed(g_nm->init(data, dsize, DT_TILE_FREE_DATA))) { dtFree(data); break; }
        g_q = dtAllocNavMeshQuery(); if (!g_q || dtStatusFailed(g_q->init(g_nm, 2048))) break;
        g_filter.setIncludeFlags(1); g_filter.setExcludeFlags(0);
        ok = 1; fprintf(stderr, "nav: %d polygons for world %d\n", pm->npolys, w->kind);
    } while (0);
    rcFreeHeightField(hf); rcFreeCompactHeightfield(chf); rcFreeContourSet(cset); rcFreePolyMesh(pm); free(areas); free(verts); free(tris);
    if (!ok) { fprintf(stderr, "nav: no walkable mesh for world %d\n", w->kind); nav_clear(); }
    return ok; }

extern "C" int nav_steer(int id, float sx, float sy, float sz, float tx, float ty, float tz, float *ox, float *oz) {
    if (!g_q || id < 0 || id >= NAV_IDS) return 0;
    if (g_cache[id].age > 0 && g_cache[id].ok >= 0) {   /* follow the last path for a few frames while the target has not moved far */
        float dx = tx - g_cache[id].tx, dz = tz - g_cache[id].tz, cx = g_cache[id].cx - sx, cz = g_cache[id].cz - sz;
        if (dx * dx + dz * dz < 4 && cx * cx + cz * cz > 0.8f) { g_cache[id].age--; if (!g_cache[id].ok) return 0; *ox = g_cache[id].cx; *oz = g_cache[id].cz; return 1; } }
    const float ext[3] = { 3, 8, 3 }; float s[3] = { sx, sy, sz }, t[3] = { tx, ty, tz }, sp[3], tp[3];
    dtPolyRef sr = 0, tr = 0; g_cache[id].age = 10; g_cache[id].tx = tx; g_cache[id].tz = tz; g_cache[id].ok = 0;
    if (dtStatusFailed(g_q->findNearestPoly(s, ext, &g_filter, &sr, sp)) || !sr) return 0;
    if (dtStatusFailed(g_q->findNearestPoly(t, ext, &g_filter, &tr, tp)) || !tr) return 0;
    dtPolyRef path[128]; int np = 0; if (dtStatusFailed(g_q->findPath(sr, tr, sp, tp, &g_filter, path, &np, 128)) || np == 0) return 0;
    if (path[np - 1] != tr) g_q->closestPointOnPoly(path[np - 1], tp, tp, 0);   /* cannot get all the way: go as near as it can */
    float st[3 * 4]; int ns = 0; g_q->findStraightPath(sp, tp, path, np, st, 0, 0, &ns, 4);
    if (ns < 2) return 0;
    g_cache[id].cx = st[3]; g_cache[id].cz = st[5]; g_cache[id].ok = 1; *ox = st[3]; *oz = st[5]; return 1; }
