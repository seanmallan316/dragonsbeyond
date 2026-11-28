#!/usr/bin/env python3
"""web_content.py <out dir>: the game's files for the web version, in <out dir>/content/.
Everything in content/ is copied as it is, except the animated characters (.kka): for the web they keep every other
baked frame (the game blends between frames, so moves stay smooth), which halves the download. The on-screen touch
buttons come from android/port/ui. files.txt lists every file, so the game never asks the site for one it does not have."""
import os, shutil, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

def half_frames(src, dst):
    b = open(src, 'rb').read()
    if b[:4] != b'KKA2':
        shutil.copyfile(src, dst); return
    nv, nt, nc = struct.unpack('>III', b[4:16]); off = 16 + nv * 8 + nt * 12
    out = bytearray(b[:off])
    for _ in range(nc):
        name = b[off:off + 16]; nf, dur, loop = struct.unpack('>IfI', b[off + 16:off + 28]); off += 28
        fsz = nv * 10; frames = [b[off + i * fsz: off + (i + 1) * fsz] for i in range(nf)]; off += nf * fsz
        keep = frames[::2] if nf >= 6 else frames
        if not loop and nf >= 6 and (nf - 1) % 2:   # a move that holds its last pose keeps that last pose
            keep.append(frames[-1])
        out += name + struct.pack('>IfI', len(keep), dur, loop) + b''.join(keep)
    open(dst, 'wb').write(out)

def main():
    out = os.path.join(sys.argv[1], 'content'); os.makedirs(out, exist_ok=True); listed = []
    for base, src in (('content', os.path.join(ROOT, 'content')), ('content/port_ui', os.path.join(ROOT, 'android', 'port', 'ui'))):
        for d, _, files in os.walk(src):
            rel = os.path.relpath(d, src); tgt = os.path.normpath(os.path.join(out, os.path.relpath(base, 'content'), rel)); os.makedirs(tgt, exist_ok=True)
            for f in files:
                s, t = os.path.join(d, f), os.path.join(tgt, f)
                if f.endswith('.kka'): half_frames(s, t)
                else: shutil.copyfile(s, t)
                listed.append(os.path.relpath(t, sys.argv[1]).replace(os.sep, '/'))
    open(os.path.join(out, 'files.txt'), 'w').write('\n'.join(sorted(listed)) + '\n')
    total = sum(os.path.getsize(os.path.join(sys.argv[1], p)) for p in listed)
    print(f'{len(listed)} files, {total / 1e6:.0f} MB')

main()
