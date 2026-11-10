// tour_test.c - the self-playing tour (DB_TOUR) must still beat Stormtalon with the real game code: scenery and boss collision,
// all three camera views. Fails if the boss is not down within 150 game seconds.
#include "game/game.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    if (!game_init(0)) { printf("FAIL: game_init\n"); return 1; }
    Pad pads[KK_PADS]; memset(pads, 0, sizeof pads); int seen[3] = {0};
    for (int f = 0; f < 15 * 150; f++) {
        game_frame(pads, KK_PADS, 1.0f / 15); const char *w = game_where(); const char *c = strstr(w, "cam ");
        if (c && c[4] >= '0' && c[4] <= '2') seen[c[4] - '0'] = 1;
        if (strstr(w, "Sky Kingdom") && strstr(w, " hp 0 ")) { printf("tour beat the boss after %.1f s, views seen %d%d%d\ntour test OK\n", f / 15.0f, seen[0], seen[1], seen[2]); return 0; } }
    printf("FAIL: boss not beaten: %s\n", game_where()); return 1; }
