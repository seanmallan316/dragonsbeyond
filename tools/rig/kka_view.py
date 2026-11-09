"""kka_view.py model.kka clip frame out.png : quick software render of one baked frame (sanity check of the game file)."""
import sys, struct, numpy as np
from PIL import Image, ImageDraw
path, clip, fi, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
d = open(path, 'rb').read(); assert d[:4] == b'KKA2'
nv, nt, nc = struct.unpack('>III', d[4:16]); o = 16
uv = np.frombuffer(d[o:o + 8 * nv], '>f4').reshape(nv, 2); o += 8 * nv
tri = np.frombuffer(d[o:o + 12 * nt], '>u4').reshape(nt, 3); o += 12 * nt
tex = Image.open(path.replace('.kka', '_anim.png')).convert('RGB'); T = np.asarray(tex); TS = T.shape[0] - 1
for c in range(nc):
    name = d[o:o + 16].rstrip(b'\0').decode(); nf, dur, loop = struct.unpack('>IfI', d[o + 16:o + 28]); o += 28
    n = nf * nv * 10
    if name == clip:
        rec = np.frombuffer(d[o + fi * nv * 10:o + (fi + 1) * nv * 10], dtype=[('p', '>i2', 3), ('n', 'i1', 3), ('pad', 'i1')])
        P = rec['p'].astype(float) / 4000; N = rec['n'].astype(float) / 127
        print(name, 'frame', fi, 'of', nf, 'size', np.ptp(P, 0).round(2))
    o += n
S = 640; img = Image.new('RGB', (S, S), (215, 215, 210)); dr = ImageDraw.Draw(img)
# camera: 3/4 view from front-left-above; game axes y up, front +z
ang = np.radians(35); R = np.array([[np.cos(ang), 0, -np.sin(ang)], [0, 1, 0], [np.sin(ang), 0, np.cos(ang)]])
Q = P @ R.T; Q[:, 1] -= P[:, 1].max() / 2; tilt = np.radians(-20); Q = Q @ np.array([[1, 0, 0], [0, np.cos(tilt), -np.sin(tilt)], [0, np.sin(tilt), np.cos(tilt)]]).T
sc = S * 0.42 / max(np.ptp(Q[:, 0]), np.ptp(Q[:, 1])); X = S / 2 + Q[:, 0] * sc; Y = S / 2 - Q[:, 1] * sc
L = np.array([0.4, 0.8, 0.45]); L /= np.linalg.norm(L)
fc = tri; depth = Q[fc, 2].mean(1); order = np.argsort(depth)   # painter's: far first (camera looks toward -z after rotation? use ascending)
for t in order:
    a, b, c_ = fc[t]; n = (N[a] + N[b] + N[c_]) / 3; n /= max(np.linalg.norm(n), 1e-6)
    uvm = uv[[a, b, c_]].mean(0); col = T[int(np.clip(uvm[1], 0, 1) * TS), int(np.clip(uvm[0], 0, 1) * TS)].astype(float)
    sh = 0.35 + 0.65 * max(0, float(n @ L)); col = tuple(int(min(255, v * sh)) for v in col)
    dr.polygon([(X[a], Y[a]), (X[b], Y[b]), (X[c_], Y[c_])], fill=col)
img.save(out); print('wrote', out)
