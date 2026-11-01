"""Focused real-engine chart checks. Item fixture is NOT acquisition evidence."""
import argparse
import json
from pathlib import Path
import shutil
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'test_cli'))
from driver import Driver, BASE

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--run', required=True)
p.add_argument('--mod', default='')
a = p.parse_args()
d = Driver()
records = []

def do(cmd, **kw):
    c = dict(cmd=cmd, **kw)
    r = d.execute(c)
    records.append(dict(command=c, response=r))
    return r

def state():
    return do('state')['state']

def check(label, value):
    d.assertions += 1
    d.failures += not value
    records.append(dict(check=label, passed=bool(value)))
    print(label, bool(value), flush=True)
    if not value:
        raise AssertionError(label)

def settled(owner):
    end = time.monotonic() + 4
    while time.monotonic() < end:
        s = state()
        if s['input_owner'] == owner:
            return s
        time.sleep(.03)
    raise TimeoutError(owner)

try:
    # A disposable binding file, leaving all player bindings untouched.
    BASE.mkdir(parents=True, exist_ok=True)
    keys = BASE / (a.run + '.keys')
    keys.write_text('w ORB_CHART\n1 INVENTORY 0\nShift-W WAIT\n')
    do('init', game='bg', mod=a.mod, run=a.run, keys_path=str(keys))
    before = state()
    check('effective supported layout', before['orb_chart_layout'] == (2 if a.mod else 1))
    do('save', slot=0)
    do('key', key='w')
    time.sleep(.05)
    check('hotkey requires physical chart', state()['input_owner'] == 'gameplay')
    do('test_orb_chart_item')
    s = state()
    check('fixture grants chart without Orb or plot changes',
          any(o['shape'] == 1600 for o in s['inventory'])
          and not any(o['shape'] == 785 for o in s['inventory'])
          and all(s[k] == before[k] for k in ('orb_given','orb_left_trinsic','orb_sphere_broken','orb_timelord_repaired')))
    do('save', slot=1)
    do('load', slot=1)
    check('ordinary item survives save/load', any(o['shape'] == 1600 for o in state()['inventory']))
    do('key', key='w')
    s = settled('modal')
    check('native modal pauses world', s['queue_pause_depth'] > 0)
    controls = do('controls')['controls']
    marks = [c for c in controls if c['action'].startswith(('near-','far-'))]
    check('all printed targets have native controls', len(marks) == (16 if a.mod else 8))
    do('screenshot')
    shutil.copyfile(d.run/'screenshot.png', d.run/('keyring-chart.png' if a.mod else 'bg-chart.png'))
    do('key', key='escape')
    s = settled('gameplay')
    check('Escape cancels without movement or leaked pause',
          s['queue_pause_depth'] == 0 and (s['x'],s['y'],s['map']) == (before['x'],before['y'],before['map']))
    do('key', key='w')
    settled('modal')
    cancel = next(c for c in do('controls')['controls'] if c['action'] == 'cancel')
    do('activate_control', id=cancel['id'])
    check('center mark closes chart', settled('gameplay')['queue_pause_depth'] == 0)
    do('key', key='w')
    settled('modal')
    mark = next(c for c in do('controls')['controls'] if c['action'] == 'near-northwest')
    do('activate_control', id=mark['id'])
    s = settled('gameplay')
    check('chart cannot travel without Orb', (s['x'],s['y'],s['map']) == (before['x'],before['y'],before['map']))
    do('key', key='1')
    settled('gump')
    inv = do('inspect_inventory')['objects']
    chart = next(o for o in inv if o['shape'] == 1600)
    check('physical item uses expected name', chart['name'] == "Lord British's Orb chart")
    owner = next((o for o in inv if o['ref'] == chart['owner']), None)
    check('chart is found recursively inside backpack', owner is not None and owner['shape'] == 801)
    do('interact_inventory', ref=owner['ref'])
    time.sleep(.05)
    do('interact_inventory', ref=chart['ref'])
    settled('modal')
    do('key', key='escape')
    settled('gump')
    do('screenshot')
    shutil.copyfile(d.run/'screenshot.png', d.run/'folded-chart-inventory.png')
    # Normal engine drop, while the item is visible in its inventory gump.
    s = state()
    inv = do('inspect_inventory')['objects']
    chart = next(o for o in inv if o['shape'] == 1600)
    dropped = False
    for dx,dy in [(x,y) for y in range(2,8,2) for x in range(-6,8,2)]:
        try:
            tile = do('inspect_tile',x=s['x']+dx,y=s['y']+dy,z=s['z'])
            if tile['blocked']: continue
            do('drop_item', ref=chart['ref'], x=s['x']+dx, y=s['y']+dy, z=s['z'])
        except Exception as error:
            records.append(dict(drop_refused=str(error)))
            continue
        time.sleep(.08)
        if not any(o['shape'] == 1600 for o in state()['inventory']):
            dropped = True
            break
    check('normal drag/drop actually removes chart from party', dropped)
    while state()['input_owner'] == 'gump':
        do('dismiss')
        time.sleep(.02)
    settled('gameplay')
    do('key', key='w')
    time.sleep(.05)
    check('dropped chart does not authorize hotkey', state()['input_owner'] == 'gameplay')
    do('save',slot=2)
    do('load',slot=2)
    ground=do('find_objects',shape=1600,radius=12,limit=50)['objects']
    check('dropped chart persists as an ordinary world item',
          not any(o['shape']==1600 for o in state()['inventory'])
          and len(ground)==1)
    do('interact_object',ref=ground[0]['ref'])
    check('reachable ground chart uses ordinary double-click reading',settled('modal')['input_owner']=='modal')
    do('key',key='escape')
    settled('gameplay')
    do('key',key='w')
    time.sleep(.05)
    check('reading on ground does not authorize inventory hotkey',state()['input_owner']=='gameplay')
    do('test_orb_chart_item')
    check('lost chart can be supplied without another Orb',
          sum(o['shape'] == 1600 for o in state()['inventory']) == 1
          and not any(o['shape'] == 785 for o in state()['inventory']))
    do('load', slot=0)
    check('pre-chart save loads without auto-grant', not any(o['shape'] == 1600 for o in state()['inventory']))
    do('key', key='w', shift=True)
    settled('modal')
    check('accepted Native Wait still opens', any(c['action'] == 'increase' for c in do('controls')['controls']))
    do('key', key='escape')
    settled('gameplay')
    do('exit')
except Exception as error:
    if not d.failures: d.failures += 1
    records.append(dict(error=str(error)))
    raise
finally:
    if d.p and d.p.poll() is None:
        try:
            if state()['input_owner'] == 'modal':
                do('key',key='escape')
                time.sleep(.05)
        except Exception:
            pass
    result = d.close()
    if d.run:
        (d.run/'chart-checks.json').write_text(json.dumps(dict(records=records,result=result),indent=2))
    print(json.dumps(result), flush=True)
if result['result'] != 'PASS':raise SystemExit(1)
