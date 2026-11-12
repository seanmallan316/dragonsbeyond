// boss.h - Ironhorn, the robot stag that guards the Sky dragonet's cage in Everleaf Wilds. Pure logic (testable on a PC).
// The fight: it paws the ground, then charges. Dodge so it rams one of the clearing's trees: its antlers stick and the
// glowing core on its back is open - tail whip it (B). A roar (Y) up close stuns it too. Three hits and it falls.
#ifndef DB_BOSS_H
#define DB_BOSS_H
#include "world.h"
enum { B_SLEEP = 0, B_STALK, B_CHARGE, B_STUCK, B_STUNNED, B_HURT, B_DOWN };
typedef struct {
    int state, hp;            /* hp 3 .. 0 */
    float x, y, z, yaw;       /* feet on the ground */
    float t;                  /* time in the current state */
    float cx, cz;             /* charge direction */
    float speed;              /* current speed (m/s), for the animation */
    int woke;                 /* has seen the player (the fight has started) */
    float cage_open;          /* 0 closed .. 1 lowered and open (after the win) */
    float core_fwd, core_h;   /* where its glowing core sits: metres forward (negative = on its back) and up */
    float max_speed;          /* top charge speed (m/s): slower on easier settings */
    int tired, tired_need;    /* charges that hit the dragon in a row; after tired_need of them it gets dizzy anyway (fewer on easier settings) */
    float orbit, prowl;       /* prowling: which way it circles the dragon (+1/-1) and for how long before it lines up a charge */
    float vx, vz;             /* its velocity on the ground (smooth starts, turns and stops) */
} Boss;
#define BOSS_HP 3
void boss_init(Boss *b, const World *w);
/* events: 1 woke up, 2 hit the dragon, 4 took a hit, 8 defeated, 16 stuck in a tree, 32 stunned, 64 started a charge */
int  boss_update(Boss *b, const World *w, float dt, float px, float py, float pz, float reach, int whip, int roar);
/* where the glowing core is (on its back), for drawing and for hits */
Vec3 boss_core(const Boss *b);
int  boss_vulnerable(const Boss *b);
#endif
