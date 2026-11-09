"""make_textures.py : procedural textures for Everleaf Wilds -> content/img/*.png (grass, dirt, bark, leaves, berries, rock, water, mushroom, grub, fish, icon)."""
import numpy as np, os
from PIL import Image, ImageDraw, ImageFilter
OUT = os.path.join(os.path.dirname(__file__), '..', 'content', 'img'); os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(3)
def noise(n, octs=5, seed=0, tile=True):
    r = np.random.default_rng(seed); out = np.zeros((n, n)); amp = 1; tot = 0
    for o in range(octs):
        s = 4 * 2 ** o; g = r.random((s, s))
        if tile: g = np.pad(g, ((0, 1), (0, 1)), mode='wrap')
        img = Image.fromarray((g * 255).astype(np.uint8)).resize((n + (n // s if tile else 0), n + (n // s if tile else 0)), Image.BICUBIC)
        a = np.asarray(img, float)[:n, :n] / 255.0; out += a * amp; tot += amp; amp *= 0.55
    return out / tot
def save(name, arr):
    Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(os.path.join(OUT, name + '.png')); print(name)
def mix(c0, c1, t): t = t[..., None]; return np.array(c0) * (1 - t) + np.array(c1) * t
N = 512
# grass: layered greens with blade streaks
n1 = noise(N, 6, 1); n2 = noise(N, 3, 2); blades = noise(N, 7, 3)
g = mix((58, 110, 40), (120, 170, 60), n1); g = mix(g, (160, 190, 80), np.clip((blades - 0.55) * 4, 0, 1) * 0.6); g = mix(g, (40, 80, 35), np.clip((0.45 - n2) * 3, 0, 1) * 0.5)
save('grass', np.dstack([g, np.full((N, N), 255)]))
# dirt path
d = mix((120, 92, 60), (160, 130, 90), noise(N, 6, 4)); d = mix(d, (90, 70, 45), np.clip((noise(N, 7, 5) - 0.6) * 5, 0, 1))
save('dirt', np.dstack([d, np.full((N, N), 255)]))
# bark: vertical streaks
yy, xx = np.mgrid[0:N, 0:N]; streak = noise(N, 6, 6); bark = mix((70, 50, 35), (130, 100, 70), np.clip(streak * 0.7 + 0.3 * np.sin(xx * 0.5 + streak * 9) ** 2, 0, 1))
bark = mix(bark, (40, 28, 20), np.clip((noise(N, 8, 7) - 0.62) * 6, 0, 1))
save('bark', np.dstack([bark, np.full((N, N), 255)]))
# leaf cluster (cutout): many overlapping leaves on transparent
im = Image.new('RGBA', (N, N), (0, 0, 0, 0)); dr = ImageDraw.Draw(im)
for _ in range(260):
    cx, cy = rng.random(2) * N; a = rng.random() * 360; L = 40 + rng.random() * 50; W = L * 0.45
    col = (int(40 + rng.random() * 60), int(120 + rng.random() * 90), int(30 + rng.random() * 50), 255)
    leaf = Image.new('RGBA', (int(L * 2), int(L * 2)), (0, 0, 0, 0)); ld = ImageDraw.Draw(leaf)
    ld.ellipse((L - W / 2, L - L / 2, L + W / 2, L + L / 2), fill=col); ld.line((L, L - L / 2, L, L + L / 2), fill=(col[0] // 2, col[1] // 2, col[2] // 2, 255), width=3)
    leaf = leaf.rotate(a, resample=Image.BICUBIC); im.alpha_composite(leaf, (int(cx - L), int(cy - L)))
arr = np.asarray(im).astype(float); sh = noise(N, 4, 8); arr[..., :3] *= (0.75 + 0.5 * sh)[..., None]
save('leaves', arr)
# fern frond (cutout): one frond, pointing up
im = Image.new('RGBA', (N, N), (0, 0, 0, 0)); dr = ImageDraw.Draw(im)
for k in range(26):
    t = k / 25; y = N - 20 - t * (N - 60); w = (1 - t) * 150 + 20; col = (50, int(130 + 60 * t), 40, 255)
    dr.polygon([(N / 2, y), (N / 2 - w, y - 28 * (1 - t) - 10), (N / 2 - w * 0.9, y - 40 * (1 - t) - 16), (N / 2, y - 14)], fill=col)
    dr.polygon([(N / 2, y), (N / 2 + w, y - 28 * (1 - t) - 10), (N / 2 + w * 0.9, y - 40 * (1 - t) - 16), (N / 2, y - 14)], fill=col)
dr.line((N / 2, N - 10, N / 2, 40), fill=(60, 90, 30, 255), width=6)
save('fern', np.asarray(im).astype(float))
# berry bush (cutout quad): green ball with red berries
im = Image.new('RGBA', (N, N), (0, 0, 0, 0)); dr = ImageDraw.Draw(im)
for _ in range(120):
    cx, cy = N / 2 + (rng.random() - 0.5) * N * 0.75, N / 2 + (rng.random() - 0.5) * N * 0.7; r = 30 + rng.random() * 40
    dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(int(40 + rng.random() * 40), int(110 + rng.random() * 70), int(40 + rng.random() * 30), 255))
for _ in range(34):
    cx, cy = N / 2 + (rng.random() - 0.5) * N * 0.6, N / 2 + (rng.random() - 0.5) * N * 0.55; r = 10 + rng.random() * 7
    dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(220, 40, 60, 255)); dr.ellipse((cx - r * 0.5, cy - r * 0.6, cx - r * 0.1, cy - r * 0.2), fill=(255, 170, 180, 255))
save('berrybush', np.asarray(im).astype(float))
# the same bush, eaten (no berries)
im2 = Image.new('RGBA', (N, N), (0, 0, 0, 0)); dr = ImageDraw.Draw(im2); rng2 = np.random.default_rng(3)
for _ in range(120):
    cx, cy = N / 2 + (rng2.random() - 0.5) * N * 0.75, N / 2 + (rng2.random() - 0.5) * N * 0.7; r = 30 + rng2.random() * 40
    dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(int(40 + rng2.random() * 40), int(110 + rng2.random() * 70), int(40 + rng2.random() * 30), 255))
save('bush', np.asarray(im2).astype(float))
# rock
r = mix((95, 95, 90), (150, 148, 140), noise(N, 6, 9)); r = mix(r, (60, 62, 60), np.clip((noise(N, 8, 10) - 0.6) * 5, 0, 1)); r = mix(r, (110, 130, 80), np.clip((noise(N, 4, 11) - 0.65) * 4, 0, 1) * 0.6)
save('rock', np.dstack([r, np.full((N, N), 255)]))
# water
wv = noise(N, 5, 12); wa = mix((40, 110, 150), (120, 190, 210), wv); save('water', np.dstack([wa, np.full((N, N), 190)]))
# mushroom (cutout): red cap with white spots on a stalk
im = Image.new('RGBA', (N, N), (0, 0, 0, 0)); dr = ImageDraw.Draw(im)
dr.rounded_rectangle((N * 0.42, N * 0.45, N * 0.58, N * 0.95), radius=30, fill=(235, 225, 200, 255))
dr.ellipse((N * 0.12, N * 0.1, N * 0.88, N * 0.6), fill=(215, 60, 50, 255))
for cx, cy, rr in [(0.3, 0.3, 0.05), (0.5, 0.2, 0.06), (0.7, 0.33, 0.045), (0.42, 0.45, 0.035), (0.62, 0.48, 0.04)]:
    dr.ellipse(((cx - rr) * N, (cy - rr) * N, (cx + rr) * N, (cy + rr) * N), fill=(250, 245, 230, 255))
save('mushroom', np.asarray(im).astype(float))
# grub (cutout): a fat striped beetle-grub, top view, facing up
im = Image.new('RGBA', (N, N), (0, 0, 0, 0)); dr = ImageDraw.Draw(im)
dr.ellipse((N * 0.3, N * 0.15, N * 0.7, N * 0.9), fill=(230, 200, 120, 255))
for k in range(6): y = N * (0.25 + k * 0.11); dr.ellipse((N * 0.31, y - 8, N * 0.69, y + 8), outline=(160, 120, 60, 255), width=5)
dr.ellipse((N * 0.36, N * 0.08, N * 0.64, N * 0.3), fill=(120, 70, 40, 255)); dr.ellipse((N * 0.4, N * 0.14, N * 0.46, N * 0.2), fill=(0, 0, 0, 255)); dr.ellipse((N * 0.54, N * 0.14, N * 0.6, N * 0.2), fill=(0, 0, 0, 255))
save('grub', np.asarray(im).astype(float))
# fish (cutout): side view facing right
im = Image.new('RGBA', (N, N), (0, 0, 0, 0)); dr = ImageDraw.Draw(im)
dr.ellipse((N * 0.15, N * 0.35, N * 0.75, N * 0.65), fill=(90, 150, 220, 255)); dr.polygon([(N * 0.18, N * 0.5), (N * 0.02, N * 0.3), (N * 0.02, N * 0.7)], fill=(70, 120, 190, 255))
dr.ellipse((N * 0.6, N * 0.43, N * 0.66, N * 0.49), fill=(0, 0, 0, 255)); dr.polygon([(N * 0.4, N * 0.36), (N * 0.5, N * 0.2), (N * 0.55, N * 0.37)], fill=(70, 120, 190, 255))
save('fish', np.asarray(im).astype(float))
# sparkle (additive): soft star
yy, xx = np.mgrid[0:128, 0:128]; dx, dy = xx - 63.5, yy - 63.5; rr = np.sqrt(dx * dx + dy * dy)
star = np.clip(1 - rr / 60, 0, 1) ** 3 + 0.8 * np.clip(1 - np.abs(dx) / 3, 0, 1) * np.clip(1 - np.abs(dy) / 60, 0, 1) + 0.8 * np.clip(1 - np.abs(dy) / 3, 0, 1) * np.clip(1 - np.abs(dx) / 60, 0, 1)
save('sparkle', np.dstack([np.full((128, 128), 255), np.full((128, 128), 250), np.full((128, 128), 200), np.clip(star, 0, 1) * 255]))
# icon 128x128
ic = Image.open(os.path.join(os.path.dirname(__file__), '..', 'concept', 'forest_dragonet_side.png')).convert('RGB').resize((128, 128), Image.LANCZOS)
ic.save(os.path.join(OUT, '..', 'icon.png')); print('icon')

# ---- second pass: richer grass, grass tufts, flowers, clouds, calmer leaves
N = 512
g1 = noise(N, 7, 21); g2 = noise(N, 4, 22); g3 = noise(N, 8, 23)
g = mix((46, 92, 34), (96, 142, 52), g1); g = mix(g, (128, 160, 70), np.clip((g3 - 0.58) * 5, 0, 1) * 0.7); g = mix(g, (36, 70, 30), np.clip((0.42 - g2) * 3, 0, 1) * 0.6)
g = mix(g, (120, 110, 60), np.clip((noise(N, 5, 24) - 0.66) * 6, 0, 1) * 0.5)   # a few dry patches
save('grass', np.dstack([g, np.full((N, N), 255)]))
# grass tuft (cutout): a fan of blades from the bottom centre
im = Image.new('RGBA', (N, N), (0, 0, 0, 0)); dr = ImageDraw.Draw(im); r2 = np.random.default_rng(5)
for _ in range(70):
    x0 = N / 2 + (r2.random() - 0.5) * 120; h = 180 + r2.random() * 280; bend = (r2.random() - 0.5) * 260; w = 10 + r2.random() * 14
    col = (int(60 + r2.random() * 60), int(130 + r2.random() * 90), int(40 + r2.random() * 40), 255)
    pts = [(x0 - w / 2, N), (x0 + w / 2, N), (x0 + bend * 0.5 + 2, N - h * 0.6), (x0 + bend, N - h), (x0 + bend * 0.5 - 2, N - h * 0.6)]
    dr.polygon(pts, fill=col)
save('grasstuft', np.asarray(im).astype(float))
# flowers (cutout): small clusters of 5-petal flowers in a few colours, on thin stems
im = Image.new('RGBA', (N, N), (0, 0, 0, 0)); dr = ImageDraw.Draw(im); r2 = np.random.default_rng(6)
cols = [(255, 230, 80), (255, 120, 150), (250, 250, 250), (170, 130, 255), (255, 150, 60)]
for _ in range(16):
    cx = N / 2 + (r2.random() - 0.5) * 300; cy = N * 0.25 + r2.random() * N * 0.35; rad = 18 + r2.random() * 14; col = cols[int(r2.random() * len(cols))]
    dr.line((cx, N, cx, cy), fill=(70, 120, 50, 255), width=5)
    for k in range(5):
        a = k * 2 * np.pi / 5 + r2.random() * 0.3; dr.ellipse((cx + np.cos(a) * rad - rad * 0.6, cy + np.sin(a) * rad - rad * 0.6, cx + np.cos(a) * rad + rad * 0.6, cy + np.sin(a) * rad + rad * 0.6), fill=col + (255,))
    dr.ellipse((cx - rad * 0.45, cy - rad * 0.45, cx + rad * 0.45, cy + rad * 0.45), fill=(255, 210, 60, 255))
save('flowers', np.asarray(im).astype(float))
# cloud (cutout, soft): a puffy cumulus
yy, xx = np.mgrid[0:N, 0:N]; cl = np.zeros((N, N)); r2 = np.random.default_rng(7)
for _ in range(14):
    cx, cy, rr = N * (0.25 + r2.random() * 0.5), N * (0.45 + r2.random() * 0.2), N * (0.1 + r2.random() * 0.12)
    cl = np.maximum(cl, np.clip(1 - np.sqrt((xx - cx) ** 2 + ((yy - cy) * 1.4) ** 2) / rr, 0, 1))
cl = np.clip(cl * 1.6 - 0.2, 0, 1); shade = 0.82 + 0.18 * np.clip((N * 0.6 - yy) / (N * 0.3), 0, 1)
save('cloud', np.dstack([255 * shade, 255 * shade, 255 * np.minimum(1, shade + 0.03), cl * 255]))
# calmer leaf texture: softer, rounder leaves, less contrast
im = Image.new('RGBA', (N, N), (0, 0, 0, 0)); r2 = np.random.default_rng(8)
for _ in range(220):
    cx, cy = r2.random(2) * N; a = r2.random() * 360; L = 46 + r2.random() * 40; W = L * 0.62
    base = 95 + r2.random() * 40; col = (int(base * 0.45), int(base * 1.25), int(base * 0.4), 255)
    leaf = Image.new('RGBA', (int(L * 2), int(L * 2)), (0, 0, 0, 0)); ld = ImageDraw.Draw(leaf)
    ld.ellipse((L - W / 2, L - L / 2, L + W / 2, L + L / 2), fill=col)
    leaf = leaf.rotate(a, resample=Image.BICUBIC); im.alpha_composite(leaf, (int(cx - L), int(cy - L)))
arr = np.asarray(im).astype(float); arr[..., :3] *= (0.8 + 0.4 * noise(N, 3, 9))[..., None]
save('leaves', arr)

# ---- third pass: photoreal-leaning ground set (grass with blade detail, dirt, rock) for bump shading
N = 1024
def blades(seed, n_blades, col_lo, col_hi, length=(14, 40)):
    im = Image.new('RGB', (N, N), (0, 0, 0)); dr = ImageDraw.Draw(im); r = np.random.default_rng(seed)
    base = noise(N, 5, seed + 1)
    arr = (mix(col_lo, col_hi, base)).astype(np.uint8); im = Image.fromarray(arr); dr = ImageDraw.Draw(im)
    for _ in range(n_blades):
        x, y = r.random() * N, r.random() * N; L = length[0] + r.random() * (length[1] - length[0]); a = (r.random() - 0.5) * 1.2 - np.pi / 2
        t = r.random(); c = tuple(int(v) for v in (np.array(col_lo) * (1 - t) + np.array(col_hi) * t) * (0.7 + 0.6 * r.random()))
        x2, y2 = x + np.cos(a) * L, y + np.sin(a) * L; dr.line((x, y, x2, y2), fill=c, width=int(1 + r.random() * 2))
        # wrap the ones that cross the edge so the tile is seamless
        for ox, oy in ((N, 0), (-N, 0), (0, N), (0, -N)): dr.line((x + ox, y + oy, x2 + ox, y2 + oy), fill=c, width=2)
    return np.asarray(im).astype(float)
g = blades(31, 26000, (34, 70, 26), (118, 160, 58))
shade = noise(N, 4, 33); g = g * (0.8 + 0.4 * shade)[..., None]
save('grass', np.dstack([g, np.full((N, N), 255)]))
# dirt: packed earth with pebbles and cracks
d = mix((86, 64, 42), (150, 122, 86), noise(N, 6, 41)); d = mix(d, (60, 44, 30), np.clip((noise(N, 8, 42) - 0.58) * 6, 0, 1)); d = mix(d, (170, 150, 120), np.clip((noise(N, 7, 43) - 0.68) * 8, 0, 1))
save('dirt', np.dstack([d, np.full((N, N), 255)]))
# rock: layered grey stone with lichen
rk = mix((78, 76, 72), (150, 146, 138), noise(N, 6, 51)); rk = mix(rk, (50, 50, 48), np.clip((noise(N, 8, 52) - 0.6) * 6, 0, 1)); rk = mix(rk, (120, 135, 80), np.clip((noise(N, 4, 53) - 0.62) * 5, 0, 1) * 0.5)
save('rock', np.dstack([rk, np.full((N, N), 255)]))
