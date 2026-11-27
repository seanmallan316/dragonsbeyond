// world_test.c - the Beyond Expansion invariants (design/plan.md section 7), for all eight worlds:
// 3.6 km across, the boss clearing far from home, every goal on solid ground, nothing inside the clearing,
// props and food sorted by chunk, and enough of everything to fill the space.
#include "../src/game/world.h"
#include <stdio.h>
#include <math.h>
#include <time.h>
static World w;
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL [%s] line %d: %s  ", NAMES[k], __LINE__, #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)
static const char *NAMES[WK_N] = {"forest", "sky", "snow", "crystal", "sand", "water", "lava", "keep"};
static int solid(const World *w, float x, float z) {
    if (WK_ISLES(w->kind) && world_is_void(w, x, z)) return 0;
    if (world_in_pond(w, x, z)) return 0;
    if (w->sea_on && w->kind != WK_WATER && world_height(w, x, z) < w->sea) return 0;
    return 1; }
int main(void) {
    for (int k = 0; k < WK_N; k++) {
        clock_t t0 = clock(); world_make_kind(&w, 2026 + k * 101, k); double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        float ad = sqrtf(w.arena_x * w.arena_x + w.arena_z * w.arena_z);
        printf("%-8s built in %4.0f ms: %6d props, %5d food, %2d drones, %2d hounds, %2d thieves, %2d lakes, %3d isles, arena %4.0f m away\n",
               NAMES[k], ms, w.nprop, w.nfood, w.ndrone, w.nhound, w.nthief, w.nlake, w.nisle, ad);
        /* 2: the size, and the journey */
        CHECK(W_SIZE >= 3600, "size %.0f", W_SIZE);
        CHECK(ad >= 700, "arena only %.0f m from the start", ad);
        /* 3: goals on solid ground */
        CHECK(solid(&w, 0, 0), "start meadow");
        CHECK(solid(&w, w.arena_x, w.arena_z), "boss clearing");
        CHECK(solid(&w, w.crystal_x, w.crystal_z) || WK_ISLES(k), "egg crystal at %.0f,%.0f", w.crystal_x, w.crystal_z);
        for (int i = 0; i < W_NSTONES; i++) CHECK(solid(&w, w.stone[i].x, w.stone[i].z), "story stone %d at %.0f,%.0f", i, w.stone[i].x, w.stone[i].z);
        for (int i = 0; i < W_NTOWERS; i++) CHECK(solid(&w, w.tower[i].x, w.tower[i].z), "signal tower %d at %.0f,%.0f", i, w.tower[i].x, w.tower[i].z);
        for (int i = 0; i < W_NTOWERS; i++) { float d = sqrtf(w.tower[i].x * w.tower[i].x + w.tower[i].z * w.tower[i].z); CHECK(d > 300, "tower %d only %.0f m out", i, d); }
        /* the goals are spread over the world, not bunched up */
        { float far = 0; for (int i = 0; i < W_NSTONES; i++) far = fmaxf(far, sqrtf(w.stone[i].x * w.stone[i].x + w.stone[i].z * w.stone[i].z)); CHECK(far > 700, "farthest stone %.0f m", far); }
        /* 4: nothing in the clearing but its ring */
        CHECK(w.narena_tree >= 5, "ring trees %d", w.narena_tree);
        for (int i = 0; i < w.narena_tree; i++) { const Prop *p = &w.prop[w.arena_tree[i]]; float dx = p->x - w.arena_x, dz = p->z - w.arena_z; CHECK(fabsf(sqrtf(dx * dx + dz * dz) - w.arena_r) < 4, "ring tree %d is %.1f m from the middle", i, sqrtf(dx * dx + dz * dz)); }
        { int bad = 0; for (int i = 0; i < w.nprop; i++) { int ring = 0; for (int r = 0; r < w.narena_tree; r++) ring |= w.arena_tree[r] == i; float dx = w.prop[i].x - w.arena_x, dz = w.prop[i].z - w.arena_z; if (!ring && dx * dx + dz * dz < w.arena_r * w.arena_r) bad++; } CHECK(bad == 0, "%d props in the clearing", bad); }
        { int bad = 0; for (int i = 0; i < w.nfood; i++) { float dx = w.food[i].x - w.arena_x, dz = w.food[i].z - w.arena_z; if (dx * dx + dz * dz < w.arena_r * w.arena_r) bad++; } CHECK(bad == 0, "%d food in the clearing", bad); }
        /* the chunk index really is sorted, and covers everything */
        { int bad = 0; for (int c = 0; c < W_NCH; c++) { for (int i = w.pch[c]; i < w.pch[c + 1]; i++) if (world_chunk_of(w.prop[i].x, w.prop[i].z) != c) bad++; } CHECK(bad == 0 && w.pch[W_NCH] == w.nprop, "%d props filed in the wrong chunk", bad); }
        { int bad = 0; for (int c = 0; c < W_NCH; c++) { for (int i = w.fch[c]; i < w.fch[c + 1]; i++) if (world_chunk_of(w.food[i].x, w.food[i].z) != c || w.food[i].ch != c) bad++; } CHECK(bad == 0 && w.fch[W_NCH] == w.nfood, "%d food filed in the wrong chunk", bad); }
        /* enough to fill a world 100x bigger (the old patch had 1100 props and 340 food) */
        CHECK(w.nprop > (WK_ISLES(k) ? 8000 : 20000), "only %d props", w.nprop);
        CHECK(w.nfood > (WK_ISLES(k) ? 3000 : 5000), "only %d food", w.nfood);
        CHECK(w.ndrone > 30, "only %d drones", w.ndrone);
        if (!WK_ISLES(k)) CHECK(w.nhound > 20, "only %d hounds", w.nhound);
        /* food is near the start (the first minute teaches eating) */
        CHECK(world_nearest_food(&w, 0, 0, 12) >= 0, "no food by the start");
        /* the far mountains: the ground rises at the edge of the world */
        if (!WK_ISLES(k)) CHECK(world_height(&w, W_SIZE * 0.48f, 0) > world_height(&w, 0, 0) + 30, "no wall of hills at the edge");
        /* towers: knock one down and its robots switch off for good */
        { int camp = 0; for (int i = 0; i < w.ndrone; i++) camp += w.drone[i].tower == 0; CHECK(camp > 0, "tower 0 has no robots");
          Attack a = {w.tower[0].x, w.tower[0].y + 2, w.tower[0].z, 2.0f}; int r1 = world_hit_tower(&w, a, 0.4f), r2 = world_hit_tower(&w, a, 0.4f), r3 = world_hit_tower(&w, a, 0.4f);
          CHECK(r1 <= -2 && r2 <= -2 && r3 == 0, "tower hits %d %d %d", r1, r2, r3);
          int on = 0; for (int i = 0; i < w.ndrone; i++) if (w.drone[i].tower == 0 && !w.drone[i].off) on++; CHECK(on == 0, "%d of tower 0's drones still on", on);
          CHECK(world_towers_down(&w) == 1, "towers down %d", world_towers_down(&w)); }
        /* regions: the start is the start meadow, the clearing is the clearing */
        CHECK(world_region_at(&w, 0, 0) == 0 && world_region_at(&w, w.arena_x, w.arena_z) == 1, "regions");
        if (WK_ISLES(k)) {   /* every island can be walked to from home: the bridges join them all */
            int seen[W_MAXISLES] = {1}, grew = 1; while (grew) { grew = 0; for (int b = 0; b < w.nbridge; b++) { int a = w.bridge[b][0], c = w.bridge[b][1]; if (seen[a] != seen[c]) { seen[a] = seen[c] = 1; grew = 1; } } }
            int lost = 0; for (int i = 0; i < w.nisle; i++) lost += !seen[i]; CHECK(lost == 0 && w.nisle > 40, "%d of %d islands cut off", lost, w.nisle); }
    }
    if (fails) { printf("world test FAILED (%d)\n", fails); return 1; }
    printf("world test OK\n"); return 0; }
