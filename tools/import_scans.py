"""import_scans.py : real photo-scanned surfaces from ambientCG (CC0, free for any use, https://ambientcg.com) -> content/img.
usage: python3 tools/import_scans.py <folder with the unzipped <Asset>_1K-JPG sets>
Each picture keeps its detail but is pulled half-way toward the old picture's average colour, so the world's look stays balanced."""
import sys, os, numpy as np
from PIL import Image
SRC = sys.argv[1]; OUT = os.path.join(os.path.dirname(__file__), '..', 'content', 'img')
PICK = {'dirt': ('Ground085', 1024), 'rock': ('Rock051', 1024), 'bark': ('Bark014', 512)}
for name, (asset, size) in PICK.items():
    new = np.asarray(Image.open(f'{SRC}/{asset}/{asset}_1K-JPG_Color.jpg').convert('RGB').resize((size, size), Image.LANCZOS), float)
    old = np.asarray(Image.open(f'{OUT}/{name}.png').convert('RGB'), float).reshape(-1, 3).mean(0)
    k = (old / new.reshape(-1, 3).mean(0)) ** 0.5   # half-way (in log terms) to the old average colour
    img = np.clip(new * k, 0, 255).astype(np.uint8)
    Image.fromarray(np.dstack([img, np.full(img.shape[:2], 255, np.uint8)])).save(f'{OUT}/{name}.png', optimize=True); print(name, '<-', asset)
