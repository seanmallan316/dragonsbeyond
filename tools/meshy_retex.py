#!/usr/bin/env python3
"""meshy_retex.py <name> <task_id> "<texture description>" : paints new textures on an existing Meshy model (keeps its UVs).
Saves meshy/props/<name>.glb. Cheap compared with making a new model."""
import json, sys, time, os, urllib.request
S = os.environ.get('DB_SCRATCH', '/tmp/claude-0/-home-claude/6f7614f4-42ff-5e17-87aa-65db3e205bfb/scratchpad')
OUT = '/home/claude/dragonsbeyond/meshy/props'; os.makedirs(OUT, exist_ok=True)
K = open(S + '/.meshykey').read().strip(); B = 'https://api.meshy.ai/openapi/v1'
def req(m, p, b=None):
    r = urllib.request.Request(B + p, data=json.dumps(b).encode() if b is not None else None, method=m, headers={'Authorization': 'Bearer ' + K, 'Content-Type': 'application/json'})
    try: return json.load(urllib.request.urlopen(r, timeout=180))
    except urllib.error.HTTPError as e: print('HTTP', e.code, e.read()[:400], flush=True); raise
name, tid, prompt = sys.argv[1], sys.argv[2], sys.argv[3]
rid = req('POST', '/retexture', {'input_task_id': tid, 'text_style_prompt': prompt, 'enable_original_uv': True, 'enable_pbr': False, 'ai_model': 'latest'})['result']
while True:
    t = req('GET', '/retexture/' + rid)
    if t['status'] in ('SUCCEEDED', 'FAILED', 'CANCELED'): print('retexture', t['status'], t.get('task_error'), 'credits', t.get('consumed_credits'), flush=True); break
    time.sleep(8)
assert t['status'] == 'SUCCEEDED', t
json.dump(t, open(f'{OUT}/{name}.json', 'w'), indent=1); urllib.request.urlretrieve(t['model_urls']['glb'], f'{OUT}/{name}.glb'); print('saved', f'{OUT}/{name}.glb')
