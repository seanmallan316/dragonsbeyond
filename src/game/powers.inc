/* powers.inc - every dragon's own power, each one a different thing to see and to use (from the game plan):
     forest  LEAFNADO: the wings light up and spin a whirl of sharp leaves round the dragon
     sky     FIRE BREATH: a roaring cone of flame with a white-hot core, embers and smoke
     snow    FROST BREATH: an icy blast that freezes enemies solid where they stand
     crystal CRYSTAL SHARDS: volleys of sharp crystals shot from both wings
     sand    SANDSTORM: a rolling wall of sand with a venom-green sting in it
     water   WATERNADO: a spinning column of water that travels ahead
     lava    LAVA SPIT: glowing lava bombs lobbed ahead, leaving burning pools
     Everwing uses each of them in turn.
   Beyonders can HOLD the button to charge, then let go for a far bigger blast (g_power_big). Included by game.c. */

static int g_power_big; static float g_charge = -1, g_pw_x, g_pw_y, g_pw_z, g_pw_yaw, g_pw_len, g_pw_dur = 1.4f, g_boom_t;
static int g_ever_i;   /* which power Everwing uses next */
typedef struct { float x, y, z, t; } IceBlock; static IceBlock g_ice[10]; static int g_nice;
static int power_of(int kind) { return kind == 7 ? g_ever_i % 7 : kind; }

static void power_start(int big) {
    Dragon *d = &g_dragon; float L = dragon_length(d->stage); int k = power_of(g_kind); if (g_kind == 7) g_ever_i++;
    g_power_kind = k; g_power_big = big; g_pw_dur = big ? 2.6f : (k == 0 || k == 4 || k == 5 || k == 6 ? 1.9f : 1.4f); g_power_t = g_pw_dur; g_power_cd = big ? 5.0f : 2.6f;
    g_pw_yaw = d->yaw; g_pw_len = L; g_pw_x = d->x; g_pw_y = d->y; g_pw_z = d->z;
    static const int SND[7] = {SFX_WIND, SFX_FIRE, SFX_ICE, SFX_ICE, SFX_WIND, SFX_SPLASH, SFX_FIRE};
    fx_power_start(k, big, d);
    sfx_play(SND[k], 1.0f, 0); sfx_play(SFX_ROAR, big ? 1.0f : 0.6f, 0); platform_rumble(big ? 1.0f : 0.7f, big ? 1.2f : 0.6f);
    if (big) { sfx_play(SFX_BOOM, 1.0f, 0); g_hitstop = 0.16f; g_shake_t = fmaxf(g_shake_t, 0.9f); g_boom_t = 1.0f; { const Dragon *dd = &g_dragon; float gy = world_height(&g_world, dd->x, dd->z); float cx = dd->x + sinf(dd->yaw) * 3, cz = dd->z + cosf(dd->yaw) * 3; smash(cx, gy + 1, cz, 0, 0, 13, -2, 99.0f); if (dd->y < gy + 4) crater(cx, cz, 6.5f + dd->stage, 2.2f); }
        static const float C[7][3] = {{0.4f, 1, 0.3f}, {1, 0.6f, 0.2f}, {0.7f, 0.9f, 1}, {0.9f, 0.6f, 1}, {1, 0.85f, 0.45f}, {0.3f, 0.7f, 1}, {1, 0.4f, 0.1f}};
        burst(v3(d->x + sinf(d->yaw) * L * 0.5f, d->y + 0.6f * L, d->z + cosf(d->yaw) * L * 0.5f), 90, 16, 0.5f, C[k][0], C[k][1], C[k][2], 1.0f); }
    if (d->mode == DM_AIR) d->act_t = 0; }

/* where the power is and how far it reaches right now: centre, direction, range, cone (-1 = all round) */
static void power_shape(float *x, float *y, float *z, float *range, float *cone) {
    float L = g_pw_len, age = g_pw_dur - g_power_t, big = g_power_big ? 1.0f : 0.0f; const Dragon *d = &g_dragon; float fx = sinf(d->yaw), fz = cosf(d->yaw), ox = sinf(g_pw_yaw), oz = cosf(g_pw_yaw);
    *x = d->x; *y = d->y + 0.5f * L; *z = d->z; *cone = -1; *range = 0;
    switch (g_power_kind) {
    case 0: if (big > 0) { float tr = fminf(age * 8, 22); *x = g_pw_x + ox * tr; *z = g_pw_z + oz * tr; *y = g_pw_y + 2; *range = 10; } else *range = 7 + 2 * L; break;   /* leafnado: round the dragon; charged: a giant one marches ahead */
    case 1: *x = d->x + fx * L; *z = d->z + fz * L; *range = (10 + 3 * L) * (1 + 0.9f * big); *cone = big > 0 ? 0.55f : 0.75f; break;                               /* fire breath: a cone */
    case 2: if (big > 0) *range = 17; else { *x = d->x + fx * L; *z = d->z + fz * L; *range = 10 + 3 * L; *cone = 0.75f; } break;                                  /* frost breath; charged: a blizzard all round */
    case 3: if (big > 0) *range = 14; else { *x = d->x + fx * L; *z = d->z + fz * L; *range = 17 + 3 * L; *cone = 0.9f; } break;                                   /* shards: long and narrow; charged: spikes burst up all round */
    case 4: { float tr = fminf(age * (big > 0 ? 6 : 8), big > 0 ? 16 : 14); *x = g_pw_x + ox * (tr + 3); *z = g_pw_z + oz * (tr + 3); *y = g_pw_y + 1.5f; *range = big > 0 ? 12 : 6.5f; } break;   /* sandstorm rolls ahead */
    case 5: { float tr = fminf(age * (big > 0 ? 13 : 9), big > 0 ? 34 : 17); *x = g_pw_x + ox * (tr + 2); *z = g_pw_z + oz * (tr + 2); *y = g_pw_y + 2; *range = big > 0 ? 10 : 5.5f; } break;        /* waternado travels; charged: a tidal wave */
    default: if (big > 0) { *x = g_pw_x; *z = g_pw_z; *y = g_pw_y + 1; *range = age > 0.45f ? 15 : 0; } else { *x = g_pw_x + ox * (6 + 2.2f * L); *z = g_pw_z + oz * (6 + 2.2f * L); *y = g_pw_y + 1; *range = age > 0.5f ? 4.5f + 0.8f * L : 0; } break; } }   /* lava: where the bombs land; charged: an eruption */

static void power_frame(const Pad *p, const Pad *q, DragonInput *in, float dt) {
    Dragon *d = &g_dragon;
    if (g_power_cd > 0) g_power_cd -= dt; if (g_power_t > 0) g_power_t -= dt; if (g_boom_t > 0) g_boom_t -= dt * 1.4f;
    for (int i = 0; i < g_nice; i++) { g_ice[i].t += dt; if (g_ice[i].t > 1.7f) { burst(v3(g_ice[i].x, g_ice[i].y + 0.8f, g_ice[i].z), 26, 6, 0.3f, 0.8f, 0.95f, 1, 0.6f); sfx_play(SFX_ICE, 0.6f, 0); g_ice[i] = g_ice[--g_nice]; i--; } }
    int can = d->stage >= ST_DRAGON && g_power_cd <= 0 && g_ko_t < 0;
    if (d->stage >= ST_BEYONDER) {   /* tap = the power; hold = charge, let go for the big blast */
        int fire = 0, big = 0; in->y_press = 0;
        if (p->y && !q->y && can) { g_charge = 0; }
        if (g_charge >= 0) { g_charge += dt; if (g_charge > 0.35f && g_charge - dt <= 0.35f) sfx_play(SFX_CHARGE, 0.9f, 0);
            if (g_charge > 0.35f) platform_rumble(0.2f + 0.3f * fminf(1, g_charge / 1.4f), 0.1f);
            if (!p->y || g_charge > 2.4f) { fire = 1; big = g_charge >= 1.1f; g_charge = -1; } }
        if (fire) { power_start(big); in->y_press = 1; }
    } else { g_charge = -1; if (in->y_press && can) power_start(0); }
    if (g_power_t > 0) { float x, y, z, range, cone; power_shape(&x, &y, &z, &range, &cone);
        int hit = range > 0 ? world_blast(&g_world, x, y, z, sinf(d->yaw), cosf(d->yaw), range, cone) : 0;
        if (range > 0) smash(x, y, z, sinf(d->yaw), cosf(d->yaw), range, cone, g_power_big ? 99.0f : 1.6f + 0.2f * d->power);   /* powers blast rocks and crystals apart; a mega blast breaks anything */
        if (hit) { d->growth += 5 * hit; sfx_play(SFX_BLOCK, 0.6f, 0);
            static const char *HIT[7] = {"SLICED BY LEAVES!", "TOASTED!", "FROZEN SOLID!", "SHATTERED!", "STUNG AND BURIED!", "WASHED AWAY!", "MELTED!"}; snprintf(g_msg, sizeof g_msg, "%s", HIT[g_power_kind]); g_msg_t = 2;
            if (g_power_kind == 2) for (int k = 0; k < g_world.nkill && g_nice < 10; k++) {   /* frost: they freeze where they stand, in a block of ice, then shatter */
                float kx = g_world.kill_x[k], kz = g_world.kill_z[k]; g_ice[g_nice].x = kx; g_ice[g_nice].y = g_world.kill_y[k]; g_ice[g_nice].z = kz; g_ice[g_nice].t = 0; g_nice++;
                for (int pass = 0; pass < 2; pass++) { Drone *arr = pass ? g_world.hound : g_world.drone; int cnt = pass ? g_world.nhound : g_world.ndrone; for (int i = 0; i < cnt; i++) if (arr[i].state == 3 && arr[i].t < 0.1f && fabsf(arr[i].x - kx) < 0.5f && fabsf(arr[i].z - kz) < 0.5f) { arr[i].kx = 0; arr[i].kz = 0; } } } } } }

/* ---- drawing ---- */
static Vec3 g_pw_rt, g_pw_eye;
static float g_pw_k = 1;   /* effects are sized to the dragon: a Beyonder's flames are far bigger than a young dragon's */
static void pq(Vec3 c, float sz, float r, float g, float b, float a) { sz *= g_pw_k; batch_quad(&g_b, c, v3(g_pw_rt.x * sz, 0, g_pw_rt.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0.18f, 0.18f, 0.82f, 0.82f, r, g, b, a); }   /* the soft round heart of the glow picture: a puff, not a star */
static void pstreak(Vec3 c, Vec3 dir, float len, float wid, float r, float g, float b, float a) {   /* a long thin glow lying along dir (a leaf, a shard, a streak of rain) */
    len *= g_pw_k; wid *= g_pw_k; Vec3 side = v3_norm(v3_cross(dir, v3_sub(g_pw_eye, c))); batch_quad(&g_b, c, v3(side.x * wid, side.y * wid, side.z * wid), v3(dir.x * len, dir.y * len, dir.z * len), v3(0, 1, 0), 0.3f, 0.3f, 0.7f, 0.7f, r, g, b, a); }
static float ph01(int i, int k) { unsigned h = (unsigned)i * 2654435761u + (unsigned)k * 40503u; h ^= h >> 15; h *= 2246822519u; return (float)((h >> 8) & 0xFFFF) / 65536.0f; }

static void powers_draw_add(Vec3 eye) {
    const Dragon *d = &g_dragon; float L = dragon_length(d->stage), fx = sinf(d->yaw), fz = cosf(d->yaw); g_pw_eye = eye;
    Vec3 fw = v3_norm(v3(g_at.x - eye.x, 0, g_at.z - eye.z)); g_pw_rt = v3(fw.z, 0, -fw.x);
    Vec3 mouth = v3(d->x + fx * 0.6f * L, d->y + 0.55f * L, d->z + fz * 0.6f * L);
    g_pw_k = 0.7f + 0.32f * L;
    static const float C[7][3] = {{0.4f, 1, 0.3f}, {1, 0.6f, 0.2f}, {0.7f, 0.9f, 1}, {0.95f, 0.6f, 1}, {1, 0.85f, 0.45f}, {0.3f, 0.7f, 1}, {1, 0.4f, 0.1f}};
    if (g_charge > 0.35f) { int k = power_of(g_kind); float c = fminf(1, (g_charge - 0.35f) / 0.75f), full = g_charge >= 1.1f;   /* charging: light rushes in to the mouth, a ball of power swells there */
        for (int i = 0; i < 46; i++) { float u = 1 - fmodf(ph01(i, 1) + g_time * (1.2f + c), 1.0f), a = ph01(i, 2) * 6.283f, e = ph01(i, 3) * 3.14159f - 1.5708f, R = (2.5f + 3 * L) * u;
            pq(v3(mouth.x + cosf(a) * cosf(e) * R, mouth.y + sinf(e) * R, mouth.z + sinf(a) * cosf(e) * R), 0.12f + 0.25f * (1 - u), C[k][0], C[k][1], C[k][2], 0.9f * (1 - u * 0.5f)); }
        float s = (0.5f + 1.4f * c) * (0.5f + 0.3f * L) * (full ? 1.0f + 0.15f * sinf(g_time * 40) : 1); pq(mouth, s, C[k][0], C[k][1], C[k][2], 0.9f); pq(mouth, s * 0.5f, 1, 1, 1, 0.9f);
        pq(v3(d->x, d->y + 0.1f, d->z), 0, 0, 0, 0, 0); }
    if (g_boom_t > 0) { int k = g_power_kind; float u = 1 - g_boom_t, R = 3 + 26 * u;   /* the charged blast: a ring of light racing out along the ground and a white flash */
        for (int i = 0; i < 48; i++) { float a = i * 0.1309f; pq(v3(g_pw_x + cosf(a) * R, g_pw_y + 0.5f, g_pw_z + sinf(a) * R), 1.6f * (1 - u) + 0.4f, C[k][0], C[k][1], C[k][2], g_boom_t); }
        if (u < 0.25f) pq(v3(g_pw_x + sinf(g_pw_yaw) * g_pw_len, g_pw_y + 0.6f * g_pw_len, g_pw_z + cosf(g_pw_yaw) * g_pw_len), 14 * (1 - u * 3), 1, 1, 1, 1 - u * 4); }
    for (int i = 0; i < g_nice; i++) { float t = g_ice[i].t, s = 1.5f + 0.2f * sinf(t * 30) * (t > 1.4f); pq(v3(g_ice[i].x, g_ice[i].y + 0.8f, g_ice[i].z), s * 1.3f, 0.55f, 0.85f, 1.0f, 0.5f); pq(v3(g_ice[i].x, g_ice[i].y + 0.8f, g_ice[i].z), s * 0.7f, 0.9f, 1.0f, 1.0f, 0.35f); }
    if (g_power_t <= 0) return;
    float age = g_pw_dur - g_power_t, fade = clampf(g_power_t / 0.35f, 0, 1) * clampf(age / 0.12f, 0, 1), big = g_power_big ? 1.0f : 0.0f, S = 1 + 0.8f * big; int N = g_power_big ? 300 : 170;
    float x, y, z, range, cone; power_shape(&x, &y, &z, &range, &cone);
    switch (g_power_kind) {
    case 0: {   /* LEAFNADO: a tall funnel of spinning leaves, each a long green blade tumbling round; the wings glow */
        float R0 = big > 0 ? 3 : 1.2f, R1 = big > 0 ? 10 : 6 + 2 * L, H = big > 0 ? 22 : 9 + 2 * L; Vec3 c = big > 0 ? v3(x, world_height(&g_world, x, z), z) : v3(d->x, d->y, d->z);
        for (int i = 0; i < N; i++) { float u = fmodf(ph01(i, 4) + age * 0.6f, 1.0f), a = ph01(i, 5) * 6.283f + age * (9 - 5 * u), r = R0 + (R1 - R0) * u * u + 0.4f * sinf(i);
            Vec3 pos = v3(c.x + cosf(a) * r, c.y + u * H + 0.3f, c.z + sinf(a) * r), dir = v3_norm(v3(-sinf(a), 0.5f * sinf(i * 1.7f + age * 6), cosf(a)));
            int gold = i % 7 == 0; pstreak(pos, dir, 0.55f * S, 0.16f * S, gold ? 1.0f : 0.25f + 0.25f * ph01(i, 6), gold ? 0.9f : 1.0f, gold ? 0.3f : 0.15f, fade * (1 - u * 0.4f)); }
        for (int s = -1; s <= 1; s += 2) pq(v3(d->x + fz * s * 0.6f * L, d->y + 0.7f * L, d->z - fx * s * 0.6f * L), 0.8f * L, 0.5f, 1, 0.3f, 0.5f * fade);
        for (int i = 0; i < 16; i++) { float a = i * 0.3927f - age * 5; pq(v3(c.x + cosf(a) * R1 * 0.7f, c.y + 0.3f, c.z + sinf(a) * R1 * 0.7f), 0.9f, 0.6f, 0.9f, 0.5f, 0.25f * fade); } } break;
    case 1: {   /* FIRE BREATH: billowing flame - white-hot at the mouth, yellow, orange, red, then dark smoke; embers fly; the ground glows */
        float len = range;
        for (int i = 0; i < N; i++) { float u = fmodf(ph01(i, 7) + age * (1.6f + 0.6f * ph01(i, 8)), 1.0f), a = ph01(i, 9) * 6.283f, sp = (0.05f + 0.34f * u) * (cone < 0.7f ? 1.5f : 1.0f); float dist = u * len, wob = sinf(age * 13 + i) * 0.15f * u * len * 0.2f;
            float sx = cosf(a) * sp * dist + wob, sy = sinf(a) * sp * dist * 0.7f + u * u * 1.6f; Vec3 c = v3(mouth.x + fx * dist + fz * sx, mouth.y + sy, mouth.z + fz * dist - fx * sx);
            float r, g, b, al = fade; if (u < 0.15f) { r = 1; g = 1; b = 0.85f; } else if (u < 0.4f) { r = 1; g = 0.85f; b = 0.25f; } else if (u < 0.75f) { r = 1; g = 0.45f; b = 0.08f; } else { r = 0.55f; g = 0.12f; b = 0.05f; al *= (1 - u) * 4; }
            pq(c, (0.25f + 1.5f * u) * S, r, g, b, al * 0.6f); }
        for (int i = 0; i < 40; i++) { float u = fmodf(ph01(i, 10) + age * 1.1f, 1.0f), a = ph01(i, 11) * 6.283f; pq(v3(mouth.x + fx * u * len + fz * cosf(a) * u * 3, mouth.y + 1.5f * u + 2.5f * u * sinf(a) + 0.5f, mouth.z + fz * u * len - fx * cosf(a) * u * 3), 0.1f, 1, 0.8f, 0.3f, fade * (1 - u)); }
        for (int i = 1; i <= 5; i++) { float dd = len * i / 5.5f; pq(v3(mouth.x + fx * dd, world_height(&g_world, mouth.x + fx * dd, mouth.z + fz * dd) + 0.3f, mouth.z + fz * dd), 1.6f * S, 1, 0.35f, 0.05f, 0.3f * fade); } } break;
    case 2: {   /* FROST BREATH: a pale, glittering blast with snowflakes tumbling in it and a creeping sheet of frost on the ground */
        if (big > 0) { for (int i = 0; i < N; i++) { float u = fmodf(ph01(i, 12) + age * 0.5f, 1.0f), a = ph01(i, 13) * 6.283f + age * (3 - u), r = 2 + 15 * u; float yy = d->y + 0.5f + 9 * ph01(i, 14) * (0.4f + 0.6f * sinf(u * 3.14159f));
                Vec3 c = v3(d->x + cosf(a) * r, yy, d->z + sinf(a) * r); if (i % 3 == 0) pstreak(c, v3_norm(v3(-sinf(a), -0.3f, cosf(a))), 0.8f, 0.07f, 1, 1, 1, fade); else pq(c, 0.3f + 0.5f * ph01(i, 15), 0.75f, 0.92f, 1, 0.75f * fade); } }
        else for (int i = 0; i < N; i++) { float u = fmodf(ph01(i, 12) + age * 1.5f, 1.0f), a = ph01(i, 13) * 6.283f + age * 2, sp = 0.06f + 0.3f * u, dist = u * range;
            Vec3 c = v3(mouth.x + fx * dist + fz * cosf(a) * sp * dist, mouth.y + sinf(a) * sp * dist * 0.7f - u * u * 1.2f, mouth.z + fz * dist - fx * cosf(a) * sp * dist);
            if (i % 4 == 0) { float fa = age * 5 + i; pstreak(c, v3(cosf(fa), sinf(fa), 0), 0.3f, 0.05f, 1, 1, 1, fade); pstreak(c, v3(-sinf(fa), cosf(fa), 0), 0.3f, 0.05f, 1, 1, 1, fade); }   /* a spinning snowflake */
            else pq(c, 0.22f + 1.0f * u, 0.55f + 0.3f * u, 0.8f, 1.0f, fade * (0.45f - 0.3f * u)); }
        for (int i = 0; i < 26; i++) { float dd = (big > 0 ? 16 : range) * ph01(i, 16) * fminf(1, age * 2), a = big > 0 ? ph01(i, 17) * 6.283f : atan2f(fx, fz) + (ph01(i, 17) - 0.5f) * 0.7f; float gx = d->x + sinf(a) * dd, gz = d->z + cosf(a) * dd;
            batch_quad(&g_b, v3(gx, world_height(&g_world, gx, gz) + 0.15f, gz), v3(1.4f, 0, 0), v3(0, 0, 1.4f), v3(0, 1, 0), 0, 0, 1, 1, 0.75f, 0.92f, 1, 0.35f * fade); } } break;
    case 3: {   /* CRYSTAL SHARDS: long sharp crystals fired in volleys from both wings, flashing pink, violet and ice blue */
        if (big > 0) { for (int i = 0; i < 60; i++) { float a = i * 0.1047f, rr = 3 + 11 * ph01(i, 18), up = fminf(1, age * 4) * (2 + 4 * ph01(i, 19)); float gx = d->x + cosf(a) * rr, gz = d->z + sinf(a) * rr, gy = world_height(&g_world, gx, gz);
                pstreak(v3(gx, gy + up * 0.5f, gz), v3(0.15f * cosf(a), 1, 0.15f * sinf(a)), up * 0.6f, 0.35f, i % 3 == 0 ? 1.0f : 0.6f, i % 3 == 1 ? 1.0f : 0.5f, 1.0f, fade); } }
        for (int i = 0; i < (big > 0 ? 90 : 70); i++) { float u = fmodf(ph01(i, 20) + age * 2.6f, 1.0f); int side = i % 2 ? 1 : -1; float ang = big > 0 ? ph01(i, 21) * 6.283f : atan2f(fx, fz) + (ph01(i, 21) - 0.5f) * 0.42f; float dx = sinf(ang), dz = cosf(ang), dist = u * (big > 0 ? 15 : range);
            Vec3 o = v3(d->x + fz * side * 0.7f * L, d->y + 0.6f * L + 0.4f * ph01(i, 22), d->z - fx * side * 0.7f * L); Vec3 c = v3(o.x + dx * dist, o.y - u * 0.5f, o.z + dz * dist); int col = i % 3;
            pstreak(c, v3(dx, -0.03f, dz), 0.9f, 0.09f, col == 0 ? 1.0f : 0.65f, col == 1 ? 1.0f : 0.6f, 1.0f, fade); if (i % 5 == 0) pq(c, 0.35f, 1, 1, 1, fade * 0.8f); }
        for (int s = -1; s <= 1; s += 2) pq(v3(d->x + fz * s * 0.7f * L, d->y + 0.65f * L, d->z - fx * s * 0.7f * L), 0.7f * L, 0.9f, 0.6f, 1, 0.5f * fade); } break;
    case 4: {   /* SANDSTORM: a rolling, churning wall of sand with darts of venom-green lightning inside */
        float R = range; Vec3 c = v3(x, world_height(&g_world, x, z), z);
        for (int i = 0; i < N; i++) { float a = ph01(i, 23) * 6.283f + age * (2.5f + 2 * ph01(i, 24)), r = R * sqrtf(ph01(i, 25)), yy = (0.3f + 6.5f * S * ph01(i, 26)) * (0.6f + 0.4f * sinf(age * 3 + i));
            float sh = 0.75f + 0.25f * ph01(i, 27); pq(v3(c.x + cosf(a) * r, c.y + yy, c.z + sinf(a) * r), 0.7f + 1.1f * ph01(i, 28), 1.0f * sh, 0.8f * sh, 0.45f * sh, 0.42f * fade); }
        for (int i = 0; i < 7; i++) { float a = ph01(i + (int)(age * 14), 29) * 6.283f, r = R * 0.6f * ph01(i, 30); pstreak(v3(c.x + cosf(a) * r, c.y + 1.5f + 3 * ph01(i, 31), c.z + sinf(a) * r), v3_norm(v3(cosf(a * 3), 0.6f, sinf(a * 3))), 1.3f, 0.07f, 0.4f, 1.0f, 0.2f, fade); } } break;
    case 5: {   /* WATERNADO: a tight spinning column of water with spray flying off; charged: a tidal wave rolling ahead with a foaming crest */
        Vec3 c = v3(x, world_height(&g_world, x, z), z);
        if (big > 0) { float px = cosf(g_pw_yaw), pz = -sinf(g_pw_yaw);
            for (int i = 0; i < N; i++) { float lat = (ph01(i, 32) - 0.5f) * 22, u = ph01(i, 33), yy = u * (7 + 1.5f * sinf(lat * 0.5f + age * 4)); float curl = 2.2f * u * u;
                Vec3 q = v3(c.x + px * lat + sinf(g_pw_yaw) * curl, c.y + yy, c.z + pz * lat + cosf(g_pw_yaw) * curl); if (u > 0.82f) pq(q, 0.9f, 1, 1, 1, 0.7f * fade); else pq(q, 1.1f, 0.15f, 0.5f + 0.3f * u, 1.0f, 0.5f * fade); } }
        else for (int i = 0; i < N; i++) { float u = fmodf(ph01(i, 34) + age * 0.9f, 1.0f), a = ph01(i, 35) * 6.283f + age * (12 - 5 * u), r = 0.6f + 2.4f * u * u + 0.2f * sinf(i), H = 9 + 2 * L;
            Vec3 pos = v3(c.x + cosf(a) * r + 0.6f * sinf(age * 4 + u * 4), c.y + u * H, c.z + sinf(a) * r); if (i % 5 == 0) pq(pos, 0.3f, 1, 1, 1, fade); else pstreak(pos, v3_norm(v3(-sinf(a), 0.35f, cosf(a))), 0.6f, 0.2f, 0.2f, 0.55f + 0.3f * u, 1.0f, 0.8f * fade); }
        for (int i = 0; i < 20; i++) { float a = i * 0.314f + age * 3, r = (big > 0 ? 2 : 1) * (2.5f + 1.5f * fmodf(age * 2 + i * 0.37f, 1.0f)); pq(v3(c.x + cosf(a) * r, c.y + 0.3f, c.z + sinf(a) * r), 0.6f, 0.8f, 0.95f, 1, 0.5f * fade); } } break;
    default: {   /* LAVA SPIT: glowing bombs arc out of the mouth trailing fire, splash, and leave a bubbling pool; charged: the ground erupts */
        if (big > 0) { Vec3 c = v3(g_pw_x, g_pw_y, g_pw_z); float e = clampf((age - 0.3f) / 0.5f, 0, 1);
            for (int i = 0; i < N; i++) { float u = fmodf(ph01(i, 36) + age * 0.9f, 1.0f), a = ph01(i, 37) * 6.283f, rr = 14 * ph01(i, 38) * u, yy = 18 * e * 4 * u * (1 - u) + 0.3f; float hot = 1 - u;
                pq(v3(c.x + cosf(a) * rr, c.y + yy, c.z + sinf(a) * rr), 0.5f + 0.9f * hot, 1.0f, 0.3f + 0.6f * hot, 0.05f + 0.3f * hot * hot, fade); }
            for (int i = 0; i < 30; i++) { float a = i * 0.2094f, rr = 14 * ph01(i, 39) * e; batch_quad(&g_b, v3(c.x + cosf(a) * rr, world_height(&g_world, c.x + cosf(a) * rr, c.z + sinf(a) * rr) + 0.2f, c.z + sinf(a) * rr), v3(2.2f, 0, 0), v3(0, 0, 2.2f), v3(0, 1, 0), 0, 0, 1, 1, 1, 0.4f, 0.05f, 0.6f * fade); } }
        else { float ox = sinf(g_pw_yaw), oz = cosf(g_pw_yaw); float pk = g_pw_k; Vec3 o = v3(g_pw_x + ox * 0.6f * g_pw_len, g_pw_y + 0.55f * g_pw_len, g_pw_z + oz * 0.6f * g_pw_len); float gyl = world_height(&g_world, x, z);
            for (int b = 0; b < 5; b++) { float t0 = b * 0.13f, u = clampf((age - t0) / 0.5f, 0, 1); if (age < t0) continue; float lat = (b - 2) * 0.9f * pk; Vec3 land = v3(x + oz * lat, gyl, z - ox * lat);
                if (u < 1) { Vec3 pos = v3(o.x + (land.x - o.x) * u, o.y + (land.y - o.y) * u + 3.0f * pk * 4 * u * (1 - u), o.z + (land.z - o.z) * u); pq(pos, 0.75f, 1, 0.75f, 0.2f, 1); pq(pos, 0.4f, 1, 1, 0.8f, 1);
                    for (int k = 1; k < 6; k++) { float uu = u - k * 0.03f; if (uu < 0) break; pq(v3(o.x + (land.x - o.x) * uu, o.y + (land.y - o.y) * uu + 3.0f * pk * 4 * uu * (1 - uu), o.z + (land.z - o.z) * uu), 0.6f - k * 0.08f, 1, 0.4f, 0.05f, 0.7f - k * 0.1f); } }
                else { float pu = 0.7f + 0.3f * sinf(age * 9 + b); batch_quad(&g_b, v3(land.x, land.y + 0.2f, land.z), v3(1.4f * pk, 0, 0), v3(0, 0, 1.4f * pk), v3(0, 1, 0), 0.15f, 0.15f, 0.85f, 0.85f, 1, 0.45f * pu, 0.05f, 0.85f * fade);
                    for (int k = 0; k < 5; k++) { float uu = fmodf(age * 1.5f + k * 0.2f + b * 0.3f, 1.0f); pq(v3(land.x + 0.8f * pk * sinf(k * 2.4f + b), land.y + 0.2f + uu * 1.6f * pk, land.z + 0.8f * pk * cosf(k * 2.4f + b)), 0.35f * (1 - uu) + 0.1f, 1, 0.6f, 0.15f, fade * (1 - uu)); } } } } } break; } }
