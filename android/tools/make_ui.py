#!/usr/bin/env python3
"""Draws the phone touch controls for Dragons Beyond (ancient gold rims, emerald glass, carved serif words) and the app icon.
Run from the repository root: python3 android/tools/make_ui.py"""
import math
from PIL import Image, ImageDraw, ImageFont, ImageFilter

OUT = "android/port/ui/"
FONT = "/usr/share/fonts/truetype/google-fonts/Lora-Variable.ttf"
S = 4  # draw big, then shrink for smooth edges
GOLD = (226, 182, 92)
GOLD_DARK = (120, 82, 30)


def font(px):
    f = ImageFont.truetype(FONT, px)
    try:
        f.set_variation_by_name("Bold")
    except Exception:
        pass
    return f


def medallion(size, word, letter, glow):
    W = size * S
    im = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    # soft outer glow in the button's colour
    g = Image.new("RGBA", (W, W), (0, 0, 0, 0)); gd = ImageDraw.Draw(g)
    gd.ellipse([W * 0.06, W * 0.06, W * 0.94, W * 0.94], fill=glow + (120,))
    im.alpha_composite(g.filter(ImageFilter.GaussianBlur(W * 0.035)))
    d = ImageDraw.Draw(im)
    p = W * 0.09
    # body: dark glass with a radial colour wash, lighter at the top
    body = Image.new("RGBA", (W, W), (0, 0, 0, 0)); bd = ImageDraw.Draw(body)
    for i in range(60):
        t = i / 59.0
        c = tuple(int(10 + (glow[k] * 0.55 - 10) * (1 - t) ** 1.4) for k in range(3))
        r0 = p + (W / 2 - p) * t * 0.98
        bd.ellipse([r0, r0 - W * 0.04 * (1 - t), W - r0, W - r0 - W * 0.04 * (1 - t)], fill=c + (205,))
    mask = Image.new("L", (W, W), 0); ImageDraw.Draw(mask).ellipse([p, p, W - p, W - p], fill=255)
    im.paste(body, (0, 0), mask)
    d = ImageDraw.Draw(im)
    # gold rim: two rings with a darker groove, and little rune notches around it
    d.ellipse([p, p, W - p, W - p], outline=GOLD_DARK + (255,), width=int(W * 0.05))
    d.ellipse([p + W * 0.012, p + W * 0.012, W - p - W * 0.012, W - p - W * 0.012], outline=GOLD + (255,), width=int(W * 0.022))
    q = p + W * 0.07
    d.ellipse([q, q, W - q, W - q], outline=GOLD + (150,), width=max(2, int(W * 0.008)))
    for k in range(16):
        a = k / 16 * math.tau
        r1, r2 = W / 2 - p - W * 0.035, W / 2 - p - W * 0.06
        d.line([(W / 2 + math.cos(a) * r1, W / 2 + math.sin(a) * r1), (W / 2 + math.cos(a) * r2, W / 2 + math.sin(a) * r2)], fill=GOLD + (200,), width=max(2, int(W * 0.01)))
    # glass shine
    sh = Image.new("RGBA", (W, W), (0, 0, 0, 0)); ImageDraw.Draw(sh).ellipse([W * 0.24, W * 0.17, W * 0.76, W * 0.46], fill=(255, 255, 255, 40))
    im.alpha_composite(sh.filter(ImageFilter.GaussianBlur(W * 0.02)))
    d = ImageDraw.Draw(im)
    # the word, carved in gold with a dark shadow
    fs = int(size * (0.22 if len(word) <= 3 else 0.17 if len(word) <= 5 else 0.14)) * S
    f = font(fs); w = d.textlength(word, font=f)
    y = W * 0.44 - fs * 0.62
    d.text((W / 2 - w / 2 + S * 2, y + S * 3), word, font=f, fill=(0, 0, 0, 170))
    d.text((W / 2 - w / 2, y), word, font=f, fill=(250, 226, 160, 255))
    if letter:
        f2 = font(int(size * 0.12) * S); w2 = d.textlength(letter, font=f2)
        d.text((W / 2 - w2 / 2, W * 0.63), letter, font=f2, fill=GOLD + (200,))
    return im.resize((size, size), Image.LANCZOS)


BUTTONS = [  # file, word, controller letter, glow colour
    ("btn_fly", "FLY", "A", (70, 200, 120)),
    ("btn_eat", "EAT", "X", (230, 170, 60)),
    ("btn_tail", "TAIL", "B", (200, 90, 70)),
    ("btn_roar", "ROAR", "Y", (240, 110, 40)),
    ("btn_run", "RUN", "R", (90, 160, 230)),
    ("btn_swap", "SWAP", "+", (170, 120, 230)),
    ("btn_cam", "VIEW", "", (120, 190, 220)),
    ("btn_ok", "OK", "A", (70, 200, 120)),
    ("btn_back", "BACK", "B", (200, 90, 70)),
    ("btn_erase", "ERASE", "X", (230, 170, 60)),
    ("btn_share", "SHARE", "X", (230, 170, 60)),
]
for name, word, letter, glow in BUTTONS:
    medallion(300, word, letter, glow).save(OUT + name + ".png")

# move stick: a carved gold ring with four arrowheads, and a jewel thumb knob
W = 300 * S
base = Image.new("RGBA", (W, W), (0, 0, 0, 0)); d = ImageDraw.Draw(base)
d.ellipse([6 * S, 6 * S, W - 6 * S, W - 6 * S], fill=(8, 18, 12, 120), outline=GOLD_DARK + (230,), width=10 * S)
d.ellipse([12 * S, 12 * S, W - 12 * S, W - 12 * S], outline=GOLD + (210,), width=4 * S)
for a in range(4):
    ang = a * math.pi / 2; cx, cy = W / 2 + math.cos(ang) * W * 0.38, W / 2 + math.sin(ang) * W * 0.38
    tip = (W / 2 + math.cos(ang) * W * 0.45, W / 2 + math.sin(ang) * W * 0.45)
    l = (cx + math.cos(ang + 2.2) * W * 0.06, cy + math.sin(ang + 2.2) * W * 0.06)
    r = (cx + math.cos(ang - 2.2) * W * 0.06, cy + math.sin(ang - 2.2) * W * 0.06)
    d.polygon([tip, l, r], fill=GOLD + (220,))
base.resize((300, 300), Image.LANCZOS).save(OUT + "stick_base.png")
K = 150 * S
knob = Image.new("RGBA", (K, K), (0, 0, 0, 0)); d = ImageDraw.Draw(knob)
for i in range(40):
    t = i / 39.0; r0 = 6 * S + (K / 2 - 6 * S) * t
    d.ellipse([r0, r0, K - r0, K - r0], fill=(int(20 + 40 * t), int(70 + 110 * t), int(40 + 60 * t), 240))
d.ellipse([6 * S, 6 * S, K - 6 * S, K - 6 * S], outline=GOLD + (255,), width=7 * S)
d.ellipse([K * 0.28, K * 0.18, K * 0.72, K * 0.45], fill=(255, 255, 255, 60))
knob.resize((150, 150), Image.LANCZOS).save(OUT + "stick_knob.png")
print("touch controls drawn")

# carved side panels for the bars beside the 16:9 picture on wide phones: dark emerald stone, a gold edge toward the
# picture, a climbing vine with leaves, and a dragon-eye gem in the middle
import random
PW, PH = 256 * 2, 1024 * 2
pan = Image.new("RGBA", (PW, PH), (0, 0, 0, 255))
px = pan.load(); rnd = random.Random(7)
noise = Image.effect_noise((PW // 4, PH // 4), 40).resize((PW, PH), Image.BICUBIC)
nz = noise.load()
for y in range(PH):
    for x in range(PW):
        n = (nz[x, y] - 128) / 128.0
        edge = x / PW   # darker away from the picture
        v = 0.55 + 0.45 * edge
        px[x, y] = (int((10 + 8 * n) * v), int((22 + 12 * n) * v), int((16 + 9 * n) * v), 255)
d = ImageDraw.Draw(pan)
# gold edge (on the right: the side that touches the picture)
d.rectangle([PW - 22, 0, PW, PH], fill=GOLD_DARK + (255,))
d.rectangle([PW - 18, 0, PW - 8, PH], fill=GOLD + (255,))
d.line([(PW - 34, 0), (PW - 34, PH)], fill=GOLD + (170,), width=4)
# the vine: a gentle S curve with gold leaves
pts = []
for i in range(0, PH + 1, 8):
    pts.append((PW * 0.48 + math.sin(i / PH * math.tau * 2.5) * PW * 0.2, i))
d.line(pts, fill=GOLD + (150,), width=7, joint="curve")
for k in range(1, 22):
    y = k * PH / 22; x = PW * 0.48 + math.sin(y / PH * math.tau * 2.5) * PW * 0.2
    side = 1 if k % 2 else -1; L = PW * 0.16
    a = -0.6 * side
    tip = (x + side * L * math.cos(a), y + L * math.sin(-0.5))
    mid1 = (x + side * L * 0.5, y - L * 0.32); mid2 = (x + side * L * 0.55, y + L * 0.05)
    d.polygon([(x, y), mid1, tip, mid2], fill=(200, 160, 80, 150), outline=GOLD + (200,))
pan.resize((256, 1024), Image.LANCZOS).transpose(Image.FLIP_LEFT_RIGHT).save(OUT + "panel_right.png")   # the right one has no gem (its buttons sit there)
# a dragon-eye gem in a gold setting, up high on the left panel (clear of the move stick)
cx, cy, R = PW * 0.48, PH * 0.3, PW * 0.2
d.ellipse([cx - R * 1.25, cy - R * 1.25, cx + R * 1.25, cy + R * 1.25], fill=GOLD_DARK + (255,))
d.ellipse([cx - R * 1.12, cy - R * 1.12, cx + R * 1.12, cy + R * 1.12], fill=GOLD + (255,))
for i in range(40):
    t = i / 39; r = R * (1 - t)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(int(20 + 60 * t), int(90 + 140 * t), int(40 + 60 * t), 255))
d.ellipse([cx - R * 0.14, cy - R * 0.8, cx + R * 0.14, cy + R * 0.8], fill=(8, 14, 6, 255))   # the slit pupil
d.ellipse([cx - R * 0.55, cy - R * 0.7, cx - R * 0.1, cy - R * 0.35], fill=(255, 255, 255, 90))
pan = pan.resize((256, 1024), Image.LANCZOS)
pan.save(OUT + "panel_left.png")
print("panels drawn")

# the loading picture shown while the game starts: the three Forest dragon stages on a deep forest glow, gold title
import numpy as np
from scipy import ndimage
src = Image.open("assets/splash_tv.png").convert("RGB")
hsv = np.array(src.convert("HSV")).astype(int)
fg = hsv[:, :, 1] > 40
fg[:190, :940] = False; fg[:112, :] = False; fg[:142, :1035] = False   # leave out the old title words
big = ndimage.binary_fill_holes(ndimage.binary_closing(fg, structure=np.ones((3, 3)), iterations=10))
keep = big & (hsv[:, :, 1] > 22)
keep = ndimage.binary_fill_holes(ndimage.binary_closing(keep, iterations=4)); keep = ndimage.binary_opening(keep, iterations=2)
keep &= ~((hsv[:, :, 1] < 20) & (hsv[:, :, 2] > 140))   # grey backdrop showing between legs and wings
keep = ndimage.binary_opening(keep, iterations=1)
lab, n = ndimage.label(keep); sizes = ndimage.sum(keep, lab, range(1, n + 1)); keep = np.isin(lab, 1 + np.where(sizes > 4000)[0])
m = Image.fromarray((keep * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.5))
cut = src.convert("RGBA"); cut.putalpha(m)
W, H = 1280, 720
sp = Image.new("RGBA", (W, H), (0, 0, 0, 255)); d = ImageDraw.Draw(sp)
for i in range(200):
    t = i / 199; rx, ry = W * 0.9 * (1 - t), H * 0.95 * (1 - t)
    d.ellipse([W / 2 - rx, H * 0.62 - ry, W / 2 + rx, H * 0.62 + ry], fill=(int(4 + 40 * t ** 1.6), int(12 + 70 * t ** 1.6), int(8 + 34 * t ** 1.6), 255))
glow = Image.new("RGBA", (W, H)); ImageDraw.Draw(glow).ellipse([340, 300, 940, 560], fill=(255, 210, 120, 70)); sp.alpha_composite(glow.filter(ImageFilter.GaussianBlur(70)))
sh = Image.new("RGBA", (W, H)); ImageDraw.Draw(sh).ellipse([90, 610, 1210, 660], fill=(0, 0, 0, 150)); sp.alpha_composite(sh.filter(ImageFilter.GaussianBlur(14)))
sp.alpha_composite(cut, (0, 20))
d = ImageDraw.Draw(sp)
ft = font(92); title = "DRAGONS BEYOND"; tw = d.textlength(title, font=ft)
d.text((W / 2 - tw / 2 + 4, 34 + 5), title, font=ft, fill=(0, 0, 0, 200))
d.text((W / 2 - tw / 2, 34), title, font=ft, fill=(246, 214, 140, 255), stroke_width=2, stroke_fill=GOLD_DARK + (255,))
fs2 = font(30); sub = "EVERLEAF WILDS"; sw2 = d.textlength(sub, font=fs2)
d.line([(W / 2 - 260, 158), (W / 2 - sw2 / 2 - 20, 158)], fill=GOLD + (200,), width=2); d.line([(W / 2 + sw2 / 2 + 20, 158), (W / 2 + 260, 158)], fill=GOLD + (200,), width=2)
d.text((W / 2 - sw2 / 2, 140), sub, font=fs2, fill=(230, 220, 190, 255))
sp.convert("RGB").save(OUT + "splash.png")
print("splash drawn")
