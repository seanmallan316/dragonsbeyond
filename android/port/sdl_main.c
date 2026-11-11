// sdl_main.c - Dragons Beyond on Android phones (and on a PC for testing). This replaces the Wii U's
// platform/wiiu_main.c: it opens the screen, plays the sound, reads the touch screen and Bluetooth controllers,
// and runs the Wii U game's own frame (game_frame) - the whole game, unchanged.
//
// Phone controls while playing: a move stick on the left (put your thumb down anywhere there and slide), RUN beside it;
// on the right FLY / JUMP (A, hold to glide), EAT (X), TAIL (B) and ROAR / POWER (Y); drag anywhere else on the right
// to turn the camera; SWAP (+) and CAMERA view at the top. In the menus: the stick, OK (A), BACK (B) and ERASE (X).
// A Bluetooth controller works like a Wii U Pro Controller: A, B, X, Y, ZR run, + swap, d-pad up camera, right stick looks.
#include <SDL.h>
#include "gles.h"
#include "port.h"
#include "render/gfx.h"
#include "game/game.h"
#include "audio/sfx.h"
#include "audio/music.h"
#include "third_party/stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>

void gfx_set_logfile(FILE *f);
static SDL_Window *g_win; static SDL_GLContext g_ctx;
static int g_sw = 1280, g_sh = 720;           /* the screen */
static int g_tw = 1280, g_th = 720;           /* the game's 16:9 picture */
static int g_ox = 0, g_oy = 0;                /* where the picture sits on the screen */
static int g_playing = 0, g_swapmenu = 0, g_touch_mode = 1;   /* out in the world (not the menus); touch used last (else a controller) */
static FILE *g_log;
static int g_shot_now; static char g_shot_path[512];   /* test runs: save this frame's picture */

void port_log(const char *fmt, ...) {
    char buf[1024]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    SDL_Log("%s", buf);
}

/* ------------------------------------------------------------------ the picture's place on the screen */
/* the whole screen is the game: the 3D world fills a wide phone edge to edge, and the game's words and panels (made for a
   16:9 TV) stay in a 16:9 area in the middle (gfx_gles.c), so nothing is stretched */
static void layout(void) {
    SDL_GL_GetDrawableSize(g_win, &g_sw, &g_sh);
    float asp = (float)g_sw / (float)(g_sh > 0 ? g_sh : 1); if (asp < 1.3f) asp = 1.3f; if (asp > 2.6f) asp = 2.6f;
    g_th = g_sh > 1080 ? 1080 : g_sh;   /* more than full HD costs battery for no visible gain on a phone */
    g_tw = (int)(g_th * asp + 0.5f);
    int dh = g_sh, dw = (int)(dh * asp + 0.5f); if (dw > g_sw) { dw = g_sw; dh = (int)(dw / asp + 0.5f); }
    g_ox = (g_sw - dw) / 2; g_oy = (g_sh - dh) / 2;
}
/* the drawing size follows how smoothly the phone keeps up: full resolution when it can, a little less on a slower phone
   (the picture is still drawn with smooth edges and scaled up to the whole screen) */
static float g_res_scale = 1.0f;
void port_target_size(int *w, int *h) { *h = (int)(g_th * g_res_scale) & ~1; *w = (int)(g_tw * g_res_scale) & ~1; if (*h < 360) { *h = 360; *w = g_tw * 360 / g_th; } }
static void res_adapt(float dt) {
    static float acc; static int n; static float settle = 3.0f;   /* wait a few seconds after start or a change before judging */
    if (settle > 0) { settle -= dt; acc = 0; n = 0; return; }
    acc += dt; if (++n < 90) return;
    float ms = acc / n * 1000.0f; acc = 0; n = 0;
    float old = g_res_scale;
    if (ms > 21.0f && g_res_scale > 0.6f) g_res_scale -= 0.1f;          /* below about 48 fps: draw smaller */
    else if (ms < 15.5f && g_res_scale < 1.0f) g_res_scale += 0.1f;     /* plenty of room: back up */
    if (g_res_scale > 1.0f) g_res_scale = 1.0f;
    if (g_res_scale != old) { settle = 4.0f; port_log("drawing size %.0f%% (%.1f ms a frame)", g_res_scale * 100, ms); }
}

/* ------------------------------------------------------------------ touch controls */
typedef struct { const char *file, *menu_file; float cx, cy, r; int key; int in_menu; float mx, my; GLuint tex, menu_tex; int down; } TButton;   /* mx, my: where it sits in the menus */   /* cx, cy, r: screen fractions (r of the height) */
enum { K_A, K_B, K_X, K_Y, K_PLUS, K_CAM };
/* laid out for a modern phone (about 20:9): the 16:9 picture sits in the middle and the right-hand buttons lean out into the
   carved side panel, so they cover as little of the game as possible. Pushing the move stick all the way runs (no RUN button). */
/* laid out round the right thumb, above the game's message box at the bottom: FLY in the corner, the others in an arc above it */
static TButton BTN[] = {
    {"btn_fly", "btn_ok", 0.928f, 0.790f, 0.145f, K_A, 1, 0.928f, 0.790f},
    {"btn_eat", "btn_erase", 0.800f, 0.610f, 0.090f, K_X, 1, 0.950f, 0.300f},
    {"btn_tail", "btn_back", 0.912f, 0.485f, 0.082f, K_B, 1, 0.950f, 0.530f},
    {"btn_roar", NULL, 0.790f, 0.380f, 0.075f, K_Y, 0},
    {"btn_cam", NULL, 0.955f, 0.095f, 0.060f, K_CAM, 0},
    {"btn_swap", NULL, 0.955f, 0.260f, 0.060f, K_PLUS, 0},
};
#define NBTN (int)(sizeof BTN / sizeof BTN[0])
static GLuint g_stick_base, g_stick_knob, g_panel_l, g_panel_r, g_share_tex; static int g_stick_finger = -1, g_look_finger = -1; static float g_stick_x0, g_stick_y0, g_stick_dx, g_stick_dy, g_look_x0, g_look_dx;
static SDL_FingerID g_fid[16]; static float g_fx[16], g_fy[16]; static int g_fdown[16];
static GLuint g_ov_prog, g_ov_vao, g_ov_vbo; static GLint g_ov_tex, g_ov_alpha;

static GLuint load_png(const char *name) {
    char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "port_ui/%s.png", name);
    FILE *f = fopen(path, "rb"); if (!f) { port_log("no picture %s", path); return 0; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); unsigned char *raw = malloc((size_t)n); fread(raw, 1, (size_t)n, f); fclose(f);
    int w, h, c; unsigned char *px = stbi_load_from_memory(raw, (int)n, &w, &h, &c, 4); free(raw); if (!px) return 0;
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px); glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(px); return t;
}
static void overlay_init(void) {
    const char *vs = "#version 300 es\nlayout(location=0) in vec2 p; layout(location=1) in vec2 uv; out vec2 v; void main(){ v = uv; gl_Position = vec4(p, 0.0, 1.0); }\n";
    const char *fs = "#version 300 es\nprecision mediump float; in vec2 v; uniform sampler2D t; uniform float a; out vec4 o; void main(){ o = texture(t, v) * vec4(1.0, 1.0, 1.0, a); }\n";
    GLuint s1 = glCreateShader(GL_VERTEX_SHADER); glShaderSource(s1, 1, &vs, NULL); glCompileShader(s1);
    GLuint s2 = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(s2, 1, &fs, NULL); glCompileShader(s2);
    g_ov_prog = glCreateProgram(); glAttachShader(g_ov_prog, s1); glAttachShader(g_ov_prog, s2); glLinkProgram(g_ov_prog);
    g_ov_tex = glGetUniformLocation(g_ov_prog, "t"); g_ov_alpha = glGetUniformLocation(g_ov_prog, "a");
    glGenVertexArrays(1, &g_ov_vao); glBindVertexArray(g_ov_vao); glGenBuffers(1, &g_ov_vbo); glBindBuffer(GL_ARRAY_BUFFER, g_ov_vbo);
    glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float) * 6, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glBindVertexArray(0);
    for (int i = 0; i < NBTN; i++) { BTN[i].tex = load_png(BTN[i].file); BTN[i].menu_tex = BTN[i].menu_file ? load_png(BTN[i].menu_file) : BTN[i].tex; }
    g_stick_base = load_png("stick_base"); g_stick_knob = load_png("stick_knob"); g_panel_l = load_png("panel_left"); g_panel_r = load_png("panel_right"); g_share_tex = load_png("btn_share");
}
static void quad(GLuint tex, float cx, float cy, float r, float alpha) {   /* screen pixels: centre and radius */
    if (!tex) return;
    float x0 = (cx - r) / g_sw * 2 - 1, x1 = (cx + r) / g_sw * 2 - 1, y0 = 1 - (cy - r) / g_sh * 2, y1 = 1 - (cy + r) / g_sh * 2;
    float v[24] = {x0, y0, 0, 0, x1, y0, 1, 0, x1, y1, 1, 1, x0, y0, 0, 0, x1, y1, 1, 1, x0, y1, 0, 1};
    glBindBuffer(GL_ARRAY_BUFFER, g_ov_vbo); glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof v, v);
    glBindTexture(GL_TEXTURE_2D, tex); glUniform1f(g_ov_alpha, alpha); glDrawArrays(GL_TRIANGLES, 0, 6);
}
static void rect(GLuint tex, float x0p, float y0p, float x1p, float y1p, float alpha) {   /* screen pixels */
    if (!tex) return;
    float x0 = x0p / g_sw * 2 - 1, x1 = x1p / g_sw * 2 - 1, y0 = 1 - y0p / g_sh * 2, y1 = 1 - y1p / g_sh * 2;
    float v[24] = {x0, y0, 0, 0, x1, y0, 1, 0, x1, y1, 1, 1, x0, y0, 0, 0, x1, y1, 1, 1, x0, y1, 0, 1};
    glBindBuffer(GL_ARRAY_BUFFER, g_ov_vbo); glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof v, v);
    glBindTexture(GL_TEXTURE_2D, tex); glUniform1f(g_ov_alpha, alpha); glDrawArrays(GL_TRIANGLES, 0, 6);
}
static int btn_visible(int i) { if (!g_touch_mode) return 0; return g_playing || BTN[i].in_menu; }
static float bx(int i) { return (g_playing ? BTN[i].cx : BTN[i].mx) * g_sw; }
static float by(int i) { return (g_playing ? BTN[i].cy : BTN[i].my) * g_sh; }
static float br(int i) { return BTN[i].r * g_sh; }

void port_present(unsigned tex, int w, int h) {
    (void)tex;
    glBindFramebuffer(GL_FRAMEBUFFER, 0); glDisable(GL_SCISSOR_TEST); glViewport(0, 0, g_sw, g_sh);
    glClearColor(0.02f, 0.015f, 0.04f, 1); glClear(GL_COLOR_BUFFER_BIT);
    int dw = g_sw - 2 * g_ox, dh = g_sh - 2 * g_oy;
    extern unsigned gfx_port_resolved_fbo(void);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gfx_port_resolved_fbo()); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, w, h, g_ox, g_oy, g_ox + dw, g_oy + dh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    /* the touch controls over the picture */
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glEnable(GL_BLEND); glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(g_ov_prog); glUniform1i(g_ov_tex, 0); glActiveTexture(GL_TEXTURE0); glBindVertexArray(g_ov_vao);
    if (g_ox > 8) { float pw = g_sh * 0.25f; if (pw < g_ox + 6) pw = g_ox + 6;   /* carved panels in the bars beside the picture, their gold edge just over it */
        rect(g_panel_l, g_ox + 6 - pw, 0, g_ox + 6, g_sh, 1); rect(g_panel_r, g_sw - g_ox - 6, 0, g_sw - g_ox - 6 + pw, g_sh, 1); }
    for (int i = 0; i < NBTN; i++) if (btn_visible(i)) quad(g_playing ? BTN[i].tex : (g_swapmenu && BTN[i].key == K_X && g_share_tex ? g_share_tex : BTN[i].menu_tex), bx(i), by(i), br(i) * (BTN[i].down ? 0.94f : 1.0f), BTN[i].down ? 1.0f : 0.8f);
    if (g_touch_mode) {
        float R = 0.16f * g_sh, sx = g_stick_finger >= 0 ? g_stick_x0 : 0.12f * g_sw, sy = g_stick_finger >= 0 ? g_stick_y0 : 0.62f * g_sh;
        quad(g_stick_base, sx, sy, R, g_stick_finger >= 0 ? 0.85f : 0.45f); quad(g_stick_knob, sx + g_stick_dx * R, sy + g_stick_dy * R, R * 0.5f, 0.95f);
    }
    glBindVertexArray(0);
    if (g_shot_now) {   /* a test run: save the whole screen as shown (the game's picture and the touch controls) */
        unsigned char *px = malloc((size_t)g_sw * g_sh * 4); glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, g_sw, g_sh, GL_RGBA, GL_UNSIGNED_BYTE, px);
        FILE *f = fopen(g_shot_path, "wb");
        if (f) { fprintf(f, "P6\n%d %d\n255\n", g_sw, g_sh);
            for (int y = g_sh - 1; y >= 0; y--) for (int x = 0; x < g_sw; x++) fwrite(px + ((size_t)y * g_sw + x) * 4, 1, 3, f);
            fclose(f); }
        free(px); port_log("picture %s - %s", g_shot_path, game_where());
    }
    { static int n; GLenum er = glGetError(); if (er && n++ < 20) port_log("GL error 0x%x after frame", er); }
    SDL_GL_SwapWindow(g_win);
}

static int finger_slot(SDL_FingerID id, int make) {
    for (int i = 0; i < 16; i++) if (g_fdown[i] && g_fid[i] == id) return i;
    if (!make) return -1;
    for (int i = 0; i < 16; i++) if (!g_fdown[i]) { g_fid[i] = id; g_fdown[i] = 1; return i; }
    return -1;
}
static SDL_Haptic *g_haptic; static int g_haptic_tried;
static void haptic_tap(void) {   /* the feel of a real button under the thumb */
    if (!g_haptic_tried) { g_haptic_tried = 1; g_haptic = SDL_HapticOpen(0); if (g_haptic && SDL_HapticRumbleInit(g_haptic) != 0) { SDL_HapticClose(g_haptic); g_haptic = NULL; } }
    if (g_haptic) SDL_HapticRumblePlay(g_haptic, 0.25f, 12);
}
static int on_any_button(float x, float y) {
    for (int i = 0; i < NBTN; i++) if (btn_visible(i)) { float dx = x - bx(i), dy = y - by(i), r = br(i) * 1.15f; if (dx * dx + dy * dy < r * r) return 1; }
    return 0;
}
static void touch_event(SDL_Event *e) {
    float x = e->tfinger.x * g_sw, y = e->tfinger.y * g_sh;
    g_touch_mode = 1;
    if (e->type == SDL_FINGERDOWN) {
        int s = finger_slot(e->tfinger.fingerId, 1); if (s < 0) return;
        g_fx[s] = x; g_fy[s] = y;
        if (on_any_button(x, y)) return;
        if (g_stick_finger < 0 && x < g_sw * 0.45f && y > g_sh * 0.2f) { g_stick_finger = s; g_stick_x0 = x; g_stick_y0 = y; g_stick_dx = g_stick_dy = 0; }
        else if (g_playing && g_look_finger < 0 && x >= g_sw * 0.45f) { g_look_finger = s; g_look_x0 = x; g_look_dx = 0; }
    } else if (e->type == SDL_FINGERMOTION) {
        int s = finger_slot(e->tfinger.fingerId, 0); if (s < 0) return;
        g_fx[s] = x; g_fy[s] = y;
        if (s == g_stick_finger) { float R = 0.16f * g_sh, dx = (x - g_stick_x0) / R, dy = (y - g_stick_y0) / R, m = sqrtf(dx * dx + dy * dy);
            if (m > 1) { /* the stick follows the thumb when it slides past the edge */ g_stick_x0 += (dx / m) * (m - 1) * R; g_stick_y0 += (dy / m) * (m - 1) * R; dx /= m; dy /= m; }
            g_stick_dx = dx; g_stick_dy = dy; }
        if (s == g_look_finger) { float d = (x - g_look_x0) / (0.10f * g_sh); g_look_dx = d < -1 ? -1 : d > 1 ? 1 : d; }
    } else {
        int s = finger_slot(e->tfinger.fingerId, 0); if (s < 0) return;
        g_fdown[s] = 0;
        if (s == g_stick_finger) { g_stick_finger = -1; g_stick_dx = g_stick_dy = 0; }
        if (s == g_look_finger) { g_look_finger = -1; g_look_dx = 0; }
    }
}
/* which buttons are held right now: every finger on a button holds it (a thumb can slide from EAT onto FLY) */
static void touch_pad(Pad *p) {
    for (int i = 0; i < NBTN; i++) BTN[i].down = 0;
    for (int s = 0; s < 16; s++) if (g_fdown[s] && s != g_stick_finger && s != g_look_finger)
        for (int i = 0; i < NBTN; i++) if (btn_visible(i)) { float dx = g_fx[s] - bx(i), dy = g_fy[s] - by(i), r = br(i) * 1.15f; if (dx * dx + dy * dy < r * r) BTN[i].down = 1; }
    { static int was[16]; for (int i = 0; i < NBTN && i < 16; i++) { if (BTN[i].down && !was[i]) haptic_tap(); was[i] = BTN[i].down; } }
    for (int i = 0; i < NBTN; i++) if (BTN[i].down) switch (BTN[i].key) {
        case K_A: p->a = 1; break; case K_B: p->b = 1; break; case K_X: p->x = 1; break; case K_Y: p->y = 1; break;
        case K_CAM: p->up = 1; break; default: break; }
    { static int was_plus = 0; int now = 0; for (int i = 0; i < NBTN; i++) if (BTN[i].key == K_PLUS && BTN[i].down) now = 1; p->plus = now && !was_plus; was_plus = now; }   /* + is a press, not a hold */
    if (g_stick_finger >= 0) {
        float dx = g_stick_dx, dy = -g_stick_dy; float m = sqrtf(dx * dx + dy * dy);
        if (m < 0.12f) dx = dy = 0;
        p->sx = dx; p->sy = dy;
        if (g_playing && m > 0.93f) p->zr = 1;   /* stick pushed right to the edge: run */
        if (!g_playing) { p->up = p->up || dy > 0.6f; p->down = dy < -0.6f; p->left = dx < -0.6f; p->right = dx > 0.6f; }
    }
    if (g_look_finger >= 0) p->rx = fabsf(g_look_dx) < 0.1f ? 0 : g_look_dx;
    p->source = 1;   /* the phone is always there, like the GamePad */
}

/* ------------------------------------------------------------------ controllers */
static SDL_GameController *g_gc[4];
static void open_controllers(void) {
    for (int i = 0; i < SDL_NumJoysticks(); i++) if (SDL_IsGameController(i)) {
        int used = 0; SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(i);
        for (int k = 0; k < 4; k++) if (g_gc[k] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_gc[k])) == id) used = 1;
        if (used) continue;
        for (int k = 0; k < 4; k++) if (!g_gc[k]) { g_gc[k] = SDL_GameControllerOpen(i); port_log("controller %d: %s", k + 1, SDL_GameControllerName(g_gc[k])); break; }
    }
}
static void controller_pad(int k, Pad *w) {
    SDL_GameController *c = g_gc[k]; memset(w, 0, sizeof *w);
    if (!c || !SDL_GameControllerGetAttached(c)) { if (c) { SDL_GameControllerClose(c); g_gc[k] = NULL; } return; }
    float sx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f, sy = -SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) sx = -1;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) sx = 1;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP)) sy = 1;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) sy = -1;
    w->sx = fabsf(sx) < 0.12f ? 0 : (sx < -1 ? -1 : sx > 1 ? 1 : sx); w->sy = fabsf(sy) < 0.12f ? 0 : (sy < -1 ? -1 : sy > 1 ? 1 : sy);
    /* by the letters printed on the buttons (Nintendo, Xbox and PlayStation-style pads all work) */
    w->a = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A); w->b = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_B);
    w->x = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_X); w->y = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_Y);
    w->zl = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) || SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 12000;
    w->zr = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) || SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000;
    static int was[4]; int st = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_START); w->plus = st && !was[k]; was[k] = st;
    w->rx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX) / 32767.0f; if (fabsf(w->rx) < 0.15f) w->rx = 0;
    w->ry = -SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.0f; if (fabsf(w->ry) < 0.15f) w->ry = 0;
    w->up = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP); w->down = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    w->left = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT); w->right = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    w->source = 2;   /* a Pro Controller */
    if (w->a || w->b || w->x || w->y || w->zr || w->zl || w->plus || w->sx || w->sy || w->rx) g_touch_mode = 0;
}
static float g_rumble_left[KK_PADS];
void platform_rumble_pad(int slot, float strength, float seconds) {
    if (strength <= 0 || seconds <= 0 || slot < 0 || slot >= KK_PADS) return;
    Uint16 s = (Uint16)(strength > 1 ? 65535 : strength * 65535);
    if (slot == 0) { if (!g_haptic_tried) haptic_tap(); if (g_haptic && g_touch_mode) SDL_HapticRumblePlay(g_haptic, strength, (Uint32)(seconds * 1000)); }
    else if (g_gc[slot - 1]) SDL_GameControllerRumble(g_gc[slot - 1], s, s, (Uint32)(seconds * 1000));
    g_rumble_left[slot] = seconds;
}
void platform_rumble(float strength, float seconds) { platform_rumble_pad(0, strength, seconds); platform_rumble_pad(1, strength, seconds); }

/* ------------------------------------------------------------------ sound */
static void audio_cb(void *ud, Uint8 *stream, int len) {
    (void)ud; int n = len / 4; static int16_t l[4096], r[4096];
    while (n > 0) { int k = n > 4096 ? 4096 : n; sfx_render(l, r, k); music_mix(l, r, k);
        int16_t *o = (int16_t *)stream; for (int i = 0; i < k; i++) { o[i * 2] = l[i]; o[i * 2 + 1] = r[i]; }
        stream += k * 4; n -= k; }
}

/* ------------------------------------------------------------------ the game's files (Android: read straight from the app) */
#ifdef __ANDROID__
#include <ftw.h>
static int rm_one(const char *p, const struct stat *st, int flag, struct FTW *fw) { (void)st; (void)flag; (void)fw; remove(p); return 0; }
/* the game's files are read straight from the app now; the copy older versions unpacked onto the phone is removed to free the space */
static void remove_old_copy(void) {
    const char *in = SDL_AndroidGetInternalStoragePath(); if (!in) return;
    char p[400]; snprintf(p, sizeof p, "%s/content", in); struct stat st; if (stat(p, &st) != 0) return;
    nftw(p, rm_one, 16, FTW_DEPTH | FTW_PHYS); port_log("removed the old unpacked copy");
}
#endif

#ifndef __ANDROID__
/* PC test of the touch controls: fingers put down and lifted by a script, as a player would (KK_TOUCHTEST=1) */
static void finger(int type, int id, float x, float y) {
    SDL_Event e; memset(&e, 0, sizeof e); e.type = type; e.tfinger.fingerId = id; e.tfinger.x = x; e.tfinger.y = y; e.tfinger.pressure = 1; touch_event(&e);
}
static void tap_at(long f, long at, int id, float x, float y) { if (f == at) finger(SDL_FINGERDOWN, id, x, y); if (f == at + 4) finger(SDL_FINGERUP, id, x, y); }
static void touch_script(long f) {
    /* menus: OK on the title, OK on save 1 (new game), OK on Normal, OK on the name, OK through the story pages */
    for (int k = 0; k < 12; k++) tap_at(f, 60 + k * 50, 1, BTN[0].mx, BTN[0].my);
    /* playing (from frame 700): walk forward-right with the stick, push it to the edge (run), drag the camera, jump, roar, eat */
    if (f == 700) finger(SDL_FINGERDOWN, 2, 0.15f, 0.65f);
    if (f > 700 && f < 900) finger(SDL_FINGERMOTION, 2, 0.15f + 0.04f * (f < 800 ? (f - 700) / 100.0f : 1.0f), 0.65f - 0.10f);
    if (f == 900) finger(SDL_FINGERUP, 2, 0.19f, 0.55f);
    if (f == 760) finger(SDL_FINGERDOWN, 3, 0.6f, 0.4f);
    if (f > 760 && f < 820) finger(SDL_FINGERMOTION, 3, 0.6f + (f - 760) * 0.002f, 0.4f);
    if (f == 820) finger(SDL_FINGERUP, 3, 0.72f, 0.4f);
    tap_at(f, 850, 4, BTN[0].cx, BTN[0].cy);
    tap_at(f, 950, 4, BTN[1].cx, BTN[1].cy);
    tap_at(f, 1000, 4, BTN[4].cx, BTN[4].cy);
}
#endif

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) != 0) { SDL_Log("SDL: %s", SDL_GetError()); return 1; }
    char *pref = SDL_GetPrefPath("Sean", "DragonsBeyond");
    if (pref) { char p[512]; snprintf(p, sizeof p, "%sdragonsbeyond_log.txt", pref); g_log = fopen(p, "w"); gfx_set_logfile(g_log);
        snprintf(p, sizeof p, "%sdragonsbeyond_save.txt", pref); game_set_save_path(p); }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0); SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    const char *test = getenv("KK_FRAMES");
#ifdef __ANDROID__
    g_win = SDL_CreateWindow("Dragons Beyond", 0, 0, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALLOW_HIGHDPI);
#else
    g_win = SDL_CreateWindow("Dragons Beyond", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, test ? 1560 : 1600, test ? 720 : 740, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
#endif
    if (!g_win) { SDL_Log("window: %s", SDL_GetError()); return 1; }
    g_ctx = SDL_GL_CreateContext(g_win); if (!g_ctx) { SDL_Log("GL: %s", SDL_GetError()); return 1; }
    SDL_GL_SetSwapInterval(test ? 0 : 1);
    if (!kk_gl_load(SDL_GL_GetProcAddress)) return 1;
    layout();
#ifdef __ANDROID__
    kk_files_init(); remove_old_copy();
#endif
    overlay_init();
    gfx_log("Dragons Beyond %s starting on %s", KK_VERSION, SDL_GetPlatform());
    int status = gfx_init();
    game_init(status);
    /* sound: try each of the phone's ways of playing sound until one opens (the plain Android one first: it works on every phone) */
    SDL_AudioDeviceID dev = 0; SDL_AudioSpec want, have;
#ifdef __ANDROID__
    static const char *drivers[] = {"android", "openslES", "aaudio", NULL};
#else
    static const char *drivers[] = {NULL};
#endif
    for (int d = 0; !dev; d++) {
        const char *drv = drivers[d];
        if (drv) { if (SDL_AudioInit(drv) != 0) { port_log("sound: %s not available (%s)", drv, SDL_GetError()); continue; } }
        else if (d > 0 || SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) { if (d > 0) break; port_log("sound: %s", SDL_GetError()); break; }
        memset(&want, 0, sizeof want); want.freq = 32000; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024; want.callback = audio_cb;
        dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (!dev) { port_log("sound: %s could not open (%s)", drv ? drv : "default", SDL_GetError()); if (drv) SDL_AudioQuit(); }
        else port_log("sound: %s, %d Hz, %d samples", SDL_GetCurrentAudioDriver(), have.freq, have.samples);
        if (!drv) break;
    }
    int rate = 32000;   /* the game's sounds are 32000 Hz, as on the Wii U; SDL converts to what the phone plays */
    sfx_init(rate); music_init(rate);
    if (dev) SDL_PauseAudioDevice(dev, 0);
    open_controllers();

    /* test runs (the build machine): a fixed number of frames at a fixed step, pictures saved every so often */
    int frames_left = test ? atoi(test) : -1; const char *shots = getenv("KK_SHOTS"); int shot_every = getenv("KK_SHOT_EVERY") ? atoi(getenv("KK_SHOT_EVERY")) : 300;
    Uint64 last = SDL_GetPerformanceCounter(); long frame = 0; int running = 1; Pad pads[KK_PADS];
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: running = 0; break;
                case SDL_FINGERDOWN: case SDL_FINGERUP: case SDL_FINGERMOTION: touch_event(&e); break;
                case SDL_CONTROLLERDEVICEADDED: open_controllers(); break;
                case SDL_WINDOWEVENT: if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) layout(); break;
                case SDL_APP_WILLENTERBACKGROUND: if (dev) SDL_PauseAudioDevice(dev, 1); break;
                case SDL_APP_DIDENTERFOREGROUND: if (dev) SDL_PauseAudioDevice(dev, 0); layout(); break;
                case SDL_KEYDOWN: if (e.key.keysym.sym == SDLK_AC_BACK) { static Uint32 t; t = SDL_GetTicks(); (void)t; } break;
            }
        }
        Uint64 now = SDL_GetPerformanceCounter(); float dt = (float)(now - last) / (float)SDL_GetPerformanceFrequency(); last = now;
        if (dt > 0.1f) dt = 0.1f;
        if (test) dt = 1.0f / 60.0f; else res_adapt(dt);
#ifndef __ANDROID__
        if (getenv("KK_TOUCHTEST")) touch_script(frame);
#endif
        memset(pads, 0, sizeof pads);
        touch_pad(&pads[0]);
        { static int was_back; const Uint8 *ks = SDL_GetKeyboardState(NULL); int bk = ks[SDL_SCANCODE_AC_BACK] || ks[SDL_SCANCODE_ESCAPE];   /* the phone's back key: BACK in the menus, SWAP while playing */
          if (bk) { if (g_playing) pads[0].plus = pads[0].plus || !was_back; else pads[0].b = 1; } was_back = bk; }
        for (int k = 0; k < 4; k++) controller_pad(k, &pads[1 + k]);
        const char *where = game_where(); g_swapmenu = !strncmp(where, "playing", 7) && strstr(where, " swap 1 ") != NULL; g_playing = !strncmp(where, "playing", 7) && !g_swapmenu;   /* the Dragon Swap menu counts as a menu */
        g_shot_now = shots && (frame + 1) % shot_every == 0;
        if (g_shot_now) snprintf(g_shot_path, sizeof g_shot_path, "%s/frame_%06ld.ppm", shots, frame + 1);
#ifndef __ANDROID__
        if (getenv("KK_TOUCHTEST") && frame % 25 == 0) port_log("touch %ld: sx %.2f sy %.2f rx %.2f a%d b%d x%d y%d zr%d up%d | %.90s", frame, pads[0].sx, pads[0].sy, pads[0].rx, pads[0].a, pads[0].b, pads[0].x, pads[0].y, pads[0].zr, pads[0].up, game_where());
#endif
        game_frame(pads, KK_PADS, dt);
        frame++;
        if (frames_left > 0 && --frames_left == 0) running = 0;
    }
    if (dev) SDL_CloseAudioDevice(dev);
    game_shutdown();
    if (g_log) fclose(g_log);
    SDL_Quit();
    return 0;
}
