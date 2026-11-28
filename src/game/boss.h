// boss.h - the world guardians (Ironhorn the robot stag and the others) that guard each captive dragon. Pure logic (testable on a PC).
// The fight, like the action games: the boss has powers and shows each attack before it comes (a "tell"):
//   paws the ground -> CHARGE (run to the side or dodge; a dodged charge rams a tree and its antlers stick),
//   rears up       -> SLAM (a shock wave rolls out along the ground: jump or fly over it, or dodge through it),
//   lowers its head -> SWIPE (close up: dodge back),
//   glows          -> VOLLEY of power bolts (dodge them, or hit one back with your tail or claws: it hurts the boss).
// It only takes real damage when it is open: stuck, dizzy, or catching its breath after a slam or a swipe. Outside those
// moments hits glance off (powers still chip it). A roar only works if it lands while the boss is winding up an attack.
// At two thirds and one third of its health it gets angrier (phases): faster, shorter tells, more bolts.
#ifndef DB_BOSS_H
#define DB_BOSS_H
#include "world.h"
enum { B_SLEEP = 0, B_STALK, B_CHARGE, B_STUCK, B_STUNNED, B_HURT, B_DOWN, B_WINDUP, B_STRIKE, B_PHASE };
enum { BA_CHARGE = 0, BA_SLAM, BA_SWIPE, BA_VOLLEY };
#define BOSS_SHOTS 12
typedef struct { float x, y, z, vx, vy, vz, life; int on, back; } BossShot;
typedef struct {
    int state, hp, max_hp;
    float x, y, z, yaw;       /* feet on the ground */
    float t;                  /* time in the current state */
    float cx, cz;             /* charge direction */
    float speed;              /* current speed (m/s), for the animation */
    int woke;                 /* has seen the player (the fight has started) */
    float cage_open;          /* 0 closed .. 1 lowered and open (after the win) */
    float core_fwd, core_h;   /* where its glowing core sits: metres forward (negative = on its back) and up */
    float max_speed;          /* top charge speed (m/s): slower on easier settings */
    int tired, tired_need;    /* attacks that hit the dragon in a row; after tired_need of them it gets dizzy anyway (fewer on easier settings) */
    float orbit, prowl;       /* prowling: which way it circles the dragon (+1/-1) and for how long before it attacks */
    float vx, vz;             /* its velocity on the ground (smooth starts, turns and stops) */
    int phase;                /* 0, 1, 2: angrier as it loses health */
    int atk;                  /* the attack it is winding up or doing (BA_*) */
    float tell;               /* windup length of the current attack */
    float open_t;             /* > 0: catching its breath after an attack (open to hits) */
    float roar_cd;            /* after a roar interrupt it won't fall for another one this soon */
    int poise;                /* hits taken in this opening; too many and it breaks free with a shock wave */
    int shots_left, hit_done, roar_prev;
    int swinging;             /* set by the game each step: the dragon has an attack out right now (to hit bolts back) */
    unsigned rng;
    float aggro;              /* 0.7 easy .. 1.0 normal .. 1.3 hard: how often and how fast it attacks */
    BossShot shot[BOSS_SHOTS];
    float ring_x, ring_z, ring_r[2]; int ring_hit[2];   /* slam shock waves: rings rolling out along the ground (r < 0 = none) */
} Boss;
#define BOSS_HP 12
#define BOSS_RING_MAX 19.0f
void boss_init(Boss *b, const World *w);
/* events: 1 woke up, 2 hit the dragon, 4 took a hit, 8 defeated, 16 stuck in a tree, 32 stunned, 64 started a charge,
   128 started a tell (b->atk says which), 256 shock wave, 512 fired a bolt, 1024 roar interrupted it, 2048 got angrier (phase),
   4096 shrugged off a roar, 8192 a hit glanced off, 16384 a bolt was hit back, 32768 broke free (too many hits) */
/* dmg: damage of a new hit by the dragon this step (0 = none); roar: the dragon is roaring this step */
int  boss_update(Boss *b, const World *w, float dt, float px, float py, float pz, float reach, int dmg, int roar);
/* a dragon power (breath blast) hit it: full damage when open, a chip otherwise; it interrupts a tell. big = mega blast (always staggers) */
int  boss_power_hit(Boss *b, int dmg, int big);
/* knocked off balance (a perfect dodge): open for a moment */
int  boss_stagger(Boss *b, float secs);
/* where the glowing core is (on its back), for drawing and for hits */
Vec3 boss_core(const Boss *b);
int  boss_vulnerable(const Boss *b);
#endif
