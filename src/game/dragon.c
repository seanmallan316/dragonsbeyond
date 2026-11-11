// dragon.c - the dragon's movement, growth and actions (see dragon.h).
#include "dragon.h"
#include <math.h>
#include <string.h>

static const float LEN[ST_N] = {1.3f, 3.6f, 7.0f};          /* metres nose to tail */
static const float WALK[ST_N] = {3.2f, 5.0f, 6.5f}, RUN[ST_N] = {6.5f, 10.0f, 13.0f}, FLY[ST_N] = {0, 14.0f, 20.0f};
static const float NEED[ST_N] = {400.0f, 1200.0f, 1e9f};   /* growing up is a journey: about 40 plants (or 13 meats, or a mix with robots knocked down) to become a dragon, three times that to become a Beyonder */
static const char *NAMES[ST_N] = {"DRAGONET", "DRAGON", "BEYONDER"};
static const char *ANIMS[DA_N] = {"idle", "walk", "run", "fly", "glide", "eat", "roar", "tailwhip", "hit", "chirp", "claw", "pounce", "talon"};
#define MODEL_LEN 1.88f

float dragon_length(int s) { return LEN[s]; }
float dragon_scale(int s) { return LEN[s] / MODEL_LEN; }
float dragon_need(int s) { return NEED[s]; }
const char *dragon_stage_name(int s) { return NAMES[s]; }
const char *dragon_anim_name(int a) { return ANIMS[a]; }
static float wrapa(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static float lerp(float a, float b, float t) { return a + (b - a) * t; }

void dragon_init(Dragon *d, const World *w, float x, float z) {
    memset(d, 0, sizeof *d); d->x = x; d->z = z; d->y = world_height(w, x, z); d->eating = -1; d->max_hearts = d->hearts = 3; d->stamina = 1; }

static void set_anim(Dragon *d, int a) { if (d->anim != a) { d->anim = a; d->anim_t = 0; } }
static void hitbox(Dragon *d, int kind, float fwd, float up, float r) {   /* an attack's hit area: a ball fwd metres ahead of the dragon */
    d->atk_kind = kind; d->atk_x = d->x + sinf(d->yaw) * fwd; d->atk_z = d->z + cosf(d->yaw) * fwd; d->atk_y = d->y + up; d->atk_r = r; }
float dragon_tail_reach(int stage) { float L = LEN[stage]; return fmaxf(2.8f, 0.9f * L + 1.2f); }

int dragon_update(Dragon *d, World *w, const DragonInput *in, float cam_yaw, float dt) {
    int ev = 0; d->anim_t += dt;
    d->atk_r = 0; d->atk_kind = ATK_NONE; if (d->invuln_t > 0) d->invuln_t -= dt; if (d->chirp_cd > 0) d->chirp_cd -= dt;
    if (d->air_act <= 0) d->roll *= expf(-dt * 10);
    float gy = world_height(w, d->x, d->z); int inpond = world_in_pond(w, d->x, d->z); float waterline = inpond ? w->pond_y : -1e9f;
    float L = LEN[d->stage];
    if (d->evolve_t > 0) { d->evolve_t -= dt; set_anim(d, DA_ROAR); d->speed = 0; return ev; }
    /* one-shot actions block steering until done */
    if (d->act_t > 0) {
        d->act_t -= dt; d->speed *= 0.9f;
        if (d->anim == DA_EAT && d->eating >= 0 && d->act_t < 1.0f) {   /* the bite lands partway through the clip */
            Food *f = &w->food[d->eating]; if (f->alive) { f->alive = 0; f->regrow = food_is_meat(f->kind) ? 45 : 30; d->growth += food_growth(f->kind); d->hearts = fminf(d->max_hearts, d->hearts + 0.5f); ev |= 1; }
            d->eating = -1; }
        float Lr = LEN[d->stage];
        if (d->anim == DA_TAILWHIP && d->act_t > 0.2f && d->act_t < 0.85f) hitbox(d, ATK_TAIL, 0, 0.3f * Lr, dragon_tail_reach(d->stage));   /* all round */
        if (d->anim == DA_CLAW) {   /* a quick swipe in front, with a small lunge */
            if (d->act_t > 0.3f) { d->x += sinf(d->yaw) * 2.2f * dt; d->z += cosf(d->yaw) * 2.2f * dt; }
            if (d->act_t > 0.12f && d->act_t < 0.42f) hitbox(d, ATK_CLAW, 0.4f * Lr + 0.7f, 0.3f * Lr, 0.35f * Lr + 1.2f); }
        if (d->anim == DA_POUNCE && d->act_t > 0.22f) hitbox(d, ATK_SLAM, 0.2f * Lr, 0.2f, 3.2f + 0.4f * Lr);   /* the landing's shock wave */
        if (d->anim == DA_CHIRP && d->stage == ST_DRAGONET && d->act_t > 0.45f && d->act_t < 0.7f) hitbox(d, ATK_CHIRP, 0.3f * Lr, 0.4f, 4.5f);   /* a sparking chirp knocks small robots over */
        if (d->act_t <= 0) { d->act_t = 0; set_anim(d, DA_IDLE); }
        if (d->growth >= NEED[d->stage] && d->stage < ST_BEYONDER && d->act_t == 0) { d->stage++; d->growth = 0; d->max_hearts += 2; d->hearts = d->max_hearts; d->evolve_t = 2.0f; ev |= 2; }
        if (d->mode == DM_GROUND) d->y = gy;
        return ev; }
    /* steering: the stick is relative to the camera */
    float mag = sqrtf(in->sx * in->sx + in->sy * in->sy); if (mag > 1) mag = 1;
    float want_yaw = d->yaw; if (mag > 0.1f) want_yaw = cam_yaw + atan2f(-in->sx, in->sy);   /* the camera's right is -yaw: stick right turns right */
    if (d->mode == DM_GROUND || d->mode == DM_SWIM) {
        float top = (in->zr ? RUN[d->stage] : WALK[d->stage]) * mag; if (d->mode == DM_SWIM) top *= 0.55f;
        float turn = 6.0f * dt; d->yaw += clampf(wrapa(want_yaw - d->yaw), -turn, turn);
        d->speed = lerp(d->speed, top, 1 - expf(-dt * (top > d->speed ? 5.0f : 8.0f)));
        float sy = sinf(d->yaw), cy = cosf(d->yaw);
        d->x += sy * d->speed * dt; d->z += cy * d->speed * dt;
        /* stay on the patch */
        float lim = W_SIZE / 2 - 6; d->x = clampf(d->x, -lim, lim); d->z = clampf(d->z, -lim, lim);
        gy = world_height(w, d->x, d->z); inpond = world_in_pond(w, d->x, d->z); waterline = inpond ? w->pond_y : -1e9f;
        /* slopes: slide down very steep ground a little */
        Vec3 n = world_normal(w, d->x, d->z); if (n.y < 0.72f) { d->x += n.x * 3.0f * dt; d->z += n.z * 3.0f * dt; }
        if (gy < waterline - 0.3f * L) { d->mode = DM_SWIM; d->y = waterline - 0.25f * L; }
        else { d->mode = DM_GROUND; d->y = gy; }
        /* jump, and from stage 2 on: fly */
        if (in->a_press) {
            if (d->stage >= ST_DRAGON) { d->mode = DM_AIR; d->vy = 4.0f; d->pitch = 0.2f; set_anim(d, DA_FLY); ev |= 4; }
            else { d->mode = DM_AIR; d->vy = 5.5f; d->vx = sinf(d->yaw) * d->speed; d->vz = cosf(d->yaw) * d->speed; set_anim(d, DA_GLIDE); }
            return ev; }
        /* actions */
        int near = world_nearest_food(w, d->x + sinf(d->yaw) * 0.45f * L, d->z + cosf(d->yaw) * 0.45f * L, 0.5f * L + 1.2f);
        if (in->x_press && near >= 0) { d->eating = near; d->act_t = 1.77f; set_anim(d, DA_EAT); Food *f = &w->food[near]; d->yaw = atan2f(f->x - d->x, f->z - d->z); return ev; }
        if (in->x_press) { d->act_t = 0.6f; set_anim(d, DA_CLAW); if (mag > 0.1f) d->yaw = want_yaw; return ev; }   /* no food in reach: X claws */
        if (in->b_press) { d->act_t = 0.97f; set_anim(d, DA_TAILWHIP); return ev; }
        if (in->y_press) { if (d->stage == ST_DRAGONET) { if (d->chirp_cd > 0) return ev; d->chirp_cd = 2.5f; d->act_t = 0.8f; set_anim(d, DA_CHIRP); } else { d->act_t = 1.97f; set_anim(d, DA_ROAR); } ev |= 16; return ev; }
        if (d->mode == DM_SWIM) set_anim(d, d->speed > 0.3f ? DA_WALK : DA_IDLE);
        else set_anim(d, d->speed > 0.6f * RUN[d->stage] ? DA_RUN : (d->speed > 0.25f ? DA_WALK : DA_IDLE));
        d->land_t -= dt;
    } else {   /* in the air */
        int canfly = d->stage >= ST_DRAGON; float La = LEN[d->stage];
        if (!canfly && !d->slam && (in->b_press || in->x_press)) {   /* from a hop: pounce down onto whatever is below */
            d->slam = 1; d->vy = -15.0f; d->vx *= 0.6f; d->vz *= 0.6f; set_anim(d, DA_POUNCE); }
        if (canfly && d->air_act <= 0 && in->x_press) { d->air_act = 0.6f; d->air_kind = ATK_TALON; set_anim(d, DA_TALON); }   /* talon strike: a lunge with the claws out */
        if (canfly && d->air_act <= 0 && in->b_press) {   /* barrel roll: a quick sideways dodge with a spinning tail that hits all round */
            d->air_act = 0.7f; d->air_kind = ATK_ROLL; d->roll_dir = in->sx < -0.2f ? -1.0f : 1.0f; d->invuln_t = 0.65f; }
        if (d->air_act > 0) {
            d->air_act -= dt; float u = 1 - d->air_act / (d->air_kind == ATK_TALON ? 0.6f : 0.7f);
            if (d->air_kind == ATK_TALON) { d->speed += 10 * dt * (u < 0.5f ? 1 : -1); if (u > 0.2f && u < 0.85f) hitbox(d, ATK_TALON, 0.35f * La + 1.0f, -0.15f * La, 0.35f * La + 1.7f); }
            else { d->roll = d->roll_dir * 6.2831853f * clampf(u, 0, 1);
                float side = d->roll_dir * 9.0f * (1 - u); d->x += cosf(d->yaw) * side * dt; d->z -= sinf(d->yaw) * side * dt;   /* slides to the dragon's right (or left) */
                if (u > 0.1f && u < 0.9f) hitbox(d, ATK_ROLL, 0, 0, fmaxf(3.0f, 0.8f * La + 1.5f)); }
            if (d->air_act <= 0) { d->air_act = 0; if (d->air_kind == ATK_TALON) set_anim(d, DA_GLIDE); } }
        float turn = 2.6f * dt; d->yaw += clampf(wrapa(want_yaw - d->yaw), -turn, turn) * (mag > 0.1f ? 1 : 0);
        if (canfly) {
            /* hold A: flap and climb; stick up/down pitches; speed builds when diving */
            float wantp = clampf(-in->sy * 0.6f, -0.7f, 0.7f); d->pitch = lerp(d->pitch, wantp, 1 - expf(-dt * 3));
            float top = FLY[d->stage] * (1.0f - 0.5f * fmaxf(0, d->pitch)) * (1.0f + 0.6f * fmaxf(0, -d->pitch));
            d->speed = lerp(d->speed, top, 1 - expf(-dt * 1.5f));
            float lift = in->a ? 5.0f : 0.5f; if (in->a) d->stamina = fmaxf(0, d->stamina - dt * 0.12f); else d->stamina = fminf(1, d->stamina + dt * 0.2f);
            if (d->stamina <= 0.01f) lift = 0.5f;
            d->vy += (lift - 3.5f - d->pitch * 6.0f) * dt; d->vy = clampf(d->vy, -12, 6);
            d->x += sinf(d->yaw) * d->speed * dt; d->z += cosf(d->yaw) * d->speed * dt; d->y += d->vy * dt;
            if (!(d->air_act > 0 && d->air_kind == ATK_TALON)) set_anim(d, in->a && d->stamina > 0.01f ? DA_FLY : DA_GLIDE);
            float ceiling = 70; if (d->y > ceiling) { d->y = ceiling; d->vy = fminf(d->vy, 0); }
        } else {   /* a dragonet only hops and glides a little */
            d->vy -= 11.0f * dt; d->x += d->vx * dt; d->z += d->vz * dt; d->y += d->vy * dt;
            if (in->a && d->vy < 0 && !d->slam) d->vy += 7.0f * dt;   /* flutter: falls slower */
            d->speed = sqrtf(d->vx * d->vx + d->vz * d->vz); }
        float lim = W_SIZE / 2 - 6; d->x = clampf(d->x, -lim, lim); d->z = clampf(d->z, -lim, lim);
        gy = world_height(w, d->x, d->z); inpond = world_in_pond(w, d->x, d->z); waterline = inpond ? w->pond_y : -1e9f;
        float floor_y = fmaxf(gy, waterline - 0.25f * L);
        if (d->y <= floor_y) { d->y = floor_y; d->mode = (gy < waterline - 0.3f * L) ? DM_SWIM : DM_GROUND; d->vy = 0; d->land_t = 0.3f; d->pitch = 0; ev |= 8; set_anim(d, DA_IDLE);
            d->air_act = 0; d->roll = 0;
            if (d->slam) { d->slam = 0; d->act_t = 0.45f; set_anim(d, DA_POUNCE); d->speed = 0; ev |= 32; hitbox(d, ATK_SLAM, 0.2f * L, 0.2f, 3.2f + 0.4f * L); } }
    }
    if (d->growth >= NEED[d->stage] && d->stage < ST_BEYONDER && d->mode == DM_GROUND) { d->stage++; d->growth = 0; d->max_hearts += 2; d->hearts = d->max_hearts; d->evolve_t = 2.0f; ev |= 2; }
    return ev; }
