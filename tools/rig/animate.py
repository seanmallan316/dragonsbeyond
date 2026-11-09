"""animate.py rig.blend out.blend : adds the game's animation clips to a dragon rig as Blender actions.
Clips (30 fps, looping unless noted): idle, walk, run, fly, glide, eat (once), roar (once), tailwhip (once), hit (once).
Bone axes (roll 0): spine/neck/tail X = nod, Z = turn; legs X = swing forward/back; wings X = raise tip (+ up)."""
import bpy, sys, math
src, out = sys.argv[1], sys.argv[2]
bpy.ops.wm.open_mainfile(filepath=src)
arm = bpy.data.objects['DragonRig']; mesh = bpy.data.objects['Dragon']
sc = bpy.context.scene; sc.render.fps = 30
pb = arm.pose.bones
has_wings = 'wingL_1' in pb
CH = 0.5 if arm.get('chibi') else 1.0   # chibi dragons nod less (short neck, big head)
dims = mesh.dimensions; H = dims.z
arm.animation_data_create()
r = math.radians
def S(t, ph=0.0): return math.sin(2 * math.pi * t + ph)
def C(t, ph=0.0): return math.cos(2 * math.pi * t + ph)

class Clip:
    def __init__(self, name, frames, loop=True):
        self.name, self.frames, self.loop = name, frames, loop
        self.act = bpy.data.actions.new(name); arm.animation_data.action = self.act
        try: arm.animation_data.action_slot = self.act.slots.new(id_type='OBJECT', name='DragonRig')
        except Exception: pass
        for b in pb: b.rotation_mode = 'XYZ'
        self.act.use_frame_range = True; self.act.frame_start = 1; self.act.frame_end = frames
        self.act.use_cyclic = loop
    def key(self, f, rots, locs=None):
        for b in pb: b.rotation_euler = (0, 0, 0); b.location = (0, 0, 0)
        for name, (x, y, z) in rots.items():
            if name in pb: pb[name].rotation_euler = (r(x), r(y), r(z))
        for name, v in (locs or {}).items():
            if name in pb: pb[name].location = v
        for b in pb:
            b.keyframe_insert('rotation_euler', frame=f)
            if b.name in ('root', 'hips'): b.keyframe_insert('location', frame=f)
    def bake(self, fn, step=2):
        for f in range(1, self.frames + 1, step):
            t = (f - 1) / self.frames
            rots, locs = fn(t); self.key(f, rots, locs)
        if self.loop:   # last frame = first frame
            rots, locs = fn(0.0); self.key(self.frames + 1, rots, locs)
        fcs = []
        if hasattr(self.act, 'layers'):
            for lay in self.act.layers:
                for st in lay.strips:
                    for cb in st.channelbags: fcs += list(cb.fcurves)
        else: fcs = list(self.act.fcurves)
        for fc in fcs:
            for kp in fc.keyframe_points: kp.interpolation = 'BEZIER'
            if self.loop: fc.modifiers.new('CYCLES')

def tail(t, amp=10, ph=0.0, nod=0.0):
    return {f'tail{i+1}': (nod, 0, amp * S(t, ph + i * 0.9)) for i in range(5)}
def wings_folded():
    return {'wingL_1': (-35, 0, -25), 'wingR_1': (-35, 0, 25), 'wingL_2': (0, 0, 45), 'wingR_2': (0, 0, -45), 'wingL_3': (0, 0, 30), 'wingR_3': (0, 0, -30)} if has_wings else {}
def wings_flap(t, amp=45, base=10):
    a = base + amp * S(t); b = 0.5 * amp * S(t, -0.9); c = 0.35 * amp * S(t, -1.6)
    return {'wingL_1': (a, 0, 0), 'wingR_1': (a, 0, 0), 'wingL_2': (b, 0, 0), 'wingR_2': (b, 0, 0), 'wingL_3': (c, 0, 0), 'wingR_3': (c, 0, 0)} if has_wings else {}
def legs_walk(t, swing=28, lift=22, ph_fl=0.0):
    out = {}
    for tag, ph in (('FL', ph_fl), ('BR', ph_fl), ('FR', ph_fl + 0.5), ('BL', ph_fl + 0.5)):
        s = S(t, 2 * math.pi * ph); c = C(t, 2 * math.pi * ph)
        out[f'leg{tag}_upper'] = (swing * s, 0, 0)
        out[f'leg{tag}_lower'] = (-lift * max(0, c) - 5, 0, 0)
        out[f'leg{tag}_foot'] = (lift * 0.6 * max(0, -c), 0, 0)
    return out

def idle(t):
    br = 2.5 * S(t)
    rots = {'spine1': (br * 0.4, 0, 0), 'spine2': (br * 0.6, 0, 0), 'chest': (br * 0.5, 0, 0),
            'neck1': (-br * 0.6 + 2 * S(t, 1.0), 0, 3 * S(0.5 * t)), 'neck2': (-br * 0.4, 0, 0), 'head': (2 * S(t, 2.0), 0, 4 * S(0.5 * t, 1.0)),
            'jaw': (max(0, 3 * S(t, 0.5)), 0, 0)}
    rots.update(tail(t, 8)); rots.update(wings_folded())
    if has_wings:
        rots['wingL_1'] = (-35 + 2 * S(t), 0, -25); rots['wingR_1'] = (-35 + 2 * S(t), 0, 25)
    return rots, {'hips': (0, 0, 0.004 * H * S(t))}
def walk(t):
    rots = legs_walk(t)
    bob = 0.01 * H * S(2 * t, 0.3)
    rots.update({'spine1': (0, 0, 4 * S(t)), 'spine2': (0, 0, 3 * S(t)), 'chest': (2 * S(2 * t), 0, -3 * S(t)),
                 'neck1': (-3 * S(2 * t), 0, 0), 'head': (3 * S(2 * t, 1.0), 0, 0)})
    rots.update(tail(t, 14, 0.5)); rots.update(wings_folded())
    return rots, {'hips': (0, 0, bob)}
def run(t):
    rots = legs_walk(t, swing=42, lift=35)
    rots.update({'spine1': (6 * S(2 * t), 0, 0), 'spine2': (6 * S(2 * t, 0.5), 0, 0), 'chest': (-5 * S(2 * t), 0, 0),
                 'neck1': (-8 - 4 * S(2 * t), 0, 0), 'neck2': (-4, 0, 0), 'head': (12 + 3 * S(2 * t, 1.0), 0, 0)})
    rots.update(tail(t, 18, 0.5, nod=-5)); rots.update(wings_folded())
    return rots, {'hips': (0, 0, 0.025 * H * max(0, S(2 * t, 0.3)))}
def fly(t):
    rots = wings_flap(t, 50, 5)
    tuck = {f'leg{tag}_upper': (35 if tag[0] == 'F' else 40, 0, 0) for tag in ('FL', 'FR', 'BL', 'BR')}
    tuck.update({f'leg{tag}_lower': (-70, 0, 0) for tag in ('FL', 'FR', 'BL', 'BR')})
    rots.update(tuck)
    rots.update({'spine1': (-6 + 2 * S(t), 0, 0), 'spine2': (-4 + 2 * S(t, 0.5), 0, 0), 'chest': (-4, 0, 0),
                 'neck1': (-6, 0, 0), 'neck2': (-6, 0, 0), 'head': (14 - 3 * S(t), 0, 0)})
    rots.update(tail(t, 6, 0.0, nod=-4 * S(t, 1.5) - 4))
    return rots, {'hips': (0, 0, 0.03 * H * S(t, -1.2))}
def glide(t):
    rots = wings_flap(t, 4, 15)
    rots.update({k: v for k, v in fly(t)[0].items() if k.startswith('leg')})
    rots.update({'spine1': (-5, 0, 0), 'spine2': (-3, 0, 0), 'neck1': (-5, 0, 0), 'head': (10, 0, 0)})
    rots.update(tail(t, 5, 0.0, nod=-6))
    return rots, {'hips': (0, 0, 0.01 * H * S(t))}
def eat(t):    # 0-0.3 lower head, 0.3-0.8 chomp x3, 0.8-1 raise
    down = min(1, t / 0.3) if t < 0.8 else max(0, 1 - (t - 0.8) / 0.2)
    chomp = max(0, S(3 * (t - 0.3) / 0.5)) if 0.3 <= t <= 0.8 else 0
    rots = {'neck1': (32 * CH * down, 0, 0), 'neck2': (28 * CH * down, 0, 0), 'head': (18 * CH * down - 8 * chomp, 0, 0), 'jaw': (28 * chomp, 0, 0),
            'chest': (8 * down, 0, 0), 'legFL_upper': (-10 * down, 0, 0), 'legFR_upper': (-10 * down, 0, 0)}
    rots.update(tail(t, 8)); rots.update(wings_folded())
    return rots, {'hips': (0, 0, -0.01 * H * down)}
def roar(t):   # rear up, head back, mouth wide, wings spread, then settle
    k = min(1, t / 0.25) if t < 0.7 else max(0, 1 - (t - 0.7) / 0.3)
    shake = 2 * S(8 * t) * k
    rots = {'chest': (-12 * k, 0, 0), 'spine2': (-6 * k, 0, 0), 'neck1': (-30 * CH * k, 0, shake), 'neck2': (-20 * CH * k, 0, 0), 'head': (-15 * CH * k, 0, shake),
            'jaw': (38 * k, 0, 0), 'legFL_upper': (-25 * k, 0, 0), 'legFR_upper': (-25 * k, 0, 0), 'legFL_lower': (30 * k, 0, 0), 'legFR_lower': (30 * k, 0, 0)}
    if has_wings:
        rots.update({'wingL_1': (30 * k - 35 * (1 - k), 0, -25 * (1 - k)), 'wingR_1': (30 * k - 35 * (1 - k), 0, 25 * (1 - k)),
                     'wingL_2': (10 * k, 0, 45 * (1 - k)), 'wingR_2': (10 * k, 0, -45 * (1 - k)), 'wingL_3': (0, 0, 30 * (1 - k)), 'wingR_3': (0, 0, -30 * (1 - k))})
    rots.update(tail(t, 12, 0, nod=-10 * k))
    return rots, {'hips': (0, 0, 0.05 * H * k)}
def tailwhip(t):   # wind up left, whip right, recover
    w = -40 * min(1, t / 0.3) if t < 0.3 else (-40 + 110 * (t - 0.3) / 0.25 if t < 0.55 else 70 * max(0, 1 - (t - 0.55) / 0.45))
    rots = {f'tail{i+1}': (0, 0, w * (0.4 + 0.2 * i)) for i in range(5)}
    rots.update({'hips': (0, 0, -w * 0.25), 'spine1': (0, 0, -w * 0.15), 'chest': (0, 0, w * 0.1), 'neck1': (0, 0, w * 0.2), 'head': (0, 0, w * 0.15)})
    rots.update(wings_folded())
    return rots, {'hips': (0, 0, 0)}
def hit(t):
    k = math.exp(-5 * t) * S(3 * t)
    rots = {'chest': (-15 * k, 0, 8 * k), 'neck1': (20 * k, 0, 0), 'head': (-25 * k, 0, 10 * k), 'jaw': (max(0, 15 * k), 0, 0)}
    rots.update(tail(t, 15)); rots.update(wings_folded())
    return rots, {'hips': (0, -0.03 * H * math.exp(-4 * t), 0.01 * H * k)}

CLIPS = [('idle', 60, True, idle), ('walk', 30, True, walk), ('run', 18, True, run), ('fly', 24, True, fly), ('glide', 60, True, glide),
         ('eat', 54, False, eat), ('roar', 60, False, roar), ('tailwhip', 30, False, tailwhip), ('hit', 24, False, hit)]
for name, frames, loop, fn in CLIPS:
    c = Clip(name, frames, loop); c.bake(fn); c.act.use_fake_user = True
    print('clip', name, frames, 'loop' if loop else 'once')
arm.animation_data.action = bpy.data.actions['idle']
bpy.ops.wm.save_as_mainfile(filepath=out); print('saved', out)
