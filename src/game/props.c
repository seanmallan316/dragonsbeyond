// props.c - loading and placing scenery models (see props.h).
#include "props.h"
#include "../render/assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static uint32_t rd32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static float rdf(const uint8_t *p) { uint32_t u = rd32(p); float f; memcpy(&f, &u, 4); return f; }
int propmodel_load(PropModel *m, const char *name) {
    memset(m, 0, sizeof *m); char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "models/%s.kkp", name);
    FILE *f = fopen(path, "rb"); if (!f) { gfx_log("prop %s: no file", name); return 0; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET); uint8_t *buf = malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return 0; } fclose(f);
    if (sz < 8 || memcmp(buf, "KKP1", 4)) { free(buf); return 0; }
    int nlod = (int)rd32(buf + 4); if (nlod > KKP_LODS) nlod = KKP_LODS; size_t off = 8; float r2 = 0;
    for (int l = 0; l < nlod; l++) { int nt = (int)rd32(buf + off); off += 4; m->ntri[l] = nt; m->v[l] = malloc(sizeof(Vertex) * 3 * (size_t)nt);
        for (int i = 0; i < nt * 3; i++, off += 32) { Vertex *o = &m->v[l][i]; o->px = rdf(buf + off); o->py = rdf(buf + off + 4); o->pz = rdf(buf + off + 8); o->nx = rdf(buf + off + 12); o->ny = rdf(buf + off + 16); o->nz = rdf(buf + off + 20); o->u = rdf(buf + off + 24); o->v = rdf(buf + off + 28); o->r = o->g = o->b = o->a = 1;
            float d = o->px * o->px + o->py * o->py + o->pz * o->pz; if (d > r2) r2 = d; }
        m->mesh[l] = gfx_mesh_static(m->v[l], (uint32_t)(3 * nt)); }
    for (int l = nlod; l < KKP_LODS; l++) { m->ntri[l] = m->ntri[nlod - 1]; m->v[l] = m->v[nlod - 1]; m->mesh[l] = m->mesh[nlod - 1]; }
    free(buf); m->radius = sqrtf(r2);
    snprintf(path, sizeof path, "models/%s_tex.png", name); m->tex = asset_texture_file(path, 1, NULL, NULL);
    m->ok = m->tex != NULL && m->ntri[0] > 0; gfx_log("prop %s: %s (%d / %d triangles)", name, m->ok ? "loaded" : "failed", m->ntri[0], m->ntri[1]); return m->ok; }

void propmodel_place(const PropModel *m, int lod, float x, float y, float z, float yaw, float s, Vertex *out, uint32_t *n, uint32_t cap) {
    float c = cosf(yaw), sn = sinf(yaw); const Vertex *v = m->v[lod]; int cnt = m->ntri[lod] * 3; if (*n + (uint32_t)cnt > cap) return;
    for (int i = 0; i < cnt; i++) { Vertex o = v[i]; float px = o.px * s, py = o.py * s, pz = o.pz * s;
        o.px = x + px * c + pz * sn; o.py = y + py; o.pz = z - px * sn + pz * c; float nx = o.nx, nz = o.nz; o.nx = nx * c + nz * sn; o.nz = -nx * sn + nz * c; out[(*n)++] = o; } }
void propmodel_draw(const PropModel *m, int lod, float x, float y, float z, float yaw, float s, float r, float g, float b) {
    if (!m->ok) return; Mat4 mm = m4_mul(m4_mul(m4_translate(x, y, z), m4_rotate_y(yaw)), m4_scale(s, s, s)); gfx_bind(m->tex); gfx_draw(m->mesh[lod], &mm, r, g, b, 1); }

void scenelayer_build(SceneLayer *L, PropModel *model, const World *w, int kind, float height_scale, float sink) {
    memset(L, 0, sizeof *L); L->model = model; if (!model->ok) return;
    for (int i = 0; i < w->nprop && L->n < SC_MAX; i++) { const Prop *p = &w->prop[i]; if (p->kind != kind) continue;
        SceneInst *I = &L->inst[L->n++]; I->x = p->x; I->z = p->z; I->y = world_height(w, p->x, p->z) - sink * p->scale; I->yaw = p->yaw; I->s = p->scale * height_scale; } }
void scenelayer_draw(const SceneLayer *L, Vec3 eye, float hi_dist, float skip_dist) {
    if (!L->model || !L->model->ok) return; gfx_bind(L->model->tex);
    for (int i = 0; i < L->n; i++) { const SceneInst *I = &L->inst[i]; float dx = I->x - eye.x, dz = I->z - eye.z; float d2 = dx * dx + dz * dz;
        if (skip_dist > 0 && d2 > skip_dist * skip_dist) continue;
        Mat4 mm = m4_mul(m4_mul(m4_translate(I->x, I->y, I->z), m4_rotate_y(I->yaw)), m4_scale(I->s, I->s, I->s)); gfx_draw(L->model->mesh[d2 < hi_dist * hi_dist ? 0 : 1], &mm, 1, 1, 1, 1); } }
