#!/usr/bin/env python3
"""meshy_prop.py <name> "<prompt>" [polycount] ["<texture prompt>"] : scenery prop by Meshy text-to-3D (preview, then refine with PBR textures).
Saves meshy/props/<name>.glb (+ .json). Key from the scratchpad .meshykey. Pass an existing preview id as the prompt prefixed 'id:' to skip the preview."""
import json, sys, time, os, urllib.request
S = os.environ.get('DB_SCRATCH', '/tmp/claude-0/-home-claude/6f7614f4-42ff-5e17-87aa-65db3e205bfb/scratchpad')
OUT = '/home/claude/dragonsbeyond/meshy/props'; os.makedirs(OUT, exist_ok=True)
K = open(S + '/.meshykey').read().strip(); B = 'https://api.meshy.ai/openapi'
def req(m, p, b=None):
    r = urllib.request.Request(B + p, data=json.dumps(b).encode() if b is not None else None, method=m, headers={'Authorization': 'Bearer ' + K, 'Content-Type': 'application/json'})
    try: return json.load(urllib.request.urlopen(r, timeout=180))
    except urllib.error.HTTPError as e: print('HTTP', e.code, e.read()[:400], flush=True); raise
def wait(tid):
    while True:
        t = req('GET', '/v2/text-to-3d/' + tid)
        if t['status'] in ('SUCCEEDED', 'FAILED', 'CANCELED'): print(t['mode'] if 'mode' in t else '', t['status'], t.get('task_error'), 'credits', t.get('consumed_credits'), flush=True); return t
        time.sleep(8)
name, prompt = sys.argv[1], sys.argv[2]; poly = int(sys.argv[3]) if len(sys.argv) > 3 else 6000
if prompt.startswith('id:'): pid = prompt[3:]
else: pid = req('POST', '/v2/text-to-3d', {'mode': 'preview', 'prompt': prompt, 'art_style': 'realistic', 'ai_model': 'latest', 'should_remesh': True, 'topology': 'triangle', 'target_polycount': poly})['result']
pv = wait(pid); assert pv['status'] == 'SUCCEEDED', pv
tex_prompt = sys.argv[4] if len(sys.argv) > 4 else None
body = {'mode': 'refine', 'preview_task_id': pid, 'enable_pbr': False}
if tex_prompt: body['texture_prompt'] = tex_prompt
rid = req('POST', '/v2/text-to-3d', body)['result']
rf = wait(rid); assert rf['status'] == 'SUCCEEDED', rf
json.dump(rf, open(f'{OUT}/{name}.json', 'w'), indent=1); urllib.request.urlretrieve(rf['model_urls']['glb'], f'{OUT}/{name}.glb')
if rf.get('thumbnail_url'): urllib.request.urlretrieve(rf['thumbnail_url'], f'{OUT}/{name}_thumb.png')
print('saved', f'{OUT}/{name}.glb')
