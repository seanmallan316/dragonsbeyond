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
    static Vertex *buf; static uint32_t cap; uint32_t need = 0;
    for (int i = 0; i < w->nprop; i++) if (w->prop[i].kind == kind) need += (uint32_t)model->ntri[0] * 3;
    if (need > cap) { cap = need; buf = realloc(buf, sizeof(Vertex) * cap); }
    float cell = W_SIZE / SC_CELLS;
    for (int cz = 0; cz < SC_CELLS; cz++) for (int cx = 0; cx < SC_CELLS; cx++) { SceneCell *C = &L->cell[cz][cx]; C->cx = -W_SIZE / 2 + (cx + 0.5f) * cell; C->cz = -W_SIZE / 2 + (cz + 0.5f) * cell;
        for (int lod = 0; lod < 2; lod++) { uint32_t n = 0;
            for (int i = 0; i < w->nprop; i++) { const Prop *p = &w->prop[i]; if (p->kind != kind) continue;
                int px = (int)((p->x + W_SIZE / 2) / cell), pz = (int)((p->z + W_SIZE / 2) / cell); if (px < 0) px = 0; if (px >= SC_CELLS) px = SC_CELLS - 1; if (pz < 0) pz = 0; if (pz >= SC_CELLS) pz = SC_CELLS - 1;
                if (px != cx || pz != cz) continue;
                propmodel_place(model, lod, p->x, world_height(w, p->x, p->z) - sink * p->scale, p->z, p->yaw, p->scale * height_scale, buf, &n, cap); }
            Mesh *m = n ? gfx_mesh_static(buf, n) : NULL; if (lod == 0) C->hi = m; else C->lo = m; } } }
void scenelayer_draw(const SceneLayer *L, Vec3 eye, float hi_dist) {
    if (!L->model || !L->model->ok) return; gfx_bind(L->model->tex); Mat4 id = m4_identity();
    for (int cz = 0; cz < SC_CELLS; cz++) for (int cx = 0; cx < SC_CELLS; cx++) { const SceneCell *C = &L->cell[cz][cx]; float dx = C->cx - eye.x, dz = C->cz - eye.z; float d = sqrtf(dx * dx + dz * dz);
        Mesh *m = d < hi_dist ? C->hi : C->lo; if (m) gfx_draw(m, &id, 1, 1, 1, 1); } }
