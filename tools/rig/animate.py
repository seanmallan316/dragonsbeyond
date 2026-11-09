"""animate.py rig.blend out.blend : adds the game's animation clips to a dragon rig as Blender actions.
Clips (30 fps, looping unless noted): idle, walk, run, fly, glide, eat (once), roar (once), tailwhip (once), hit (once).
Bone axes (roll 0): spine/neck/tail X = nod, Z = turn; legs X = swing forward/back; wings X = raise tip (+ up)."""
import bpy, sys, math, mathutils
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
            if name.endswith('@body'):   # x = turn about the body's left-right axis: + swings the foot back toward the tail
                bn = name[:-5]
                if bn in pb:
                    inv = pb[bn].bone.matrix_local.to_3x3().inverted()
                    pb[bn].rotation_euler = mathutils.Quaternion(inv @ mathutils.Vector((1, 0, 0)), r(x)).to_euler('XYZ')
                continue
            if name not in pb: continue
            if name.startswith('wing'):   # wings: x = flap about the body's axis (+ = tip up), z = sweep about the vertical, whatever the bone's own axes are
                inv = pb[name].bone.matrix_local.to_3x3().inverted()
                sgn = 1 if name.startswith('wingL') else -1
                q = mathutils.Quaternion(inv @ mathutils.Vector((0, 1, 0)), r(x) * -sgn) @ mathutils.Quaternion(inv @ mathutils.Vector((0, 0, 1)), r(z))
                pb[name].rotation_euler = q.to_euler('XYZ')
            else: pb[name].rotation_euler = (r(x), r(y), r(z))
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
    # the shoulder does the work (a deep, slightly quick downstroke), the outer wing lags behind and bends further
    s1 = S(t); down = s1 - 0.25 * (1 - s1 * s1)   # a little sharper at the bottom of the stroke
    a = base + amp * down * (1.0 if down > 0 else 1.45)   # the wing is swept back, so the downstroke needs more turn to bring the tips under the belly
    b = 0.12 * amp * S(t, -0.6); c = 0.1 * amp * S(t, -1.0)   # outer wing follows the shoulder, only a slight lag
    sweep = 0.18 * amp * S(t, -0.5)   # the wing also sweeps a little forward on the downstroke
    return {'wingL_1': (a, 0, -sweep), 'wingR_1': (a, 0, sweep), 'wingL_2': (b, 0, 0), 'wingR_2': (b, 0, 0), 'wingL_3': (c, 0, 0), 'wingR_3': (c, 0, 0)} if has_wings else {}
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
    if CH < 1:   # chibi idle: a curious head tilt and a little weight shift
        rots['head'] = (2 * S(t, 2.0), 12 * S(0.5 * t, 0.7), 6 * S(0.5 * t, 1.0)); rots['hips'] = (0, 0, 3 * S(0.5 * t))
    return rots, {'hips': (0, 0, 0.004 * H * S(t))}
def walk(t):
    rots = legs_walk(t)
    bob = 0.01 * H * S(2 * t, 0.3) + (0.03 * H * max(0.0, S(2 * t, 0.3)) if CH < 1 else 0)   # chibi: a springy hop in every step
    rots.update({'spine1': (0, 0, 4 * S(t)), 'spine2': (0, 0, 3 * S(t)), 'chest': (2 * S(2 * t), 0, -3 * S(t)),
                 'neck1': (-3 * S(2 * t), 0, 0), 'head': (3 * S(2 * t, 1.0), 0, 0)})
    rots.update(tail(t, 14, 0.5)); rots.update(wings_folded())
    return rots, {'hips': (0, 0, bob)}
def run(t):
    rots = legs_walk(t, swing=42, lift=35)
    rots.update({'spine1': (6 * S(2 * t), 0, 0), 'spine2': (6 * S(2 * t, 0.5), 0, 0), 'chest': (-5 * S(2 * t), 0, 0),
                 'neck1': (-8 - 4 * S(2 * t), 0, 0), 'neck2': (-4, 0, 0), 'head': (12 + 3 * S(2 * t, 1.0), 0, 0)})
    rots.update(tail(t, 18, 0.5, nod=-5)); rots.update(wings_folded())
    return rots, {'hips': (0, 0, (0.055 if CH < 1 else 0.025) * H * max(0, S(2 * t, 0.3)))}
def fly(t):
    # bird-like stroke: the wing rests ~42 deg up, so -42 = level; +-88 around level brings the tips nearly together
    # over the back on the upstroke and under the belly on the downstroke
    rots = wings_flap(t, 88, -42)
    # flying pose: front legs folded up under the chest, back legs swept back along the tail
    tuck = {}
    for tag in ('FL', 'FR'): tuck[f'leg{tag}_upper@body'] = (88, 0, 0); tuck[f'leg{tag}_lower@body'] = (8, 0, 0); tuck[f'leg{tag}_foot@body'] = (15, 0, 0)
    for tag in ('BL', 'BR'): tuck[f'leg{tag}_upper@body'] = (72, 0, 0); tuck[f'leg{tag}_lower@body'] = (38, 0, 0); tuck[f'leg{tag}_foot@body'] = (25, 0, 0)
    rots.update(tuck)
    # the body answers every beat: chest rises on the downstroke, a wave runs from the neck back through the tail
    rots.update({'spine1': (-6 + 5 * S(t, 0.6), 0, 0), 'spine2': (-4 + 5 * S(t, 1.2), 0, 0), 'chest': (-4 + 4 * S(t), 0, 0),
                 'neck1': (-6 - 5 * S(t, -0.4), 0, 0), 'neck2': (-6 - 4 * S(t, -0.8), 0, 0), 'head': (14 + 6 * S(t, -1.2), 0, 0)})
    rots.update({f'tail{i+1}': (-4 + 9 * S(t, 1.6 + i * 0.8), 0, 6 * S(t * 0.5, i * 0.9)) for i in range(5)})
    return rots, {'hips': (0, 0, -0.07 * H * S(t))}   # the body sinks while the wings are up, lifts on the downstroke
def glide(t):
    rots = wings_flap(t, 4, -40)
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


# ---- life: small motions layered on every clip so the dragon never looks frozen.
# Integer harmonics of t keep every loop seamless. Values are added to whatever the clip already does.
def add(rots, name, x=0, y=0, z=0):
    if name not in pb: return
    ox, oy, oz = rots.get(name, (0, 0, 0)); rots[name] = (ox + x, oy + y, oz + z)
def life(t, kind, rots):
    if kind == 'idle':        # curious: looks around, sniffs, shifts a front foot, breathes deep
        add(rots, 'head', 6 * S(t, 0.3) + 3 * S(3 * t, 1.1), 10 * S(t, 2.0), 22 * S(t, 0.2) + 6 * S(2 * t, 1.7))
        add(rots, 'neck2', 0, 0, 10 * S(t, 0.0)); add(rots, 'neck1', 2 * S(2 * t), 0, 5 * S(t, -0.3))
        sniff = max(0, S(4 * t, 0.4)) ** 3; add(rots, 'jaw', 7 * sniff + 3 * max(0, S(2 * t, 2.2)))
        tap = max(0, S(t, 1.0)) ** 6   # one front foot lifts and taps once per loop
        add(rots, 'legFL_upper', -14 * tap); add(rots, 'legFL_lower', 26 * tap); add(rots, 'legFL_foot', -20 * tap)
        for leg in ('FR', 'BL', 'BR'): add(rots, f'leg{leg}_foot', 6 * S(2 * t, ord(leg[0]) * 0.7))   # toes flex
        add(rots, 'chest', 2.5 * S(2 * t)); add(rots, 'spine2', 1.5 * S(2 * t, 0.4))   # slower, deeper breaths
    elif kind == 'walk':
        add(rots, 'head', 0, 0, 8 * S(t, 0.5)); add(rots, 'jaw', 3 + 2 * S(2 * t))
    elif kind == 'run':       # panting, head pushed forward, toes splay
        add(rots, 'jaw', 10 + 6 * max(0, S(2 * t, 0.8))); add(rots, 'head', 0, 0, 5 * S(t, 0.2))
        for leg in ('FL', 'FR', 'BL', 'BR'): add(rots, f'leg{leg}_foot', 8 * S(2 * t, 1.3))
    elif kind in ('fly', 'glide'):   # scanning the ground, mouth a little open in the wind, claws flexing
        add(rots, 'head', 3 * S(t, 0.9), 6 * S(t, 2.4), 12 * S(t, 0.1)); add(rots, 'jaw', 5 + 3 * S(2 * t, 0.6))
        add(rots, 'neck2', 0, 0, 6 * S(t, -0.4))
    elif kind == 'eat':       # chews a few more times with the head tilting
        add(rots, 'head', 0, 6 * S(2 * t), 0); add(rots, 'jaw', 6 * max(0, S(5 * t)))
    elif kind == 'roar':
        add(rots, 'jaw', 6 * max(0, S(6 * t)))   # the jaw shudders with the roar
    return rots
def with_life(kind, fn):
    def g(t):
        rots, locs = fn(t); return life(t, kind, dict(rots)), locs
    return g

CLIPS = [('idle', 120, True, idle), ('walk', 30, True, walk), ('run', 18, True, run), ('fly', 24, True, fly), ('glide', 60, True, glide),
         ('eat', 54, False, eat), ('roar', 60, False, roar), ('tailwhip', 30, False, tailwhip), ('hit', 24, False, hit)]
for name, frames, loop, fn in CLIPS:
    c = Clip(name, frames, loop); c.bake(with_life(name, fn)); c.act.use_fake_user = True
    print('clip', name, frames, 'loop' if loop else 'once')
arm.animation_data.action = bpy.data.actions['idle']
bpy.ops.wm.save_as_mainfile(filepath=out); print('saved', out)
