#!/usr/bin/env python3
"""sheet.py <frames_dir> <out_prefix> : contact sheet (8 angles) + spinning GIF from turntable frames."""
import sys, glob
from PIL import Image
d, out = sys.argv[1], sys.argv[2]
fr = [Image.open(p).convert('RGB') for p in sorted(glob.glob(d + '/f*.png'))]
# sheet: every 3rd frame (45 deg steps), 4 x 2
pick = fr[::3][:8]; w = 512
sheet = Image.new('RGB', (w * 4, w * 2), (220, 220, 216))
for i, im in enumerate(pick):
    sheet.paste(im.resize((w, w)), ((i % 4) * w, (i // 4) * w))
sheet.save(out + '_sheet.jpg', quality=88)
small = [im.resize((480, 480)) for im in fr]
small[0].save(out + '.gif', save_all=True, append_images=small[1:], duration=80, loop=0)
print('wrote', out + '_sheet.jpg', out + '.gif')
