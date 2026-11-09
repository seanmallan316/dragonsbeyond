"""rig_build.py model.glb out.blend [out_preview.png]
Fits a dragon skeleton to a Meshy dragon model and binds it with automatic weights.
Blender axes: z up, front = -y (glTF +z). Bones: root, hips, spine1, spine2, chest, neck1, neck2, head, jaw,
tail1..tail5, legFL/FR/BL/BR (upper, lower, foot), wingL/R (1,2,3). Prints the landmarks it found."""
import bpy, sys, numpy as np
from mathutils import Vector

src, out = sys.argv[1], sys.argv[2]
prev = sys.argv[3] if len(sys.argv) > 3 else None
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
obs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
# join into one mesh object
for o in bpy.context.scene.objects: o.select_set(o in obs)
bpy.context.view_layer.objects.active = obs[0]
if len(obs) > 1: bpy.ops.object.join()
mesh = bpy.context.view_layer.objects.active; mesh.name = 'Dragon'
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
P = np.array([tuple(v.co) for v in mesh.data.vertices])
lo, hi = P.min(0), P.max(0); H = hi[2] - lo[2]; L = hi[1] - lo[1]; W = hi[0] - lo[0]
ground = lo[2]

def kmeans(X, k, it=30):
    rng = np.random.default_rng(0); C = X[rng.choice(len(X), k, replace=False)]
    for _ in range(it):
        lab = np.argmin(((X[:, None, :] - C[None]) ** 2).sum(-1), 1)
        C = np.array([X[lab == i].mean(0) if (lab == i).any() else C[i] for i in range(k)])
    return C, lab

# ---- legs: lowest 8% of the height, clustered; feet are the mirrored pairs (chin and tail also touch the ground)
feet_pts = P[P[:, 2] < ground + 0.08 * H]
C, lab = kmeans(feet_pts[:, :2], 7)
C = [c for c in C if abs(c[0]) > 0.04 * W]
pairs = []
for i, a in enumerate(C):
    for b in C[i + 1:]:
        if a[0] * b[0] < 0 and abs(a[1] - b[1]) < 0.08 * L and abs(abs(a[0]) - abs(b[0])) < 0.08 * W:
            pairs.append((0.5 * (a[1] + b[1]), 0.5 * (abs(a[0]) + abs(b[0]))))
pairs.sort()
if len(pairs) < 2: raise SystemExit(f'could not find two leg pairs: {pairs} from {C}')
# the two pairs farthest apart in y
(yf, xf), (yb, xb) = pairs[0], pairs[-1]
front = [(-xf, yf), (xf, yf)]; back = [(-xb, yb), (xb, yb)]
# ---- medial axis along y (body centre line), using the core of each slice (|x| small, excluding wings)
n = 40; ys = np.linspace(lo[1], hi[1], n + 1); axis = []
for i in range(n):
    s = P[(P[:, 1] >= ys[i]) & (P[:, 1] < ys[i + 1])]
    if len(s) < 20: continue
    core = s[np.abs(s[:, 0]) < max(0.08 * W, np.percentile(np.abs(s[:, 0]), 40))]
    if len(core) < 5: core = s
    axis.append((0.5 * (ys[i] + ys[i + 1]), 0.5 * (np.percentile(core[:, 2], 15) + np.percentile(core[:, 2], 85)), len(s), core[:, 2].max(), core[:, 2].min()))
axis = np.array(axis)   # y, zmid, count, zmax, zmin
def zmid_at(y):
    return float(np.interp(y, axis[:, 0], axis[:, 1]))
y_front_leg = np.mean([f[1] for f in front]); y_back_leg = np.mean([f[1] for f in back])
# body: between the leg lines; neck/head ahead of the front legs; tail behind the back legs
y_shoulder = y_front_leg; y_hip = y_back_leg
body_top = float(np.interp(0.5 * (y_shoulder + y_hip), axis[:, 0], axis[:, 3]))
body_bot = float(np.interp(0.5 * (y_shoulder + y_hip), axis[:, 0], axis[:, 4]))
spine_z = 0.5 * (body_top + body_bot) + 0.15 * (body_top - body_bot)
# head: the slice with the largest vertex count ahead of the shoulders is the head's middle
ahead = axis[axis[:, 0] < y_shoulder - 0.05 * L]
head_mid = ahead[np.argmax(ahead[:, 2])] if len(ahead) else axis[0]
y_head_c = head_mid[0]; z_head_c = head_mid[1]
y_snout = lo[1]; z_snout = zmid_at(y_snout + 0.02 * L)
y_neck0 = y_shoulder - 0.02 * L
# tail: from the hips back to the far end
y_tail_end = hi[1]
# ---- wings: points far out in x, above the body middle, between hips and shoulders (plus a bit)
wing_pts = P[(np.abs(P[:, 0]) > 0.5 * max(W / 2, 1e-6)) & (P[:, 2] > body_bot + 0.3 * (body_top - body_bot))]
has_wings = len(wing_pts) > 200
if has_wings:
    wl = wing_pts[wing_pts[:, 0] > 0]
    y_wing = float(np.median(wl[:, 1])); tip = wl[np.argmax(wl[:, 0])]
    root_x = float(np.percentile(np.abs(P[(np.abs(P[:, 1] - y_wing) < 0.05 * L) & (P[:, 2] > spine_z)][:, 0]), 50)) * 0.6
    wing_root = (root_x, y_wing, body_top - 0.1 * (body_top - body_bot))
    wing_tip = (float(tip[0]), float(tip[1]), float(tip[2]))
print('landmarks: ground', round(ground, 3), 'front feet', front, 'back feet', back)
print(' shoulders y', round(y_shoulder, 3), 'hips y', round(y_hip, 3), 'spine z', round(spine_z, 3), 'head', round(y_head_c, 3), round(z_head_c, 3), 'snout', round(y_snout, 3))
if has_wings: print(' wing root', wing_root, 'tip', wing_tip)

# ---- build the armature
arm_data = bpy.data.armatures.new('DragonRig'); arm = bpy.data.objects.new('DragonRig', arm_data)
bpy.context.collection.objects.link(arm); bpy.context.view_layer.objects.active = arm; arm.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
EB = arm_data.edit_bones
def bone(name, head, tail, parent=None, connect=False, roll=0.0):
    b = EB.new(name); b.head = Vector(head); b.tail = Vector(tail); b.roll = roll
    if parent is not None: b.parent = EB[parent]; b.use_connect = connect
    return b
hips = (0, y_hip, spine_z); shoulders = (0, y_shoulder, spine_z)
bone('root', (0, 0, ground), (0, -0.2 * L, ground))
bone('hips', hips, (0, y_hip + 0.08 * L, spine_z), 'root')
# spine from hips forward to the shoulders in 3 bones
sp = np.linspace(y_hip, y_shoulder, 4)
bone('spine1', (0, sp[0], spine_z), (0, sp[1], spine_z), 'hips')
bone('spine2', (0, sp[1], spine_z), (0, sp[2], spine_z), 'spine1', True)
bone('chest', (0, sp[2], spine_z), (0, sp[3], spine_z), 'spine2', True)
# neck: 2 bones from the shoulders up to the head centre
neck0 = (0, y_neck0, spine_z + 0.1 * (body_top - body_bot)); head_c = (0, y_head_c, z_head_c)
mid = tuple(0.5 * (a + b) for a, b in zip(neck0, head_c))
bone('neck1', neck0, mid, 'chest')
bone('neck2', mid, head_c, 'neck1', True)
bone('head', head_c, (0, y_snout + 0.05 * L, z_snout + 0.05 * H), 'neck2', True)
snout_pts = P[P[:, 1] < 0.5 * (y_head_c + y_snout)]
jz_lo, jz_hi = float(snout_pts[:, 2].min()), float(np.percentile(snout_pts[:, 2], 90))
jaw_line = jz_lo + 0.35 * (jz_hi - jz_lo)
bone('jaw', (0, y_head_c, jaw_line), (0, y_snout + 0.02 * L, jz_lo + 0.12 * (jz_hi - jz_lo)), 'head')
# tail: 5 bones from the hips to the far end along the medial axis
ty = np.linspace(y_hip + 0.08 * L, y_tail_end, 6)
prevb = 'hips'
for i in range(5):
    bone(f'tail{i+1}', (0, ty[i], zmid_at(ty[i])), (0, ty[i + 1], zmid_at(ty[i + 1])), prevb, i > 0); prevb = f'tail{i+1}'
# legs
def leg(tag, foot_xy, attach, parent):
    fx, fy = foot_xy; top = (fx * 0.8, fy, body_bot + 0.1 * (body_top - body_bot))
    knee = (fx * 0.95, fy + (0.06 * L if tag[0] == 'B' else -0.04 * L), ground + 0.5 * (top[2] - ground))
    ankle = (fx, fy, ground + 0.18 * (top[2] - ground)); toe = (fx, fy - 0.08 * L, ground)
    bone(f'leg{tag}_upper', top, knee, parent); bone(f'leg{tag}_lower', knee, ankle, f'leg{tag}_upper', True); bone(f'leg{tag}_foot', ankle, toe, f'leg{tag}_lower', True)
leg('FL', front[1], None, 'chest'); leg('FR', front[0], None, 'chest')
leg('BL', back[1], None, 'hips'); leg('BR', back[0], None, 'hips')
# wings
if has_wings:
    for side, sgn in (('L', 1), ('R', -1)):
        r = (sgn * wing_root[0], wing_root[1], wing_root[2]); t = (sgn * wing_tip[0], wing_tip[1], wing_tip[2])
        a = tuple(r[i] + (t[i] - r[i]) * 0.35 for i in range(3)); a = (a[0], a[1], a[2] + 0.08 * H)
        b_ = tuple(r[i] + (t[i] - r[i]) * 0.7 for i in range(3))
        bone(f'wing{side}_1', r, a, 'chest'); bone(f'wing{side}_2', a, b_, f'wing{side}_1', True); bone(f'wing{side}_3', b_, t, f'wing{side}_2', True)
bpy.ops.object.mode_set(mode='OBJECT')
# ---- bind
for o in bpy.context.scene.objects: o.select_set(False)
mesh.select_set(True); arm.select_set(True); bpy.context.view_layer.objects.active = arm
bpy.ops.object.parent_set(type='ARMATURE_NAME')   # empty groups, one per bone; weights computed below
# ---- custom skinning: smooth falloff from each bone segment, 3 nearest bones, then Laplacian smoothing
bones = [(b.name, np.array(b.head_local), np.array(b.tail_local)) for b in arm_data.bones if b.name != 'root']
names = [b[0] for b in bones]
def seg_dist(Pts, a, b):
    ab = b - a; t = np.clip(((Pts - a) @ ab) / max(ab @ ab, 1e-12), 0, 1)
    return np.linalg.norm(Pts - (a + t[:, None] * ab), axis=1)
D = np.stack([seg_dist(P, a, b) for _, a, b in bones], 1)              # verts x bones
blen = np.array([np.linalg.norm(b - a) for _, a, b in bones])
radius = np.maximum(0.6 * blen, 0.06 * H)                                  # influence radius per bone
for i, nm in enumerate(names):                                            # body bones reach further than limbs
    if nm in ('hips', 'spine1', 'spine2', 'chest'): radius[i] = max(radius[i], 0.5 * (body_top - body_bot))
    if nm in ('neck1', 'neck2', 'head'): radius[i] = max(radius[i], 0.3 * (body_top - body_bot))
    if nm.startswith('tail'): radius[i] = max(radius[i], 0.12 * H)
    if nm == 'jaw': radius[i] *= 0.5
Wt = np.exp(-(D / radius) ** 2)
# keep the 3 strongest bones per vertex
k = 3; idx = np.argsort(-Wt, 1)[:, :k]; keep = np.zeros_like(Wt); np.put_along_axis(keep, idx, np.take_along_axis(Wt, idx, 1), 1)
Wt = keep / np.maximum(keep.sum(1, keepdims=True), 1e-9)
# wings must not grab the body, legs must not grab the belly: zero wing weight on vertices inside the body core
core = (np.abs(P[:, 0]) < 0.88 * wing_root[0]) if has_wings else np.zeros(len(P), bool)
for i, nm in enumerate(names):
    if nm.startswith('wing'): Wt[core, i] = 0
# and the wing membrane answers to wing bones only (no neck, head, leg or tail pull)
if has_wings:
    # chain skinning for the wings: the weight follows the position from the shoulder to the tip, so the whole wing
    # swings at the shoulder (wing_1) and the outer bones only bend it further - a real wingbeat, not a flutter at the tip
    def sstep(a, b, x): u = np.clip((x - a) / max(b - a, 1e-9), 0, 1); return u * u * (3 - 2 * u)
    wing_ids = [i for i, nm in enumerate(names) if nm.startswith('wing')]
    wing_share = Wt[:, wing_ids].sum(1) / np.maximum(Wt.sum(1), 1e-9)   # how much of each vertex the plain distance weights already gave the wings
    ax = np.abs(P[:, 0]); wingzone = (ax > 0.88 * wing_root[0]) & (P[:, 2] > body_bot + 0.3 * (body_top - body_bot)) & (wing_share > 0.3)
    u = (ax - wing_root[0]) / max(wing_tip[0] - wing_root[0], 1e-6)
    w1 = 1 - sstep(0.28, 0.5, u); w3 = sstep(0.62, 0.86, u); w2 = np.clip(1 - w1 - w3, 0, 1)
    rootmix = sstep(0.88 * wing_root[0], 0.98 * wing_root[0], ax)   # the wing is all shoulder bone from where it leaves the body: it pivots at the body, not out on the wing
    for side, sgn in (('L', 1), ('R', -1)):
        zone = wingzone & ((P[:, 0] > 0) if sgn > 0 else (P[:, 0] < 0))
        for i, nm in enumerate(names):
            if nm == 'chest': Wt[zone, i] = 1 - rootmix[zone]
            elif nm == f'wing{side}_1': Wt[zone, i] = rootmix[zone] * w1[zone]
            elif nm == f'wing{side}_2': Wt[zone, i] = rootmix[zone] * w2[zone]
            elif nm == f'wing{side}_3': Wt[zone, i] = rootmix[zone] * w3[zone]
            else: Wt[zone, i] = 0
Wt = Wt / np.maximum(Wt.sum(1, keepdims=True), 1e-9)
# ---- neck + head: chain skinning. Position along the chain neck0 -> mid -> head centre -> snout decides the bone.
ih, ij = names.index('head'), names.index('jaw'); in1, in2, ich = names.index('neck1'), names.index('neck2'), names.index('chest')
chain = np.array([neck0, mid, head_c, (0, y_snout, z_snout)], float)
def chain_param(Pts):
    best_d = np.full(len(Pts), np.inf); best_u = np.zeros(len(Pts))
    for s in range(3):
        a, b = chain[s], chain[s + 1]; ab = b - a; t = np.clip(((Pts - a) @ ab) / max(ab @ ab, 1e-12), 0, 1)
        d = np.linalg.norm(Pts - (a + t[:, None] * ab), axis=1); m = d < best_d; best_d[m] = d[m]; best_u[m] = s + t[m]
    return best_u, best_d
u, dchain = chain_param(P)
strongest0 = np.argmax(Wt, 1)
is_leg = np.array([names[i].startswith('leg') for i in strongest0])
is_wing = np.array([names[i].startswith('wing') for i in strongest0])
front = (P[:, 1] < y_neck0 + 0.04 * L) & ~is_leg & ~is_wing
crown = (P[:, 2] > z_head_c + 0.03 * H) & (P[:, 1] < y_neck0 + 0.02 * L) & ~is_wing     # horns, crown, frills above the head centre
front |= crown
# weights along the chain: chest centred at u=-0.5, neck1 at 0.5, neck2 at 1.5, head from 2.0 on
uu = u.copy(); uu[crown] = 3.0
neck_len = np.linalg.norm(chain[2] - chain[0]); head_len = abs(y_head_c - y_snout)
if neck_len < 1.0 * head_len:      # chibi build (no real neck): the whole front end is the head, back to the crown's rear
    y_crown_back = float(P[crown, 1].max()) + 0.02 * L if crown.any() else y_neck0
    chibi_head = ((P[:, 1] < y_neck0) | ((P[:, 1] < y_crown_back) & (P[:, 2] > spine_z))) & ~is_leg & ~is_wing
    front |= chibi_head; uu[chibi_head] = 3.0
    arm['chibi'] = 1
    print(' chibi head mode (neck', round(neck_len, 3), 'head', round(head_len, 3), ')')
w_ch = np.clip(1 - np.abs(uu + 0.5), 0, 1); w_n1 = np.clip(1 - np.abs(uu - 0.5), 0, 1); w_n2 = np.clip(1 - np.abs(uu - 1.5), 0, 1)
w_hd = np.clip(uu - 1.5, 0, 1); w_hd[uu >= 2.0] = 1; w_n2[uu >= 2.0] = 0
# jaw: head vertices below the jaw line, ahead of the jaw pivot
jaw_b = arm_data.bones['jaw']; jp = np.array(jaw_b.head_local)
jaw_zone = (uu >= 2.0) & (P[:, 2] < jaw_line + 0.02 * H) & (P[:, 1] < y_head_c)
w_jw = np.where(jaw_zone, np.clip((jaw_line + 0.02 * H - P[:, 2]) / (0.06 * H), 0, 1), 0)
Wf = np.zeros_like(Wt)
Wf[:, ich] = w_ch; Wf[:, in1] = w_n1; Wf[:, in2] = w_n2; Wf[:, ih] = w_hd * (1 - w_jw); Wf[:, ij] = w_hd * w_jw
Wf = Wf / np.maximum(Wf.sum(1, keepdims=True), 1e-9)
Wt[front] = Wf[front]
# spatial smoothing (loose pieces such as eyes and fern fronds must agree with the surface next to them)
from scipy.spatial import cKDTree
tree = cKDTree(P); _, nb = tree.query(P, k=12, distance_upper_bound=0.04 * H)
nb[nb >= len(P)] = np.arange(len(P)).repeat(12).reshape(len(P), 12)[nb >= len(P)]   # missing -> self
for _ in range(5):
    Wt = 0.4 * Wt + 0.6 * Wt[nb].mean(1)
# the head stays rigid after smoothing
rigid = front & (uu >= 2.0)
Wt[rigid] = Wf[rigid]
Wt = Wt / np.maximum(Wt.sum(1, keepdims=True), 1e-9)
for i, nm in enumerate(names):
    g = mesh.vertex_groups[nm]
    vs = np.nonzero(Wt[:, i] > 0.005)[0]
    for v in vs: g.add([int(v)], float(Wt[v, i]), 'REPLACE')
cnt = {g.name: 0 for g in mesh.vertex_groups}
for v in mesh.data.vertices:
    for g in v.groups:
        if g.weight > 0.01: cnt[mesh.vertex_groups[g.group].name] += 1
print('bound; verts per bone:', {k: v for k, v in cnt.items()})
print('EMPTY bones:', [k for k, v in cnt.items() if v == 0])
bpy.ops.wm.save_as_mainfile(filepath=out); print('saved', out)
