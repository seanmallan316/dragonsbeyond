// coop_test.c - with Scarlet rescued (from a save), a second controller joins with A, moves its own dragon, and leaves with +.
#include "../src/game/game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(void) {
    remove("coop_test_save1.txt"); remove("coop_test_save2.txt"); remove("coop_test_save3.txt");   /* no leftovers from an earlier run */
    FILE *f = fopen("coop_test_save.txt", "w"); fprintf(f, "DB1 0 1 10.0 -40.00 60.00 0.000 5.0 0 1 0 0.0 3.0 0\n"); fclose(f);
    game_set_save_path("coop_test_save.txt");
    if (!game_init(0)) { printf("FAIL: init\n"); return 1; }
    Pad pads[KK_PADS]; float x0 = 0, z0 = 0;
    for (int fr = 0; fr < 60 * 40; fr++) {
        memset(pads, 0, sizeof pads); float t = fr / 60.0f;
        pads[0].source = 1; pads[1].source = 4;
        if (t < 3) pads[0].a = (fr % 40) < 4;                 /* get through the title, name and story with the GamePad */
        else if (t < 12) pads[0].a = (fr % 30) < 3;
        else pads[0].sy = 0.3f;                                /* player 1 walks slowly */
        if (fr == 60 * 16) pads[1].a = 1;                      /* player 2 joins */
        if (t > 17 && t < 25) { pads[1].sx = 1; pads[1].zr = 1; }   /* and runs off to the side */
        if (fr == 60 * 30) pads[1].plus = 1;                   /* then leaves */
        game_frame(pads, KK_PADS, 1.0f / 60);
        const char *w = game_where();
        if (fr == 60 * 17) { const char *q = strstr(w, "p2 "); if (!q || atoi(q + 3) != 1) { printf("FAIL: player 2 did not join: %s\n", w); return 1; } sscanf(strstr(w, "p2 1 at ") + 8, "%f,%f", &x0, &z0); }
        if (fr == 60 * 25) { float x1, z1; sscanf(strstr(w, "p2 1 at ") + 8, "%f,%f", &x1, &z1); float d = (x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0);
            printf("player 2 moved %.1f m\n", d > 0 ? __builtin_sqrtf(d) : 0); if (d < 25) { printf("FAIL: player 2 did not move\n"); return 1; } }
        if (fr == 60 * 31) { const char *q = strstr(w, "p2 "); if (!q || atoi(q + 3) != -1) { printf("FAIL: player 2 did not leave: %s\n", w); return 1; } } }
    remove("coop_test_save.txt"); remove("coop_test_save1.txt");
    printf("coop test OK\n"); return 0; }
