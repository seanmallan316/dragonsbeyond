"""boss_look.py : makes the robot bosses read clearly: lifts the near-black Meshy textures to polished gunmetal and pulls the red/orange
parts out into a glow map (drawn again, glowing, on top). Keeps the untouched originals as *_tex_src.png.
usage: python3 tools/boss_look.py"""
import os, shutil, numpy as np
from PIL import Image, ImageFilter
D = os.path.join(os.path.dirname(__file__), '..', 'content', 'models')
for name in ('ironhorn', 'stormtalon'):
    src = f'{D}/{name}_tex_src.png'
    if not os.path.exists(src): shutil.copy(f'{D}/{name}_tex.png', src)
    im = Image.open(src).convert('RGBA'); a = np.asarray(im).astype(float) / 255; rgb, al = a[..., :3], a[..., 3:]
    lum = rgb @ np.array([0.3, 0.55, 0.15])
    hot = np.clip((rgb[..., 0] - np.maximum(rgb[..., 1], rgb[..., 2]) * 1.15 - 0.08) * 4, 0, 1)   # red/orange: eyes, core, vents
    steel = np.array([0.42, 0.46, 0.52]) if name == 'ironhorn' else np.array([0.40, 0.40, 0.46])
    base = steel * (0.35 + 1.6 * lum[..., None] ** 0.85) + (rgb - lum[..., None]) * 0.8   # gunmetal, keeping a little of the original colour
    out = base * (1 - hot[..., None]) + np.array([1.0, 0.35, 0.1]) * hot[..., None]
    Image.fromarray((np.clip(np.concatenate([out, al], -1), 0, 1) * 255).astype(np.uint8)).save(f'{D}/{name}_tex.png', optimize=True)
    g = Image.fromarray((np.clip(hot, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(2))
    gl = np.asarray(g).astype(float) / 255; glow = np.dstack([np.full_like(gl, 1.0), gl * 0.45 + 0.1, gl * 0.15, gl])
    Image.fromarray((np.clip(glow, 0, 1) * 255).astype(np.uint8)).save(f'{D}/{name}_glow.png', optimize=True)
    print(name, 'glow pixels', round(float((hot > 0.3).mean()) * 100, 2), '%')
