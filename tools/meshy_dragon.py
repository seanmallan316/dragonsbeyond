#!/usr/bin/env python3
"""meshy_dragon.py <name> <front.png> [side.png] [back.png] [left.png]
Makes a dragon model in 3D from approved concept renders (Meshy image-to-3D, multi-view when
several views are given), textured with PBR maps, high polygon count. Saves <out>/<name>.json,
<name>.glb and a turntable preview. The Meshy key is read from the scratchpad file .meshykey."""
import json, sys, time, base64, os, urllib.request, mimetypes
S = os.environ.get('DB_SCRATCH', '/tmp/claude-0/-home-claude/6f7614f4-42ff-5e17-87aa-65db3e205bfb/scratchpad')
OUT = '/home/claude/dragonsbeyond/meshy'
K = open(S + '/.meshykey').read().strip()
B = 'https://api.meshy.ai/openapi/v1'

def req(m, p, b=None):
    r = urllib.request.Request(B + p, data=json.dumps(b).encode() if b is not None else None, method=m,
                               headers={'Authorization': 'Bearer ' + K, 'Content-Type': 'application/json'})
    try:
        return json.load(urllib.request.urlopen(r, timeout=180))
    except urllib.error.HTTPError as e:
        print('HTTP', e.code, e.read()[:500], flush=True); raise

def wait(ep, tid):
    while True:
        t = req('GET', ep + '/' + tid)
        if t['status'] in ('SUCCEEDED', 'FAILED', 'CANCELED'):
            print(ep, t['status'], t.get('task_error'), 'credits', t.get('consumed_credits'), flush=True)
            return t
        print(' ', t.get('progress', 0), '%', flush=True)
        time.sleep(10)

def data_uri(path):
    mt = mimetypes.guess_type(path)[0] or 'image/png'
    return f'data:{mt};base64,' + base64.b64encode(open(path, 'rb').read()).decode()

name, views = sys.argv[1], sys.argv[2:]
os.makedirs(OUT, exist_ok=True)
body = {'ai_model': 'latest', 'should_texture': True, 'enable_pbr': True, 'should_remesh': True,
        'target_polycount': 30000, 'topology': 'quad', 'symmetry_mode': 'auto',
        'texture_prompt': 'stylized 3D animated film dragon, smooth clean scales, soft subsurface skin, crisp white horns, vivid saturated colors'}
if len(views) == 1:
    ep = '/image-to-3d'; body['image_url'] = data_uri(views[0])
else:
    ep = '/multi-image-to-3d'; body['image_urls'] = [data_uri(v) for v in views]
t = wait(ep, req('POST', ep, body)['result'])
json.dump(t, open(f'{OUT}/{name}.json', 'w'), indent=1)
urllib.request.urlretrieve(t['model_urls']['glb'], f'{OUT}/{name}.glb')
if t.get('thumbnail_url'):
    urllib.request.urlretrieve(t['thumbnail_url'], f'{OUT}/{name}_thumb.png')
print('saved', f'{OUT}/{name}.glb')
