// gfx_stub.c - a renderer that draws nothing, so the whole game compiles and runs on a PC for tests. Counts draw calls.
#include "../src/render/gfx.h"
#include "../src/render/assets.h"
#include "../src/game/game.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
struct Mesh { uint32_t n; }; struct Texture { int w, h; };
int g_stub_draws, g_stub_frames;
int gfx_init(void) { return GFX_OK; } void gfx_shutdown(void) {}
void gfx_log(const char *fmt, ...) { va_list a; va_start(a, fmt); vprintf(fmt, a); va_end(a); printf("\n"); }
Mesh *gfx_mesh_static(const Vertex *v, uint32_t n) { (void)v; Mesh *m = calloc(1, sizeof *m); m->n = n; return m; }
Mesh *gfx_mesh_dynamic(uint32_t c) { Mesh *m = calloc(1, sizeof *m); m->n = c; return m; }
void gfx_mesh_update(Mesh *m, const Vertex *v, uint32_t n) { (void)v; if (n > m->n) { printf("STUB: mesh overflow %u > %u\n", n, m->n); exit(2); } }
void gfx_mesh_free(Mesh *m) { free(m); }
void gfx_sync(void) {}
Texture *gfx_texture_rgba(const uint32_t *p, int w, int h, int r, int n) { (void)p; (void)r; (void)n; Texture *t = calloc(1, sizeof *t); t->w = w; t->h = h; return t; }
Texture *gfx_texture_rgba_ex(const uint32_t *p, int w, int h, int r, int n, int mm) { (void)mm; return gfx_texture_rgba(p, w, h, r, n); }
void gfx_texture_free(Texture *t) { free(t); }
Texture *gfx_texture_stream(int w, int h) { return gfx_texture_rgba(NULL, w, h, 0, 0); }
void gfx_texture_stream_put(Texture *t, const uint32_t *p, int w, int h) { (void)t; (void)p; (void)w; (void)h; }
void *gfx_texture_stream_pixels(Texture *t, int *pitch) { (void)t; *pitch = 0; return NULL; }
void gfx_texture_stream_flush_cpu(Texture *t) { (void)t; } void gfx_texture_stream_gpu(Texture *t) { (void)t; }
void gfx_frame_begin(void) { g_stub_frames++; } void gfx_view_begin(int d, float r, float g, float b) { (void)d; (void)r; (void)g; (void)b; } void gfx_view_end(int d) { (void)d; } void gfx_frame_end(void) {}
float gfx_aspect(int d) { return d ? 854.0f / 480 : 1280.0f / 720; }
void gfx_viewport(int d, float a, float b, float c, float e) { (void)d; (void)a; (void)b; (void)c; (void)e; }
void gfx_camera(const Mat4 *vp, Vec3 l, float am, float r, float g, float b, float s, float e) { (void)vp; (void)l; (void)am; (void)r; (void)g; (void)b; (void)s; (void)e; }
void gfx_clear_depth(int d) { (void)d; }
int gfx_shadow_begin(const Mat4 *l) { (void)l; return 1; } void gfx_shadow_end(void) {}
Texture *gfx_grab(int d) { (void)d; return NULL; } Texture *gfx_grab_ex(int s, int d) { (void)s; (void)d; return NULL; }
void gfx_bump(int l) { (void)l; } void gfx_shadow_use(int on) { (void)on; } void gfx_shadow_none(void) {} Texture *gfx_shadow_texture(void) { return NULL; }
void gfx_set_time(float t) { (void)t; } void gfx_camera_2d(void) {} void gfx_grade(float a, float b, float c, float d) { (void)a; (void)b; (void)c; (void)d; }
void gfx_camera_ui3d(float e) { (void)e; } void gfx_camera_ui3d_v(float e, float w, float h) { (void)e; (void)w; (void)h; } void gfx_camera_2d_virtual(float w, float h) { (void)w; (void)h; }
void gfx_bind(Texture *t) { (void)t; } void gfx_state(int f) { (void)f; }
void gfx_light(Vec3 a, Vec3 b, Vec3 c, Vec3 d) { (void)a; (void)b; (void)c; (void)d; }
void gfx_draw(const Mesh *m, const Mat4 *mm, float r, float g, float b, float a) { (void)m; (void)mm; (void)r; (void)g; (void)b; (void)a; g_stub_draws++; }
void gfx_draw_ex(const Mesh *m, const Mat4 *mm, float r, float g, float b, float a, float s, int u, float su, float sv) { (void)s; (void)u; (void)su; (void)sv; gfx_draw(m, mm, r, g, b, a); }
void platform_rumble(float s, float t) { (void)s; (void)t; } void platform_rumble_pad(int p, float s, float t) { (void)p; (void)s; (void)t; }
