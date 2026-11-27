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
# feet groups along the body: low points split into runs along y (a stride pose puts one foot ahead of the other, so the
# mirrored-pair test above can miss a pair); the front group is the first run with feet on both sides, the back group the next
fp = feet_pts[np.abs(feet_pts[:, 0]) > 0.04 * W]
ordy = np.sort(fp[:, 1]); runs = []; start = ordy[0]; prev = ordy[0]
for v in ordy[1:]:
    if v - prev > 0.06 * L: runs.append((start, prev)); start = v
    prev = v
runs.append((start, prev))
groups = []
for a, b in runs:
    g = fp[(fp[:, 1] >= a) & (fp[:, 1] <= b)]; lft = g[g[:, 0] < 0]; rgt = g[g[:, 0] > 0]
    if len(lft) >= 5 and len(rgt) >= 5 and min(np.abs(lft[:, 0]).mean(), rgt[:, 0].mean()) > 0.06 * W:
        groups.append(((lft[:, 0].mean(), lft[:, 1].mean()), (rgt[:, 0].mean(), rgt[:, 1].mean())))
groups.sort(key=lambda g: g[0][1] + g[1][1])
if len(groups) >= 2 and (groups[1][0][1] + groups[1][1][1]) * 0.5 - (groups[0][0][1] + groups[0][1][1]) * 0.5 > 0.15 * L:
    front = [groups[0][0], groups[0][1]]; back = [groups[1][0], groups[1][1]]
    print('feet by runs: front', front, 'back', back)
else:
    if len(pairs) < 2: raise SystemExit(f'could not find two leg pairs: {pairs} from {C}')
    (yf, xf), (yb, xb) = pairs[0], pairs[-1]   # the two pairs farthest apart in y
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
# trace the real tail along the surface (it may curl up, over or to the side): geodesic distance from where it leaves the body
tail_verts = None; tail_g = None; tail_joints = None
try:
    import scipy.sparse as sps
    from scipy.sparse.csgraph import dijkstra, connected_components
    E = np.array([tuple(e.vertices) for e in mesh.data.edges], int)
    from scipy.spatial import cKDTree as _KDt
    _seam = _KDt(P).query_pairs(2e-4 * L, output_type='ndarray')   # texture seams split the surface: stitch the copies back together
    if len(_seam): E = np.vstack([E, _seam])
    y_cut = y_hip + 0.10 * L
    _mid = P[(P[:, 1] > y_shoulder + 0.25 * (y_hip - y_shoulder)) & (P[:, 1] < y_shoulder + 0.75 * (y_hip - y_shoulder)) & (P[:, 2] < spine_z)]
    _hw = min(max(float(np.percentile(np.abs(_mid[:, 0]), 97)) if len(_mid) > 50 else 0.1 * W, 0.04 * W), 0.2 * W)
    region = (P[:, 1] > y_cut) & ~((np.abs(P[:, 0]) > 1.05 * _hw) & (P[:, 2] > spine_z + 0.15 * (body_top - body_bot)))   # never the wings: out to the side and above the back
    ein = region[E[:, 0]] & region[E[:, 1]]; Er = E[ein]
    el = np.linalg.norm(P[Er[:, 0]] - P[Er[:, 1]], axis=1) + 1e-7
    G = sps.coo_matrix((np.r_[el, el], (np.r_[Er[:, 0], Er[:, 1]], np.r_[Er[:, 1], Er[:, 0]])), shape=(len(P), len(P))).tocsr()
    ncomp, lab = connected_components(G, directed=False)
    cross = (region[E[:, 0]] != region[E[:, 1]]) & (np.minimum(P[E[:, 0], 1], P[E[:, 1], 1]) <= y_cut)   # edges cut by the plane: their region end is a boundary vertex
    bnd = np.unique(np.where(region[E[cross, 0]], E[cross, 0], E[cross, 1]))
    best = None
    for c in np.unique(lab[bnd]):
        cb = bnd[lab[bnd] == c]; cv = np.nonzero(lab == c)[0]
        if len(cv) < 200: continue
        bx = np.abs(P[cb, 0]).mean()
        if bx > 0.12 * W: continue                  # a wing's trailing edge, not the tail
        if best is None or len(cv) > len(best[1]): best = (cb, cv)
    if best is not None:
        cb, cv = best
        dist = dijkstra(G, directed=False, indices=cb, min_only=True)
        g = dist[cv]; ok = np.isfinite(g); cv = cv[ok]; g = g[ok]; gmax = g.max()
        # a tail sculpted in a full ring, its tip fused back onto its own root, reads as TWO arms leaving the body. Find where they part,
        # tell the thin arm (the tip end) from the thick one (the real root), and cut the tip free there, so the tail is one long chain again.
        try:
            gn = g / gmax; cut = None
            for q in np.arange(0.06, 0.5, 0.03):
                bandv = cv[(gn > q) & (gn < q + 0.1)]
                if len(bandv) < 40: continue
                nb, lb = connected_components(G[bandv][:, bandv], directed=False)
                cnt = np.bincount(lb); bigc = np.nonzero(cnt > 0.2 * len(bandv))[0]
                if len(bigc) == 2:
                    def thick(c):
                        X = P[bandv[lb == c]]; ev = np.linalg.eigvalsh(np.cov((X - X.mean(0)).T)); return float(np.sqrt(ev[0] + ev[1]))
                    t0, t1 = thick(bigc[0]), thick(bigc[1]); thin = bigc[0] if t0 < t1 else bigc[1]
                    if min(t0, t1) > 0.6 * max(t0, t1) or q < 0.15: break   # two arms of the same width, or a split right at the root, are not a ring (legs, a forked tip)
                    cut = bandv[lb == thin]; print(' tail is a closed ring: cutting the tip free at g=%.2f (arm widths %.3f / %.3f, %d verts)' % (q, t0, t1, len(cut))); break
                if len(bigc) > 2: break
            if cut is not None:
                import bmesh as _bm
                wall = np.zeros(len(P), bool); wall[cut[gn[np.searchsorted(cv, cut)] < gn[np.searchsorted(cv, cut)].min() + 0.035]] = True   # a thin slice of the tip arm, right where it meets the root
                bmc = _bm.new(); bmc.from_mesh(mesh.data); bmc.faces.ensure_lookup_table()
                dead = [f for f in bmc.faces if any(wall[v.index] for v in f.verts)]
                _bm.ops.delete(bmc, geom=dead, context='FACES_ONLY'); bmc.to_mesh(mesh.data); bmc.free(); mesh.data.update()
                keep = ~(wall[Er[:, 0]] | wall[Er[:, 1]]); Er2 = Er[keep]; el2 = el[keep]
                G = sps.coo_matrix((np.r_[el2, el2], (np.r_[Er2[:, 0], Er2[:, 1]], np.r_[Er2[:, 1], Er2[:, 0]])), shape=(len(P), len(P))).tocsr()
                dist = dijkstra(G, directed=False, indices=cb, min_only=True)
                cv = np.nonzero(np.isfinite(dist) & region & ~wall)[0]; g = dist[cv]; gmax = g.max(); print('  tail length along the ring now %.2f (was %.2f)' % (gmax, gmax if False else 0))
        except Exception as ex2:
            print(' ring check skipped:', ex2)
        if gmax > 0.25 * L:
            g = g / gmax
            joints = [P[cb].mean(0)]
            for q in np.linspace(0.2, 1.0, 5):
                band = cv[np.abs(g - q) < (0.06 if q < 1 else 0.08)]
                if q >= 1.0: band = cv[g > 0.93]
                joints.append(P[band].mean(0) if len(band) else joints[-1] + np.array([0, 0.05 * L, 0]))
            tail_joints = np.array(joints); tail_verts = cv; tail_g = g
            # loose pieces (spikes, fins) floating near the tail join it: nearest traced tail vertex decides
            from scipy.spatial import cKDTree as _KD
            loose = np.nonzero(region & (lab != lab[cv[0]]))[0]
            if len(loose):
                dd, ii = _KD(P[cv]).query(P[loose]); near = dd < 0.06 * H
                comp_ok = {}
                for vi, n_ok, j in zip(loose, near, ii):
                    pass
                lv = loose[near]; tail_verts = np.r_[cv, lv]; tail_g = np.r_[g, g[ii[near]]]
            print(' tail traced along the surface:', len(tail_verts), 'verts, joints', np.round(tail_joints, 3).tolist())
except Exception as ex:
    print(' tail trace skipped:', ex)
# ---- wings: points far out in x, above the body middle, between hips and shoulders (plus a bit)
wing_pts = P[(np.abs(P[:, 0]) > 0.5 * max(W / 2, 1e-6)) & (P[:, 2] > body_bot + 0.3 * (body_top - body_bot))]
has_wings = len(wing_pts) > 200
if has_wings:
    wl = wing_pts[wing_pts[:, 0] > 0]
    y_wing = float(np.median(wl[:, 1])); tip = wl[np.argmax(wl[:, 0])]
    root_x = float(np.percentile(np.abs(P[(np.abs(P[:, 1] - y_wing) < 0.05 * L) & (P[:, 2] > spine_z)][:, 0]), 50)) * 0.6
    wing_root = (root_x, y_wing, body_top - 0.1 * (body_top - body_bot))
    wing_tip = (float(tip[0]), float(tip[1]), float(tip[2]))
# ---- wings, traced on the surface: everything hanging off the body beyond its sides, joined to it at the shoulder blade.
# The wing pivots where it really grows out of the back (the front of that join), not at some point out in the air.
wing_tr = {}
if has_wings:
    try:
        import scipy.sparse as sps
        from scipy.sparse.csgraph import dijkstra, connected_components
        Ew = np.array([tuple(e.vertices) for e in mesh.data.edges], int); bh = body_top - body_bot
        from scipy.spatial import cKDTree as _KDw
        seam = _KDw(P).query_pairs(2e-4 * L, output_type='ndarray')   # texture seams split the surface: stitch the copies back together
        if len(seam): Ew = np.vstack([Ew, seam])
        ya, yb = y_shoulder + 0.25 * (y_hip - y_shoulder), y_shoulder + 0.75 * (y_hip - y_shoulder)
        belly = P[(P[:, 1] > ya) & (P[:, 1] < yb) & (P[:, 2] < spine_z)]
        hw = float(np.percentile(np.abs(belly[:, 0]), 97)) if len(belly) > 50 else 0.1 * W
        hw = min(max(hw, 0.04 * W), 0.2 * W)
        def _segd(Pts, a, b):
            a = np.array(a, float); b = np.array(b, float); ab = b - a; t = np.clip(((Pts - a) @ ab) / max(ab @ ab, 1e-12), 0, 1); return np.linalg.norm(Pts - (a + t[:, None] * ab), axis=1)
        legzone = np.zeros(len(P), bool)   # the four legs: a fat column from each foot up to where it joins the body
        for (fx_, fy_) in list(front) + list(back):
            legzone |= (_segd(P, (fx_, fy_, ground), (np.sign(fx_) * hw * 0.8, fy_, spine_z)) < 0.085 * L) & (P[:, 2] < spine_z + 0.02 * H)
        for side, sgn in (('L', 1), ('R', -1)):
            X = P[:, 0] * sgn
            reg = (X > 1.05 * hw) & ~legzone & (P[:, 1] > y_neck0 - 0.02 * L)   # a wing may hang right down to the ground beside the body
            ein = reg[Ew[:, 0]] & reg[Ew[:, 1]]; Er = Ew[ein]
            el = np.linalg.norm(P[Er[:, 0]] - P[Er[:, 1]], axis=1) + 1e-7
            G = sps.coo_matrix((np.r_[el, el], (np.r_[Er[:, 0], Er[:, 1]], np.r_[Er[:, 1], Er[:, 0]])), shape=(len(P), len(P))).tocsr()
            ncomp, lab = connected_components(G, directed=False)
            cand = np.nonzero(reg)[0]; big = np.bincount(lab[cand]); cand = cand[big[lab[cand]] >= 300]   # the tip of a real wing, not of a loose shard
            tipv = cand[np.argmax(X[cand])]
            cv = np.nonzero((lab == lab[tipv]) & reg)[0]
            if len(cv) < 300: raise RuntimeError('wing component too small')
            # loose shards near the wing (spikes, feathers, crystal plates) ride with it
            from scipy.spatial import cKDTree as _KD2
            if tail_verts is not None: cv = np.setdiff1d(cv, tail_verts)   # a tail that curls up against a wing stays tail
            loose = np.nonzero(reg & (lab != lab[tipv]))[0]
            if tail_verts is not None: loose = np.setdiff1d(loose, tail_verts)
            cvx = cv
            if len(loose):   # a loose piece rides with the wing only as a whole, and only if nearly all of it lies against the wing (never half a piece: that tears it)
                dd, _ii = _KD2(P[cv]).query(P[loose]); near = dd < 0.04 * H; take = []
                for c in np.unique(lab[loose]):
                    m_ = lab[loose] == c; whole = np.nonzero(lab == c)[0]
                    if near[m_].mean() > 0.7 and m_.sum() >= 0.9 * len(whole): take.append(loose[m_])
                if take: cvx = np.r_[cv, np.concatenate(take)]
            cross = reg[Ew[:, 0]] != reg[Ew[:, 1]]
            bnd = np.unique(np.where(reg[Ew[cross, 0]], Ew[cross, 0], Ew[cross, 1])); bnd = bnd[lab[bnd] == lab[tipv]]
            if len(bnd) < 3: raise RuntimeError('wing has no join to the body')
            by = P[bnd, 1]; front_b = bnd[by <= by.min() + 0.3 * max(by.max() - by.min(), 1e-6)]
            root = P[front_b].mean(0); root[0] = sgn * 0.75 * hw
            root[1] = float(np.clip(root[1], y_shoulder - 0.06 * L, y_hip))
            tip = P[tipv].copy(); span = np.linalg.norm(tip - root)
            top = cv[np.argmax(P[cv, 2])]; wrist = P[top].copy()
            dw = np.linalg.norm(wrist - root)
            if dw < 0.22 * span or dw > 0.8 * span: wrist = root + (tip - root) * 0.38 + np.array([0, 0, 0.06 * H])
            gb = dijkstra(G, directed=False, indices=bnd, min_only=True)
            cen = P[cv].mean(0); elev = float(np.degrees(np.arctan2(cen[2] - root[2], (cen[0] - root[0]) * sgn)))
            sweep = float(np.degrees(np.arctan2(cen[1] - root[1], (cen[0] - root[0]) * sgn)))   # how far back the wing points in the model's pose
            wing_tr[side] = dict(root=root, wrist=wrist, tip=tip, verts=cvx, core=cv, gb=gb, elev=elev, sweep=sweep)
            print(f' wing {side} traced: {len(cvx)} verts, root', np.round(root, 3).tolist(), 'wrist', np.round(wrist, 3).tolist(), 'tip', np.round(tip, 3).tolist(), 'rest elevation', round(elev, 1))
    except Exception as ex:
        print(' wing trace skipped:', ex); wing_tr = {}
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
    if tail_joints is not None: a, b = tail_joints[i], tail_joints[i + 1]
    else: a, b = (0, ty[i], zmid_at(ty[i])), (0, ty[i + 1], zmid_at(ty[i + 1]))
    if np.linalg.norm(np.array(b) - np.array(a)) < 1e-3: b = np.array(a) + np.array([0, 0.03 * L, 0])
    bone(f'tail{i+1}', tuple(a), tuple(b), prevb, i > 0); prevb = f'tail{i+1}'
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
        if side in wing_tr:   # shoulder blade -> wrist (the top of the wing) -> halfway along the fingers -> tip
            tr = wing_tr[side]; r = tuple(tr['root']); a = tuple(tr['wrist']); t = tuple(tr['tip']); b_ = tuple(tr['wrist'] + (tr['tip'] - tr['wrist']) * 0.5)
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
    ax = np.abs(P[:, 0]); wingzone = (ax > 0.88 * wing_root[0]) & (P[:, 2] > body_bot + 0.3 * (body_top - body_bot)) & (wing_share > 0.3) & (P[:, 1] > y_neck0)   # never the head: a wide chibi head's cheeks and eyes are out past the wing roots
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
# traced wings: the whole wing belongs to the shoulder bone (it swings as one piece at the shoulder blade), the fingers
# beyond the wrist bend it further; only the skin right at the join blends into the chest
for side in wing_tr:
    tr = wing_tr[side]; vv = tr['verts']; Pv = P[vv]
    ax_ = tr['tip'] - tr['wrist']; ln = max(np.linalg.norm(ax_), 1e-6); sp = ((Pv - tr['wrist']) @ ax_) / (ln * ln)
    w2 = sstep(-0.08, 0.22, sp); w3 = sstep(0.42, 0.72, sp)
    gbv = tr['gb'][vv]; gbv = np.where(np.isfinite(gbv), gbv, 1.0); mixr = sstep(0.0, 0.05 * L, gbv)
    Wt[vv] = 0
    Wt[vv, names.index('chest')] = 1 - mixr
    Wt[vv, names.index(f'wing{side}_1')] = mixr * (1 - w2)
    Wt[vv, names.index(f'wing{side}_2')] = mixr * (w2 - w3)
    Wt[vv, names.index(f'wing{side}_3')] = mixr * w3
    tr['W'] = Wt[vv].copy()
if wing_tr: arm['wing_elev'] = float(np.mean([wing_tr[k]['elev'] for k in wing_tr])); arm['wing_sweep'] = float(np.mean([wing_tr[k]['sweep'] for k in wing_tr]))
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
is_wing = np.array([names[i].startswith('wing') for i in strongest0]) & (P[:, 1] > y_neck0)   # anything ahead of the shoulders is head or neck, whatever the distance weights said
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
for side in wing_tr:   # the wing stays one clean piece after smoothing (the membrane lies close to the back, but is not part of it)
    tr = wing_tr[side]; far = tr['gb'][tr['verts']]; far = ~np.isfinite(far) | (far > 0.05 * L)
    Wt[tr['verts'][far]] = tr['W'][far]
# the traced tail answers only to the tail chain (and the hips right at its root): the whole tail swings, curl and all
if tail_verts is not None:
    ihp = names.index('hips'); it = [names.index(f'tail{i+1}') for i in range(5)]
    jg = np.r_[0.0, np.linspace(0.2, 1.0, 5)]          # geodesic position of each joint
    uq = np.interp(tail_g, jg, np.arange(6))           # 0..5 along the chain
    Wtl = np.zeros((len(tail_verts), len(names)))
    for i in range(5): Wtl[:, it[i]] = np.clip(1 - np.abs(uq - (i + 0.5)), 0, 1)
    Wtl[:, it[0]] += np.clip(0.5 - uq, 0, 0.5); Wtl[:, it[4]] += np.clip(uq - 4.5, 0, 1)
    root = np.clip(1 - uq / 0.6, 0, 1); Wtl[:, ihp] = root; Wtl[:, it[0]] *= (1 - root) + root * 0.5
    Wtl = Wtl / np.maximum(Wtl.sum(1, keepdims=True), 1e-9)
    Wt[tail_verts] = Wtl
Wt = Wt / np.maximum(Wt.sum(1, keepdims=True), 1e-9)
# loose pieces (spikes, plates, feathers that float free of the main skin) each move as one solid piece with whatever
# they sit on: never half with one bone and half with another (that tore them into long streaks)
try:
    import scipy.sparse as sps
    from scipy.sparse.csgraph import connected_components
    from scipy.spatial import cKDTree as _KDl
    Ea = np.array([tuple(e.vertices) for e in mesh.data.edges], int); sm = _KDl(P).query_pairs(2e-4 * L, output_type='ndarray')
    if len(sm): Ea = np.vstack([Ea, sm])
    Ga = sps.coo_matrix((np.ones(len(Ea)), (Ea[:, 0], Ea[:, 1])), shape=(len(P), len(P))).tocsr()
    nc, la = connected_components(Ga, directed=False); sizes = np.bincount(la); main = np.argmax(sizes)
    mainv = np.nonzero(la == main)[0]; tm = _KDl(P[mainv]); nfix = 0
    for c in range(nc):
        if c == main: continue
        vv = np.nonzero(la == c)[0]; dd, ii = tm.query(P[vv]); j = np.argmin(dd)
        Wt[vv] = Wt[mainv[ii[j]]]; nfix += 1
    print(' loose pieces made solid:', nfix)
except Exception as ex:
    print(' loose pieces skipped:', ex)
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
