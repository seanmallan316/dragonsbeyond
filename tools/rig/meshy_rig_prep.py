"""meshy_rig_prep.py out.blend walk.glb [run.glb] : turns Meshy's rigged-and-animated GLBs into a blend the dragon baker reads
(armature 'DragonRig', mesh 'Dragon', actions named after the game's clips: idle, walk, run)."""
import bpy, sys
out, walk = sys.argv[1], sys.argv[2]; run = sys.argv[3] if len(sys.argv) > 3 else None
bpy.ops.wm.read_factory_settings(use_empty=True)
def load(path):
    before = set(bpy.data.objects); acts0 = set(bpy.data.actions)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]; acts = [a for a in bpy.data.actions if a not in acts0]
    return new, acts
objs, acts = load(walk)
arm = [o for o in objs if o.type == 'ARMATURE'][0]; meshes = [o for o in objs if o.type == 'MESH']
bpy.ops.object.select_all(action='DESELECT')
for m in meshes: m.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1: bpy.ops.object.join()
me = bpy.context.view_layer.objects.active
arm.name = 'DragonRig'; me.name = 'Dragon'
wa = acts[0]; wa.name = 'walk'; wa.use_cyclic = True; wa.use_fake_user = True
if run:
    o2, a2 = load(run); ra = a2[0]; ra.name = 'run'; ra.use_cyclic = True; ra.use_fake_user = True
    for o in o2: bpy.data.objects.remove(o, do_unlink=True)
# idle: the walk's first pose, held, with a slow breath in the chest (a gentle bob of the hips)
arm.animation_data_create(); arm.animation_data.action = wa
try: arm.animation_data.action_slot = wa.slots[0]
except Exception: pass
f0 = int(wa.frame_range[0]); bpy.context.scene.frame_set(f0)
idle = bpy.data.actions.new('idle'); idle.use_cyclic = True; idle.use_fake_user = True
pose = {pb.name: (pb.location.copy(), pb.rotation_quaternion.copy(), pb.rotation_euler.copy(), pb.rotation_mode) for pb in arm.pose.bones}
arm.animation_data.action = idle
hips = arm.pose.bones[0].name
for f in (0, 30, 60):
    for pb in arm.pose.bones:
        l, q, e, mode = pose[pb.name]; pb.location = l; pb.rotation_quaternion = q; pb.rotation_euler = e
        if pb.name == hips and f == 30: pb.location = (l[0], l[1], l[2] - 0.01)
        pb.keyframe_insert('location', frame=f); pb.keyframe_insert('rotation_quaternion' if mode == 'QUATERNION' else 'rotation_euler', frame=f)
idle.frame_range = (0, 59)
for a in bpy.data.actions:
    fr = tuple(a.frame_range); a.use_frame_range = True; a.frame_start = round(fr[0]); a.frame_end = round(fr[1]); print('action', a.name, a.frame_start, a.frame_end)
bpy.ops.wm.save_as_mainfile(filepath=out); print('saved', out)
