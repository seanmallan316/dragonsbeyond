/* ---------- Multiplayer: side-view levels (included into game.c) ----------
   One or two players start as baby dragons and grow up together, walking, hopping and later flying along a
   straight lane through the same worlds, with the same robots and bosses, seen from the side like the classic
   platformers. Each level runs from x0 to x1 along z = lane; a boss level ends when the cage opens. */

typedef struct { int wk; float z, x0, x1; int boss; const char *name, *story; } SideLevel;
static const SideLevel SLV[] = {
    { WK_FOREST, -70,   88,  -92, 0, "LEVEL 1  -  THE BERRY PATH",
      "TWO BABY DRAGONS WAKE UP HUNGRY IN EVERLEAF WILDS. EAT EVERYTHING ON THE WAY (X) AND GROW UP TOGETHER!" },
    { WK_FOREST,   4,  -92,   92, 0, "LEVEL 2  -  ROBOT HOLLOW",
      "KAINE BLACKFEATHER'S ROBOTS PROWL THE WOODS. CLAW THEM (X), WHIP THEM (B) AND KEEP GROWING!" },
    { WK_FOREST,  60,   88,  -62, 1, "LEVEL 3  -  IRONHORN'S CLEARING",
      "A DRAGONET CRIES IN A CAGE. IRONHORN GUARDS IT: JUMP OVER ITS CHARGE SO IT HITS A TREE, THEN HIT IT!" },
    { WK_SKY,    -40,  -62,   58, 1, "LEVEL 4  -  THE SKY KINGDOM",
      "THE WIND CARRIES YOU UP TO THE SKY KINGDOM. FLY ACROSS THE CLOUDS (HOLD A) TO STORMTALON'S ISLAND!" } };
#define SLV_N (int)(sizeof SLV / sizeof SLV[0])

static float side_progress(float x) { const SideLevel *L = &SLV[g_lv]; return (x - L->x0) * g_lane_dir; }   /* metres along the level */

/* clear the lane and the strip between it and the camera: trees and rocks there move behind the lane (a backdrop);
   a third of the food moves onto the lane, the rest behind it */
static void side_clear_lane(void) {
    World *w = &g_world; float cs = g_lane_dir > 0 ? 1.0f : -1.0f;   /* the camera's side of the lane */
    for (int i = 0; i < w->nprop; i++) { Prop *p = &w->prop[i]; float rel = (p->z - g_lane_z) * cs;
        if (rel > -3.5f && rel < 24) { float nz = g_lane_z - cs * (5 + 20 * ui_hash(i * 13 + 7)); if (fabsf(nz) > W_SIZE / 2 - 4) nz = g_lane_z - cs * 5;
            if (w->kind == WK_SKY && world_is_void(w, p->x, nz)) p->x = 9999; else p->z = nz; } }
    int k = 0;
    for (int i = 0; i < w->nfood; i++) { Food *f = &w->food[i]; if (f->kind == F_FISH) continue;
        if ((k++ % 3) == 0 && !world_is_void(w, f->x, g_lane_z) && !world_in_pond(w, f->x, g_lane_z)) { f->z = g_lane_z; f->y = world_height(w, f->x, f->z); continue; }
        float rel = (f->z - g_lane_z) * cs;
        if (rel > -3.5f && rel < 24) { f->z = g_lane_z - cs * (6 + 12 * ui_hash(i * 5 + 3)); if (world_is_void(w, f->x, f->z)) f->x = 9999; else f->y = world_height(w, f->x, f->z); } } }

/* a dragon that ran out of hearts comes back beside the other player, or a little behind the furthest point reached */
static void dragon_respawn(Dragon *d) {
    if (!g_side) { d->x = 0; d->z = 0; d->y = world_height(&g_world, 0, 0); d->mode = DM_GROUND; return; }
    Dragon *o = d == &g_dragon ? &g_other : &g_dragon; float x;
    if (g_side_n > 1 && o->hearts > 0) x = o->x - g_lane_dir * 3;
    else { x = d->x - g_lane_dir * 6; if (side_progress(x) < 0) x = SLV[g_lv].x0; }
    if (g_world.kind == WK_SKY && world_is_void(&g_world, x, g_lane_z)) x = SLV[g_lv].x0;
    d->x = x; d->z = g_lane_z; d->y = world_height(&g_world, x, g_lane_z) + 0.2f; d->mode = DM_GROUND; d->vy = 0; d->speed = 0; d->dive = 0;
    d->hearts = fmaxf(d->hearts, 0.6f * d->max_hearts); d->invuln_t = 2.0f; }

static void side_place(Dragon *d, float x) {
    d->x = x; d->z = g_lane_z; d->y = world_height(&g_world, x, g_lane_z); d->mode = DM_GROUND; d->vy = 0; d->speed = 0; d->act_t = 0; d->anim = DA_IDLE; d->dive = 0;
    d->yaw = g_lane_dir > 0 ? 1.5707963f : -1.5707963f; }

static void side_start_level(int lv) {
    const SideLevel *L = &SLV[lv]; g_lv = lv; g_lane_z = L->z; g_lane_dir = L->x1 > L->x0 ? 1.0f : -1.0f;
    g_wrescued[L->wk] = 0;   /* the boss is back, the cage is shut */
    enter_world(L->wk, L->x0, L->z, g_lane_dir > 0 ? 1.5707963f : -1.5707963f);
    if (L->wk == WK_SKY) for (int i = 0; i < g_side_n; i++) { Dragon *d = i ? &g_other : &g_dragon;   /* up here you need wings */
        if (d->stage < ST_DRAGON) { d->stage = ST_DRAGON; d->growth = 0; d->max_hearts = fmaxf(d->max_hearts, 5); d->hearts = d->max_hearts; } }
    side_place(&g_dragon, L->x0); g_friend.on = g_side_n > 1;
    if (g_side_n > 1) { side_place(&g_other, L->x0 - g_lane_dir * 3); g_friend.x = g_other.x; g_friend.y = g_other.y; g_friend.z = g_other.z; g_friend.yaw = g_other.yaw; g_friend.anim = DA_IDLE; g_friend.mode = 0; }
    g_safe_x = L->x0; g_safe_z = L->z; g_cam_yaw = g_lane_dir > 0 ? 3.14159265f : 0; g_cam_mode = 0;
    g_eye = v3(L->x0, g_dragon.y + 4, L->z + (g_lane_dir > 0 ? 14 : -14)); g_at = v3(L->x0, g_dragon.y + 1, L->z);
    g_lv_t = 0; g_lv_done_t = -1; snprintf(g_msg, sizeof g_msg, "%s", L->story); g_msg_t = 6; g_kaine_t = -1; g_travel_t = -1; g_travel_cd = 1e9f; }

static void side_new_game(void) {
    g_side = 1; g_side_n = g_p2 >= 0 ? 2 : 1; g_diff = 1; g_swap_open = 0; g_tale_i = -1;
    g_kind = g_pick[0]; g_fk = g_pick[1];   /* g_other is that kind's record */
    for (int k = 0; k < K_N; k++) g_has[k] = k == g_kind || (g_side_n > 1 && k == g_fk);
    ensure_kind(g_kind, ST_BEYONDER); ensure_kind(g_fk, ST_BEYONDER);
    dragon_init(&g_dragon, &g_world, 0, 0); dragon_init(&g_other, &g_world, 0, 0);
    side_start_level(0); g_state = G_PLAY; g_title_t = 0; }

/* the side camera: from the camera side of the lane, framing both players, a little ahead of where they go */
static void side_camera(float dt) {
    const Dragon *a = &g_dragon; float L = dragon_length(a->stage);
    float cx = a->x, cy = a->y, spread = 0;
    if (g_side_n > 1) { cx = 0.5f * (a->x + g_other.x); cy = 0.5f * (a->y + g_other.y); spread = fabsf(a->x - g_other.x) + 0.5f * fabsf(a->y - g_other.y); }
    float cs = g_lane_dir > 0 ? 1.0f : -1.0f, lead = g_lane_dir * (2.5f + L);
    float dist = 2.6f + 1.6f * L + spread * 0.55f, up = 1.1f + 0.45f * L + spread * 0.12f;
    Vec3 at = v3(cx + lead * 0.6f, cy + 0.12f * L, g_lane_z), want = v3(at.x, cy + up, g_lane_z + cs * dist);
    for (int i = 1; i <= 6; i++) { float u = i / 6.0f; float h = world_height(&g_world, at.x, g_lane_z + cs * dist * u) + 1.0f; float yy = at.y + (want.y - at.y) * u;   /* hills between: look over them */
        if (h > yy) want.y += (h - yy) / u; }
    float kk = 1 - expf(-dt * 4), ka = 1 - expf(-dt * 5);
    g_eye = v3(g_eye.x + (want.x - g_eye.x) * kk, g_eye.y + (want.y - g_eye.y) * kk, g_eye.z + (want.z - g_eye.z) * kk);
    g_at = v3(g_at.x + (at.x - g_at.x) * ka, g_at.y + (at.y - g_at.y) * ka, g_at.z + (at.z - g_at.z) * ka);
    g_cam_yaw = g_lane_dir > 0 ? 3.14159265f : 0;
#ifndef __wiiu__
    if (getenv("DB_CAMDBG") && fmodf(g_time, 1.0f) < dt) printf("cam eye %.1f %.1f %.1f at %.1f %.1f %.1f  want %.1f %.1f %.1f L %.2f\n", g_eye.x, g_eye.y, g_eye.z, g_at.x, g_at.y, g_at.z, want.x, want.y, want.z, L);
#endif
    }

/* every step: everyone stays on the lane, food eaten by either player grows both, nobody gets left behind, and the level ends */
static void side_frame(float dt) {
    const SideLevel *L = &SLV[g_lv]; World *w = &g_world;
    g_lv_t += dt;
    g_dragon.z = g_lane_z; if (g_side_n > 1) g_other.z = g_lane_z;
    for (int i = 0; i < w->ndrone; i++) { w->drone[i].z = g_lane_z; w->drone[i].hz = g_lane_z; }
    for (int i = 0; i < w->nhound; i++) { w->hound[i].z = g_lane_z; w->hound[i].hz = g_lane_z; }
    for (int i = 0; i < w->nthief; i++) { w->thief[i].z = g_lane_z; w->thief[i].hz = g_lane_z; }
    for (int i = 0; i < w->nfood; i++) if (fabsf(w->food[i].z - g_lane_z) < 1.5f) w->food[i].z = g_lane_z;
    if (L->boss) { g_boss.z = g_lane_z; g_boss.vz = 0; g_boss.cz = 0; g_boss.cx = g_boss.cx >= 0 ? 1 : -1; }
    {   /* growing together: what one eats, both get (a bit more than in story mode: levels are short) */
        static float pg1, pg2; static int ps1 = -1, ps2 = -1;
        if (g_dragon.stage != ps1 || (g_side_n > 1 && g_other.stage != ps2)) { pg1 = g_dragon.growth; pg2 = g_other.growth; ps1 = g_dragon.stage; ps2 = g_other.stage; }
        float d1 = fmaxf(0, g_dragon.growth - pg1), d2 = g_side_n > 1 ? fmaxf(0, g_other.growth - pg2) : 0, gain = (d1 + d2) * 1.6f;
        g_dragon.growth += gain - d1; if (g_side_n > 1) { g_other.growth += gain - d2; pg2 = g_other.growth; } pg1 = g_dragon.growth; }
    if (g_side_n > 1) {   /* the one left behind pops up beside the leader */
        float gap = fabsf(g_dragon.x - g_other.x); if (gap > 26) { Dragon *back = side_progress(g_dragon.x) < side_progress(g_other.x) ? &g_dragon : &g_other, *front = back == &g_dragon ? &g_other : &g_dragon;
            back->x = front->x - g_lane_dir * 3; back->y = fmaxf(front->y, world_height(w, back->x, g_lane_z)); back->mode = front->mode == DM_AIR ? DM_AIR : DM_GROUND; back->vy = 0;
            snprintf(g_msg, sizeof g_msg, "STICK TOGETHER, LITTLE DRAGONS!"); g_msg_t = 2; } }
    if (g_lv_done_t < 0) {
        float lead = side_progress(g_dragon.x); if (g_side_n > 1) lead = fmaxf(lead, side_progress(g_other.x));
        int done = L->boss ? g_rescued : lead >= fabsf(L->x1 - L->x0);
        if (done) { g_lv_done_t = 0; sfx_play(SFX_WIN, 0.9f, 0); platform_rumble(0.8f, 0.6f);
            snprintf(g_msg, sizeof g_msg, L->boss ? "%s IS FREE! LEVEL CLEAR!" : "LEVEL CLEAR!", L->boss ? kind_name(CAPTIVE[L->wk]) : ""); g_msg_t = 5; } }
    else { g_lv_done_t += dt;
        if (g_lv_done_t > 5) { if (g_lv + 1 < SLV_N) side_start_level(g_lv + 1);
            else if (g_lv_done_t < 5 + dt * 1.5f) { snprintf(g_msg, sizeof g_msg, "THE END... FOR NOW! MORE WORLDS ARE COMING. THANKS FOR PLAYING!"); g_msg_t = 10; }
            else if (g_lv_done_t > 15) { g_side = 0; g_p2 = -1; g_friend.on = 0; g_state = G_TITLE; g_state_t = 0; } } } }

/* the finish: a gate of two glowing stone posts across the lane, with a golden beam between (boss levels end at the cage instead) */
static void side_draw_goal(void) {
    const SideLevel *L = &SLV[g_lv]; if (L->boss) return; float x = L->x1, gy = world_height(&g_world, x, g_lane_z), p = 0.75f + 0.25f * sinf(g_time * 4);
    for (int k = -1; k <= 1; k += 2) { Vec3 b = v3(x, gy - 0.2f, g_lane_z + k * 2.2f); batch_cyl(b, v3(b.x, gy + 5.5f, b.z), 0.42f, 0.32f, 8, 0.55f, 0.5f, 0.42f); batch_ball(v3(b.x, gy + 5.8f, b.z), 0.55f, 0.55f, 0.55f, 5, 8, 1.0f * p, 0.85f * p, 0.35f * p, 0); }
    batch_cyl(v3(x, gy + 5.2f, g_lane_z - 2.2f), v3(x, gy + 5.2f, g_lane_z + 2.2f), 0.18f, 0.18f, 6, 1.0f * p, 0.8f * p, 0.3f * p);
    for (int i = 0; i < 6; i++) { float a = g_time * 1.5f + i; batch_ball(v3(x + 0.3f * sinf(a * 2), gy + 0.8f + fmodf(g_time * 1.2f + i * 0.9f, 4.5f), g_lane_z + 1.6f * sinf(a)), 0.12f, 0.12f, 0.12f, 3, 5, 1, 0.9f, 0.5f, 0); } }

/* the HUD's level bar: start, finish flag, a dot for each player */
static void side_hud(void) {
    const SideLevel *L = &SLV[g_lv]; float x0 = 480, x1 = 800, y = 150, len = fabsf(L->x1 - L->x0);
    ui_bar(x0, y, x1 - x0, 8, 0, 0.3f, 0.3f, 0.3f);
    map_tri2d(x1 + 2, y - 26, x1 + 26, y - 18, x1 + 2, y - 10, 1, 0.85f, 0.3f, 1); ui_rect(&g_ub, x1, y - 26, 3, 34, 0.9f, 0.85f, 0.7f, 1);   /* flag */
    float p1 = clampf(side_progress(g_dragon.x) / len, 0, 1); map_disc(x0 + p1 * (x1 - x0), y + 4, 9, 12, 0.35f, 1.0f, 0.45f, 1);
    if (g_side_n > 1) { float p2 = clampf(side_progress(g_other.x) / len, 0, 1); map_disc(x0 + p2 * (x1 - x0), y + 4, 7, 12, 0.4f, 0.75f, 1.0f, 1); }
    text_c(L->name, y + 16, 15, 1, 0.92f, 0.75f, 0.9f);
    if (g_lv_t < 4) { float a = clampf(g_lv_t * 2, 0, 1) * clampf((4 - g_lv_t) * 1.5f, 0, 1); ui_panel(340, 250, 600, 110, a, 1); text_cg(L->name, 290, 34, a); } }
