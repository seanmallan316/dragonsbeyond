/* Pathfinding with Recast & Detour (github.com/recastnavigation, zlib licence): a walkable mesh is baked from each world's
   ground and scenery when you enter it, and the ground enemies, bosses and your friend dragon find their way round trees,
   rocks, crystals, ponds and cliffs on it instead of walking into them. */
#ifndef DB_NAV_H
#define DB_NAV_H
#ifdef __cplusplus
extern "C" {
#endif
#include "world.h"
int  nav_build(const World *w);   /* returns 1 if a walkable mesh was made (a little under a second) */
void nav_clear(void);
int  nav_ready(void);
/* where agent `id` (0..31) should head next to walk from (sx, sz) to (tx, tz): writes the next corner of the path.
   Returns 1 if it found a path, 0 if not (no mesh, or one end is off the walkable ground): then go straight. */
int  nav_steer(int id, float sx, float sy, float sz, float tx, float ty, float tz, float *ox, float *oz);
#ifdef __cplusplus
}
#endif
#endif
