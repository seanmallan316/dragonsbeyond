// game.h - what the platform layer calls each frame (Dragons Beyond).
#ifndef DB_GAME_H
#define DB_GAME_H
#include <stdint.h>

#define KK_VERSION "0.1 proof of concept"
#define KK_PADS 5      /* controller slots: 0 the GamePad, 1..4 the Wii controllers */
typedef struct { float sx, sy; int a, b, zl, zr, plus, x, y; int source; int touch; float tx, ty; int up, down, left, right; } Pad;

int  game_init(int gfx_status);
void game_frame(const Pad *pads, int npads, float dt);
const char *game_where(void);
void game_shutdown(void);
void platform_rumble(float strength, float seconds);
void platform_rumble_pad(int slot, float strength, float seconds);
#endif
