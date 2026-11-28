#!/usr/bin/env bash
# Builds the PC version for a web browser (WebAssembly + WebGL 2): build_web.sh <out dir>
# Needs Emscripten (emcc) on the PATH. The same game code as the phone and the PC; android/port/web/ replaces the file layer.
# The page needs the game's files next to it in content/ (make them with android/port/web/web_content.py).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../../.." && pwd); PORT=$ROOT/android/port; OUT=${1:-$PORT/web/out}
mkdir -p "$OUT"
FLAGS="-O3 -w -std=gnu11 -ffast-math -fno-math-errno -I$ROOT/src -I$PORT -DKK_CONTENT_DIR=\"content/\" -D_GNU_SOURCE -include $PORT/kk_files.h -sUSE_SDL=2"
emcc $FLAGS -c "$ROOT/src/render/assets.c" -o "$OUT/assets.o" -Dgfx_texture_rgba=gfx_texture_rgba_bytes -Dgfx_texture_rgba_ex=gfx_texture_rgba_ex_bytes
emcc $FLAGS -o "$OUT/dragonsbeyond.js" "$ROOT"/src/game/*.c "$ROOT"/src/audio/*.c "$PORT/gfx_gles.c" "$PORT/sdl_main.c" "$PORT/web/kk_files_web.c" "$OUT/assets.o" \
    -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sGL_ENABLE_GET_PROC_ADDRESS=1 -sASYNCIFY -sASYNCIFY_STACK_SIZE=262144 \
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sMAXIMUM_MEMORY=4294967296 -sSTACK_SIZE=4194304 \
    -sFORCE_FILESYSTEM=1 -lidbfs.js -sEXIT_RUNTIME=0 -sENVIRONMENT=web -sINVOKE_RUN=0 -sEXPORTED_RUNTIME_METHODS=callMain,FS
rm -f "$OUT/assets.o"
cp "$PORT/web/index.html" "$OUT/index.html"
echo "built $OUT"
