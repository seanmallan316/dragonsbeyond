// world.h - Everleaf Wilds: rolling ground, trees, bushes, rocks, and the food that grows a dragon. Pure logic, no graphics
// (the renderer builds its meshes from these tables), so it can be tested on a PC.
#ifndef DB_WORLD_H
#define DB_WORLD_H
#include "../render/kkmath.h"

#define W_SIZE 240.0f          /* the patch is W_SIZE x W_SIZE metres, centred on the origin */
#define W_GRID 96              /* height samples per side */
#define W_MAXPROPS 700
#define W_MAXFOOD 160
#define W_MAXDRONES 6
#define W_MAXHOUNDS 4

enum { P_TREE = 0, P_PALM, P_BUSH, P_ROCK, P_FERN, P_SPIRE, P_N };   /* spires: tall rock needles on the sky islands */
enum { WK_FOREST = 0, WK_SKY, WK_N };   /* which world: Everleaf Wilds, the Sky Kingdom */
#define W_VOID -60.0f        /* ground height where there is no island (the cloud sea hides it) */
#define W_CLOUDS -22.0f      /* the top of the cloud sea: fall below it and you are lost in the clouds */
enum { F_BERRY = 0, F_MUSHROOM, F_GRUB, F_FISH, F_N };   /* berries and mushrooms are plants; grubs and fish are meat */

typedef struct { int kind; float x, z, yaw, scale; } Prop;
typedef struct { int kind, alive; float x, y, z, yaw, regrow, wander, phase; } Food;
/* Kaine's Feather Drones: black robot birds. state 0 patrol, 1 dive at the dragon, 2 fleeing, 3 knocked down (respawn timer in t) */
typedef struct { int state; float x, y, z, yaw, t, hx, hz, flap; } Drone;   /* regrow: seconds until it is back; grubs wander */

typedef struct {
    int kind;                       /* WK_* */
    float h[W_GRID][W_GRID];
    Prop prop[W_MAXPROPS]; int nprop;
    Food food[W_MAXFOOD]; int nfood;
    float pond_x, pond_z, pond_r;   /* a round pond, water at y = pond_y */
    float pond_y;
    unsigned seed;
    float crystal_x, crystal_y, crystal_z; int crystal_found;   /* the hidden egg crystal (one per world) */
    Drone drone[W_MAXDRONES]; int ndrone;
    Drone hound[W_MAXHOUNDS]; int nhound;
    float arena_x, arena_z, arena_r, arena_y;   /* the boss clearing: flat ground ringed by big trees */
    int arena_tree[8]; int narena_tree;         /* prop indices of the ring trees (the boss rams these) */
    float cage_x, cage_y, cage_z;               /* where the captured dragonet's cage hangs */   /* Shock Hounds: same record, on the ground. state 0 patrol, 1 chase, 2 retreat, 3 knocked out */
} World;

void  world_make(World *w, unsigned seed);                    /* Everleaf Wilds */
void  world_make_kind(World *w, unsigned seed, int kind);     /* any world */
int   world_is_void(const World *w, float x, float z);         /* nothing to stand on here (sky world gaps) */
void  world_safe_spot(const World *w, float x, float z, float *ox, float *oz);   /* the nearest solid ground to (x, z) */
float world_height(const World *w, float x, float z);        /* ground height, smooth */
Vec3  world_normal(const World *w, float x, float z);
int   world_in_pond(const World *w, float x, float z);
void  world_update(World *w, float dt, float px, float pz);   /* regrowth, grubs wandering away from the player */
int   world_nearest_food(const World *w, float x, float z, float reach);   /* index or -1 */
/* drones: returns a bitmask: 1 a drone hit the dragon (take damage), 2 a drone was knocked down. whip = tail whip landing this frame, roar = roaring */
int   world_drones(World *w, float dt, float px, float py, float pz, float reach, int whip, int roar);
int   world_hounds(World *w, float dt, float px, float py, float pz, float reach, int whip, int roar, int in_air);   /* same bitmask as the drones */
int   food_is_meat(int kind);
float food_growth(int kind);                                  /* growth points it gives */
float world_fbm(float x, float y, unsigned seed);   /* layered noise 0..1, for scenery builders */
#endif
