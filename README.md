# Dragons Beyond

An original 3D multiplayer dragon adventure for the modded Wii U (Aroma homebrew), designed by a 10-year-old.

Seven dragon eggs hatch on the same night. Six dragonets are snatched by the dragon tamer Kaine Blackfeather.
The little forest dragonet is left behind, grows up by eating, learns to fly, rescues the others, and together
the seven dragons free the Master Dragon, Everwing.

Game plan: see the shared design doc (Claude Docs, "Dragons Beyond — Game Plan").

## Folders
- `ref/` the designer's original drawings
- `concept/` approved concept renders (`forest_APPROVED.jpg` is the look to match)
- `meshy/` 3D models from Meshy (`.glb`, PBR textured)
- `rig/` Blender files: fitted skeleton + skin weights (`*_anim.blend` adds the 9 animation clips)
- `preview/` turntables, pose tests and animation previews
- `tools/` the pipeline: Meshy generation, turntable renderer, rig fitting, animation, clip rendering

## Pipeline (per dragon)
1. `tools/meshy_dragon.py <name> <approved side render>` → `meshy/<name>.glb`
2. `node tools/turntable.js meshy/<name>.glb preview/<name>` + `tools/sheet.py` → spinning preview
3. `python3 tools/rig/rig_build.py meshy/<name>.glb rig/<name>.blend` → skeleton + weights
4. `python3 tools/rig/animate.py rig/<name>.blend rig/<name>_anim.blend` → idle/walk/run/fly/glide/eat/roar/tailwhip/hit
5. `python3 tools/rig/render_clip.py rig/<name>_anim.blend <clip> preview/clips/<name>_<clip>` → preview frames
