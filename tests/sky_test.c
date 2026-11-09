// sky_test.c - from a save with Scarlet freed: walk into the wind column, arrive in the Sky Kingdom, walk off an island edge
// and get put back on solid ground; the Snow dragonet's cage and Stormtalon are there.
#include "../src/game/game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
static float num_after(const char *w, const char *key) { const char *q = strstr(w, key); return q ? (float)atof(q + strlen(key)) : -999; }
int main(void) {
    FILE *f = fopen("sky_test_save.txt", "w");   /* DB2: name diff world kind fk x z yaw res0 res1 cry0 cry1 | has stage growth hearts x3 */
    fprintf(f, "DB2 0 1 0 0 1 -54.00 52.00 0.785 1 0 0 0 1 1 10.0 5.0 1 0 0.0 3.0 0 0 0.0 3.0\n"); fclose(f);
    game_set_save_path("sky_test_save.txt");
    if (!game_init(0)) { printf("FAIL: init\n"); return 1; }
    Pad pads[KK_PADS]; int arrived = -1, fell = 0;
    for (int fr = 0; fr < 60 * 60; fr++) {
        memset(pads, 0, sizeof pads); pads[0].source = 1; float t = fr / 60.0f;
        const char *w = game_where();
        if (t < 4) pads[0].a = (fr % 40) < 4;                        /* title, slot 1 (continues the save) */
        else if (arrived < 0) {                                       /* walk to the wind column at the clearing's middle (-62, 60) */
            float x = 0, y = 0, z = 0; const char *q = strstr(w, " at "); if (q) sscanf(q + 4, "%f,%f,%f", &x, &y, &z); float dx = -62 - x, dz = 60 - z;
            float ang = atan2f(dx, dz); pads[0].sx = sinf(ang - num_after(w, "camyaw ")); pads[0].sy = cosf(ang - num_after(w, "camyaw ")); }
        else if (t - arrived / 60.0f > 6 && !fell) { pads[0].sy = 1; pads[0].zr = 1; }   /* run straight off the edge of the first island */
        game_frame(pads, KK_PADS, 1.0f / 60);
        w = game_where();
        if (arrived < 0 && strstr(w, "Sky Kingdom")) { arrived = fr; printf("arrived in the Sky Kingdom after %.1f s: %s\n", t, w); }
        if (arrived >= 0 && strstr(w, "msg LOST IN THE CLOUDS")) { fell = 1; printf("fell and came back: %s\n", w); break; } }
    remove("sky_test_save.txt");
    if (arrived < 0) { printf("FAIL: never reached the Sky Kingdom: %s\n", game_where()); return 1; }
    if (!fell) { printf("FAIL: never fell off an edge: %s\n", game_where()); return 1; }
    printf("sky test OK\n"); return 0; }
