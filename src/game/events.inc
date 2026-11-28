/* events.inc - world events: every few minutes of exploring, something happens somewhere in the world.
   A card tells you what, where (distance and direction), how hard, the reward and the time left; a marker goes on the map
   and a tracker line stays on screen. Go and do it, or ignore it and it simply ends. Included by game.c after treasure.inc.
     TREASURE GLINT  - a golden chest has surfaced: reach it before it sinks again (easy)
     ROBOT INVASION  - Kaine's robots gather at a spot: knock down 6 of them there in time (medium)
     SKY RINGS       - rings of wind in the sky: fly through all of them in time (medium, needs a dragon that flies) */
enum { EV_NONE = 0, EV_GLINT, EV_INVASION, EV_RINGS, EV_KINDS };
static const char *EV_NAME[EV_KINDS] = {"", "TREASURE GLINT", "ROBOT INVASION", "SKY RINGS"};
static const char *EV_DIFF[EV_KINDS] = {"", "EASY", "MEDIUM", "MEDIUM"};
static const char *EV_REWARD[EV_KINDS] = {"", "ARMOR OR GEMS", "A POWER RUNE OR ARMOR", "DRAGON GEMS"};
#define EV_RINGS_N 6
static struct { int kind, wk, got, ring; float t, len, x, y, z, card_t, next; float rx[EV_RINGS_N], ry[EV_RINGS_N], rz[EV_RINGS_N]; } g_ev = {0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 150};
static float ev_rand(void) { static unsigned s = 4242u; s = s * 1664525u + 1013904223u ^ (unsigned)(g_time * 1000); return (float)((s >> 8) & 0xFFFFFF) / 16777216.0f; }
static int ev_spot(float *ox, float *oz, float rmin, float rmax) {
    const Dragon *d = &g_dragon;
    for (int t = 0; t < 200; t++) { float a = ev_rand() * 6.2831853f, r = rmin + (rmax - rmin) * ev_rand(), x = d->x + cosf(a) * r, z = d->z + sinf(a) * r;
        if (!tr_ok(x, z) || world_in_pond(&g_world, x, z)) continue; if (g_world.sea_on && world_height(&g_world, x, z) < g_world.sea + 0.5f) continue;
        *ox = x; *oz = z; return 1; }
    return 0; }
static void ev_start(int kind) {
    float x, z; if (!ev_spot(&x, &z, 70, 170)) return;
    g_ev.kind = kind; g_ev.wk = g_wk; g_ev.x = x; g_ev.z = z; g_ev.y = world_height(&g_world, x, z); g_ev.t = 0; g_ev.got = 0; g_ev.ring = 0; g_ev.card_t = 7;
    g_ev.len = kind == EV_GLINT ? 150 : kind == EV_INVASION ? 150 : 75;
    if (kind == EV_INVASION) {   /* robots that are down or far away come to the spot */
        int moved = 0; for (int pass = 0; pass < 2; pass++) { Drone *arr = pass ? g_world.hound : g_world.drone; int n = pass ? g_world.nhound : g_world.ndrone;
            for (int i = 0; i < n && moved < 7; i++) { Drone *r = &arr[i]; float dx = r->x - g_dragon.x, dz = r->z - g_dragon.z; if (dx * dx + dz * dz < 40 * 40 && r->state != 3) continue;
                float a = moved * 0.9f; r->hx = x + cosf(a) * (8 + moved); r->hz = z + sinf(a) * (8 + moved); r->x = r->hx; r->z = r->hz; r->y = world_height(&g_world, r->x, r->z) + (pass ? 0 : 6); r->state = 0; r->t = 0; moved++; } }
        if (moved < 4) { g_ev.kind = EV_NONE; return; } }
    if (kind == EV_RINGS) { float a = ev_rand() * 6.2831853f;   /* a curving line of rings, high up */
        for (int i = 0; i < EV_RINGS_N; i++) { float u = i * 22.0f; a += 0.25f * (ev_rand() - 0.5f); float rx = x + cosf(a) * u, rz = z + sinf(a) * u;
            rx = clampf(rx, -W_SIZE / 2 + 20, W_SIZE / 2 - 20); rz = clampf(rz, -W_SIZE / 2 + 20, W_SIZE / 2 - 20);
            g_ev.rx[i] = rx; g_ev.rz[i] = rz; g_ev.ry[i] = fmaxf(world_height(&g_world, rx, rz), g_world.sea_on ? g_world.sea : -1e9f) + 14 + 5 * sinf(i * 1.3f); } }
    sfx_play(SFX_CHARGE, 0.8f, 0); sfx_play(SFX_CHIRP, 0.6f, 0); platform_rumble(0.4f, 0.4f); }
static void ev_end(int won) {
    if (won) { int k = g_ev.kind; burst(v3(g_dragon.x, g_dragon.y + 1.5f, g_dragon.z), 60, 8, 0.6f, 1, 0.85f, 0.35f, 1);
        if (k == EV_GLINT) tr_reward(ev_rand() < 0.5f ? 0 : 2, "WORLD EVENT DONE! THE GOLDEN CHEST HELD");
        else if (k == EV_INVASION) tr_reward(g_runes < 5 && ev_rand() < 0.6f ? 3 : 0, "INVASION STOPPED! YOU EARNED");
        else tr_reward(2, "ALL THE SKY RINGS! YOU EARNED"); }
    else { snprintf(g_msg, sizeof g_msg, "THE %s IS OVER. ANOTHER WORLD EVENT WILL COME", EV_NAME[g_ev.kind]); g_msg_t = 3; }
    g_ev.kind = EV_NONE; g_ev.next = 220 + 140 * ev_rand(); }
static void ev_frame(float dt) {
    if (g_side) return;
    if (g_ev.kind && g_ev.wk != g_wk) { g_ev.kind = EV_NONE; g_ev.next = 120; }
    int busy = (g_boss.woke && g_boss.state != B_DOWN && !g_rescued) || g_ko_t >= 0 || g_tale_i >= 0;
    if (!g_ev.kind) { if (!busy) g_ev.next -= dt; if (g_ev.next <= 0) { int k = 1 + (int)(ev_rand() * 3) % 3; if (k == EV_RINGS && g_dragon.stage < ST_DRAGON) k = EV_GLINT; ev_start(k); if (!g_ev.kind) g_ev.next = 30; } return; }
    g_ev.t += dt; if (g_ev.card_t > 0) g_ev.card_t -= dt;
    const Dragon *d = &g_dragon; float L = dragon_length(d->stage);
    if (g_ev.kind == EV_GLINT) { float dx = d->x - g_ev.x, dz = d->z - g_ev.z; if (dx * dx + dz * dz < (2 + 0.5f * L) * (2 + 0.5f * L) && fabsf(d->y - g_ev.y) < 3 + L) { ev_end(1); return; } }
    else if (g_ev.kind == EV_RINGS) { int i = g_ev.ring; float dx = d->x - g_ev.rx[i], dy = d->y + 0.4f * L - g_ev.ry[i], dz = d->z - g_ev.rz[i];
        if (dx * dx + dy * dy + dz * dz < (4.5f + 0.3f * L) * (4.5f + 0.3f * L)) { g_ev.ring++; sfx_play(SFX_PICKUP, 0.9f, 0); platform_rumble(0.3f, 0.15f); burst(v3(g_ev.rx[i], g_ev.ry[i], g_ev.rz[i]), 24, 6, 0.4f, 0.7f, 0.95f, 1, 0.8f);
            if (g_ev.ring >= EV_RINGS_N) { ev_end(1); return; } snprintf(g_msg, sizeof g_msg, "RING %d OF %d!", g_ev.ring, EV_RINGS_N); g_msg_t = 1.5f; } }
    if (g_ev.t > g_ev.len) ev_end(0); }
static void ev_robot_down(float x, float z) { if (g_ev.kind != EV_INVASION) return; float dx = x - g_ev.x, dz = z - g_ev.z; if (dx * dx + dz * dz > 45 * 45) return;
    if (++g_ev.got >= 6) ev_end(1); else { snprintf(g_msg, sizeof g_msg, "INVASION: %d OF 6 ROBOTS DOWN", g_ev.got); g_msg_t = 2; } }
/* where the event is for the tracker and the map */
static void ev_target(float *x, float *z) { if (g_ev.kind == EV_RINGS) { *x = g_ev.rx[g_ev.ring]; *z = g_ev.rz[g_ev.ring]; } else { *x = g_ev.x; *z = g_ev.z; } }
static void ev_draw_add(Vec3 eye) {   /* the glowing beacon over the spot, the golden chest's glow, the rings */
    if (!g_ev.kind) return; Vec3 up = v3(0, 1, 0);
    float tx, tz; ev_target(&tx, &tz); float gy = world_height(&g_world, tx, tz); Vec3 rt = v3_norm(v3_cross(up, v3_sub(v3(tx, gy, tz), eye)));
    const float c[3] = {g_ev.kind == EV_INVASION ? 1.0f : 1.0f, g_ev.kind == EV_INVASION ? 0.35f : 0.85f, g_ev.kind == EV_INVASION ? 0.2f : 0.35f};
    if (g_ev.kind != EV_RINGS) for (int k = 0; k < 14; k++) { float h = 4 + k * 5.0f, w = 1.4f - k * 0.07f; batch_quad(&g_b, v3(tx, gy + h, tz), v3(rt.x * w, 0, rt.z * w), v3(0, 2.6f, 0), up, 0, 0, 1, 1, c[0], c[1], c[2], 0.5f * (1 - k / 14.0f)); }   /* a column of light */
    if (g_ev.kind == EV_GLINT) { float p = 0.7f + 0.3f * sinf(g_time * 6); batch_quad(&g_b, v3(tx, gy + 1.2f, tz), v3(rt.x * 2.4f * p, 0, rt.z * 2.4f * p), v3(0, 2.4f * p, 0), up, 0, 0, 1, 1, 1, 0.85f, 0.35f, 0.8f); }
    if (g_ev.kind == EV_RINGS) for (int i = g_ev.ring; i < EV_RINGS_N; i++) { float R = 4.5f; int n = 40; float a0 = i + 1 < EV_RINGS_N ? atan2f(g_ev.rx[i + 1] - g_ev.rx[i], g_ev.rz[i + 1] - g_ev.rz[i]) : atan2f(g_ev.rx[i] - g_ev.rx[i - 1], g_ev.rz[i] - g_ev.rz[i - 1]);
        Vec3 side = v3(cosf(a0), 0, -sinf(a0)); float on = i == g_ev.ring ? 1.0f : 0.5f;
        for (int k = 0; k < n; k++) { float a = k * 6.2831853f / n + g_time * 0.8f; Vec3 p = v3(g_ev.rx[i] + side.x * cosf(a) * R, g_ev.ry[i] + sinf(a) * R, g_ev.rz[i] + side.z * cosf(a) * R);
            Vec3 r2 = v3_norm(v3_cross(up, v3_sub(p, eye))); float sz = 0.45f; batch_quad(&g_b, p, v3(r2.x * sz, r2.y * sz, r2.z * sz), v3(0, sz, 0), up, 0, 0, 1, 1, 0.6f, 0.95f, 1, 0.8f * on); } } }
static void ev_hud(void) {   /* the event card, then a tracker line */
    if (!g_ev.kind || g_swap_open || g_pause_open) return; const Dragon *d = &g_dragon; float tx, tz; ev_target(&tx, &tz);
    float dx = tx - d->x, dz = tz - d->z, dist = sqrtf(dx * dx + dz * dz); float rel = atan2f(dx, dz) - g_cam_yaw; while (rel > 3.14159f) rel -= 6.2831853f; while (rel < -3.14159f) rel += 6.2831853f;
    const char *dir = fabsf(rel) < 0.4f ? "AHEAD" : fabsf(rel) > 2.7f ? "BEHIND YOU" : rel > 0 ? "TO YOUR LEFT" : "TO YOUR RIGHT";
    int left = (int)fmaxf(0, g_ev.len - g_ev.t); char s[120];
    if (g_ev.card_t > 0) { float a = clampf(g_ev.card_t, 0, 1);
        ui_panel(380, 220, 520, 200, 0.95f * a, 1); batch_text_g(&g_b, "WORLD EVENT", 640 - text_width("WORLD EVENT", 22) / 2, 236, 22, 1, 0.85f, 0.4f, a, 1);
        text_cg(EV_NAME[g_ev.kind], 268, 34, a);
        snprintf(s, sizeof s, "%d M %s   DIFFICULTY: %s", (int)dist, dir, EV_DIFF[g_ev.kind]); text_c(s, 318, 18, 1, 1, 1, a);
        snprintf(s, sizeof s, "REWARD: %s", EV_REWARD[g_ev.kind]); text_c(s, 346, 18, 1, 0.9f, 0.5f, a);
        snprintf(s, sizeof s, "TIME: %d:%02d   FOLLOW THE BEAM, OR IGNORE IT", left / 60, left % 60); text_c(s, 374, 16, 0.9f, 0.9f, 0.9f, a); }
    if (g_ev.kind == EV_INVASION) snprintf(s, sizeof s, "%s %d/6  %d M  %d:%02d", EV_NAME[g_ev.kind], g_ev.got, (int)dist, left / 60, left % 60);
    else if (g_ev.kind == EV_RINGS) snprintf(s, sizeof s, "%s %d/%d  %d M  %d:%02d", EV_NAME[g_ev.kind], g_ev.ring, EV_RINGS_N, (int)dist, left / 60, left % 60);
    else snprintf(s, sizeof s, "%s  %d M  %d:%02d", EV_NAME[g_ev.kind], (int)dist, left / 60, left % 60);
    batch_text(&g_b, s, 1240 - text_width(s, 20), 236, 20, 1, 0.85f, 0.4f, 1); }
