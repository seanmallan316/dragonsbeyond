/* ================= Effekseer effects (phone and PC; the Wii U keeps the game's own particles, gfx_fx_ok() is 0 there) =================
   Effects made with the effekseer-vfx toolkit (tools/vfxproj), packed in content/efk. Each power gets its own effect, moved along with
   the power as it flies and finished with an impact; the mega blast gets a full explosion; hits, smashed rocks, growing up and the
   Master Dragon's spell get theirs. */
static int g_fx_pw, g_fx_charge, g_fx_spell, g_fx_pw_kind = -1; static float g_fx_pw_end;
static const float FXC[7][3] = {{0.45f, 1.0f, 0.35f}, {1.0f, 0.65f, 0.3f}, {0.75f, 0.92f, 1.0f}, {0.85f, 0.6f, 1.0f}, {1.0f, 0.85f, 0.5f}, {0.35f, 0.8f, 1.0f}, {1.0f, 0.45f, 0.15f}};
static int fx_play_c(const char *n, Vec3 p, float yaw, float s, float r, float g, float b) { int h = gfx_fx_play(n, p.x, p.y, p.z, yaw, s); if (h) gfx_fx_color(h, r, g, b, 1); return h; }
static void fx_hit(Vec3 at, float power, float r, float g, float b) { if (!gfx_fx_ok()) return; fx_play_c("VFX_Hit_Heavy", at, g_cam_yaw, 0.45f + 1.4f * power, 0.6f + 0.4f * r, 0.6f + 0.4f * g, 0.6f + 0.4f * b); }
static void fx_shatter(Vec3 at, float s, float r, float g, float b) {
    if (!gfx_fx_ok()) return;
    if (g_wk == WK_CRYSTAL || g_wk == WK_SNOW) fx_play_c("VFX_Ice_Spikes", at, g_time, 0.35f * s, r, g, b); else fx_play_c("VFX_Hit_Heavy", at, g_time, 0.5f * s, r, g, b); }
static const char *FX_PW[7] = {"VFX_Poison_Cloud", "VFX_Projectile_Fire", "VFX_Projectile_Ice", "VFX_Ice_Spikes", "VFX_Poison_Cloud", "VFX_Dark_Vortex", "VFX_Projectile_Fire"};
static const char *FX_END[7] = {0, "VFX_Fire_Explosion", "VFX_Ice_Spikes", 0, 0, 0, "VFX_Fire_Explosion"};
static void fx_power_start(int k, int big, const Dragon *d) {
    if (!gfx_fx_ok()) return; float L = dragon_length(d->stage), s = (0.4f + 0.35f * d->stage) * (big ? 1.8f : 1.0f); const float *c = FXC[k];
    if (g_fx_charge) { gfx_fx_stop(g_fx_charge); g_fx_charge = 0; }
    Vec3 mouth = v3(d->x + sinf(d->yaw) * L * 0.6f, d->y + 0.55f * L, d->z + cosf(d->yaw) * L * 0.6f);
    g_fx_pw = fx_play_c(FX_PW[k], mouth, d->yaw, s, c[0], c[1], c[2]); g_fx_pw_kind = k; g_fx_pw_end = 0;
    if (big) { Vec3 at = v3(d->x + sinf(d->yaw) * 3, world_height(&g_world, d->x, d->z) + 0.5f, d->z + cosf(d->yaw) * 3);
        fx_play_c("VFX_Kaiju_Explosion", at, d->yaw, 0.06f + 0.035f * d->stage, 0.6f + 0.4f * c[0], 0.6f + 0.4f * c[1], 0.6f + 0.4f * c[2]); } }
/* every frame: the power's effect follows the power; the charge glow builds in the mouth; the spell swirls round the Master Dragon */
static void fx_frame(float dt) {
    if (!gfx_fx_ok()) return; gfx_fx_update(dt); const Dragon *d = &g_dragon; float L = dragon_length(d->stage);
    if (g_fx_pw && g_power_t > 0) { float x, y, z, range, cone; power_shape(&x, &y, &z, &range, &cone); float s = (0.4f + 0.35f * d->stage) * (g_power_big ? 1.8f : 1.0f);
        gfx_fx_move(g_fx_pw, x, y, z, g_pw_yaw, s * (0.8f + 0.25f * fminf(range, 8) / 8)); g_fx_pw_end = 1; }
    else if (g_fx_pw && g_fx_pw_end) { float x, y, z, range, cone; power_shape(&x, &y, &z, &range, &cone); gfx_fx_stop(g_fx_pw); const char *e = FX_END[g_fx_pw_kind];
        if (e) fx_play_c(e, v3(x, y, z), g_pw_yaw, 0.35f + 0.3f * d->stage, FXC[g_fx_pw_kind][0], FXC[g_fx_pw_kind][1], FXC[g_fx_pw_kind][2]); g_fx_pw = 0; g_fx_pw_end = 0; }
    if (g_charge > 0.35f) { Vec3 mouth = v3(d->x + sinf(d->yaw) * L * 0.6f, d->y + 0.55f * L, d->z + cosf(d->yaw) * L * 0.6f); const float *c = FXC[power_of(g_kind)];
        float s = (0.12f + 0.08f * d->stage) * (0.5f + fminf(1, g_charge / 1.1f));
        if (!gfx_fx_alive(g_fx_charge)) g_fx_charge = fx_play_c("VFX_Magic_Charge", mouth, d->yaw, s * 3, c[0], c[1], c[2]); else gfx_fx_move(g_fx_charge, mouth.x, mouth.y, mouth.z, d->yaw, s * 3); }
    else if (g_fx_charge) { gfx_fx_stop(g_fx_charge); g_fx_charge = 0; }
    if (final_master() && g_boss.woke && g_boss.state != B_DOWN) { Vec3 at = v3(g_boss.x, g_boss.y + 0.5f, g_boss.z);
        if (!gfx_fx_alive(g_fx_spell)) g_fx_spell = fx_play_c("VFX_Dark_Vortex", at, g_time, 1.6f, 0.8f, 0.5f, 1.0f); else gfx_fx_move(g_fx_spell, at.x, at.y, at.z, g_time * 0.3f, 1.6f); }
    else if (g_fx_spell) { gfx_fx_stop(g_fx_spell); g_fx_spell = 0; } }
static void fx_grow(const Dragon *d) { if (gfx_fx_ok()) fx_play_c("VFX_Heal", v3(d->x, world_height(&g_world, d->x, d->z), d->z), 0, 0.6f + 0.4f * d->stage, 1, 1, 1); }
static void fx_boom(Vec3 at, float s) { if (gfx_fx_ok()) fx_play_c("VFX_Kaiju_Explosion", at, g_time, s, 1, 1, 1); }
