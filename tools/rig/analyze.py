"""analyze.py model.glb : prints the mesh layout (bounds, slices along the body) to place bones. Run with python3."""
import bpy, sys, numpy as np
src = sys.argv[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
obs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
P = []
for o in obs:
    mw = o.matrix_world
    P += [tuple(mw @ v.co) for v in o.data.vertices]
P = np.array(P)            # Blender: z up, glTF +z front becomes Blender -y front
print('objects', [o.name for o in obs], 'verts', len(P))
lo, hi = P.min(0), P.max(0); print('bounds x', lo[0], hi[0], ' y', lo[1], hi[1], ' z', lo[2], hi[2])
# slices along y (front/back axis). For each slice: z range, x range, count
n = 24
ys = np.linspace(lo[1], hi[1], n + 1)
for i in range(n):
    s = P[(P[:, 1] >= ys[i]) & (P[:, 1] < ys[i + 1])]
    if len(s) == 0: continue
    print(f'y {ys[i]:+.3f}..{ys[i+1]:+.3f} n={len(s):6d} z {s[:,2].min():.3f}..{s[:,2].max():.3f} x {s[:,0].min():+.3f}..{s[:,0].max():+.3f}  zmed {np.median(s[:,2]):.3f}')
