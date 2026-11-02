"""Native SI Jawbone regression, using an explicit item fixture after real opening."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'test_cli'))
from driver import BASE, Driver, EngineRefusal

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--run', required=True)
p.add_argument('--checkpoint', required=True)
p.add_argument('--mod', default='sifixes', choices=('', 'sifixes'))
a = p.parse_args()
d = Driver()
events = []
original_popen = subprocess.Popen

def launch(args, **kw):
    cfg = Path(args[args.index('-c')+1])
    tree = ET.parse(cfg)
    video = tree.getroot().find('video')
    for path, value in {'game/width':640, 'game/height':400, 'ui/width':640,
                        'ui/height':400, 'display/width':640, 'display/height':400, 'scale':1}.items():
        element = video
        for part in path.split('/'):
            child = element.find(part)
            if child is None:
                child = ET.SubElement(element, part)
            element = child
        element.text = str(value)
    tree.write(cfg, encoding='unicode')
    return original_popen(args, **kw)

def do(cmd, **kw):
    response = d.execute(dict(cmd=cmd, **kw))
    events.append(dict(command=dict(cmd=cmd, **kw), response=response))
    return response

def check(label, passed):
    d.assertions += 1
    d.failures += not passed
    events.append(dict(check=label, passed=bool(passed)))
    print(label, bool(passed), flush=True)
    if not passed:
        raise AssertionError(label)

def inspect(ref):
    return do('inspect_object', ref=ref)['object']

try:
    BASE.mkdir(parents=True, exist_ok=True)
    keys = BASE / (a.run + '.keys')
    keys.write_text('1 INVENTORY 0\n')
    subprocess.Popen = launch
    do('init', game='si', mod=a.mod, run=a.run, checkpoint=a.checkpoint, keys_path=str(keys))
    subprocess.Popen = original_popen
    # Reach legal player control through the real SI opening before loading.
    deadline = time.monotonic() + 90
    while time.monotonic() < deadline:
        state = d.state()
        if state['input']:
            do('choose', index=0) if state['choices_active'] else do('continue')
        elif state['si_first_scene'] and not state['dont_move'] and not state['in_usecode']:
            break
        time.sleep(.1)
    else:
        raise TimeoutError('natural SI opening did not return control')
    do('load', slot=0)
    before = d.state()['inventory']
    try:
        do('test_virtue_case')
        raise AssertionError('BG-only feature became available in SI')
    except EngineRefusal as error:
        check('BG case unavailable in SI without inventory mutation', 'fixture_requires_BG' in str(error)
              and d.state()['inventory'] == before)
    fixture = do('test_jawbone_items')
    jawbone, tooth, pack = (fixture[name]['ref'] for name in ('jawbone', 'tooth', 'backpack'))
    do('key', key='1')
    do('interact_inventory', ref=pack)
    do('interact_inventory', ref=jawbone)
    check('original specialized Jawbone gump opens', any('Jawbone_gump' in g['type'] for g in d.state()['gumps']))
    do('move_gump', ref=jawbone, x=400, y=30)
    do('move_gump', ref=pack, x=220, y=220)
    do('transfer_item', ref=tooth, destination=jawbone)
    check('real tooth insertion updates original jawbone', inspect(tooth)['owner'] == jawbone and inspect(jawbone)['frame'] == 1)
    do('screenshot')
    do('transfer_item', ref=tooth, destination=pack)
    removed = inspect(tooth)
    check('enlarged tooth drags out as the same ordinary object', removed['owner'] == pack and removed['shape'] == 559 and removed['frame'] == 0)
    check('jawbone returns to empty frame', inspect(jawbone)['frame'] == 0)
    do('save', slot=1)
    do('load', slot=1)
    items = do('inspect_inventory')['objects']
    check('ordinary SI save/load retains both real items', sum(o['shape'] == 555 for o in items) == 1
          and sum(o['shape'] == 559 for o in items) == 1)
    do('exit')
except Exception as error:
    events.append(dict(error=str(error)))
    d.failures += 1
    raise
finally:
    result = d.close()
    if d.run:
        (d.run / 'jawbone-checks.json').write_text(json.dumps(dict(events=events, result=result), indent=2))
    print(json.dumps(result), flush=True)
sys.exit(0 if result and result['result'] == 'PASS' else 1)
