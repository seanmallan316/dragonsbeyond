// game_test.c - runs the whole game (with a renderer that draws nothing) for a few thousand frames: it must start, load the
// dragon models from content/, run the first minute with a scripted player, and never crash or overflow a mesh.
#include "../src/game/game.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
extern int g_stub_draws, g_stub_frames;
int main(void) {
    if (!game_init(0)) { printf("FAIL: game_init\n"); return 1; }
    Pad pads[KK_PADS]; memset(pads, 0, sizeof pads);
    for (int f = 0; f < 3600; f++) {
        float t = f / 60.0f; memset(pads, 0, sizeof pads); pads[0].source = 1;
        pads[0].sy = 1; pads[0].sx = sinf(t * 0.5f);                 /* wander */
        pads[0].x = (f % 90) == 0;                                     /* try to eat now and then */
        pads[0].zr = t > 10;                                           /* run */
        pads[0].a = (f % 300) < 60; pads[0].y = f == 1200;             /* hop or flap in bursts, roar once */
        pads[0].up = (f % 1200) == 599;                                /* change the view */
        game_frame(pads, KK_PADS, 1.0f / 60);
        if (f % 600 == 0) printf("t=%.0fs  %s\n", t, game_where());
    }
    printf("frames %d, draw calls %d (%.1f per frame)\n", g_stub_frames, g_stub_draws, g_stub_draws / (float)g_stub_frames);
    if (g_stub_frames != 3600 || g_stub_draws < 3600) { printf("FAIL: frames\n"); return 1; }
    printf("game test OK\n"); return 0; }
