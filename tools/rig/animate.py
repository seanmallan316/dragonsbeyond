"""animate.py rig.blend out.blend : adds the game's animation clips to a dragon rig as Blender actions.
Clips (30 fps, looping unless noted): idle, walk, run, fly, glide, dive, eat (once), roar (once), tailwhip (once), hit (once), chirp (once),
claw (once), pounce, talon (once).
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
        rots = dict(rots)
        for nm in TAIL_STRAIGHT: rots.setdefault(nm, (0, 0, 0))   # every pose gets the combed-out tail
        for name, (x, y, z) in rots.items():
            if name.endswith('@body'):   # x = turn about the body's left-right axis: + swings the foot back toward the tail
                bn = name[:-5]
                if bn in pb:
                    inv = pb[bn].bone.matrix_local.to_3x3().inverted()
                    pb[bn].rotation_euler = mathutils.Quaternion(inv @ mathutils.Vector((1, 0, 0)), r(x)).to_euler('XYZ')
                continue
            if name not in pb: continue
            if name.startswith('tail'):   # tails: x = lift (about the body's left-right axis), z = swing to the side (about the vertical), in the body's frame whatever way the modelled tail curls;
                # and first the curl the model was sculpted with is mostly combed out, so the tail trails behind and waves along its whole length instead of holding a hook at the tip
                inv = pb[name].bone.matrix_local.to_3x3().inverted()
                q = mathutils.Quaternion(inv @ mathutils.Vector((0, 0, 1)), r(z)) @ mathutils.Quaternion(inv @ mathutils.Vector((1, 0, 0)), -r(x)) @ TAIL_STRAIGHT.get(name, mathutils.Quaternion())
                pb[name].rotation_euler = q.to_euler('XYZ'); continue
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

ELEV = float(arm.get('wing_elev', 42.0))   # how far above level the wings sit in the model's own pose
TRACED = 'wing_elev' in arm                # wings pivot at the real shoulder blade
FWD = max(0.0, min(50.0, float(arm.get('wing_sweep', 0.0)) - 18.0))   # in flight the wings swing forward out of their swept-back pose, to reach out sideways
import os
def _wing_ratio():   # how long the wing is next to how high its shoulder stands off the ground
    B = arm.data.bones
    if 'wingL_1' not in B or 'wingL_3' not in B: return 1.3
    ln = sum(B[f'wingL_{i}'].length for i in (1, 2, 3)); zmin = min(min(b.head_local.z, b.tail_local.z) for b in B)
    return ln / max(0.05, B['wingL_1'].head_local.z - zmin)
WING_RATIO = _wing_ratio()
TAIL_STRAIGHT = {}
def _tail_straight():
    K = [float(v) for v in os.environ.get('DB_TAILK', '0.85 0.75 0.8 0.88 0.92').split()]
    prev = None
    for i in range(5):
        nm = f'tail{i+1}'
        if nm not in pb: return
        b = pb[nm].bone; d = (b.tail_local - b.head_local).normalized()
        if prev is None:   # the root joint: only its sideways lean is taken out (a tail that leaves the body going up or down keeps doing so)
            want = mathutils.Vector((0.0, 1.0 if d.y >= 0 else -1.0, -0.10)).normalized()   # the root joint: the tail leaves the body pointing straight back, drooping only a touch, so it never drags through the ground
        else: want = prev
        inv = b.matrix_local.to_3x3().inverted(); q = (inv @ d).rotation_difference(inv @ want)
        TAIL_STRAIGHT[nm] = mathutils.Quaternion().slerp(q, K[i]); prev = d
_tail_straight()
def tail(t, amp=10, ph=0.0, nod=0.0):
    # a whip-like wave: small at the root, growing all the way to the tip, travelling down the tail, with a slower second swell under it so it never repeats stiffly; the tip also lifts and dips
    return {f'tail{i+1}': (nod + 0.3 * amp * (0.3 + 0.3 * i) * S(t * 2, ph + i * 1.1), 0, amp * (0.5 + 0.34 * i) * S(t, ph - i * 0.8) + 0.3 * amp * (0.4 + 0.3 * i) * S(t * 2, ph * 1.7 - i * 1.2 + 0.6)) for i in range(5)}
def wings_folded():
    if TRACED:   # tucked: the wing swings a little back, then folds DOWN flat against the dragon's side, its tip toward the belly
        k = max(0.0, WING_RATIO - 1.3)   # long wings are swung further back first (and their outer joints curl in more), so the tip never hangs below the feet
        S0 = float(arm.get('wing_sweep', 40.0))   # (swung back past sideways-on, 'down' would turn into 'up': the swing back stops short of that, and long wings curl their outer joints in under the belly instead)
        D, SW, F2, F3 = [float(v) for v in os.environ.get('DB_FOLD', '78 40 60 45').split()]
        SW = SW - S0; F2 = 20.0 + F2 * k; F3 = 15.0 + F3 * k   # the wing is brought to a fixed lean back (wherever the model had it), then folded down
        d = -(ELEV + D)
        return {'wingL_1': (d, 0, SW), 'wingR_1': (d, 0, -SW), 'wingL_2': (-F2, 0, 0), 'wingR_2': (-F2, 0, 0), 'wingL_3': (-F3, 0, 0), 'wingR_3': (-F3, 0, 0)}
    return {'wingL_1': (-35, 0, -25), 'wingR_1': (-35, 0, 25), 'wingL_2': (0, 0, 45), 'wingR_2': (0, 0, -45), 'wingL_3': (0, 0, 30), 'wingR_3': (0, 0, -30)} if has_wings else {}
def wings_flap(t, amp=45, base=10):
    # the shoulder does the work (a deep, slightly quick downstroke), the outer wing lags behind and bends further
    s1 = S(t); down = s1 - 0.25 * (1 - s1 * s1)   # a little sharper at the bottom of the stroke
    a = base + amp * down * (1.0 if down > 0 else 1.45)   # the wing is swept back, so the downstroke needs more turn to bring the tips under the belly
    b = 0.12 * amp * S(t, -0.6); c = 0.1 * amp * S(t, -1.0)   # outer wing follows the shoulder, only a slight lag
    if TRACED: b = 0.26 * amp * S(t, -0.9); c = 0.3 * amp * S(t, -1.5)   # a wave rolls out along the wing: wrist, then fingers, then tip (like a whip, not a plank)
    sweep = 0.18 * amp * S(t, -0.5)   # the wing also sweeps a little forward on the downstroke
    # on the way up the wing half-folds at the wrist and elbow (less air to push), then opens wide for the downstroke
    f = max(0.0, C(t, -0.2)) * 0.42 * amp; b -= 0.15 * f
    if TRACED:
        a = base + amp * down * (1.0 if down > 0 else 0.8); f = max(0.0, C(t, -0.2)) * 0.22 * amp
        return {'wingL_1': (a, 0, -FWD - sweep), 'wingR_1': (a, 0, FWD + sweep), 'wingL_2': (b, 0, f - 0.2 * FWD), 'wingR_2': (b, 0, -f + 0.2 * FWD), 'wingL_3': (c, 0, 0.7 * f - 0.1 * FWD), 'wingR_3': (c, 0, -0.7 * f + 0.1 * FWD)}   # (left wing: - = forward)
    return {'wingL_1': (a, 0, -sweep), 'wingR_1': (a, 0, sweep), 'wingL_2': (b, 0, f), 'wingR_2': (b, 0, -f), 'wingL_3': (c, 0, 0.7 * f), 'wingR_3': (c, 0, -0.7 * f)} if has_wings else {}
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
    rots.update({f'tail{i+1}': (1.5 * S(t, i * 0.7), 0, 13 * S(0.5 * t, i * 0.75) + 3 * S(t, i * 0.9)) for i in range(5)}); rots.update(wings_folded())   # a lazy, travelling sway: never still
    if has_wings:
        rots['wingL_1'] = (-35 + 2 * S(t), 0, -25); rots['wingR_1'] = (-35 + 2 * S(t), 0, 25)
    if CH < 1:   # chibi idle: a curious head tilt and a little weight shift
        rots['head'] = (2 * S(t, 2.0), 12 * S(0.5 * t, 0.7), 6 * S(0.5 * t, 1.0)); rots['hips'] = (0, 0, 3 * S(0.5 * t))
    return rots, {'hips': (0, 0, 0.004 * H * S(t))}
def walk(t):
    rots = legs_walk(t, swing=44, lift=40)
    bob = 0.01 * H * S(2 * t, 0.3) + (0.03 * H * max(0.0, S(2 * t, 0.3)) if CH < 1 else 0)   # chibi: a springy hop in every step
    rots.update({'hips': (0, 3 * S(t, 0.4), 4 * S(t)), 'spine1': (0, 0, 4 * S(t, 0.2)), 'spine2': (0, 0, 3 * S(t, 0.5)), 'chest': (2 * S(2 * t), -2 * S(t, 0.4), -4 * S(t, 0.7)),
                 'neck1': (-3 * S(2 * t), 0, 3 * S(t, 1.2)), 'neck2': (0, 0, -2 * S(t, 1.4)), 'head': (3 * S(2 * t, 1.0), 0, -2 * S(t, 1.6))})   # hips and shoulders roll against each other, the head steadies itself
    rots.update(tail(t, 20, 0.9)); rots.update(wings_folded())
    return rots, {'hips': (0, 0, bob)}
def run(t):
    rots = legs_walk(t, swing=62, lift=55)
    rots.update({'spine1': (6 * S(2 * t), 0, 0), 'spine2': (6 * S(2 * t, 0.5), 0, 0), 'chest': (-5 * S(2 * t), 0, 0),
                 'neck1': (-8 - 4 * S(2 * t), 0, 0), 'neck2': (-4, 0, 0), 'head': (12 + 3 * S(2 * t, 1.0), 0, 0)})
    rots.update({'hips': (0, 2 * S(t, 0.4), 3 * S(t))})
    rots.update(tail(t, 22, 0.9, nod=-5)); rots.update(wings_folded())
    return rots, {'hips': (0, 0, (0.055 if CH < 1 else 0.025) * H * max(0, S(2 * t, 0.3)))}
def fly(t):
    # bird-like stroke: the wing rests ~42 deg up, so -42 = level; +-88 around level brings the tips nearly together
    # over the back on the upstroke and under the belly on the downstroke
    rots = wings_flap(t, 52, 6 - ELEV) if TRACED else wings_flap(t, 88, -42)
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
    # soaring: wings spread flat with the tips a little up (a shallow V), only small trimming movements
    rots = wings_flap(t, 3, 10 - ELEV) if TRACED else wings_flap(t, 3, -35)
    if has_wings: rots.update({'wingL_3': (3 + 2 * S(t, 1.0), 0, 0), 'wingR_3': (3 + 2 * S(t, 1.0), 0, 0)})
    rots.update({k: v for k, v in fly(t)[0].items() if k.startswith('leg')})
    rots.update({'spine1': (-5, 0, 0), 'spine2': (-3, 0, 0), 'neck1': (-5, 0, 0), 'head': (10, 0, 0)})
    rots.update(tail(t, 5, 0.0, nod=-6))
    return rots, {'hips': (0, 0, 0.01 * H * S(t))}
def dive(t):
    # diving: wings half-folded and swept back along the body, neck stretched forward, tail straight behind
    rots = {}
    if has_wings:
        q = 2 * S(t * 2)   # the wing tips shiver in the rushing air
        rots.update({'wingL_1': (-28 + q, 0, -88), 'wingR_1': (-28 + q, 0, 88), 'wingL_2': (-4, 0, 95), 'wingR_2': (-4, 0, -95), 'wingL_3': (q, 0, 50), 'wingR_3': (q, 0, -50)})
        if TRACED: rots.update({'wingL_1': (-12 + q, 0, 22), 'wingR_1': (-12 + q, 0, -22), 'wingL_2': (-6, 0, 30), 'wingR_2': (-6, 0, -30), 'wingL_3': (q, 0, 26), 'wingR_3': (q, 0, -26)})   # tucked back tight along the body
    rots.update({k: v for k, v in fly(t)[0].items() if k.startswith('leg')})
    rots.update({'spine1': (-4, 0, 0), 'spine2': (-2, 0, 0), 'neck1': (-12, 0, 0), 'neck2': (-8, 0, 0), 'head': (16, 0, 0)})
    rots.update(tail(t, 3, 0.0, nod=-3))
    return rots, {}
def swim(t):
    # swimming: wings flat against the sides, legs trailing and paddling gently in turn, the neck reaching forward, and a slow strong wave
    # running down the back into the tail, which does most of the work (the game adds a gentle whole-body sway on top)
    rots = dict(wings_folded())
    for tag, ph in (('FL', 0.0), ('FR', 3.14)): rots[f'leg{tag}_upper@body'] = (58 + 12 * S(t, ph), 0, 0); rots[f'leg{tag}_lower@body'] = (18 + 8 * S(t, ph - 0.8), 0, 0); rots[f'leg{tag}_foot@body'] = (26 + 10 * S(t, ph - 1.4), 0, 0)
    for tag, ph in (('BL', 3.14), ('BR', 0.0)): rots[f'leg{tag}_upper@body'] = (70 + 10 * S(t, ph), 0, 0); rots[f'leg{tag}_lower@body'] = (32 + 8 * S(t, ph - 0.8), 0, 0); rots[f'leg{tag}_foot@body'] = (26 + 10 * S(t, ph - 1.4), 0, 0)
    w = lambda ph: S(t, ph)
    rots.update({'hips': (0, 0, 5 * w(-1.2)), 'spine1': (-3, 0, 5 * w(-0.6)), 'spine2': (-2, 0, -4 * w(0.0)), 'chest': (0, 0, -5 * w(0.5)), 'neck1': (-9, 0, -4 * w(1.0)), 'neck2': (-7, 0, 3 * w(1.5)), 'head': (13, 0, 5 * w(1.9))})
    rots.update(tail(t, 15, -1.9, nod=-2))
    return rots, {}
def eat(t):    # 0-0.3 lower head, 0.3-0.8 chomp x3, 0.8-1 raise
    down = min(1, t / 0.3) if t < 0.8 else max(0, 1 - (t - 0.8) / 0.2)
    chomp = max(0, S(3 * (t - 0.3) / 0.5)) if 0.3 <= t <= 0.8 else 0
    rots = {'neck1': (32 * CH * down, 0, 0), 'neck2': (28 * CH * down, 0, 0), 'head': (18 * CH * down - 8 * chomp, 0, 0), 'jaw': (28 * chomp, 0, 0),
            'chest': (8 * down, 0, 0), 'legFL_upper': (-10 * down, 0, 0), 'legFR_upper': (-10 * down, 0, 0)}
    rots.update(tail(t, 8)); rots.update(wings_folded())
    return rots, {'hips': (0, 0, -0.01 * H * down)}
# lip-sync: how loud each voice is every 40 ms (measured from content/sfx/sfx_roar.wav and sfx_chirp.wav); the jaw opens as wide as the voice is loud
ROAR_ENV = [0.01, 0.04, 0.11, 0.13, 0.53, 0.67, 0.71, 0.84, 0.73, 0.86, 0.61, 0.93, 0.71, 1.00, 0.87, 0.88, 0.64, 0.77, 0.70, 0.68, 0.43, 0.53, 0.28, 0.21, 0.09, 0.03, 0.0]
CHIRP_ENV = [0.12, 0.37, 0.83, 1.00, 0.89, 0.71, 0.42, 0.32, 0.24, 0.16, 0.11, 0.08, 0.0]
def voice(envl, sec, lead=0.04):   # the voice's loudness at sec seconds (the jaw leads the sound by a hair, as real mouths do)
    i = (sec + lead) / 0.04
    if i <= 0 or i >= len(envl) - 1: return 0.0
    k = int(i); f = i - k; return envl[k] * (1 - f) + envl[k + 1] * f
def roar(t):   # rear up, head back, mouth wide, wings spread, then settle
    # timed to the roar sound: the mouth is open by the time the sound swells (~0.25 s) and shuts as it dies (~1.4 s)
    k = min(1, t / 0.12) if t < 0.6 else max(0, 1 - (t - 0.6) / 0.2)
    shake = 2 * S(8 * t) * k
    rots = {'chest': (-12 * k, 0, 0), 'spine2': (-6 * k, 0, 0), 'neck1': (-30 * CH * k, 0, shake), 'neck2': (-20 * CH * k, 0, 0), 'head': (-15 * CH * k, 0, shake),
            'jaw': (6 * k + 34 * voice(ROAR_ENV, 2.0 * t), 0, 0), 'legFL_upper': (-25 * k, 0, 0), 'legFR_upper': (-25 * k, 0, 0), 'legFL_lower': (30 * k, 0, 0), 'legFR_lower': (30 * k, 0, 0)}
    if has_wings:
        rots.update({'wingL_1': (30 * k - 35 * (1 - k), 0, -25 * (1 - k)), 'wingR_1': (30 * k - 35 * (1 - k), 0, 25 * (1 - k)),
                     'wingL_2': (10 * k, 0, 45 * (1 - k)), 'wingR_2': (10 * k, 0, -45 * (1 - k)), 'wingL_3': (0, 0, 30 * (1 - k)), 'wingR_3': (0, 0, -30 * (1 - k))})
    rots.update(tail(t, 12, 0, nod=-10 * k))
    return rots, {'hips': (0, 0, 0.05 * H * k)}
def chirp(t):   # a dragonet's chirp, 0.8 s: head up and tilted, two quick snaps of the jaw, one per note of the chirp sound (0-0.12 s, 0.13-0.23 s)
    up = min(1, t / 0.1) if t < 0.6 else max(0, 1 - (t - 0.6) / 0.4)
    jaw = 26 * voice(CHIRP_ENV, 0.8 * t)
    rots = {'neck1': (-14 * CH * up, 0, 0), 'neck2': (-10 * CH * up, 0, 0), 'head': (-10 * CH * up, 8 * up, 0), 'jaw': (jaw, 0, 0), 'chest': (-4 * up, 0, 0)}
    rots.update(tail(t, 14, 0, nod=-4 * up)); rots.update(wings_folded())
    if has_wings: rots.update({'wingL_1': (-35 + 12 * up, 0, -25), 'wingR_1': (-35 + 12 * up, 0, 25)})   # a little flutter of happiness
    return rots, {'hips': (0, 0, 0.02 * H * up)}
def tailwhip(t):   # the WHOLE tail swings from its root: cocked to one side, then a full sweep across behind the body, the tip lagging
    def W(u):          # how far round the tail is (degrees at the root): wind up, fast sweep, overshoot, settle
        if u < 0.26: return -75 * math.sin(0.5 * math.pi * u / 0.26)
        if u < 0.46: return -75 + 185 * (0.5 - 0.5 * math.cos(math.pi * (u - 0.26) / 0.2))
        return 110 * max(0.0, 1 - (u - 0.46) / 0.54) ** 1.5 * math.cos(3.0 * (u - 0.46))
    share = (0.62, 0.2, 0.2, 0.22, 0.26)   # a big turn at the root, and each joint after it cracks a little later and a little further: a whip
    rots = {f'tail{i+1}': (-5 + 6 * math.sin(math.pi * min(1, t / 0.5)), 0, W(max(0.0, t - 0.035 * i)) * share[i]) for i in range(5)}
    w = W(t)
    rots.update({'hips': (0, 0, w * 0.14), 'spine1': (0, 0, -w * 0.12), 'spine2': (0, 0, -w * 0.08), 'chest': (0, 0, -w * 0.05), 'neck1': (0, 0, w * 0.12), 'head': (0, 0, w * 0.12)})   # the body leans into the swing a little; the tail does the work
    rots.update(wings_folded())
    return rots, {'hips': (0, 0, 0)}
def claw(t):   # 0.6 s: rear up a little, the right front paw lifts high and slashes down and across, head lunges with the jaw open
    up = math.sin(math.pi * min(1, t / 0.45)) if t < 0.45 else 0.0
    lift = 1 - abs(t - 0.32) / 0.32 if t < 0.64 else 0.0; lift = max(0.0, lift)
    slash = 0.0 if t < 0.32 else min(1.0, (t - 0.32) / 0.12)   # fast down-stroke
    a = 115 * lift - 30 * slash * (1 - t)   # the paw goes up/forward, then strikes down past the start
    rots = {'legFR_upper@body': (-a, 0, 0), 'legFR_lower@body': (-55 * lift, 0, 0), 'legFR_foot@body': (-40 * lift + 30 * slash * (1 - t), 0, 0),
            'legFL_upper@body': (-12 * up, 0, 0),
            'chest': (-18 * up, 0, -8 * lift), 'spine2': (-8 * up, 0, 0), 'neck1': (8 * up, 0, -6 * slash), 'neck2': (6 * up, 0, 0),
            'head': (-10 * up + 14 * slash * (1 - t), 0, -8 * slash), 'jaw': (22 * max(0.0, slash * (1 - t) * 1.4), 0, 0)}
    rots.update(tail(t, 12)); rots.update(wings_folded())
    if has_wings: rots['wingL_1'] = (-25 + 10 * up, 0, -25); rots['wingR_1'] = (-25 + 10 * up, 0, 25)
    return rots, {'hips': (0, -0.06 * H * slash, 0.06 * H * up)}
def pounce(t):   # dropping onto prey from a jump: front legs reaching down and forward, claws out, wings up, tail high
    rots = {}
    for tag in ('FL', 'FR'): rots[f'leg{tag}_upper@body'] = (-45, 0, 0); rots[f'leg{tag}_lower@body'] = (-20, 0, 0); rots[f'leg{tag}_foot@body'] = (-25, 0, 0)
    for tag in ('BL', 'BR'): rots[f'leg{tag}_upper@body'] = (30, 0, 0); rots[f'leg{tag}_lower@body'] = (10, 0, 0)
    rots.update({'chest': (12, 0, 0), 'neck1': (14, 0, 0), 'head': (6, 0, 0), 'jaw': (16 + 4 * S(2 * t), 0, 0)})
    rots.update({f'tail{i+1}': (-10, 0, 6 * S(t, i * 0.9)) for i in range(5)})
    if has_wings: rots.update({'wingL_1': (20 + 6 * S(2 * t), 0, -10), 'wingR_1': (20 + 6 * S(2 * t), 0, 10)})
    return rots, {}
def talon(t):   # in flight, 0.6 s: wings swept back, back legs swing forward with the talons open, then rake back
    k = math.sin(math.pi * min(1.0, t / 0.6))
    rots = {k2: v for k2, v in glide(t)[0].items() if k2.startswith('wing')}
    for tag in ('BL', 'BR'): rots[f'leg{tag}_upper@body'] = (72 - 120 * k, 0, 0); rots[f'leg{tag}_lower@body'] = (38 - 50 * k, 0, 0); rots[f'leg{tag}_foot@body'] = (25 - 45 * k, 0, 0)
    for tag in ('FL', 'FR'): rots[f'leg{tag}_upper@body'] = (88 - 70 * k, 0, 0); rots[f'leg{tag}_lower@body'] = (8 - 30 * k, 0, 0); rots[f'leg{tag}_foot@body'] = (15 - 30 * k, 0, 0)
    rots.update({'chest': (8 * k, 0, 0), 'neck1': (10 * k, 0, 0), 'head': (8 + 6 * k, 0, 0), 'jaw': (18 * k, 0, 0)})
    rots.update(tail(t, 4, 0.0, nod=-4))
    return rots, {}
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

CLIPS = [('idle', 120, True, idle), ('walk', 30, True, walk), ('run', 18, True, run), ('fly', 24, True, fly), ('glide', 60, True, glide), ('dive', 30, True, dive), ('swim', 30, True, swim),
         ('eat', 54, False, eat), ('roar', 60, False, roar), ('tailwhip', 30, False, tailwhip), ('hit', 24, False, hit), ('chirp', 24, False, chirp),
         ('claw', 18, False, claw), ('pounce', 30, True, pounce), ('talon', 18, False, talon)]
for name, frames, loop, fn in CLIPS:
    c = Clip(name, frames, loop); c.bake(with_life(name, fn)); c.act.use_fake_user = True
    print('clip', name, frames, 'loop' if loop else 'once')
arm.animation_data.action = bpy.data.actions['idle']
bpy.ops.wm.save_as_mainfile(filepath=out); print('saved', out)
