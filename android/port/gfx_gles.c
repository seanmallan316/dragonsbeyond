// gfx_gles.c - the Wii U game's drawing (render/gfx.h) on OpenGL ES 3.0, for Android phones and PCs.
// The same lighting as the Wii U shader (sun, sky and ground light, rim light, film look, bumpy roads, sun shadows),
// drawn at the phone's own resolution with smooth edges (multisampling) into a 16:9 picture, then put on the screen.
#include "render/gfx.h"
#include "gles.h"
#include "port.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define X(ret, name, args) PFN_##name kk_##name;
KK_GL_FUNCS
#undef X
int kk_gl_load(void *(*getproc)(const char *)) {
    int ok = 1;
#define X(ret, name, args) kk_##name = (PFN_##name)getproc(#name); if (!kk_##name) { ok = 0; gfx_log("GL call missing: %s", #name); }
    KK_GL_FUNCS
#undef X
    return ok;
}

struct Mesh { GLuint vbo, vao; uint32_t count, cap; int dynamic; };
struct Texture { GLuint id; int w, h; int stream; uint8_t *cpu; int dirty; };

static const char *VS_SRC =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout(location = 0) in vec3 in_position;\n"
    "layout(location = 1) in vec3 in_normal;\n"
    "layout(location = 2) in vec2 in_texcoord;\n"
    "layout(location = 3) in vec4 in_color;\n"
    "uniform mat4 u_viewproj; uniform vec4 u_lightdir; uniform vec4 u_fog; uniform vec4 u_eye; uniform mat4 u_shadowvp; uniform vec4 u_shadowp;\n"
    "uniform mat4 u_model; uniform vec4 u_tint; uniform vec4 u_params;\n"
    "out vec2 v_texcoord; out vec4 v_color; out vec4 v_n; out float v_fog; out vec4 v_v; out vec4 v_shadow;\n"
    "void main() {\n"
    "  vec4 wp = u_model * vec4(in_position, 1.0);\n"
    "  float sw = max(length(in_normal) - 1.05, 0.0);\n"
    "  if (sw > 0.0) { float tt = u_eye.w;\n"
    "    wp.x += sw * (sin(tt * 1.6 + wp.x * 0.17 + wp.z * 0.11) * 0.7 + sin(tt * 3.7 + wp.z * 0.5 + wp.y) * 0.3);\n"
    "    wp.z += sw * (cos(tt * 1.25 + wp.z * 0.15 + wp.x * 0.09) * 0.55 + sin(tt * 4.1 + wp.x * 0.45) * 0.25);\n"
    "    wp.y += sw * sin(tt * 2.3 + wp.x * 0.3) * 0.15; }\n"
    "  vec3 n = normalize((u_model * vec4(in_normal, 0.0)).xyz + vec3(0.0, 1e-6, 0.0));\n"
    "  vec3 L = normalize(u_lightdir.xyz);\n"
    "  v_n = vec4(n, u_params.x);\n"
    "  v_v = vec4(u_eye.xyz - wp.xyz, u_params.y);\n"
    "  vec4 sp = u_shadowvp * vec4(wp.xyz + n * u_shadowp.w, 1.0);\n"
    "  v_shadow = vec4(sp.xy * 0.5 + 0.5, sp.z * 0.5 + 0.5, max(dot(n, L), 0.0) + 2.0 * u_shadowp.x * (1.0 - u_params.y));\n"
    "  v_texcoord = in_texcoord + u_params.zw;\n"
    "  v_color = in_color * u_tint;\n"
    "  vec4 cp = u_viewproj * wp;\n"
    "  v_fog = clamp((cp.w - u_fog.x) / max(u_fog.y - u_fog.x, 0.01), 0.0, 1.0) * u_fog.z * (1.0 - u_params.y);\n"
    "  gl_Position = cp;\n"
    "}\n";
static const char *PS_SRC =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 v_texcoord; in vec4 v_color; in vec4 v_n; in float v_fog; in vec4 v_v; in vec4 v_shadow;\n"
    "uniform vec4 u_fogcolor; uniform vec4 u_speccolor; uniform vec4 u_shadowpx; uniform vec4 u_light; uniform vec4 u_skyc; uniform vec4 u_groundc; uniform vec4 u_grade;\n"
    "uniform sampler2D u_tex; uniform highp sampler2D u_shadowmap;\n"
    "out vec4 out_color;\n"
    "void main() {\n"
    "  vec4 t = texture(u_tex, v_texcoord) * v_color;\n"
    "  if (t.a < 0.03) discard;\n"
    "  vec3 c = t.rgb;\n"
    "  if (v_v.w < 0.5 && u_light.w < 0.999) {\n"
    "    vec3 N = normalize(v_n.xyz); float vl = length(v_v.xyz); vec3 V = vl > 0.0001 ? v_v.xyz / vl : N; vec3 L = normalize(u_light.xyz); float amb = u_light.w;\n"
    "    if (dot(N, V) < 0.0) N = -N;\n"   /* thin things seen from behind (wing membranes, leaves) light as their back side */
    "    float bl = floor(v_n.w * 0.1 + 0.001); float spw = v_n.w - bl * 10.0;\n"
    "    if (bl > 0.5) {\n"
    "      vec3 dp1 = -dFdx(v_v.xyz); vec3 dp2 = -dFdy(v_v.xyz); vec2 du1 = dFdx(v_texcoord); vec2 du2 = dFdy(v_texcoord);\n"
    "      vec3 d2p = cross(dp2, N); vec3 d1p = cross(N, dp1);\n"
    "      vec3 T = d2p * du1.x + d1p * du2.x; vec3 B = d2p * du1.y + d1p * du2.y;\n"
    "      float im = inversesqrt(max(max(dot(T, T), dot(B, B)), 1e-20)); T *= im; B *= im;\n"
    "      vec3 lw = vec3(0.299, 0.587, 0.114);\n"
    "      float h0 = dot(texture(u_tex, v_texcoord).rgb, lw);\n"
    "      vec2 vts = vec2(dot(V, T), dot(V, B)) / max(dot(V, N), 0.3);\n"
    "      vec2 uv = v_texcoord + vts * (h0 - 0.5) * 0.006 * bl;\n"
    "      vec2 fw = max(fwidth(uv), vec2(1.0 / 512.0));\n"
    "      float hc = dot(texture(u_tex, uv).rgb, lw);\n"
    "      float hx = dot(texture(u_tex, uv + vec2(fw.x, 0.0)).rgb, lw) - hc;\n"
    "      float hy = dot(texture(u_tex, uv + vec2(0.0, fw.y)).rgb, lw) - hc;\n"
    "      N = normalize(N - (hx * T + hy * B) * bl * 1.6);\n"
    "      t = texture(u_tex, uv) * v_color; c = t.rgb;\n"
    "    }\n"
    "    float nl = dot(N, L);\n"
    "    float lit = 1.0;\n"
    "    if (v_shadow.w > 1.5) {\n"
    /* softer than the Wii U: 9 samples instead of 4 */
    "      vec2 uv = v_shadow.xy; float d = v_shadow.z - u_shadowpx.y; float o = u_shadowpx.x;\n"
    "      if (uv.x > 0.0 && uv.x < 1.0 && uv.y > 0.0 && uv.y < 1.0) {\n"
    "        float s = 0.0;\n"
    "        for (int i = -1; i <= 1; i++) for (int j = -1; j <= 1; j++) s += step(d, texture(u_shadowmap, uv + vec2(float(i), float(j)) * o).r);\n"
    "        lit = s / 9.0;\n"
    "        lit = mix(1.0, lit, clamp((v_shadow.w - 2.0) * 4.0, 0.0, 1.0) * u_shadowpx.z); } }\n"
    "    float w = u_grade.z; float diff = clamp((nl + w) / (1.0 + w), 0.0, 1.0); diff = diff * diff * (3.0 - 2.0 * diff);\n"
    "    vec3 hemi = mix(u_groundc.rgb, u_skyc.rgb, N.y * 0.5 + 0.5);\n"
    "    vec3 light = hemi * amb + u_speccolor.rgb * diff * (1.0 - amb) * lit * 1.15;\n"
    "    float fr = 1.0 - max(dot(N, V), 0.0); float rim = fr * fr * fr * u_grade.y;\n"
    "    light += mix(u_skyc.rgb, u_speccolor.rgb, 0.5 + 0.5 * clamp(nl, 0.0, 1.0)) * rim * (0.45 + 0.55 * lit);\n"
    "    vec3 H = normalize(L + V + vec3(0.0, 0.0001, 0.0)); float sp = pow(max(dot(N, H), 0.0), 48.0) * spw + pow(max(dot(N, H), 0.0), 8.0) * 0.04 * u_grade.x;\n"
    "    c = t.rgb * light + u_speccolor.rgb * sp * lit;\n"
    "  }\n"
    "  c = mix(c, u_fogcolor.rgb, v_fog);\n"
    "  if (u_grade.x > 0.0) {\n"
    "    vec3 hi = max(c - 0.78, 0.0); c = min(c, 0.78) + 0.22 * (1.0 - exp(-hi / 0.22));\n"
    "    vec3 s = c * c * (3.0 - 2.0 * c); c = mix(c, s, 0.18 * u_grade.x);\n"
    "    float lum = dot(c, vec3(0.299, 0.587, 0.114)); c = mix(vec3(lum), c, mix(1.0, u_grade.w, u_grade.x));\n"
    "    c = clamp(c, 0.0, 1.0); }\n"
    "  out_color = vec4(c, t.a);\n"
    "}\n";
static const char *VS_SHADOW_SRC =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout(location = 0) in vec3 in_position;\n"
    "layout(location = 1) in vec3 in_normal;\n"
    "layout(location = 2) in vec2 in_texcoord;\n"
    "layout(location = 3) in vec4 in_color;\n"
    "uniform mat4 u_viewproj; uniform vec4 u_eye; uniform mat4 u_model; uniform vec4 u_tint; uniform vec4 u_params;\n"
    "out vec2 v_texcoord; out float v_alpha;\n"
    "void main() {\n"
    "  vec4 wp = u_model * vec4(in_position, 1.0);\n"
    "  float sw = max(length(in_normal) - 1.05, 0.0);\n"
    "  if (sw > 0.0) { float tt = u_eye.w;\n"
    "    wp.x += sw * (sin(tt * 1.6 + wp.x * 0.17 + wp.z * 0.11) * 0.7 + sin(tt * 3.7 + wp.z * 0.5 + wp.y) * 0.3);\n"
    "    wp.z += sw * (cos(tt * 1.25 + wp.z * 0.15 + wp.x * 0.09) * 0.55 + sin(tt * 4.1 + wp.x * 0.45) * 0.25);\n"
    "    wp.y += sw * sin(tt * 2.3 + wp.x * 0.3) * 0.15; }\n"
    "  v_texcoord = in_texcoord + u_params.zw; v_alpha = in_color.a * u_tint.a;\n"
    "  gl_Position = u_viewproj * wp;\n"
    "}\n";
static const char *PS_SHADOW_SRC =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 v_texcoord; in float v_alpha;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 out_color;\n"
    "void main() { if (texture(u_tex, v_texcoord).a * v_alpha < 0.5) discard; out_color = vec4(1.0); }\n";

/* what the shaders need, kept here and sent when a draw happens */
typedef struct { GLuint prog; GLint viewproj, lightdir, fog, eye, shadowvp, shadowp, model, tint, params,
                 fogcolor, speccolor, shadowpx, light, skyc, groundc, grade, tex, shmap; } Prog;
static Prog P, PS;   /* the main shader and the shadow shader */
static struct { float viewproj[16], lightdir[4], fog[4], eye[4], shadowvp[16], shadowp[4];
                float fogcolor[4], speccolor[4], shadowpx[4], light[4], skyc[4], groundc[4], grade[4]; int ver; } F;
static int g_prog_ver = -1, g_prog_ver_s = -1;
static float g_grade[4] = {1.0f, 0.38f, 0.32f, 1.14f};
static float g_sun[3] = {1, 1, 1}, g_sky[3] = {1, 1, 1}, g_ground[3] = {1, 1, 1}, g_eye[3] = {0, 0, 0}, g_wind_t = 0;
static Texture *g_white; static Texture *g_bound; static int g_flags = -1, g_initialized = 0, g_bump = 0;
static int g_skip = 0;            /* drawing the GamePad's screen: a phone has only one, so it is skipped */
static FILE *g_logfile;

/* the picture the game draws into: 16:9, at the phone's resolution, multisampled */
static GLuint g_fbo, g_rb_color, g_rb_depth; static int g_w = 1280, g_h = 720, g_samples = 0;
static GLuint g_res_fbo, g_res_tex;              /* the resolved picture (shown on screen, and grabbed for the boost blur) */
static Texture g_grab_tex;
/* sun shadows */
#define SHADOW_RES 2048
static GLuint g_sh_fbo, g_sh_tex; static int g_shadow_ok = 0, g_in_shadow = 0, g_shadow_use = 0, g_shadow_have = 0; static float g_shadow_vp[16];
static int g_aniso = 0;

void gfx_log(const char *fmt, ...) {
    char buf[1024]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    port_log("%s", buf);
    if (g_logfile) { fprintf(g_logfile, "%s\n", buf); fflush(g_logfile); }
}
void gfx_set_logfile(FILE *f) { g_logfile = f; }

static GLuint compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type); glShaderSource(s, 1, &src, NULL); glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[2048]; glGetShaderInfoLog(s, sizeof log, NULL, log); gfx_log("shader failed: %s", log); return 0; }
    return s;
}
static int link_prog(Prog *p, const char *vs, const char *ps) {
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, ps); if (!v || !f) return 0;
    p->prog = glCreateProgram(); glAttachShader(p->prog, v); glAttachShader(p->prog, f); glLinkProgram(p->prog);
    GLint ok = 0; glGetProgramiv(p->prog, GL_LINK_STATUS, &ok);
    if (!ok) { char log[2048]; glGetProgramInfoLog(p->prog, sizeof log, NULL, log); gfx_log("shader link failed: %s", log); return 0; }
#define U(n) p->n = glGetUniformLocation(p->prog, "u_" #n)
    U(viewproj); U(lightdir); U(fog); U(eye); U(shadowvp); U(shadowp); U(model); U(tint); U(params);
    U(fogcolor); U(speccolor); U(shadowpx); U(light); U(skyc); U(groundc); U(grade); U(tex); p->shmap = glGetUniformLocation(p->prog, "u_shadowmap");
#undef U
    glUseProgram(p->prog); if (p->tex >= 0) glUniform1i(p->tex, 0); if (p->shmap >= 0) glUniform1i(p->shmap, 1);
    return 1;
}


/* the finishing pass over the whole 3D picture (as on the Wii U): smooth edges, sharpen, glow round bright light, darker corners */
static const char *VS_POST_SRC =
    "#version 300 es\n"
    "layout(location = 0) in vec2 p; layout(location = 1) in vec2 uv; out vec2 v_uv;\n"
    "void main() { v_uv = uv; gl_Position = vec4(p, 0.0, 1.0); }\n";
static const char *PS_POST_SRC =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 v_uv; uniform sampler2D u_scene; uniform sampler2D u_bloom; uniform vec4 u_px; uniform vec4 u_k; out vec4 out_color;\n"
    "float lu(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
    "vec3 at(vec2 o) { return texture(u_scene, v_uv + o * u_px.xy).rgb; }\n"
    "void main() {\n"
    "  vec3 cM = at(vec2(0.0)), cN = at(vec2(0.0, -1.0)), cS = at(vec2(0.0, 1.0)), cE = at(vec2(1.0, 0.0)), cW = at(vec2(-1.0, 0.0));\n"
    "  float lM = lu(cM), lN = lu(cN), lS = lu(cS), lE = lu(cE), lW = lu(cW);\n"
    "  float mn = min(lM, min(min(lN, lS), min(lE, lW))), mx = max(lM, max(max(lN, lS), max(lE, lW)));\n"
    "  vec3 c = cM;\n"
    "  if (mx - mn > max(0.035, mx * 0.1)) {\n"
    "    float lNW = lu(at(vec2(-1.0, -1.0))), lNE = lu(at(vec2(1.0, -1.0))), lSW = lu(at(vec2(-1.0, 1.0))), lSE = lu(at(vec2(1.0, 1.0)));\n"
    "    vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), (lNW + lSW) - (lNE + lSE));\n"
    "    float red = max((lNW + lNE + lSW + lSE) * 0.03125, 0.0078125);\n"
    "    dir = clamp(dir / (min(abs(dir.x), abs(dir.y)) + red), vec2(-8.0), vec2(8.0));\n"
    "    vec3 a = 0.5 * (at(dir * (1.0 / 3.0 - 0.5)) + at(dir * (2.0 / 3.0 - 0.5)));\n"
    "    vec3 b = a * 0.5 + 0.25 * (at(dir * -0.5) + at(dir * 0.5));\n"
    "    float lb = lu(b); c = (lb < mn || lb > mx) ? a : b;\n"
    "  } else {\n"
    "    c = max(cM + (cM * 4.0 - cN - cS - cE - cW) * 0.25 * u_k.z, vec3(0.0));\n"
    "  }\n"
    "  vec3 bl = texture(u_bloom, v_uv).rgb;\n"
    "  c += bl * u_k.y * 0.9 * vec3(1.0, 0.95, 0.86);\n"
    "  vec2 d = v_uv - 0.5; c *= 1.0 - u_k.w * dot(d, d) * 1.2;\n"
    "  out_color = vec4(min(c, vec3(1.0)), 1.0);\n"
    "}\n";
static const char *PS_BLOOM_SRC =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 v_uv; uniform sampler2D u_src; uniform vec4 u_d; out vec4 out_color;\n"
    "void main() {\n"
    "  if (u_d.z > 0.5) {\n"   /* bright pass, 4 taps (a box down to a quarter size) */
    "    vec3 c = vec3(0.0); for (int i = 0; i < 4; i++) { vec2 o = vec2(float(i & 1) - 0.5, float(i >> 1) - 0.5) * u_d.xy * 2.0;\n"
    "      vec3 t = texture(u_src, v_uv + o).rgb; float l = dot(t, vec3(0.299, 0.587, 0.114)); c += t * smoothstep(0.62, 0.95, l); }\n"
    "    out_color = vec4(c * 0.25, 1.0); return; }\n"
    "  vec3 c = texture(u_src, v_uv).rgb * 0.2270270270;\n"   /* 9-tap gaussian in 5 reads (linear sampling) */
    "  c += (texture(u_src, v_uv + u_d.xy * 1.3846153846).rgb + texture(u_src, v_uv - u_d.xy * 1.3846153846).rgb) * 0.3162162162;\n"
    "  c += (texture(u_src, v_uv + u_d.xy * 3.2307692308).rgb + texture(u_src, v_uv - u_d.xy * 3.2307692308).rgb) * 0.0702702703;\n"
    "  out_color = vec4(c, 1.0);\n"
    "}\n";
static GLuint g_post_prog, g_post_vao, g_post_vbo; static GLint g_post_px, g_post_k;
static GLuint g_bloom_prog, g_bl_tex[2], g_bl_fbo[2]; static GLint g_bloom_d; static int g_bl_w, g_bl_h;
static void bloom_targets(int w, int h) {
    int bw = w / 4 > 1 ? w / 4 : 1, bh = h / 4 > 1 ? h / 4 : 1; if (g_bl_tex[0] && bw == g_bl_w && bh == g_bl_h) return;
    if (!g_bl_tex[0]) { glGenTextures(2, g_bl_tex); glGenFramebuffers(2, g_bl_fbo); }
    g_bl_w = bw; g_bl_h = bh;
    for (int i = 0; i < 2; i++) { glBindTexture(GL_TEXTURE_2D, g_bl_tex[i]); glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, bw, bh, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, g_bl_fbo[i]); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_bl_tex[i], 0); }
}
static void post_init(void) {
    GLuint v = compile(GL_VERTEX_SHADER, VS_POST_SRC), f = compile(GL_FRAGMENT_SHADER, PS_POST_SRC); if (!v || !f) return;
    GLuint pr = glCreateProgram(); glAttachShader(pr, v); glAttachShader(pr, f); glLinkProgram(pr);
    GLint ok = 0; glGetProgramiv(pr, GL_LINK_STATUS, &ok); if (!ok) { gfx_log("finishing pass link failed"); return; }
    g_post_px = glGetUniformLocation(pr, "u_px"); g_post_k = glGetUniformLocation(pr, "u_k");
    glUseProgram(pr); glUniform1i(glGetUniformLocation(pr, "u_scene"), 0); glUniform1i(glGetUniformLocation(pr, "u_bloom"), 2);
    { GLuint bf = compile(GL_FRAGMENT_SHADER, PS_BLOOM_SRC); if (bf) { GLuint b = glCreateProgram(); glAttachShader(b, v); glAttachShader(b, bf); glLinkProgram(b);
        GLint bok = 0; glGetProgramiv(b, GL_LINK_STATUS, &bok); if (bok) { g_bloom_prog = b; g_bloom_d = glGetUniformLocation(b, "u_d"); glUseProgram(b); glUniform1i(glGetUniformLocation(b, "u_src"), 0); } } }
    static const float q[24] = {-1, -1, 0, 1,  1, -1, 1, 1,  1, 1, 1, 0,  -1, -1, 0, 1,  1, 1, 1, 0,  -1, 1, 0, 0};
    glGenVertexArrays(1, &g_post_vao); glBindVertexArray(g_post_vao); glGenBuffers(1, &g_post_vbo); glBindBuffer(GL_ARRAY_BUFFER, g_post_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof q, q, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glBindVertexArray(0); g_post_prog = pr;
}

static void make_target(int w, int h) {
    if (g_fbo && w == g_w && h == g_h) return;
    if (g_fbo) { glDeleteFramebuffers(1, &g_fbo); glDeleteRenderbuffers(1, &g_rb_color); glDeleteRenderbuffers(1, &g_rb_depth); glDeleteFramebuffers(1, &g_res_fbo); glDeleteTextures(1, &g_res_tex); }
    g_w = w; g_h = h;
    GLint maxs = 0; glGetIntegerv(GL_MAX_SAMPLES, &maxs); g_samples = maxs >= 4 ? 4 : (maxs >= 2 ? 2 : 0);
    glGenFramebuffers(1, &g_fbo); glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    glGenRenderbuffers(1, &g_rb_color); glBindRenderbuffer(GL_RENDERBUFFER, g_rb_color); glRenderbufferStorageMultisample(GL_RENDERBUFFER, g_samples, GL_RGBA8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, g_rb_color);
    glGenRenderbuffers(1, &g_rb_depth); glBindRenderbuffer(GL_RENDERBUFFER, g_rb_depth); glRenderbufferStorageMultisample(GL_RENDERBUFFER, g_samples, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_rb_depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) gfx_log("picture buffer incomplete (%dx%d, %d samples)", w, h, g_samples);
    glGenTextures(1, &g_res_tex); glBindTexture(GL_TEXTURE_2D, g_res_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &g_res_fbo); glBindFramebuffer(GL_FRAMEBUFFER, g_res_fbo); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_res_tex, 0);
    /* the grab copy: kept the right way up for the game (row 0 at the top, as on the Wii U) */
    if (!g_grab_tex.id) glGenTextures(1, &g_grab_tex.id);
    glBindTexture(GL_TEXTURE_2D, g_grab_tex.id); glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g_grab_tex.w = w; g_grab_tex.h = h;
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    gfx_log("picture %dx%d, %d samples", w, h, g_samples);
}

int gfx_init(void) {
    gfx_log("GL: %s / %s", (const char *)glGetString(GL_VERSION), (const char *)glGetString(GL_RENDERER));
    if (!link_prog(&P, VS_SRC, PS_SRC)) return GFX_SHADER_FAILED;
    const char *ext = (const char *)glGetString(GL_EXTENSIONS); g_aniso = ext && strstr(ext, "texture_filter_anisotropic");
    uint32_t white[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
    g_white = gfx_texture_rgba(white, 2, 2, 1, 1);
    if (link_prog(&PS, VS_SHADOW_SRC, PS_SHADOW_SRC)) {
        glGenTextures(1, &g_sh_tex); glBindTexture(GL_TEXTURE_2D, g_sh_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_RES, SHADOW_RES, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glGenFramebuffers(1, &g_sh_fbo); glBindFramebuffer(GL_FRAMEBUFFER, g_sh_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_sh_tex, 0);
        GLenum none = GL_NONE; glDrawBuffers(1, &none); glReadBuffer(GL_NONE);
        g_shadow_ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }
    gfx_log("sun shadows %s", g_shadow_ok ? "on" : "off");
    post_init(); gfx_log("finishing pass %s", g_post_prog ? "on" : "off");
    int w, h; port_target_size(&w, &h); make_target(w, h);
    g_initialized = 1;
    return GFX_OK;
}
void gfx_shutdown(void) {}

/* ------------------------------------------------------------------ meshes */
static void mesh_vao(Mesh *m) {
    glGenVertexArrays(1, &m->vao); glBindVertexArray(m->vao);
    glGenBuffers(1, &m->vbo); glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)12);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)24);
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)32);
}
Mesh *gfx_mesh_static(const Vertex *v, uint32_t n) {
    Mesh *m = calloc(1, sizeof *m); if (!m) return NULL;
    mesh_vao(m); m->count = n; m->cap = n;
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * (n ? n : 1)), n ? v : NULL, GL_STATIC_DRAW);
    glBindVertexArray(0);
    return m;
}
Mesh *gfx_mesh_dynamic(uint32_t cap) {
    Mesh *m = calloc(1, sizeof *m); if (!m) return NULL;
    mesh_vao(m); m->cap = cap; m->dynamic = 1;
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * (cap ? cap : 1)), NULL, GL_DYNAMIC_DRAW);
    glBindVertexArray(0);
    return m;
}
void gfx_mesh_update(Mesh *m, const Vertex *v, uint32_t n) {
    if (!m) return; if (n > m->cap) n = m->cap;
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * (m->cap ? m->cap : 1)), NULL, GL_DYNAMIC_DRAW);   /* a fresh buffer: no waiting on the GPU */
    if (n) glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(Vertex) * n), v);
    m->count = n;
}
void gfx_mesh_free(Mesh *m) { if (!m) return; glDeleteBuffers(1, &m->vbo); glDeleteVertexArrays(1, &m->vao); free(m); }

/* ------------------------------------------------------------------ textures */
/* The game's own pictures are 0xRRGGBBAA words; pictures read from files arrive as bytes R,G,B,A. */
static int g_bytes_mode = 0;
static Texture *tex_make(const uint8_t *rgba, int w, int h, int repeat, int nearest, int mipmaps) {
    Texture *t = calloc(1, sizeof *t); if (!t) return NULL;
    t->w = w; t->h = h;
    glGenTextures(1, &t->id); glBindTexture(GL_TEXTURE_2D, t->id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    int mip = mipmaps && !nearest && (w > 16 || h > 16);
    if (mip) glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, nearest ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, nearest ? GL_NEAREST : (mip ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    if (mip && g_aniso) glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8.0f);
    g_bound = NULL;
    return t;
}
Texture *gfx_texture_rgba(const uint32_t *pixels, int w, int h, int repeat, int nearest) { return gfx_texture_rgba_ex(pixels, w, h, repeat, nearest, 1); }
Texture *gfx_texture_rgba_ex(const uint32_t *pixels, int w, int h, int repeat, int nearest, int mipmaps) {
    if (g_bytes_mode) return tex_make((const uint8_t *)pixels, w, h, repeat, nearest, mipmaps);
    uint8_t *b = malloc((size_t)w * h * 4); if (!b) return NULL;
    for (size_t i = 0; i < (size_t)w * h; i++) { uint32_t p = pixels[i]; b[i * 4] = p >> 24; b[i * 4 + 1] = p >> 16; b[i * 4 + 2] = p >> 8; b[i * 4 + 3] = p; }
    Texture *t = tex_make(b, w, h, repeat, nearest, mipmaps); free(b); return t;
}
/* the file-loading code (render/assets.c) is built with these names, so its bytes go straight in */
Texture *gfx_texture_rgba_bytes(const uint32_t *pixels, int w, int h, int repeat, int nearest) { g_bytes_mode = 1; Texture *t = gfx_texture_rgba_ex(pixels, w, h, repeat, nearest, 1); g_bytes_mode = 0; return t; }
Texture *gfx_texture_rgba_ex_bytes(const uint32_t *pixels, int w, int h, int repeat, int nearest, int mipmaps) { g_bytes_mode = 1; Texture *t = gfx_texture_rgba_ex(pixels, w, h, repeat, nearest, mipmaps); g_bytes_mode = 0; return t; }

/* video frames: decoded (as bytes) on helper threads into memory, sent to the GPU on the drawing thread */
Texture *gfx_texture_stream(int w, int h) {
    Texture *t = tex_make(NULL, w, h, 0, 0, 0); if (!t) return NULL;
    t->stream = 1; t->cpu = calloc((size_t)w * h, 4); return t;
}
void *gfx_texture_stream_pixels(Texture *t, int *pitch_px) { if (!t) return NULL; if (pitch_px) *pitch_px = t->w; return t->cpu; }
void gfx_texture_stream_flush_cpu(Texture *t) { if (t) t->dirty = 1; }
void gfx_texture_stream_gpu(Texture *t) {
    if (!t || !t->cpu || !t->dirty) return; t->dirty = 0;
    glBindTexture(GL_TEXTURE_2D, t->id); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, t->w, t->h, GL_RGBA, GL_UNSIGNED_BYTE, t->cpu); g_bound = NULL;
}
void gfx_texture_stream_put(Texture *t, const uint32_t *pixels, int w, int h) {
    if (!t || !pixels) return; if (w > t->w) w = t->w; if (h > t->h) h = t->h;
    for (int y = 0; y < h; y++) memcpy(t->cpu + (size_t)y * t->w * 4, (const uint8_t *)pixels + (size_t)y * w * 4, (size_t)w * 4);
    t->dirty = 1; gfx_texture_stream_gpu(t);
}
void gfx_texture_free(Texture *t) { if (!t || t == &g_grab_tex) return; glDeleteTextures(1, &t->id); free(t->cpu); if (g_bound == t) g_bound = NULL; free(t); }

/* ------------------------------------------------------------------ frames, screens, state */
void gfx_frame_begin(void) {
    int w, h; port_target_size(&w, &h); make_target(w, h);
    g_shadow_have = 0;
}
static void set_viewport_px(int x, int y, int w, int h) { glViewport(x, g_h - y - h, w, h); glScissor(x, g_h - y - h, w, h); }
void gfx_view_begin(int drc, float r, float g, float b) {
    g_skip = drc != 0;
    if (g_skip) return;
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    glEnable(GL_SCISSOR_TEST); set_viewport_px(0, 0, g_w, g_h);
    glDepthMask(GL_TRUE); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(r, g, b, 1.0f); glClearDepthf(1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glUseProgram(P.prog); g_prog_ver = -1; g_flags = -1; g_bound = NULL;
    glFrontFace(GL_CCW); glCullFace(GL_BACK);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, g_shadow_ok ? g_sh_tex : g_white->id); glActiveTexture(GL_TEXTURE0);
    gfx_bind(g_white);
}
void gfx_view_end(int drc) {
    if (drc || g_skip) { g_skip = 0; return; }
    /* resolve the smooth edges into the picture shown on screen */
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_fbo); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_res_fbo);
    glBlitFramebuffer(0, 0, g_w, g_h, 0, 0, g_w, g_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void gfx_frame_end(void) { port_present(g_res_tex, g_w, g_h); }
void gfx_clear_depth(int drc) {
    if (drc || g_skip) return;
    glDepthMask(GL_TRUE); glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); g_flags = -1;
}
float gfx_aspect(int drc) { return drc ? 854.0f / 480.0f : (float)g_w / (float)g_h; }
void gfx_viewport(int drc, float fx, float fy, float fw, float fh) {
    if (drc || g_skip || g_in_shadow) return;
    set_viewport_px((int)floorf(fx * g_w), (int)floorf(fy * g_h), (int)floorf(fw * g_w), (int)floorf(fh * g_h));
}
int gfx_shadow_begin(const Mat4 *lightvp) {
    if (!g_shadow_ok) return 0;
    glBindFramebuffer(GL_FRAMEBUFFER, g_sh_fbo); glDisable(GL_SCISSOR_TEST); glViewport(0, 0, SHADOW_RES, SHADOW_RES);
    glDepthMask(GL_TRUE); glClearDepthf(1.0f); glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDisable(GL_CULL_FACE); glDisable(GL_BLEND);
    glUseProgram(PS.prog); g_prog_ver_s = -1;
    g_in_shadow = 1; g_flags = -1; g_bound = NULL;
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, g_white->id); glActiveTexture(GL_TEXTURE0);
    gfx_bind(g_white);
    memcpy(g_shadow_vp, lightvp->m, sizeof g_shadow_vp);
    gfx_camera(lightvp, v3(0, 1, 0), 1, 0, 0, 0, 1e6f, 2e6f);
    return 1;
}
void gfx_shadow_end(void) {
    if (!g_in_shadow) return;
    g_in_shadow = 0; g_shadow_have = 1; g_bound = NULL; g_flags = -1;
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
}
Texture *gfx_grab(int drc) { return gfx_grab_ex(drc, drc); }
Texture *gfx_grab_ex(int src, int drc) {
    if (!g_initialized || g_in_shadow || drc || g_skip || src) return NULL;
    /* the picture so far, flipped so its first row is the top (as the game expects) */
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_fbo); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_res_fbo);
    glBlitFramebuffer(0, 0, g_w, g_h, 0, 0, g_w, g_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_res_fbo);
    glBindTexture(GL_TEXTURE_2D, g_grab_tex.id);
    /* copy row by row upside down is slow; instead flip with a second blit through the grab texture's own framebuffer */
    static GLuint grab_fbo; if (!grab_fbo) glGenFramebuffers(1, &grab_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, grab_fbo); glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_grab_tex.id, 0);
    glBlitFramebuffer(0, 0, g_w, g_h, 0, g_h, g_w, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo); glEnable(GL_SCISSOR_TEST);
    g_bound = NULL; g_flags = -1;
    return &g_grab_tex;
}
void gfx_sync(void) { glFinish(); }
/* the finishing pass over what is drawn so far (after the 3D scene, before the words and panels) */
void gfx_post(int drc, float bloom, float sharpen, float vignette) {
    if (!g_post_prog || g_in_shadow || drc || g_skip) return;
    Texture *t = gfx_grab(drc); if (!t) return;
    gfx_viewport(drc, 0, 0, 1, 1);
    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_CULL_FACE); glDisable(GL_BLEND); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (g_bloom_prog) {   /* the glow: the bright parts at a quarter size, blurred across and down twice */
        bloom_targets(g_w, g_h); glDisable(GL_SCISSOR_TEST); glViewport(0, 0, g_bl_w, g_bl_h);
        glUseProgram(g_bloom_prog); glBindVertexArray(g_post_vao); glActiveTexture(GL_TEXTURE0);
        float d0[4] = {1.0f / (float)t->w, 1.0f / (float)t->h, 1, 0}; glUniform4fv(g_bloom_d, 1, d0);
        glBindFramebuffer(GL_FRAMEBUFFER, g_bl_fbo[0]); glBindTexture(GL_TEXTURE_2D, t->id); glDrawArrays(GL_TRIANGLES, 0, 6);
        for (int it = 0; it < 2; it++) {
            float dh[4] = {1.5f / (float)g_bl_w, 0, 0, 0}, dv[4] = {0, 1.5f / (float)g_bl_h, 0, 0};
            glUniform4fv(g_bloom_d, 1, dh); glBindFramebuffer(GL_FRAMEBUFFER, g_bl_fbo[1]); glBindTexture(GL_TEXTURE_2D, g_bl_tex[0]); glDrawArrays(GL_TRIANGLES, 0, 6);
            glUniform4fv(g_bloom_d, 1, dv); glBindFramebuffer(GL_FRAMEBUFFER, g_bl_fbo[0]); glBindTexture(GL_TEXTURE_2D, g_bl_tex[1]); glDrawArrays(GL_TRIANGLES, 0, 6); }
        glBindFramebuffer(GL_FRAMEBUFFER, g_fbo); glEnable(GL_SCISSOR_TEST); gfx_viewport(drc, 0, 0, 1, 1);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, g_bl_tex[0]); glActiveTexture(GL_TEXTURE0);
    } else { glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, g_white->id); glActiveTexture(GL_TEXTURE0); }
    glUseProgram(g_post_prog);
    float sc = (float)g_h / 720.0f;   /* the glow reaches as far across the picture as on the TV, whatever the phone's resolution */
    { float px[4] = {1.0f / (float)t->w, 1.0f / (float)t->h, sc, 0}, k[4] = {1, bloom, sharpen, vignette}; glUniform4fv(g_post_px, 1, px); glUniform4fv(g_post_k, 1, k); }
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, t->id);
    glBindVertexArray(g_post_vao); glDrawArrays(GL_TRIANGLES, 0, 6); glBindVertexArray(0);
    glUseProgram(P.prog); g_prog_ver = -1; g_flags = -1; g_bound = NULL; gfx_bind(g_white);
}
void gfx_bump(int level) { g_bump = level < 0 ? 0 : level > 9 ? 9 : level; }
void gfx_shadow_use(int on) { g_shadow_use = on; }
Texture *gfx_shadow_texture(void) { return NULL; }
void gfx_shadow_none(void) { g_shadow_have = 0; }
void gfx_set_time(float t) { g_wind_t = t; }

void gfx_camera(const Mat4 *vp, Vec3 l, float ambient, float fr, float fg, float fb, float fs, float fe) {
    memcpy(F.viewproj, vp->m, sizeof F.viewproj);
    F.lightdir[0] = l.x; F.lightdir[1] = l.y; F.lightdir[2] = l.z; F.lightdir[3] = ambient;
    F.fog[0] = fs; F.fog[1] = fe; F.fog[2] = 1.0f; F.fog[3] = 0;
    for (int i = 0; i < 3; i++) F.eye[i] = g_eye[i]; F.eye[3] = g_wind_t;
    int sh = g_shadow_ok && g_shadow_have && g_shadow_use && !g_in_shadow;
    memcpy(F.shadowvp, g_shadow_vp, sizeof F.shadowvp); F.shadowp[0] = sh ? 1.0f : 0.0f; F.shadowp[1] = F.shadowp[2] = 0; F.shadowp[3] = 0.08f;
    float fc[4] = {fr, fg, fb, 1}, sc[4] = {g_sun[0], g_sun[1], g_sun[2], 1}, spx[4] = {0.6f / SHADOW_RES, 0.0009f, 0.78f, 0}, lt[4] = {l.x, l.y, l.z, ambient};
    float sk[4] = {g_sky[0], g_sky[1], g_sky[2], 0}, gr[4] = {g_ground[0], g_ground[1], g_ground[2], 0};
    memcpy(F.fogcolor, fc, 16); memcpy(F.speccolor, sc, 16); memcpy(F.shadowpx, spx, 16); memcpy(F.light, lt, 16); memcpy(F.skyc, sk, 16); memcpy(F.groundc, gr, 16); memcpy(F.grade, g_grade, 16);
    F.ver++;
}
void gfx_grade(float film, float rim, float wrap, float rich) { g_grade[0] = film; g_grade[1] = rim; g_grade[2] = wrap; g_grade[3] = rich; }
void gfx_light(Vec3 sun, Vec3 sky, Vec3 ground, Vec3 eye) {
    g_sun[0] = sun.x; g_sun[1] = sun.y; g_sun[2] = sun.z; g_sky[0] = sky.x; g_sky[1] = sky.y; g_sky[2] = sky.z;
    g_ground[0] = ground.x; g_ground[1] = ground.y; g_ground[2] = ground.z; g_eye[0] = eye.x; g_eye[1] = eye.y; g_eye[2] = eye.z;
}
static void flat_frame(const float *vp) {
    memcpy(F.viewproj, vp, sizeof F.viewproj);
    float ld[4] = {0, 0, 1, 1}, fog[4] = {0, 1, 0, 0}; memcpy(F.lightdir, ld, 16); memcpy(F.fog, fog, 16);
    for (int i = 0; i < 4; i++) F.eye[i] = 0; memset(F.shadowvp, 0, sizeof F.shadowvp); memset(F.shadowp, 0, sizeof F.shadowp);
    float z[4] = {0, 0, 0, 1}, zz[4] = {0, 0, 0, 0}, l[4] = {0, 0, 1, 1}, one[4] = {1, 1, 1, 0}, gr[4] = {0, 0, 0, 1};
    memcpy(F.fogcolor, z, 16); memcpy(F.speccolor, zz, 16); memcpy(F.shadowpx, zz, 16); memcpy(F.light, l, 16); memcpy(F.skyc, one, 16); memcpy(F.groundc, one, 16); memcpy(F.grade, gr, 16);
    F.ver++;
}
/* the game's words and panels are laid out for a 16:9 TV: on a wider phone they keep their shape in a 16:9 area in the middle */
static float hud_k(void) { float a = (float)g_w / (float)g_h; return a > 16.0f / 9.0f ? (16.0f / 9.0f) / a : 1.0f; }
void gfx_camera_2d(void) { gfx_camera_2d_virtual(1280.0f, 720.0f); }
void gfx_camera_ui3d(float eye) { gfx_camera_ui3d_v(eye, 1280.0f, 720.0f); }
void gfx_camera_ui3d_v(float eye, float vw, float vh) {
    Mat4 m; memset(&m, 0, sizeof m); float k = hud_k();
    m.m[0] = k * eye / (vw * 0.5f); m.m[12] = -eye * k; m.m[5] = -eye / (vh * 0.5f); m.m[13] = eye; m.m[11] = -1.0f; m.m[15] = eye;
    flat_frame(m.m);
}
void gfx_camera_2d_virtual(float vw, float vh) {
    Mat4 id = m4_identity(); float k = hud_k();
    float a = 1280.0f / vw, b = 720.0f / vh;
    id.m[0] = a * k; id.m[5] = b; id.m[12] = (a - 1.0f) * k; id.m[13] = 1.0f - b;
    flat_frame(id.m);
}
void gfx_bind(Texture *t) {
    if (!t) t = g_white;
    if (t == g_bound) return;
    g_bound = t;
    if (t->stream) gfx_texture_stream_gpu(t);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, t->id);
}
void gfx_state(int flags) {
    if (g_in_shadow || g_skip) return;
    if (flags == g_flags) return;
    g_flags = flags;
    if (flags & ST_DEPTH) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glDepthMask((flags & ST_DEPTH_WRITE) ? GL_TRUE : GL_FALSE);
    glDepthFunc((flags & ST_LESS) ? GL_LESS : GL_LEQUAL);
    if (flags & ST_CULL) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (flags & ST_AMASK) glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE); else glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBlendEquation(GL_FUNC_ADD);
    if (flags & ST_MULDA) { glEnable(GL_BLEND); glBlendFuncSeparate(GL_ZERO, GL_DST_ALPHA, GL_ONE, GL_ZERO); }
    else if (flags & ST_BLEND) { glEnable(GL_BLEND);
        if (flags & ST_ADD) glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
        else glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA); }
    else glDisable(GL_BLEND);
}
void gfx_draw(const Mesh *m, const Mat4 *model, float r, float g, float b, float a) { gfx_draw_ex(m, model, r, g, b, a, 0, 0, 0, 0); }
void gfx_draw_ex(const Mesh *m, const Mat4 *model, float r, float g, float b, float a, float specular, int unlit, float scroll_u, float scroll_v) {
    if (!m || m->count == 0 || g_skip) return;
    Prog *p = g_in_shadow ? &PS : &P; int *ver = g_in_shadow ? &g_prog_ver_s : &g_prog_ver;
    if (*ver != F.ver) {
        *ver = F.ver;
        glUniformMatrix4fv(p->viewproj, 1, GL_FALSE, F.viewproj);
        if (p->eye >= 0) glUniform4fv(p->eye, 1, F.eye);
        if (!g_in_shadow) {
            glUniform4fv(p->lightdir, 1, F.lightdir); glUniform4fv(p->fog, 1, F.fog);
            glUniformMatrix4fv(p->shadowvp, 1, GL_FALSE, F.shadowvp); glUniform4fv(p->shadowp, 1, F.shadowp);
            glUniform4fv(p->fogcolor, 1, F.fogcolor); glUniform4fv(p->speccolor, 1, F.speccolor); glUniform4fv(p->shadowpx, 1, F.shadowpx);
            glUniform4fv(p->light, 1, F.light); glUniform4fv(p->skyc, 1, F.skyc); glUniform4fv(p->groundc, 1, F.groundc); glUniform4fv(p->grade, 1, F.grade);
        }
    }
    float tint[4] = {r, g, b, a};
    float params[4] = {specular + (unlit ? 0.0f : 10.0f * (float)g_bump), unlit ? 1.0f : 0.0f, scroll_u, scroll_v};
    glUniformMatrix4fv(p->model, 1, GL_FALSE, model->m); glUniform4fv(p->tint, 1, tint); glUniform4fv(p->params, 1, params);
    glBindVertexArray(m->vao);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m->count);
}
unsigned gfx_port_resolved_fbo(void) { return g_res_fbo; }
