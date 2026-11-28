/* treasure.inc - secrets, side quests and dragon armor. Included by game.c (after fx.inc).
   Every world hides 4 treasure chests (one on the highest hill, one under a big boulder you must smash, one far out at the
   world's edge, one by the water) and 5 lost dragon eggs. Two side quests per world: find all 5 eggs, and knock down 12 of
   Kaine's robots. Chests and quests give dragon armor (helm, scale mail, wing guards, tail blade: bronze, then silver, then
   gold), heart crystals (+1 heart for every dragon), power runes and piles of gems. */
#define TR_CHESTS 4
#define TR_EGGS 5
#define TR_ROBOTS 12
enum { AR_HELM = 0, AR_MAIL, AR_WINGS, AR_BLADE, AR_N };
static const char *AR_NAME[AR_N] = {"HELM", "SCALE MAIL", "WING GUARDS", "TAIL BLADE"};
static const char *AR_TIER[4] = {"", "BRONZE", "SILVER", "GOLD"};
static const char *AR_WHAT[AR_N] = {"YOU TAKE LESS DAMAGE", "HEARTS SLOWLY COME BACK", "MORE STAMINA FOR FLYING AND DODGING", "YOUR CLAWS AND TAIL HIT HARDER"};
static int g_tfound[WK_N];     /* bits 0-3: chests opened, bits 4-8: eggs found */
static int g_trobots[WK_N];    /* robots knocked down in each world (for its quest) */
static int g_tquest[WK_N];     /* bit 0: the egg quest's reward given, bit 1: the robot quest's */
static int g_armor[AR_N];      /* 0 none, 1 bronze, 2 silver, 3 gold */
typedef struct { float x, y, z; int rock; } TSpot;   /* rock: index of the boulder sitting on it (-1 = none) */
static TSpot g_chest[TR_CHESTS], g_egg[TR_EGGS]; static int g_tr_wk = -1; static float g_tr_open_t[TR_CHESTS];
static unsigned g_trs;
static float tr_rand(void) { g_trs = g_trs * 1664525u + 1013904223u; return (float)((g_trs >> 8) & 0xFFFFFF) / 16777216.0f; }
static int tr_ok(float x, float z) {   /* solid, dry, not in the boss clearing, not on a steep cliff */
    if (fabsf(x) > W_SIZE / 2 - 10 || fabsf(z) > W_SIZE / 2 - 10) return 0;
    if (WK_ISLES(g_world.kind) && world_is_void(&g_world, x, z)) return 0;
    float ax = x - g_world.arena_x, az = z - g_world.arena_z; if (ax * ax + az * az < (g_world.arena_r + 6) * (g_world.arena_r + 6)) return 0;
    if (x * x + z * z < 30 * 30) return 0;
    return world_normal(&g_world, x, z).y > 0.8f; }
static void tr_place(void) {
    g_tr_wk = g_wk; g_trs = 777u + 131u * (unsigned)g_wk; float S = W_SIZE / 2;
    for (int i = 0; i < TR_CHESTS; i++) { g_chest[i].rock = -1; g_tr_open_t[i] = (g_tfound[g_wk] >> i) & 1 ? 9 : -1; }
    /* 0: the highest ground */
    { float best = -1e9f, bx = 60, bz = 60; for (int t = 0; t < 900; t++) { float x = (tr_rand() - 0.5f) * 2 * S * 0.9f, z = (tr_rand() - 0.5f) * 2 * S * 0.9f; if (!tr_ok(x, z)) continue; float h = world_height(&g_world, x, z); if (h > best) { best = h; bx = x; bz = z; } }
      g_chest[0].x = bx; g_chest[0].z = bz; }
    /* 1: under a boulder (a big rock prop; one is made if the world has none far enough out) */
    { int pick = -1; for (int t = 0; t < 400 && pick < 0; t++) { int i = (int)(tr_rand() * g_world.nprop) % (g_world.nprop > 0 ? g_world.nprop : 1); const Prop *p = &g_world.prop[i];
          if (p->kind == P_ROCK && p->x * p->x + p->z * p->z > 70 * 70 && tr_ok(p->x, p->z)) pick = i; }
      if (pick < 0 && g_world.nprop < W_MAXPROPS) { for (int t = 0; t < 300; t++) { float a = tr_rand() * 6.2831853f, r = S * (0.4f + 0.4f * tr_rand()), x = cosf(a) * r, z = sinf(a) * r; if (!tr_ok(x, z)) continue;
              Prop *p = &g_world.prop[g_world.nprop]; memset(p, 0, sizeof *p); p->kind = P_ROCK; p->x = x; p->z = z; p->yaw = a; pick = g_world.nprop++; break; } }
      if (pick >= 0) { Prop *p = &g_world.prop[pick]; p->scale = 1.45f; g_chest[1].x = p->x; g_chest[1].z = p->z; g_chest[1].rock = pick; } else { g_chest[1].x = -80; g_chest[1].z = 80; } }
    /* 2: far out at the edge of the world */
    for (int t = 0; t < 400; t++) { float a = tr_rand() * 6.2831853f, r = S * (0.78f + 0.12f * tr_rand()), x = cosf(a) * r, z = sinf(a) * r; g_chest[2].x = x; g_chest[2].z = z; if (tr_ok(x, z)) break; }
    /* 3: by the water (the pond's shore; out on the sea bed in the water world; a small far island in the sky) */
    for (int t = 0; t < 400; t++) { float x, z;
        if (g_world.sea_on) { float a = tr_rand() * 6.2831853f, r = 70 + 60 * tr_rand(); x = cosf(a) * r; z = sinf(a) * r; }
        else { float a = tr_rand() * 6.2831853f, r = g_world.pond_r + 2 + 4 * tr_rand(); x = g_world.pond_x + cosf(a) * r; z = g_world.pond_z + sinf(a) * r; }
        g_chest[3].x = x; g_chest[3].z = z; if (tr_ok(x, z) && !world_in_pond(&g_world, x, z)) break; }
    for (int i = 0; i < TR_CHESTS; i++) g_chest[i].y = world_height(&g_world, g_chest[i].x, g_chest[i].z);
    /* the eggs: tucked by tree trunks and rocks, on hilltops, and out on the edges */
    for (int e = 0; e < TR_EGGS; e++) { float x = 0, z = 0;
        for (int t = 0; t < 600; t++) {
            if (e < 3 && g_world.nprop > 0) { const Prop *p = &g_world.prop[(int)(tr_rand() * g_world.nprop) % g_world.nprop]; if (p->kind == P_SPIRE) continue; float a = tr_rand() * 6.2831853f; x = p->x + cosf(a) * 1.6f; z = p->z + sinf(a) * 1.6f;
                if (x * x + z * z < (55 + 35 * e) * (55 + 35 * e)) continue; }
            else { float a = tr_rand() * 6.2831853f, r = S * (0.35f + 0.5f * tr_rand()); x = cosf(a) * r; z = sinf(a) * r; }
            if (!tr_ok(x, z) || world_in_pond(&g_world, x, z)) continue;
            int far = 1; for (int k = 0; k < e; k++) { float dx = g_egg[k].x - x, dz = g_egg[k].z - z; if (dx * dx + dz * dz < 50 * 50) far = 0; } if (far) break; }
        g_egg[e].x = x; g_egg[e].z = z; g_egg[e].y = world_height(&g_world, x, z); g_egg[e].rock = -1; } }
static int armor_total(void) { int n = 0; for (int i = 0; i < AR_N; i++) n += g_armor[i]; return n; }
/* the next piece of armor (helm, mail, wings, blade in turn; bronze first): returns its slot, or -1 once all of it is gold */
static int armor_next(void) { for (int t = 1; t <= 3; t++) for (int i = 0; i < AR_N; i++) if (g_armor[i] < t) { g_armor[i] = t; return i; } return -1; }
static void tr_reward(int kind, const char *why) {   /* kind 0: armor, 1: heart crystal, 2: dragon gems (growth), 3: power rune */
    int a = kind == 0 ? armor_next() : -1; if (kind == 0 && a < 0) kind = 2;
    if (a >= 0) snprintf(g_msg, sizeof g_msg, "%s %s %s! %s", why, AR_TIER[g_armor[a]], AR_NAME[a], AR_WHAT[a]);
    else if (kind == 1) { g_hcryst++; g_dragon.hearts += 1; snprintf(g_msg, sizeof g_msg, "%s A HEART CRYSTAL! EVERY DRAGON GETS +1 HEART", why); }
    else if (kind == 3) { if (g_runes < 5) g_runes++; snprintf(g_msg, sizeof g_msg, "%s A POWER RUNE! YOUR BREATH POWER COMES BACK FASTER", why); }
    else { g_dragon.growth += 120; g_dragon.hearts = g_dragon.max_hearts; snprintf(g_msg, sizeof g_msg, "%s A PILE OF DRAGON GEMS! +120 GROWTH, HEARTS FULL", why); }
    g_msg_t = 6; sfx_play(SFX_WIN, 0.8f, 0); sfx_play(SFX_GROW, 0.7f, 0); platform_rumble(0.8f, 0.6f); g_growpulse_t = 1.5f; save_progress(); }
static float tr_hurt_mul(void) { return 1.0f - 0.15f * g_armor[AR_HELM]; }
static int tr_blade_bonus(void) { return (g_armor[AR_BLADE] >= 1) + (g_armor[AR_BLADE] >= 3); }
static void tr_frame(float dt) {
    if (g_side) return; if (g_tr_wk != g_wk) tr_place();
    Dragon *d = &g_dragon; float L = dragon_length(d->stage);
    /* armor at work */
    if (g_armor[AR_MAIL] && d->hearts < d->max_hearts && !(g_boss.woke && g_boss.state != B_DOWN && !g_rescued)) d->hearts = fminf(d->max_hearts, d->hearts + dt * 0.03f * g_armor[AR_MAIL]);
    if (g_armor[AR_WINGS]) d->stamina = fminf(1, d->stamina + dt * 0.05f * g_armor[AR_WINGS]);
    /* chests: walk up to one to open it (the boulder one only once the boulder is smashed) */
    for (int i = 0; i < TR_CHESTS; i++) { TSpot *c = &g_chest[i]; if (g_tr_open_t[i] >= 0) { if (g_tr_open_t[i] < 9) g_tr_open_t[i] += dt; continue; }
        if (c->rock >= 0 && g_world.prop[c->rock].broken <= 0) continue;
        float dx = d->x - c->x, dz = d->z - c->z, dy = d->y - c->y, R = 1.6f + 0.5f * L;
        if (dx * dx + dz * dz < R * R && fabsf(dy) < 2 + L) { g_tfound[g_wk] |= 1 << i; g_tr_open_t[i] = 0; burst(v3(c->x, c->y + 1.0f, c->z), 50, 7, 0.6f, 1, 0.85f, 0.35f, 1.0f);
            tr_reward(i < 2 ? 0 : 2, "TREASURE CHEST!"); } }
    /* eggs */
    for (int e = 0; e < TR_EGGS; e++) { if ((g_tfound[g_wk] >> (4 + e)) & 1) continue; TSpot *g = &g_egg[e]; float dx = d->x - g->x, dz = d->z - g->z, dy = d->y - g->y, R = 1.3f + 0.5f * L;
        if (dx * dx + dz * dz < R * R && fabsf(dy) < 2 + L) { g_tfound[g_wk] |= 1 << (4 + e); int n = 0; for (int k = 0; k < TR_EGGS; k++) n += (g_tfound[g_wk] >> (4 + k)) & 1;
            burst(v3(g->x, g->y + 0.6f, g->z), 30, 5, 0.5f, 1, 1, 0.8f, 0.9f); sfx_play(SFX_PICKUP, 0.9f, 0); platform_rumble(0.4f, 0.3f); d->growth += 40;
            if (n >= TR_EGGS && !(g_tquest[g_wk] & 1)) { g_tquest[g_wk] |= 1; tr_reward(1, "ALL 5 LOST EGGS FOUND! QUEST DONE:"); }
            else { snprintf(g_msg, sizeof g_msg, "LOST DRAGON EGG! %d OF %d IN THIS WORLD  +40", n, TR_EGGS); g_msg_t = 4; save_progress(); } } }
    if (g_trobots[g_wk] >= TR_ROBOTS && !(g_tquest[g_wk] & 2)) { g_tquest[g_wk] |= 2; tr_reward(3, "ROBOT SMASHER QUEST DONE!"); } }
static void tr_robot_down(int n) { if (g_side || n <= 0) return; int was = g_trobots[g_wk]; g_trobots[g_wk] += n;
    if (was < TR_ROBOTS && g_trobots[g_wk] < TR_ROBOTS && (g_trobots[g_wk] % 4 == 0)) { snprintf(g_msg, sizeof g_msg, "ROBOT SMASHER QUEST: %d OF %d", g_trobots[g_wk], TR_ROBOTS); g_msg_t = 2.5f; } }
/* drawing: chests (wood with gold bands; the lid swings open) and the eggs, into the food batch */
static void tr_draw(Vec3 eye) {
    if (g_side || g_tr_wk != g_wk) return;
    for (int i = 0; i < TR_CHESTS; i++) { const TSpot *c = &g_chest[i]; float dx = c->x - eye.x, dz = c->z - eye.z; if (dx * dx + dz * dz > 120 * 120) continue;
        if (c->rock >= 0 && g_world.prop[c->rock].broken <= 0 && g_tr_open_t[i] < 0) continue;   /* still hidden under its boulder */
        float yaw = c->x * 0.37f + c->z * 0.11f, fx = sinf(yaw), fz = cosf(yaw), rx = fz, rz = -fx, y = c->y;
        for (int k = 0; k < 3; k++) { float off = (k - 1) * 0.36f; batch_ball(v3(c->x + rx * off, y + 0.38f, c->z + rz * off), 0.2f, 0.38f, 0.5f, 2, 4, 0.55f, 0.32f, 0.14f, 1); }   /* the wooden box */
        for (int k = 0; k < 2; k++) { float off = (k ? 0.42f : -0.42f); batch_ball(v3(c->x + rx * off, y + 0.4f, c->z + rz * off), 0.05f, 0.42f, 0.53f, 2, 4, 1.3f, 1.0f, 0.35f, 1); }   /* gold bands */
        float open = g_tr_open_t[i] < 0 ? 0 : fminf(1, g_tr_open_t[i] / 0.6f); Vec3 hinge = v3(c->x - fx * 0.45f, y + 0.78f, c->z - fz * 0.45f);
        float la = open * 1.9f; Vec3 lc = v3(hinge.x + fx * 0.45f * cosf(la), hinge.y + 0.45f * sinf(la) + 0.08f, hinge.z + fz * 0.45f * cosf(la));
        batch_ball(lc, 0.62f, 0.12f, 0.5f * 0.95f, 2, 6, 0.6f, 0.36f, 0.16f, 0.3f);   /* the lid */
        batch_ball(v3(c->x + fx * 0.5f, y + 0.62f, c->z + fz * 0.5f), 0.09f, 0.11f, 0.06f, 2, 4, 1.4f, 1.1f, 0.4f, 1);   /* the lock */
        if (g_tr_open_t[i] < 0) { float gl = 0.6f + 0.4f * sinf(g_time * 4 + i); batch_ball(v3(c->x, y + 1.1f + 0.1f * sinf(g_time * 2), c->z), 0.12f, 0.12f, 0.12f, 2, 5, 1.6f * gl, 1.3f * gl, 0.4f * gl, 1); } }   /* a glint above an unopened one */
    for (int e = 0; e < TR_EGGS; e++) { if ((g_tfound[g_wk] >> (4 + e)) & 1) continue; const TSpot *g = &g_egg[e]; float dx = g->x - eye.x, dz = g->z - eye.z; if (dx * dx + dz * dz > 90 * 90) continue;
        float gl = 0.75f + 0.25f * sinf(g_time * 3 + e * 1.7f); const float *pc = THEMES[g_wk].portal;
        batch_ball(v3(g->x, g->y + 0.42f, g->z), 0.3f, 0.42f, 0.3f, 4, 8, (0.5f + 0.7f * pc[0]) * gl, (0.5f + 0.7f * pc[1]) * gl, (0.5f + 0.7f * pc[2]) * gl, 0.6f);
        batch_ball(v3(g->x, g->y + 0.05f, g->z), 0.42f, 0.06f, 0.42f, 2, 8, 0.45f, 0.38f, 0.25f, 0); } }   /* in a little nest */
/* the pause menu's side panel: armor, and this world's secrets and quests */
static void tr_panel(void) {
    if (g_side) return; ui_panel(905, 150, 350, 420, 0.95f, 0); text_c("TREASURE", 166, 30, 1, 1, 0.9f, 0.6f); ui_divider(1080, 210, 280, 1);
    float y = 228; char s[80];
    for (int i = 0; i < AR_N; i++) { snprintf(s, sizeof s, "%s: %s", AR_NAME[i], g_armor[i] ? AR_TIER[g_armor[i]] : "-"); static const float TC[4][3] = {{0.6f, 0.6f, 0.6f}, {0.95f, 0.65f, 0.4f}, {0.85f, 0.9f, 1.0f}, {1.0f, 0.85f, 0.3f}};
        batch_text(&g_b, s, 930, y, 20, TC[g_armor[i]][0], TC[g_armor[i]][1], TC[g_armor[i]][2], 1); y += 30; }
    snprintf(s, sizeof s, "HEART CRYSTALS: %d   RUNES: %d", g_hcryst, g_runes); batch_text(&g_b, s, 930, y, 20, 1, 0.6f, 0.65f, 1); y += 44;
    int ch = 0, eg = 0; for (int k = 0; k < TR_CHESTS; k++) ch += (g_tfound[g_wk] >> k) & 1; for (int k = 0; k < TR_EGGS; k++) eg += (g_tfound[g_wk] >> (4 + k)) & 1;
    batch_text(&g_b, "IN THIS WORLD", 930, y, 20, 1, 0.9f, 0.6f, 1); y += 32;
    snprintf(s, sizeof s, "CHESTS: %d OF %d", ch, TR_CHESTS); batch_text(&g_b, s, 930, y, 20, 1, 1, 1, 0.9f); y += 28;
    snprintf(s, sizeof s, "LOST EGGS: %d OF %d%s", eg, TR_EGGS, g_tquest[g_wk] & 1 ? "  DONE" : ""); batch_text(&g_b, s, 930, y, 20, 1, 1, 1, 0.9f); y += 28;
    snprintf(s, sizeof s, "ROBOTS: %d OF %d%s", g_trobots[g_wk] < TR_ROBOTS ? g_trobots[g_wk] : TR_ROBOTS, TR_ROBOTS, g_tquest[g_wk] & 2 ? "  DONE" : ""); batch_text(&g_b, s, 930, y, 20, 1, 1, 1, 0.9f); y += 36;
    text_c(ch < TR_CHESTS ? "TIP: ONE CHEST HIDES UNDER A BIG BOULDER" : "EVERY CHEST HERE IS OPEN!", y, 16, 1, 0.95f, 0.85f, 0.8f); }
/* save: a second line the older builds ignore */
static void tr_save(FILE *f) { fprintf(f, "DBT %d %d %d %d %d %d", g_runes, g_hcryst, g_armor[0], g_armor[1], g_armor[2], g_armor[3]); for (int w = 0; w < WK_N; w++) fprintf(f, " %d %d %d", g_tfound[w], g_trobots[w], g_tquest[w]); fprintf(f, " %d\n", g_finv); }
static void tr_reset(void) { memset(g_tfound, 0, sizeof g_tfound); memset(g_trobots, 0, sizeof g_trobots); memset(g_tquest, 0, sizeof g_tquest); memset(g_armor, 0, sizeof g_armor); g_hcryst = 0; g_runes = 0; g_tr_wk = -1; }
static void tr_load(FILE *f) {
    tr_reset(); char line[400]; if (!fgets(line, sizeof line, f) || strncmp(line, "DBT", 3)) return; char *p = line + 3; int used = 0;
    if (sscanf(p, "%d %d %d %d %d %d%n", &g_runes, &g_hcryst, &g_armor[0], &g_armor[1], &g_armor[2], &g_armor[3], &used) < 6) { tr_reset(); return; } p += used;
    int w = 0; for (; w < WK_N; w++) { if (sscanf(p, "%d %d %d%n", &g_tfound[w], &g_trobots[w], &g_tquest[w], &used) < 3) break; p += used; }
    { int fv; if (w == WK_N && sscanf(p, "%d", &fv) == 1) g_finv = fv ? 1 : 0; }
    for (int i = 0; i < AR_N; i++) g_armor[i] = g_armor[i] < 0 ? 0 : g_armor[i] > 3 ? 3 : g_armor[i]; if (g_hcryst < 0 || g_hcryst > 40) g_hcryst = 0; if (g_runes < 0 || g_runes > 5) g_runes = 0; }
