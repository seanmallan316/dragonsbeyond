#!/usr/bin/env python3
"""meshy_rig.py <name> : Meshy auto-rigging (humanoids) of meshy/props/<name>.glb's refine task, with Meshy's walking and running
animations. Saves meshy/props/<name>_rigged.glb, <name>_walk.glb, <name>_run.glb. Key from $DB_SCRATCH/.meshykey."""
import json, sys, time, os, urllib.request
S = os.environ.get('DB_SCRATCH', '/home/claude'); K = open(S + '/.meshykey').read().strip(); B = 'https://api.meshy.ai/openapi/v1'
OUT = '/home/claude/dragonsbeyond/meshy/props'
def req(m, p, b=None):
    r = urllib.request.Request(B + p, data=json.dumps(b).encode() if b is not None else None, method=m, headers={'Authorization': 'Bearer ' + K, 'Content-Type': 'application/json'})
    try: return json.load(urllib.request.urlopen(r, timeout=180))
    except urllib.error.HTTPError as e: print('HTTP', e.code, e.read()[:400], flush=True); raise
name = sys.argv[1]; height = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
src = json.load(open(f'{OUT}/{name}.json'))
tid = req('POST', '/rigging', {'input_task_id': src['id'], 'height_meters': height})['result']
while True:
    t = req('GET', '/rigging/' + tid)
    if t['status'] in ('SUCCEEDED', 'FAILED', 'CANCELED'): break
    time.sleep(8)
print(t['status'], t.get('task_error'), 'credits', t.get('consumed_credits'), flush=True)
json.dump(t, open(f'{OUT}/{name}_rig.json', 'w'), indent=1)
assert t['status'] == 'SUCCEEDED'
r = t['result']; urllib.request.urlretrieve(r['rigged_character_glb_url'], f'{OUT}/{name}_rigged.glb')
ba = r.get('basic_animations') or {}
if ba.get('walking_glb_url'): urllib.request.urlretrieve(ba['walking_glb_url'], f'{OUT}/{name}_walk.glb')
if ba.get('running_glb_url'): urllib.request.urlretrieve(ba['running_glb_url'], f'{OUT}/{name}_run.glb')
print('saved', name)
