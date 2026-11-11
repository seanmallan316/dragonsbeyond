# Dragons Beyond — Android

The Wii U game itself, running on Android phones. Every build uses this repository's own game code (`src/`) and files
(`content/`), so the phone version always has everything the Wii U version has. Only the Wii U hardware parts
(`src/platform/`) are replaced, by the files in `port/`:
- `port/gfx_gles.c`: the Wii U lighting on OpenGL ES 3 at the phone's full resolution, 4x smooth edges, 2048 sun shadows,
  and the same finishing pass (edge smoothing, sharpen, glow, vignette);
- `port/sdl_main.c`: touch controls (move stick — push to the edge to run — FLY, EAT, TAIL, ROAR, VIEW, SWAP, drag on the
  right to turn the camera), Bluetooth controllers as Pro Controllers, sound, vibration, carved side panels on wide phones.

Built by `.github/workflows/android.yml` on every push to main; the APK goes on the Releases page (`android-N`).
`tools/make_ui.py` redraws the touch controls, side panels and icon art.
