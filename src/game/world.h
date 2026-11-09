// world.h - Everleaf Wilds: rolling ground, trees, bushes, rocks, and the food that grows a dragon. Pure logic, no graphics
// (the renderer builds its meshes from these tables), so it can be tested on a PC.
#ifndef DB_WORLD_H
#define DB_WORLD_H
#include "../render/kkmath.h"

#define W_SIZE 240.0f          /* the patch is W_SIZE x W_SIZE metres, centred on the origin */
#define W_GRID 96              /* height samples per side */
#define W_MAXPROPS 700
#define W_MAXFOOD 160

enum { P_TREE = 0, P_PALM, P_BUSH, P_ROCK, P_FERN, P_N };
enum { F_BERRY = 0, F_MUSHROOM, F_GRUB, F_FISH, F_N };   /* berries and mushrooms are plants; grubs and fish are meat */

typedef struct { int kind; float x, z, yaw, scale; } Prop;
typedef struct { int kind, alive; float x, y, z, yaw, regrow, wander, phase; } Food;   /* regrow: seconds until it is back; grubs wander */

typedef struct {
    float h[W_GRID][W_GRID];
    Prop prop[W_MAXPROPS]; int nprop;
    Food food[W_MAXFOOD]; int nfood;
    float pond_x, pond_z, pond_r;   /* a round pond, water at y = pond_y */
    float pond_y;
    unsigned seed;
    float crystal_x, crystal_y, crystal_z; int crystal_found;   /* the hidden egg crystal (one per world) */
} World;

void  world_make(World *w, unsigned seed);
float world_height(const World *w, float x, float z);        /* ground height, smooth */
Vec3  world_normal(const World *w, float x, float z);
int   world_in_pond(const World *w, float x, float z);
void  world_update(World *w, float dt, float px, float pz);   /* regrowth, grubs wandering away from the player */
int   world_nearest_food(const World *w, float x, float z, float reach);   /* index or -1 */
int   food_is_meat(int kind);
float food_growth(int kind);                                  /* growth points it gives */
#endif
