"""bake_kka.py anim.blend out_base [target_tris] [fps]
Bakes every clip of a rigged dragon into the game's KKA2 format (per-frame corner positions + normals, big-endian),
after reducing the mesh to about target_tris triangles. Writes out_base.kka and out_base_anim.png (base colour).
Game axes: y up, front +z (Blender z up, front -y)  ->  (x, z, -y)."""
import bpy, sys, struct, numpy as np
src, out = sys.argv[1], sys.argv[2]
target = int(sys.argv[3]) if len(sys.argv) > 3 else 7000; fps = float(sys.argv[4]) if len(sys.argv) > 4 else 15.0
bpy.ops.wm.open_mainfile(filepath=src)
arm = bpy.data.objects['DragonRig']; me = bpy.data.objects['Dragon']
# base colour texture
img = None
for m in me.data.materials:
    if m and m.use_nodes:
        for l in m.node_tree.links:
            if l.to_socket.name == 'Base Color' and l.from_node.type == 'TEX_IMAGE': img = l.from_node.image
if img is None:
    for m in me.data.materials:
        for nd in m.node_tree.nodes:
            if nd.type == 'TEX_IMAGE' and nd.image: img = nd.image; break
assert img, 'no base colour texture'
img.filepath_raw = out + '_anim.png'; img.file_format = 'PNG'; img.save(); print('texture', img.size[:])
# reduce the mesh (before the armature in the stack) so the game moves far fewer corners
ntri0 = sum(len(p.vertices) - 2 for p in me.data.polygons)
bpy.context.view_layer.objects.active = me
d = me.modifiers.new('lod', 'DECIMATE'); d.ratio = min(1.0, target / max(1, ntri0)); d.use_collapse_triangulate = True
while me.modifiers[0] != d: bpy.ops.object.modifier_move_up(modifier='lod')
bpy.ops.object.modifier_apply(modifier='lod')
for p in me.data.polygons: p.use_smooth = True
m0 = me.data; m0.calc_loop_triangles(); uvl = m0.uv_layers.active.data
keys = {}; cv = []; cuv = []; tris = []
for lt in m0.loop_triangles:
    tri = []
    for li in lt.loops:
        vi = m0.loops[li].vertex_index; uv = (round(uvl[li].uv[0], 5), round(1.0 - uvl[li].uv[1], 5)); k = (vi, uv)
        if k not in keys: keys[k] = len(cv); cv.append(vi); cuv.append(uv)
        tri.append(keys[k])
    tris.append(tri)
cv = np.array(cv); print('corners', len(cv), 'tris', len(tris), '(from', ntri0, ')')
dg = bpy.context.evaluated_depsgraph_get()
def grab():
    dg.update(); ev = me.evaluated_get(dg); m = ev.to_mesh()
    P = np.array([(me.matrix_world @ v.co)[:] for v in m.vertices]); N = np.array([(me.matrix_world.to_3x3() @ v.normal).normalized()[:] for v in m.vertices]); ev.to_mesh_clear()
    P = P[cv]; N = N[cv]
    return np.stack([P[:, 0], P[:, 2], -P[:, 1]], 1), np.stack([N[:, 0], N[:, 2], -N[:, 1]], 1)
sc = bpy.context.scene
clips = []
for a in bpy.data.actions:
    if a.name not in ('idle', 'walk', 'run', 'fly', 'glide', 'eat', 'roar', 'tailwhip', 'hit', 'dive'): continue
    arm.animation_data.action = a
    try: arm.animation_data.action_slot = a.slots[0]
    except Exception: pass
    f0, f1 = a.frame_start, a.frame_end; loop = bool(a.use_cyclic); dur = (f1 - f0 + (1 if loop else 0)) / 30.0
    nf = max(2, int(round(dur * fps))); frames = []
    for i in range(nf):
        f = f0 + (f1 - f0 + (1 if loop else 0)) * i / (nf if loop else max(nf - 1, 1))
        sc.frame_set(int(f), subframe=f - int(f)); frames.append(grab())
    clips.append((a.name, nf, dur, loop, frames)); print('clip', a.name, nf, 'frames', round(dur, 2), 's')
order = ['idle', 'walk', 'run', 'fly', 'glide', 'eat', 'roar', 'tailwhip', 'hit', 'dive']; clips.sort(key=lambda c: order.index(c[0]))
y0 = min(P[:, 1].min() for c in clips for P, N in c[4]) if clips else 0   # feet on y = 0
y0 = clips[0][4][0][0][:, 1].min()
with open(out + '.kka', 'wb') as f:
    f.write(b'KKA2'); f.write(struct.pack('>III', len(cv), len(tris), len(clips)))
    f.write(np.array(cuv, '>f4').tobytes()); f.write(np.array(tris, '>u4').tobytes())
    for name, nf, dur, loop, frames in clips:
        f.write(name.encode().ljust(16, b'\0')); f.write(struct.pack('>IfI', nf, dur, int(loop)))
        for P, N in frames:
            P = P.copy(); P[:, 1] -= y0
            q = np.clip(np.round(P * 4000), -32767, 32767).astype('>i2'); n = np.clip(np.round(N * 127), -127, 127).astype('i1')
            rec = np.zeros(len(P), dtype=[('p', '>i2', 3), ('n', 'i1', 3), ('pad', 'i1')]); rec['p'] = q; rec['n'] = n; f.write(rec.tobytes())
import os; print('wrote', out + '.kka', os.path.getsize(out + '.kka') // 1024, 'KB; size (m)', np.ptp(clips[0][4][0][0], 0).round(2))
