"""Capture the real native case at explicit UI scales using a copied synthetic save.

Requires a checkpoint from checks.py, never a player save. No desktop automation.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import traceback
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'test_cli'))
from driver import BASE, Driver

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--run', required=True)
p.add_argument('--checkpoint', type=Path, required=True)
p.add_argument('--scale', type=int, choices=(1, 2, 3), default=1)
p.add_argument('--ultrawide', action='store_true')
p.add_argument('--mod', choices=('', 'Keyring'), default='')
a = p.parse_args()
d = Driver()
events = []
original_popen = subprocess.Popen

def launch(args, **kw):
    if '--test-cli' in args:
        cfg = Path(args[args.index('-c')+1])
        tree = ET.parse(cfg)
        video = tree.getroot().find('video')
        settings = {'display/width': 960 if a.ultrawide else 320*a.scale,
                    'display/height': 400 if a.ultrawide else 200*a.scale,
                    'game/width':320, 'game/height':200,
                    'ui/width':320, 'ui/height':200,
                    'ui/scale_method':'Point',
                    'ui/fill_mode':'Fit',
                    'ui/fill_scaler':'Point', 'scale':a.scale,
                    'scale_method':'Point', 'fill_mode':'Fit', 'fill_scaler':'Point'}
        for path, value in settings.items():
            elem = video
            for part in path.split('/'):
                child = elem.find(part)
                if child is None: child = ET.SubElement(elem, part)
                elem = child
            elem.text = str(value)
        tree.write(cfg, encoding='unicode')
    return original_popen(args, **kw)

def do(cmd, **kw):
    r = d.execute(dict(cmd=cmd, **kw))
    events.append(dict(command=dict(cmd=cmd, **kw), response=r))
    return r

def check(label, passed):
    d.assertions += 1
    if not passed: d.failures += 1
    events.append(dict(check=label, passed=bool(passed)))
    print(label, bool(passed), flush=True)
    if not passed: raise AssertionError(label)

def inventory(ref=0): return do('inspect_inventory', ref=ref)['objects']

def find_case(ref=0, depth=0):
    if depth > 6: return None
    for o in inventory(ref):
        if o['shape'] == 1601: return o['ref']
        if o['class_id'] == 6:
            found = find_case(o['ref'], depth+1)
            if found: return found
    return None

def show(ref, avatar):
    owner = do('inspect_object', ref=ref)['object']['owner']
    if owner and owner != avatar: show(owner, avatar)
    do('interact_inventory', ref=ref)
    if ref != case: do('move_gump', ref=ref, x=80, y=40)

try:
    keys = BASE/(a.run+'.keys'); keys.parent.mkdir(parents=True, exist_ok=True)
    keys.write_text('1 INVENTORY 0\n')
    subprocess.Popen = launch
    do('init', game='bg', mod=a.mod, run=a.run, keys_path=str(keys), checkpoint=str(a.checkpoint.resolve()))
    subprocess.Popen = original_popen
    do('load', slot=3)
    case = find_case()
    check('restored physical case located', bool(case))
    contents = inventory(case)
    check('eight real marked stones restored', len(contents)==8 and all(o['shape']==330 and o['stone_target']['x']==1000+o['frame'] for o in contents))
    avatar = do('inspect_world', radius=2)['avatar']
    do('key', key='1')
    show(case, avatar)
    gump = next(g for g in d.state()['gumps'] if 'Case_gump' in g['type'])
    events.append(dict(case_gump=gump))
    do('screenshot', full=True)
    shutil.copyfile(d.run/'screenshot.png', d.run/'case-engine.png')
    # Inspect actual rendered pixels; configuration alone does not prove scale.
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'data/bg/virtue_case'))
    from asset_tools import read_png
    width,height,mode,pixels,_,_ = read_png(d.run/'case-engine.png')
    factor = 2 if a.ultrawide else a.scale
    expected_size = (960,400) if a.ultrawide else (320*factor,200*factor)
    check('renderer capture has expected display dimensions', (width,height)==expected_size)
    channels = 4 if mode==6 else 3
    check('renderer capture is RGB/RGBA', mode in (2,6))
    left = 160 if a.ultrawide else 0
    exact = True
    for y in range(12,188):
        for x in range(12,308):
            start = ((y*factor)*width+left+x*factor)*channels
            pixel = pixels[start:start+channels]
            for dy in range(factor):
                for dx in range(factor):
                    pos = ((y*factor+dy)*width+left+x*factor+dx)*channels
                    if pixels[pos:pos+channels] != pixel: exact = False
    check('rendered case pixels are exact integer nearest-neighbor blocks', exact)
    controls = [c for c in do('controls')['controls'] if c['gump_id']==gump['id']]
    check('three physical controls exposed', {c['action'] for c in controls}=={'close','mark_unimplemented','recall_unimplemented'})
    before = {o['ref']:(o['frame'],o['stone_target']) for o in inventory(case)}
    for action in ('mark_unimplemented','recall_unimplemented'):
        do('activate_control', id=next(c['id'] for c in controls if c['action']==action))
        check(action+' preserves all stone identities/targets', {o['ref']:(o['frame'],o['stone_target']) for o in inventory(case)}==before)
    do('activate_control', id=next(c['id'] for c in controls if c['action']=='close'))
    check('brass catch closes specialized gump', not any('Case_gump' in g['type'] for g in d.state()['gumps']))
    show(case, avatar)
    check('case reopens with identical contents', {o['ref']:(o['frame'],o['stone_target']) for o in inventory(case)}==before)
    while d.state()['input_owner']=='gump': do('dismiss')
    do('exit')
except Exception:
    d.failures += 1
    events.append(dict(error=traceback.format_exc()))
    print(traceback.format_exc(), flush=True)
finally:
    subprocess.Popen = original_popen
    if d.run: (d.run/'presentation-checks.json').write_text(json.dumps(events, indent=2))
    result = d.close()
    print(json.dumps(result), flush=True)
sys.exit(0 if result and result['result']=='PASS' else 1)
