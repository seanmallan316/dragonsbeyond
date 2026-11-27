#!/usr/bin/env bash
# Builds the PC version of the port (for testing on the build machine): build_pc.sh <Wii U repo> <SDL2 headers dir> <content dir> [extra flags]
set -euo pipefail
WIIU=$(cd "$1" && pwd); SDLINC=$2; CONTENT=$3; shift 3
HERE=$(cd "$(dirname "$0")" && pwd); OUT=${OUT:-$HERE/build-pc}
mkdir -p "$OUT"
SRC=$(cd "$WIIU/src" && ls game/*.c audio/*.c | sed "s|^|$WIIU/src/|")
CFLAGS="-O2 -g -w -std=gnu11 -I$WIIU/src -I$HERE -I$SDLINC -DKK_CONTENT_DIR=\"$CONTENT/\" -D_GNU_SOURCE $*"
gcc $CFLAGS -c "$WIIU/src/render/assets.c" -o "$OUT/assets.o" -Dgfx_texture_rgba=gfx_texture_rgba_bytes -Dgfx_texture_rgba_ex=gfx_texture_rgba_ex_bytes
for f in "$WIIU"/src/game/*.cpp; do g++ -O2 -g -w -std=gnu++17 -fno-exceptions -fno-rtti -I$WIIU/src -c "$f" -o "$OUT/$(basename "$f" .cpp).o"; done
NAVO=$(ls "$OUT"/nav*.o)
# Effekseer (effects): compiled once into $OUT/efk, then reused
E=$HERE/effekseer; EFL="-O2 -w -std=gnu++17 -D__EFFEKSEER_RENDERER_GLES3__ -DGL_GLEXT_PROTOTYPES -include $E/efk_prefix.h -I$E/Effekseer -I$E/EffekseerRendererCommon -I$E/EffekseerRendererGL"
mkdir -p "$OUT/efk"; export EFL E OUT; find "$E" -name "*.cpp" | xargs -P 4 -I{} sh -c 'f={}; o="$OUT/efk/$(echo "${f#$E/}" | tr / _).o"; [ -f "$o" ] || g++ $EFL -c "$f" -o "$o"' 
g++ $EFL -I$WIIU/src -I$HERE -DKK_CONTENT_DIR=\"$CONTENT/\" -c "$HERE/efk.cpp" -o "$OUT/efk_glue.o"
for f in $SRC "$HERE/gfx_gles.c" "$HERE/sdl_main.c"; do gcc $CFLAGS -c "$f" -o "$OUT/c_$(basename "$f" .c).o"; done
g++ -o "$OUT/dragonsbeyond" "$OUT"/c_*.o $NAVO "$OUT"/efk/*.o "$OUT/efk_glue.o" "$OUT/assets.o" /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0 -lGLESv2 -lEGL -lm -lpthread -ldl
echo "built $OUT/dragonsbeyond"
