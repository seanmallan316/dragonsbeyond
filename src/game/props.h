// props.h - scenery made from real 3D models (content/models/<name>.kkp + _tex.png): trees, rocks, bushes...
// Static kinds are baked into meshes per map cell at two detail levels; per-instance kinds are drawn one by one with a tint.
#ifndef DB_PROPS_H
#define DB_PROPS_H
#include "../render/gfx.h"
#include "world.h"
#define KKP_LODS 2
typedef struct { int ok, ntri[KKP_LODS]; Vertex *v[KKP_LODS]; Texture *tex; Mesh *mesh[KKP_LODS]; float radius; } PropModel;
int  propmodel_load(PropModel *m, const char *name);
/* a copy of the model's lod placed at (x, y, z), turned yaw, scaled s, appended to a vertex list */
void propmodel_place(const PropModel *m, int lod, float x, float y, float z, float yaw, float s, Vertex *out, uint32_t *n, uint32_t cap);
/* draw one instance on its own (bushes that can be eaten, mushrooms) */
void propmodel_draw(const PropModel *m, int lod, float x, float y, float z, float yaw, float s, float r, float g, float b);

/* a layer draws every instance of one kind on its own, near ones in high detail; far ones (beyond skip_dist) not at all */
#define SC_MAX 320
typedef struct { float x, y, z, yaw, s; } SceneInst;
typedef struct { PropModel *model; int n; SceneInst inst[SC_MAX]; } SceneLayer;
void scenelayer_build(SceneLayer *L, PropModel *model, const World *w, int kind, float height_scale, float sink);
void scenelayer_draw(const SceneLayer *L, Vec3 eye, float hi_dist, float skip_dist);
#endif
