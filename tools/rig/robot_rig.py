"""robot_rig.py hound|drone model.glb out.blend : rigs a Meshy robot with rigid parts (each vertex follows one bone, the way a machine
moves) and adds its game clips, in the layout the dragon baker reads (armature 'DragonRig', mesh 'Dragon').
hound: body, head, tail, four legs found by where they are; clips idle, walk, run, crouch (wind-up before a lunge), lunge.
drone: body, left and right wing, tail; clips fly (fast wingbeat), glide, dive (wings swept back)."""
import bpy, sys, math, mathutils, numpy as np
kind, src, out = sys.argv[1], sys.argv[2], sys.argv[3]
bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.import_scene.gltf(filepath=src)
obs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
bpy.ops.object.select_all(action='DESELECT')
for o in obs: o.select_set(True)
bpy.context.view_layer.objects.active = obs[0]
if len(obs) > 1: bpy.ops.object.join()
me = bpy.context.view_layer.objects.active; me.name = 'Dragon'
for o in list(bpy.context.scene.objects):
    if o != me: bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
P = np.array([tuple(v.co) for v in me.data.vertices]); lo, hi = P.min(0), P.max(0); C = (lo + hi) / 2; D = hi - lo
# stand on z = 0, centred on x/y
off = mathutils.Vector((-C[0], -C[1], -lo[2]))
for v in me.data.vertices: v.co += off
P = np.array([tuple(v.co) for v in me.data.vertices]); lo, hi = P.min(0), P.max(0); D = hi - lo; H = D[2]
print('size', D.round(3))
bpy.ops.object.armature_add(location=(0, 0, 0)); arm = bpy.context.object; arm.name = 'DragonRig'
bpy.ops.object.mode_set(mode='EDIT'); eb = arm.data.edit_bones; eb.remove(eb[0])
def bone(name, head, tail, parent=None):
    b = eb.new(name); b.head = head; b.tail = tail; b.roll = 0
    if parent: b.parent = eb[parent]
    return b
x, y, z = P[:, 0], P[:, 1], P[:, 2]; front_y, back_y = lo[1], hi[1]   # front of the model is -y
assign = np.zeros(len(P), dtype=int); names = []
if kind == 'hound':
    belly = lo[2] + 0.42 * H   # below this: legs
    body_mid = 0.5 * (front_y + back_y)
    names = ['body', 'head', 'tail', 'legFL', 'legFR', 'legBL', 'legBR']
    bone('body', (0, body_mid, 0.6 * H), (0, body_mid - 0.2 * D[1], 0.6 * H))
    head_y = front_y + 0.3 * D[1]; bone('head', (0, head_y, 0.72 * H), (0, front_y, 0.8 * H), 'body')
    tail_y = back_y - 0.18 * D[1]; bone('tail', (0, tail_y, 0.62 * H), (0, back_y, 0.95 * H), 'body')
    legs = P[z < belly]; fy = np.median(legs[legs[:, 1] < body_mid][:, 1]) if (legs[:, 1] < body_mid).any() else front_y + 0.25 * D[1]
    by = np.median(legs[legs[:, 1] >= body_mid][:, 1]) if (legs[:, 1] >= body_mid).any() else back_y - 0.25 * D[1]
    for tag, sx, ly in (('FL', 1, fy), ('FR', -1, fy), ('BL', 1, by), ('BR', -1, by)):
        lx = np.median(legs[(np.sign(legs[:, 0]) == sx)][:, 0]) if (np.sign(legs[:, 0]) == sx).any() else sx * 0.2 * D[0]
        bone('leg' + tag, (lx, ly, belly + 0.08 * H), (lx, ly, 0.0))   # not parented to the body: the feet stay on the ground when the body bobs or crouches
    for i, (px, py, pz) in enumerate(P):
        if pz < belly: assign[i] = 3 + (0 if py < body_mid else 2) + (0 if px > 0 else 1)
        elif py < head_y and pz > 0.45 * H: assign[i] = 1
        elif py > tail_y and pz > 0.5 * H: assign[i] = 2
        else: assign[i] = 0
else:   # drone
    half = 0.17 * D[0]   # the body is the middle third across
    names = ['body', 'wingL', 'wingR', 'tail']
    cz = float(np.median(P[np.abs(x) < half][:, 2])); bone('body', (0, 0, cz), (0, -0.3 * D[1], cz))
    bone('wingL', (half, 0, cz), (hi[0], 0, cz), 'body'); bone('wingR', (-half, 0, cz), (lo[0], 0, cz), 'body')
    bone('tail', (0, 0.25 * D[1], cz), (0, back_y, cz - 0.2 * H), 'body')
    for i, (px, py, pz) in enumerate(P):
        if px > half: assign[i] = 1
        elif px < -half: assign[i] = 2
        elif py > 0.25 * D[1] and pz < cz: assign[i] = 3
        else: assign[i] = 0
bpy.ops.object.mode_set(mode='OBJECT')
for k, n in enumerate(names):
    vg = me.vertex_groups.new(name=n); idx = [int(i) for i in np.where(assign == k)[0]]
    if idx: vg.add(idx, 1.0, 'REPLACE')
    print(n, len(idx), 'vertices')
mod = me.modifiers.new('arm', 'ARMATURE'); mod.object = arm; me.parent = arm
pb = arm.pose.bones; r = math.radians
for b in pb: b.rotation_mode = 'XYZ'
arm.animation_data_create()
def clip(name, frames, loop, fn):
    a = bpy.data.actions.new(name); a.use_fake_user = True; a.use_cyclic = loop; arm.animation_data.action = a
    try: arm.animation_data.action_slot = a.slots[0]
    except Exception: pass
    for f in list(range(0, frames, 2)) + [frames]:
        t = f / frames; rots, locs = fn(t)
        for b in pb:
            b.rotation_euler = tuple(r(v) for v in rots.get(b.name, (0, 0, 0))); b.location = locs.get(b.name, (0, 0, 0))
            b.keyframe_insert('rotation_euler', frame=f); b.keyframe_insert('location', frame=f)
    a.use_frame_range = True; a.frame_start = 0; a.frame_end = frames - (1 if loop else 0); print('clip', name, frames)
S = lambda t, ph=0: math.sin(2 * math.pi * t + ph)
if kind == 'hound':
    def gait(t, amp, bob, head):   # diagonal pairs swing together (a trot); legs swing about the side-to-side axis
        s = S(t); return ({'legFL': (amp * s, 0, 0), 'legBR': (amp * s, 0, 0), 'legFR': (-amp * s, 0, 0), 'legBL': (-amp * s, 0, 0),
                           'head': (head * S(2 * t), 0, 3 * S(t)), 'tail': (0, 0, 25 * S(t)), 'body': (2 * S(2 * t), 0, 0)}, {'body': (0, bob * abs(S(t)) * H, 0)})
    clip('idle', 60, True, lambda t: ({'head': (4 * S(t), 0, 18 * S(t, 1)), 'tail': (0, 0, 15 * S(2 * t)), 'body': (1.5 * S(t), 0, 0)}, {}))
    clip('walk', 24, True, lambda t: gait(t, 26, 0.03, 4))
    clip('run', 14, True, lambda t: gait(t, 42, 0.06, 6))
    clip('crouch', 24, True, lambda t: ({'body': (-5, 0, 0), 'head': (8 + 3 * S(4 * t), 0, 4 * S(2 * t)), 'tail': (-25, 0, 35 * S(3 * t))}, {'body': (0, 0, -0.07 * H + 0.008 * H * S(4 * t))}))   # low, shivering, tail lashing
    clip('lunge', 14, True, lambda t: ({'legFL': (32, 0, 0), 'legFR': (32, 0, 0), 'legBL': (-28, 0, 0), 'legBR': (-28, 0, 0), 'head': (-6, 0, 0), 'tail': (15, 0, 0), 'body': (3, 0, 0)}, {}))   # stretched out mid-leap
else:
    def flap(t, amp, base):
        a = base + amp * S(t); return {'wingL': (a, 0, 0), 'wingR': (a, 0, 0), 'tail': (8 * S(t, 1), 0, 0)}
    clip('fly', 12, True, lambda t: (flap(t, 32, 5), {'body': (0, 0, 0.03 * H * S(t, 3.14))}))
    clip('glide', 40, True, lambda t: (flap(t, 6, 8), {}))
    clip('dive', 20, True, lambda t: ({'wingL': (-20, 0, 35), 'wingR': (-20, 0, -35), 'tail': (-10, 0, 0)}, {}))
arm.animation_data.action = bpy.data.actions[0]
bpy.ops.wm.save_as_mainfile(filepath=out); print('saved', out)
