// world.h - the worlds: rolling ground, trees, bushes, rocks, lakes, and the food that grows a dragon. Pure logic, no graphics
// (the renderer builds its meshes from these tables), so it can be tested on a PC.
// The Beyond Expansion: every world is 3.6 km across (100x the area of the first 360 m patch). Props and food are stored
// sorted by 100 m chunk, so anything that wants "what is near here" only looks at a few chunks.
#ifndef DB_WORLD_H
#define DB_WORLD_H
#include "../render/kkmath.h"

#define W_SIZE 3600.0f         /* the world is W_SIZE x W_SIZE metres, centred on the origin */
#define W_GRID 1153            /* height samples per side (3.125 m apart) */
#define W_CELL (W_SIZE / (W_GRID - 1))
#define W_CHN 36               /* chunks per side */
#define W_CHS (W_SIZE / W_CHN) /* a chunk is 100 m square */
#define W_NCH (W_CHN * W_CHN)
#define W_MAXPROPS 120000
#define W_MAXFOOD 36000
#define W_MAXDRONES 96
#define W_MAXHOUNDS 72
#define W_MAXTHIEVES 32
#define W_MAXLAKES 10
#define W_MAXISLES 128
#define W_MAXBRIDGES 160
#define W_NSTONES 6            /* Story Stones in every world */
#define W_NTOWERS 3            /* Kaine's Signal Towers in every world */
#define W_NREGIONS 6           /* named places in every world (region 0 is the start meadow, 1 the boss clearing) */
#define W_START_R 18.0f        /* the flat start meadow round the origin */

enum { P_TREE = 0, P_PALM, P_BUSH, P_ROCK, P_FERN, P_SPIRE, P_N };   /* spires: tall rock needles on the sky islands */
/* which world. Each one holds the dragon of the same number in Kaine's cage (the forest is home: its boss guards the way out) */
enum { WK_FOREST = 0, WK_SKY, WK_SNOW, WK_CRYSTAL, WK_SAND, WK_WATER, WK_LAVA, WK_KAINE, WK_N };   /* the last: Kaine Blackfeather's keep */
#define WK_ISLES(k) ((k) == WK_SKY)   /* island worlds: floating islands over clouds, or islands in the sea */
#define W_VOID -60.0f        /* ground height where there is no island (the cloud sea hides it) */
#define W_CLOUDS -22.0f      /* the top of the cloud sea: fall below it and you are lost in the clouds */
enum { F_BERRY = 0, F_MUSHROOM, F_GRUB, F_FISH, F_N };   /* berries and mushrooms are plants; grubs and fish are meat */

typedef struct { int kind; float x, z, yaw, scale; } Prop;
typedef struct { int kind, alive; float x, y, z, yaw, regrow, wander, phase; int ch; } Food;   /* ch: the chunk it belongs to (grubs and fish stay near it) */
/* a dragon's attack this step: anything within r of (x, y, z) is hit (r = 0: no attack) */
typedef struct { float x, y, z, r; } Attack;
/* Kaine's Feather Drones: black robot birds. state 0 patrol, 4 winding up (hovers and flashes red: get ready), 1 dive at the dragon, 2 fleeing, 3 knocked down (respawn timer in t).
   Shock Hounds: 0 roam, 1 stalk, 5 crouch (flashing: it is about to lunge), 6 lunge, 2 back off, 3 knocked out.
   off = 1: its Signal Tower is down, so it stays switched off for good. */
typedef struct { int state; float x, y, z, yaw, t, hx, hz, flap, kx, kz; int tower, off; } Drone;   /* kx, kz: which way it was knocked flying; tower: which Signal Tower controls it (-1 none) */
typedef struct { float x, z, r, top; } Isle;
typedef struct { float x, z, y; int found; } Spot;   /* a Story Stone, a Signal Tower (found = knocked down) or the middle of a named region */

typedef struct {
    int kind;                       /* WK_* */
    float h[W_GRID][W_GRID];
    Prop prop[W_MAXPROPS]; int nprop;
    Food food[W_MAXFOOD]; int nfood;
    int pch[W_NCH + 1], fch[W_NCH + 1];   /* props / food of chunk c are pch[c] .. pch[c + 1] - 1 (and the same for food) */
    float pond_x, pond_z, pond_r;   /* the first lake (by the start meadow), water at y = pond_y. Also lake 0 below */
    float pond_y;
    float lake_x[W_MAXLAKES], lake_z[W_MAXLAKES], lake_r[W_MAXLAKES], lake_y[W_MAXLAKES]; int nlake;
    unsigned seed;
    float crystal_x, crystal_y, crystal_z; int crystal_found;   /* the hidden egg crystal (one per world) */
    Drone drone[W_MAXDRONES]; int ndrone;
    Drone hound[W_MAXHOUNDS]; int nhound;
    Drone thief[W_MAXTHIEVES]; int nthief;   /* Scale Thieves: state 0 lurk, 1 sneak up, 2 run off with food (flap = how much), 3 knocked out, 4 hiding */
    float arena_x, arena_z, arena_r, arena_y;   /* the boss clearing: flat ground ringed by big trees */
    int arena_tree[8]; int narena_tree;         /* prop indices of the ring trees (the boss rams these) */
    int sea_on; float sea;   /* the water world: everything below this height is under the sea (dragons swim there) */
    float kill_x[8], kill_y[8], kill_z[8]; int nkill;   /* where robots were knocked out this step (the game drops a treat there) */
    float cage_x, cage_y, cage_z;               /* where the captured dragonet's cage hangs */
    Isle isle[W_MAXISLES]; int nisle;           /* sky world: the floating islands */
    int bridge[W_MAXBRIDGES][2]; int nbridge;   /* and the stone walkways joining them */
    Spot stone[W_NSTONES], tower[W_NTOWERS], region[W_NREGIONS];
    float tower_hp[W_NTOWERS];
} World;

void  world_make(World *w, unsigned seed);                    /* Everleaf Wilds */
void  world_flatten(World *w, float H);                          /* one flat floor (multiplayer side levels) */
void  world_make_kind(World *w, unsigned seed, int kind);     /* any world */
int   world_is_void(const World *w, float x, float z);         /* nothing to stand on here (sky world gaps) */
void  world_safe_spot(const World *w, float x, float z, float *ox, float *oz);   /* the nearest solid ground to (x, z) */
float world_height(const World *w, float x, float z);        /* ground height, smooth */
Vec3  world_normal(const World *w, float x, float z);
int   world_in_pond(const World *w, float x, float z);        /* in any lake */
float world_pond_y(const World *w, float x, float z);         /* the water level of the lake at (x, z) (the first lake's if none) */
int   world_lake_at(const World *w, float x, float z);        /* which lake, or -1 */
void  world_update(World *w, float dt, float px, float pz);   /* regrowth, grubs wandering away from the player */
int   world_nearest_food(const World *w, float x, float z, float reach);   /* index or -1 */
/* drones: returns a bitmask: 1 a drone hit the dragon (take damage), 2 a drone was knocked down. whip = tail whip landing this frame, roar = roaring */
int   world_drones(World *w, float dt, float px, float py, float pz, float reach, Attack atk, int roar);
int   world_hounds(World *w, float dt, float px, float py, float pz, float reach, Attack atk, int roar, int in_air);   /* same bitmask as the drones */
/* a dragon's power hits Kaine's helpers: everything within range (and inside the cone ahead, cos_cone = -1 for all round) is knocked out. Returns how many. */
/* Scale Thieves: returns 1 when one steals (amount in *stolen), 2 when one is caught (food back in *recovered) */
int   world_thieves(World *w, float dt, float px, float py, float pz, float reach, Attack atk, float carry, float *stolen, float *recovered);
int   world_blast(World *w, float x, float y, float z, float dx, float dz, float range, float cos_cone);
int   food_is_meat(int kind);
float food_growth(int kind);                                  /* growth points it gives */
int   world_push_out(const World *w, float *x, float *y, float *z, float r, int air);   /* keep a body of radius r out of trees, rocks and spires; 1 if pushed */
float world_fbm(float x, float y, unsigned seed);   /* layered noise 0..1, for scenery builders */
/* Signal Towers: damage the tower an attack lands on. Returns the tower index when one falls (its robots switch off), else -1 */
int   world_hit_tower(World *w, Attack atk, float damage);
int   world_towers_down(const World *w);
int   world_region_at(const World *w, float x, float z);      /* the named region a point is in (nearest region middle) */
/* chunks: which chunk a point is in, and the box of chunks within r of a point */
static inline int world_chunk_xy(float v) { int c = (int)((v + W_SIZE / 2) / W_CHS); return c < 0 ? 0 : c >= W_CHN ? W_CHN - 1 : c; }
static inline int world_chunk_of(float x, float z) { return world_chunk_xy(z) * W_CHN + world_chunk_xy(x); }
#define WORLD_FOR_CHUNKS(X, Z, R, CVAR) \
    for (int _cz = world_chunk_xy((Z) - (R)), _cz1 = world_chunk_xy((Z) + (R)), _cx0 = world_chunk_xy((X) - (R)), _cx1 = world_chunk_xy((X) + (R)); _cz <= _cz1; _cz++) \
        for (int _cx = _cx0, CVAR = _cz * W_CHN + _cx; _cx <= _cx1; _cx++, CVAR = _cz * W_CHN + _cx)
#endif
