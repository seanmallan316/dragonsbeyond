// dragon.h - a player's dragon: its stage of growth, how it moves on the ground and in the air, eating. Pure logic.
#ifndef DB_DRAGON_H
#define DB_DRAGON_H
#include "world.h"

enum { ST_DRAGONET = 0, ST_DRAGON = 1, ST_BEYONDER = 2, ST_N };
enum { DM_GROUND = 0, DM_AIR = 1, DM_SWIM = 2 };
enum { DA_IDLE = 0, DA_WALK, DA_RUN, DA_FLY, DA_GLIDE, DA_EAT, DA_ROAR, DA_TAILWHIP, DA_HIT, DA_CHIRP, DA_CLAW, DA_POUNCE, DA_TALON, DA_N };
/* attacks: what is hitting right now (Dragon.atk_kind) */
enum { ATK_NONE = 0, ATK_CLAW, ATK_TAIL, ATK_SLAM, ATK_TALON, ATK_ROLL, ATK_CHIRP };

typedef struct {
    int stage;                  /* ST_* */
    float growth;               /* 0..100 toward the next stage */
    float x, y, z, yaw;         /* feet on the ground at y when on the ground */
    float vx, vy, vz;           /* velocity */
    float speed;                /* forward speed (m/s), for the animation */
    int mode;                   /* DM_* */
    int anim; float anim_t;     /* clip and time into it */
    float act_t;                /* time left in a one-shot action (eat, roar, tail whip), 0 = free */
    int eating;                 /* food index being eaten, -1 */
    float hearts, max_hearts;
    float evolve_t;             /* > 0 while the grow-up sparkle plays */
    float pitch;                /* nose up/down in flight */
    float stamina;              /* 0..1 flapping energy */
    float land_t;
    /* combat: the hit area this step (atk_r 0 = none), set by dragon_update */
    int atk_kind; float atk_x, atk_y, atk_z, atk_r;
    int slam;                   /* 1 while dropping onto the ground in a pounce */
    float air_act;              /* time left in an attack done in flight (talon strike, barrel roll) */
    int air_kind;               /* ATK_TALON or ATK_ROLL */
    float roll, roll_dir;       /* barrel roll angle (radians) for drawing, and which way */
    float invuln_t;             /* can't be hurt (rolling) */
    float chirp_cd;             /* the dragonet's spark chirp needs a breath between uses */
} Dragon;

typedef struct { float sx, sy; int a, a_press, b_press, x_press, zr, zl, y_press; } DragonInput;

/* sizes and speeds per stage: model length (m), walk, run, fly speeds */
float dragon_length(int stage);
float dragon_scale(int stage);        /* model units -> metres for the baked models (they are ~1.9 units long) */
float dragon_need(int stage);         /* growth points to reach the next stage */
const char *dragon_stage_name(int stage);
const char *dragon_anim_name(int anim);

void dragon_init(Dragon *d, const World *w, float x, float z);
/* one step; returns a bitmask of events: 1 ate something, 2 grew a stage, 4 took off, 8 landed, 16 roared, 32 pounce landed */
float dragon_tail_reach(int stage);
int  dragon_update(Dragon *d, World *w, const DragonInput *in, float cam_yaw, float dt);
#endif
