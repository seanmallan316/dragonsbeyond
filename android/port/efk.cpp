// efk.cpp - Effekseer effects (github.com/effekseer/Effekseer, MIT) for the phone version and the PC test build.
// The game asks for an effect by name ("VFX_Hit_Heavy"); it is loaded once from content/efk/<name>.efk and played in the
// 3D scene after everything else (it reads the scene's depth, so it sits behind walls properly). The Wii U build has no
// Effekseer: there the gfx_fx_* calls answer 0 and the game keeps drawing its own particles.
#include <Effekseer.h>
#include <EffekseerRendererGL.h>
#include <string.h>
#include <stdio.h>
#include <string>
extern "C" {
#include "render/gfx.h"
}
static Effekseer::ManagerRef g_mgr; static EffekseerRendererGL::RendererRef g_ren; static int g_ok = -1;
#define EFK_MAX 40
static struct { char name[48]; Effekseer::EffectRef fx; int tried; } g_fx[EFK_MAX]; static int g_nfx;
static float g_pend_dt;
static int efk_start(void) {
    if (g_ok >= 0) return g_ok; g_ok = 0;
    g_ren = EffekseerRendererGL::Renderer::Create(12000, EffekseerRendererGL::OpenGLDeviceType::OpenGLES3); if (!g_ren) { gfx_log("effects: no renderer"); return 0; }
    g_mgr = Effekseer::Manager::Create(3000); if (!g_mgr) return 0;
    g_mgr->SetSpriteRenderer(g_ren->CreateSpriteRenderer()); g_mgr->SetRibbonRenderer(g_ren->CreateRibbonRenderer()); g_mgr->SetRingRenderer(g_ren->CreateRingRenderer());
    g_mgr->SetTrackRenderer(g_ren->CreateTrackRenderer()); g_mgr->SetModelRenderer(g_ren->CreateModelRenderer());
    g_mgr->SetTextureLoader(g_ren->CreateTextureLoader()); g_mgr->SetModelLoader(g_ren->CreateModelLoader());
    g_mgr->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>());
    g_mgr->SetCoordinateSystem(Effekseer::CoordinateSystem::RH);
    g_ok = 1; gfx_log("effects: Effekseer ready"); return 1; }
static Effekseer::EffectRef find_fx(const char *name) {
    for (int i = 0; i < g_nfx; i++) if (!strcmp(g_fx[i].name, name)) return g_fx[i].fx;
    if (g_nfx >= EFK_MAX) return nullptr;
    int i = g_nfx++; snprintf(g_fx[i].name, sizeof g_fx[i].name, "%s", name);
    std::string p = std::string(KK_CONTENT_DIR) + "efk/" + name + ".efk"; std::u16string up(p.begin(), p.end());
    g_fx[i].fx = Effekseer::Effect::Create(g_mgr, up.c_str(), 1.0f);
    gfx_log("effects: %s %s", name, g_fx[i].fx ? "loaded" : "missing"); return g_fx[i].fx; }
static Effekseer::Matrix43 place(float x, float y, float z, float yaw, float s) {
    Effekseer::Matrix43 m; float c = cosf(yaw) * s, sn = sinf(yaw) * s;
    m.Value[0][0] = c;   m.Value[0][1] = 0; m.Value[0][2] = -sn;
    m.Value[1][0] = 0;   m.Value[1][1] = s; m.Value[1][2] = 0;
    m.Value[2][0] = sn;  m.Value[2][1] = 0; m.Value[2][2] = c;
    m.Value[3][0] = x;   m.Value[3][1] = y; m.Value[3][2] = z; return m; }
extern "C" int gfx_fx_ok(void) { return efk_start(); }
extern "C" int gfx_fx_play(const char *name, float x, float y, float z, float yaw, float scale) {
    if (!efk_start()) return 0; Effekseer::EffectRef e = find_fx(name); if (!e) return 0;
    Effekseer::Handle h = g_mgr->Play(e, 0, 0, 0); g_mgr->SetMatrix(h, place(x, y, z, yaw, scale)); return h + 1; }
extern "C" void gfx_fx_move(int h, float x, float y, float z, float yaw, float scale) { if (g_ok == 1 && h > 0) g_mgr->SetMatrix(h - 1, place(x, y, z, yaw, scale)); }
extern "C" void gfx_fx_color(int h, float r, float g, float b, float a) {
    if (g_ok != 1 || h <= 0) return; auto c8 = [](float v) { v = v < 0 ? 0 : v > 1 ? 1 : v; return (uint8_t)(v * 255); };
    g_mgr->SetAllColor(h - 1, Effekseer::Color(c8(r), c8(g), c8(b), c8(a))); }
extern "C" void gfx_fx_input(int h, int i, float v) { if (g_ok == 1 && h > 0) g_mgr->SetDynamicInput(h - 1, i, v); }
extern "C" void gfx_fx_stop(int h) { if (g_ok == 1 && h > 0) g_mgr->StopRoot(h - 1); }   /* stop spawning; what is out fades on its own */
extern "C" int gfx_fx_alive(int h) { return g_ok == 1 && h > 0 && g_mgr->Exists(h - 1); }
extern "C" void gfx_fx_update(float dt) { if (g_ok != 1) return; g_pend_dt += dt; }
extern "C" void gfx_fx_draw(const Mat4 *view, const Mat4 *proj) {
    if (g_ok != 1) return;
    if (g_pend_dt > 0) { float f = g_pend_dt * 60.0f; if (f > 6) f = 6; g_mgr->Update(f); g_pend_dt = 0; }   /* Effekseer counts in 60ths of a second */
    Effekseer::Matrix44 v, p; memcpy(v.Values, view->m, sizeof v.Values); memcpy(p.Values, proj->m, sizeof p.Values);
    g_ren->SetProjectionMatrix(p); g_ren->SetCameraMatrix(v);
    g_ren->BeginRendering(); Effekseer::Manager::DrawParameter dp; dp.ZNear = 0; dp.ZFar = 1; dp.ViewProjectionMatrix = g_ren->GetCameraProjectionMatrix(); g_mgr->Draw(dp); g_ren->EndRendering();
    gfx_state_reset(); }
