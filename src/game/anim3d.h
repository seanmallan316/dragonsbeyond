// anim3d.h - a character with real animations (Goliath): every clip baked into frames by tools/model3d (from a rigged,
// animated model), played back smoothly by blending between frames. content/models/<name>.kka + <name>_anim.png.
#ifndef KK_ANIM3D_H
#define KK_ANIM3D_H
#include "../render/gfx.h"
#include "../render/kkmath.h"
#define A3_MAXCLIPS 16
typedef struct { char name[16]; int nf, loop; float dur; uint8_t *data; } A3Clip;   /* data: nf frames x nv corners x 10 bytes (position 3 x int16 / 4000 m, normal 3 x int8 / 127) */
typedef struct { int ok, nv, nt, nc; float *uv; uint32_t *idx; A3Clip clip[A3_MAXCLIPS]; Texture *tex; Mesh *mesh; Vertex *vb; int last_c, filled; float last_t, last_bend; } Anim3D;
int anim3d_load(Anim3D *a, const char *name);
/* the same body and moves as 'geom' (shared, not copied) with another skin: models/<texname>_anim.png. For dragons that
   share one shape and differ only in colour. */
int anim3d_skin(Anim3D *a, const Anim3D *geom, const char *texname);
int anim3d_clip(const Anim3D *a, const char *name);   /* -1 if missing */
/* poses it 't' seconds into clip c (looping clips wrap, the others hold their last frame) and draws it at 'base' */
/* bend the posed body (model space, front +z): arc = curve into a turn (-1..1, + = toward +x), side/up = a travelling
   wave down the body (snake-like) sideways / up-down, phase in radians. Stays set until changed. */
void anim3d_set_bend(float arc, float side, float up, float phase);
void anim3d_draw(Anim3D *a, int c, float t, const Mat4 *base, float r, float g, float b, float alpha);
#endif
