#!/usr/bin/env bash
# Builds the PC version of the port (for testing on the build machine): build_pc.sh <Wii U repo> <SDL2 headers dir> <content dir> [extra flags]
set -euo pipefail
WIIU=$(cd "$1" && pwd); SDLINC=$2; CONTENT=$3; shift 3
HERE=$(cd "$(dirname "$0")" && pwd); OUT=${OUT:-$HERE/build-pc}
mkdir -p "$OUT"
SRC=$(cd "$WIIU/src" && ls game/*.c audio/*.c | sed "s|^|$WIIU/src/|")
CFLAGS="-O2 -g -w -std=gnu11 -I$WIIU/src -I$HERE -I$SDLINC -DKK_CONTENT_DIR=\"$CONTENT/\" -D_GNU_SOURCE $*"
gcc $CFLAGS -c "$WIIU/src/render/assets.c" -o "$OUT/assets.o" -Dgfx_texture_rgba=gfx_texture_rgba_bytes -Dgfx_texture_rgba_ex=gfx_texture_rgba_ex_bytes
gcc $CFLAGS -o "$OUT/dragonsbeyond" $SRC "$HERE/gfx_gles.c" "$HERE/sdl_main.c" "$OUT/assets.o" /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0 -lm -lpthread -ldl
echo "built $OUT/dragonsbeyond"
