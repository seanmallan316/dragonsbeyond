"""prop_pack.py model.glb out_base height_m [hi_tris] [lo_tris] [texsize]
Scenery prop -> content/models/<name>.kkp (+ _tex.png). Two detail levels (near, far). Game axes: y up, standing on y = 0,
scaled so the model is height_m tall. Format (big-endian): 'KKP1', uint32 nlod, per lod: uint32 ntri, then ntri*3 corners
of 8 floats (x y z nx ny nz u v)."""
import bpy, sys, struct, numpy as np
src, out, height = sys.argv[1], sys.argv[2], float(sys.argv[3])
hi = int(sys.argv[4]) if len(sys.argv) > 4 else 1500; lo = int(sys.argv[5]) if len(sys.argv) > 5 else 350; texsize = int(sys.argv[6]) if len(sys.argv) > 6 else 1024
bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.import_scene.gltf(filepath=src)
obs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
for o in bpy.context.scene.objects: o.select_set(o in obs)
bpy.context.view_layer.objects.active = obs[0]
if len(obs) > 1: bpy.ops.object.join()
me = bpy.context.view_layer.objects.active; bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
# base colour texture, shrunk
img = None
for m in me.data.materials:
    if m and m.node_tree:
        for l in m.node_tree.links:
            if l.to_socket.name == 'Base Color' and l.from_node.type == 'TEX_IMAGE': img = l.from_node.image
        if img is None:
            for nd in m.node_tree.nodes:
                if nd.type == 'TEX_IMAGE' and nd.image: img = nd.image
assert img, 'no texture'
img.scale(texsize, texsize); img.filepath_raw = out + '_tex.png'; img.file_format = 'PNG'; img.save()
# size: Blender z is up here (glTF y-up imported as z-up)
P = np.array([tuple(v.co) for v in me.data.vertices]); zmin, zmax = P[:, 2].min(), P[:, 2].max(); cx, cy = P[:, 0].mean(), P[:, 1].mean()
scale = height / max(zmax - zmin, 1e-6)
ntri0 = sum(len(p.vertices) - 2 for p in me.data.polygons); print('source tris', ntri0, 'height', round(zmax - zmin, 3), '-> scale', round(scale, 3))
def lod(target):
    bpy.ops.object.select_all(action='DESELECT'); me.select_set(True); bpy.context.view_layer.objects.active = me
    bpy.ops.object.duplicate(); d = bpy.context.view_layer.objects.active
    if ntri0 > target: mod = d.modifiers.new('d', 'DECIMATE'); mod.ratio = target / ntri0; mod.use_collapse_triangulate = True; bpy.ops.object.modifier_apply(modifier='d')
    for p in d.data.polygons: p.use_smooth = True
    m = d.data; m.calc_loop_triangles(); uvl = m.uv_layers.active.data; out_ = []
    for lt in m.loop_triangles:
        for li in lt.loops:
            v = m.vertices[m.loops[li].vertex_index]; p = v.co; n = m.loops[li].normal if lt.use_smooth else lt.normal; uv = uvl[li].uv
            out_ += [(p.x - cx) * scale, (p.z - zmin) * scale, -(p.y - cy) * scale, n.x, n.z, -n.y, uv[0], 1 - uv[1]]
    bpy.data.objects.remove(d); return np.array(out_, '>f4').reshape(-1, 8)
lods = [lod(hi), lod(lo)]
with open(out + '.kkp', 'wb') as f:
    f.write(b'KKP1' + struct.pack('>I', len(lods)))
    for L in lods: f.write(struct.pack('>I', len(L) // 3)); f.write(L.tobytes())
print('wrote', out + '.kkp', [len(L) // 3 for L in lods], 'tris')
