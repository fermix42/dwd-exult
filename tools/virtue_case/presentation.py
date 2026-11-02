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
p.add_argument('--labels', action='store_true', help='Explicit synthetic two-line label visual trial')
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
    if a.labels:
        do('screenshot', full=True)
        shutil.copyfile(d.run/'screenshot.png', d.run/'case-engine-blank.png')
        do('test_virtue_case_labels')
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
    manifest_path = Path(__file__).resolve().parents[2]/'data/bg/virtue_case/asset_manifest.json'
    canvas = json.loads(manifest_path.read_text())['canvas']
    check('case centered at declared native canvas size', (gump['x'],gump['y'])==((320-canvas[0])//2,(200-canvas[1])//2))
    # The existing renderer uses integer midpoint coordinates, with4px layer
    # padding. Odd-sized canvases therefore have a half-pixel-origin offset.
    ox = left + int((gump['x']+canvas[0]//2)*factor-(canvas[0]+8)*factor/2)+4*factor
    oy = int((gump['y']+canvas[1]//2)*factor-(canvas[1]+8)*factor/2)+4*factor
    _,_,_,background,_,_ = read_png(manifest_path.parent/'open_case.png')
    exact = True
    for y in range(8,canvas[1]-8):
        for x in range(8,canvas[0]-8):
            if background[y*canvas[0]+x]==255: continue
            start = ((oy+y*factor)*width+ox+x*factor)*channels
            pixel = pixels[start:start+channels]
            for dy in range(factor):
                for dx in range(factor):
                    pos = ((oy+y*factor+dy)*width+ox+x*factor+dx)*channels
                    if pixels[pos:pos+channels] != pixel: exact = False
    check('rendered case pixels are exact integer nearest-neighbor blocks', exact)
    if a.labels:
        bw,bh,bmode,blank,_,_ = read_png(d.run/'case-engine-blank.png')
        check('blank comparison frame matches renderer format', (bw,bh,bmode)==(width,height,mode))
        for slot in json.loads(manifest_path.read_text())['slots']:
            tx,ty,tw,th = slot['label_text_safe_rect']
            ink_changes = 0
            for py in range(oy+ty*factor,oy+(ty+th)*factor):
                for px in range(ox+tx*factor,ox+(tx+tw)*factor):
                    pos=(py*width+px)*channels
                    if pixels[pos:pos+3] == bytes([0,0,0]) and blank[pos:pos+3] != pixels[pos:pos+3]: ink_changes += 1
            check('native text visibly painted in '+slot['virtue']+' label', ink_changes>=10*factor*factor)

    controls = [c for c in do('controls')['controls'] if c['gump_id']==gump['id']]
    check('three physical controls exposed', {c['action'] for c in controls}=={'close','mark','recall'} | {'edit_label_'+str(i) for i in range(8)})
    close_control = next(c for c in controls if c['action']=='close')
    check('close control is on four-grommet leather side tab', gump['x']+3 <= close_control['x'] <= gump['x']+23 and gump['y']+47 <= close_control['y'] <= gump['y']+69)
    check('bottom catch is decorative', do('control_at', x=gump['x']+100, y=gump['y']+124)['action']!='close')

    before = {o['ref']:(o['frame'],o['stone_target']) for o in inventory(case)}
    do('activate_control', id=next(c['id'] for c in controls if c['action']=='close'))
    check('side-tab checkmark closes specialized gump', not any('Case_gump' in g['type'] for g in d.state()['gumps']))
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
