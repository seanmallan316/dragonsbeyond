// coop_test.c - multiplayer: from the title pick MULTIPLAYER, player 2 joins with A on a second controller, both pick
// dragons and get ready, then both walk along the side-view lane (they must stay on it and move), and a
// story save is never written by it.
#include "../src/game/game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
int main(void) {
    remove("coop_test_save.txt"); remove("coop_test_save1.txt"); remove("coop_test_save2.txt"); remove("coop_test_save3.txt");
    game_set_save_path("coop_test_save.txt");
    if (!game_init(0)) { printf("FAIL: init\n"); return 1; }
    Pad pads[KK_PADS]; float x1a = 0, x2a = 0, z1a = 0;
    for (int fr = 0; fr < 60 * 40; fr++) {
        memset(pads, 0, sizeof pads); float t = fr / 60.0f;
        pads[0].source = 1; pads[1].source = 4;
        if (fr == 60) pads[0].a = 1;                        /* title */
        if (fr == 90) pads[0].down = 1;                     /* MULTIPLAYER */
        if (fr == 120) pads[0].a = 1;
        if (fr == 150) pads[1].a = 1;                       /* player 2 joins */
        if (fr == 170) pads[1].right = 1;                   /* and picks another dragon */
        if (fr == 190) pads[0].a = 1;                       /* both ready */
        if (fr == 210) pads[1].a = 1;
        if (t > 6) { pads[0].sx = 1; pads[1].sx = 1; pads[0].zr = pads[1].zr = 1; if ((fr % 50) < 3) pads[0].a = 1; }   /* both run along, hopping now and then */
        game_frame(pads, KK_PADS, 1.0f / 60);
        const char *w = game_where();
        if (fr == 60 * 5) { if (strncmp(w, "playing", 7) || !strstr(w, "p2 1 ")) { printf("FAIL: multiplayer did not start with 2 players: %s\n", w); return 1; }
            sscanf(strstr(w, " at ") + 4, "%f,%*f,%f", &x1a, &z1a); sscanf(strstr(w, "p2 1 at ") + 8, "%f", &x2a); }
        if (fr == 60 * 25) { float x1, y1, z1, x2, z2; sscanf(strstr(w, " at ") + 4, "%f,%f,%f", &x1, &y1, &z1); sscanf(strstr(w, "p2 1 at ") + 8, "%f,%f", &x2, &z2);
            printf("player 1 moved %.1f m, player 2 moved %.1f m, lane z %.1f / %.1f / %.1f\n", fabsf(x1 - x1a), fabsf(x2 - x2a), z1a, z1, z2);
            if (fabsf(x1 - x1a) < 15 || fabsf(x2 - x2a) < 15) { printf("FAIL: the players did not move along the lane: %s\n", w); return 1; }
            if (fabsf(z1 - z1a) > 0.6f || fabsf(z2 - z1a) > 0.6f) { printf("FAIL: left the lane: %s\n", w); return 1; } } }
    {   /* the pause menu: + then MAIN MENU goes back to the title */
        for (int fr = 0; fr < 60; fr++) { memset(pads, 0, sizeof pads); pads[0].source = 1; pads[1].source = 4;
            if (fr == 5) pads[0].plus = 1; if (fr == 20) pads[0].down = 1; if (fr == 35) pads[0].a = 1; game_frame(pads, KK_PADS, 1.0f / 60); }
        if (strncmp(game_where(), "title", 5)) { printf("FAIL: pause > MAIN MENU did not return to the title: %s\n", game_where()); return 1; } printf("pause menu back to title OK\n"); }
    FILE *f = fopen("coop_test_save.txt", "r"); if (f) { fclose(f); printf("FAIL: multiplayer wrote a story save\n"); return 1; }
    f = fopen("coop_test_save1.txt", "r"); if (f) { fclose(f); printf("FAIL: multiplayer wrote a story save\n"); return 1; }
    printf("coop test OK\n"); return 0; }
