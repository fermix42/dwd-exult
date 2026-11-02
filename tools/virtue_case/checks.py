"""Real engine Stage A physical-object checks with explicitly synthetic fixtures.

No quest, spellcasting or natural acquisition is certified by these tests.
"""
import argparse
import json
from pathlib import Path
import shutil
import sys
import traceback
import subprocess
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'test_cli'))
from driver import BASE, Driver

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--run', required=True)
p.add_argument('--mod', default='', choices=('', 'Keyring'))
a = p.parse_args()
d = Driver()
records = []

# Existing presentation-test pattern: edit only the fresh config before launch.
# A 640x400 logical viewport keeps both native container gumps visible at 1x.
original_popen = subprocess.Popen
def launch(args, **kw):
    if '--test-cli' in args:
        cfg = Path(args[args.index('-c')+1])
        tree = ET.parse(cfg)
        video = tree.getroot().find('video')
        for path, value in {'display/width':640, 'display/height':400,
                            'game/width':640, 'game/height':400,
                            'ui/width':640, 'ui/height':400,
                            'ui/scale_method':'Point', 'ui/fill_mode':'Fit',
                            'ui/fill_scaler':'Point', 'scale':1, 'scale_method':'Point'}.items():
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
    command = dict(cmd=cmd, **kw)
    response = d.execute(command)
    records.append(dict(command=command, response=response))
    return response


def check(label, value):
    d.assertions += 1
    d.failures += not value
    records.append(dict(check=label, passed=bool(value)))
    print(label, bool(value), flush=True)
    if not value:
        raise AssertionError(label)


def inspect(ref):
    return do('inspect_object', ref=ref)['object']


def inventory(ref=0):
    return do('inspect_inventory', ref=ref)['objects']


def capture(name):
    do('screenshot')
    shutil.copyfile(d.run / 'screenshot.png', d.run / (name + '.png'))


def show_party_item(item, avatar):
    owner = inspect(item)['owner']
    if owner != avatar:
        show_party_item(owner, avatar)
    do('interact_inventory', ref=item)


def close_gumps():
    while d.state()['input_owner'] == 'gump':
        do('dismiss')


def open_pair(case, bag, case_open=True):
    close_gumps()
    avatar = do('inspect_world', radius=2)['avatar']
    do('key', key='1')
    show_party_item(bag, avatar)
    if case_open:
        show_party_item(case, avatar)
        do('move_gump', ref=case, x=310, y=5)
    else:
        owner = inspect(case)['owner']
        if owner != avatar:
            show_party_item(owner, avatar)
    do('move_gump', ref=bag, x=220, y=220)
    do('move_gump', ref=bag, x=220, y=220)
    gumps = d.state()['gumps']
    check('repeat positioning retains one entry per real gump', len({g['id'] for g in gumps}) == len(gumps))


try:
    subprocess.Popen = launch
    keys = BASE / (a.run + '.keys')
    keys.parent.mkdir(parents=True, exist_ok=True)
    keys.write_text('1 INVENTORY 0\n')
    do('init', game='bg', mod=a.mod, run=a.run, keys_path=str(keys))
    subprocess.Popen = original_popen
    check('effective BG/Keyring profile matches selected game', d.state()['orb_chart_layout'] == (2 if a.mod else 1))
    fixture = do('test_virtue_case')
    case, bag = fixture['case']['ref'], fixture['source_bag']['ref']
    check('real native container and empty initial contents', inspect(case)['class_id'] == 6 and not inventory(case))
    stones = [o for o in inventory(bag) if o['shape'] == 330]
    check('fixture supplies nine original pre-marked stones', len(stones) == 9 and all(o['class_id'] == 11 for o in stones))
    originals = {}
    duplicate = None
    for stone in stones:
        if stone['frame'] in originals:
            duplicate = stone
        else:
            originals[stone['frame']] = stone
    open_pair(case, bag)
    capture('empty-case')
    first = originals[2]
    do('transfer_item', ref=first['ref'], destination=case)
    seated = inspect(first['ref'])
    check('one-stone insertion retains same object and marking', seated['owner'] == case
          and seated['frame'] == first['frame'] and seated['stone_target'] == first['stone_target']
          and seated['stone_target_map'] == first['stone_target_map'])
    capture('one-stone')
    do('transfer_item', ref=first['ref'], destination=bag)
    removed = inspect(first['ref'])
    check('one-stone drag out retains original sprite and marking', removed['owner'] == bag
          and removed['shape'] == 330 and removed['frame'] == first['frame']
          and removed['stone_target'] == first['stone_target'] and removed['stone_target_map'] == first['stone_target_map']
          and not inventory(case))
    for frame, original in originals.items():
        do('transfer_item', ref=original['ref'], destination=case)
        observed = inspect(original['ref'])
        check(f'frame {frame} insertion retains identity and target', observed['owner'] == case
              and observed['shape'] == 330 and observed['stone_target'] == original['stone_target']
              and observed['stone_target_map'] == original['stone_target_map'])
    capture('eight-stones')
    slots = {2: (44,52), 0: (44,70), 1: (44,88), 4: (44,106),
             3: (115,52), 5: (115,70), 7: (115,88), 6: (115,106)}
    contents = inventory(case)
    check('all eight fixed compartments occupied correctly', len(contents) == 8
          and all((o['gump_tx'], o['gump_ty']) == slots[o['frame']] for o in contents))
    for px, py in [(20,5), (98,31), (178,120), (20,80), (179,80), (115,106)]:
        do('transfer_item', ref=first['ref'], destination=bag)
        do('transfer_item', ref=first['ref'], destination=case, x=px, y=py)
        check(f'drop at case local {px},{py} selects Honesty', inspect(first['ref'])['owner'] == case
              and (inspect(first['ref'])['gump_tx'], inspect(first['ref'])['gump_ty']) == slots[2])
    do('transfer_item', ref=first['ref'], destination=bag)
    open_pair(case, bag, case_open=False)
    do('transfer_item', ref=first['ref'], destination=case)
    check('closed-case insertion uses the same real object', inspect(first['ref'])['owner'] == case)
    open_pair(case, bag)
    do('transfer_item', ref=duplicate['ref'], destination=case)
    check('duplicate rejected with original object intact', inspect(duplicate['ref'])['owner'] == bag
          and len(inventory(case)) == 8)
    gold = next(o for o in inventory(bag) if o['shape'] == 644)
    do('transfer_item', ref=gold['ref'], destination=case)
    check('non-stone rejected safely', inspect(gold['ref'])['owner'] == bag and len(inventory(case)) == 8)
    for frame, original in originals.items():
        do('transfer_item', ref=original['ref'], destination=bag)
        observed = inspect(original['ref'])
        check(f'frame {frame} drag out preserves real stone', observed['owner'] == bag
              and observed['shape'] == 330 and observed['frame'] == frame
              and observed['stone_target'] == original['stone_target']
              and observed['stone_target_map'] == original['stone_target_map'])
    check('case empty after all eight removals', not inventory(case))
    do('save', slot=0)
    do('load', slot=0)
    root_items = inventory()
    case = next(o['ref'] for o in root_items if o['shape'] == 1601)
    bag = next(o['ref'] for o in root_items if o['shape'] == 801 and len(inventory(o['ref'])) == 10)
    check('case and ordinary bag survive save/load', not inventory(case) and len(inventory(bag)) == 10)
    check('stone destinations survive save/load', sorted((o['frame'], tuple(o['stone_target'].values()))
          for o in inventory(bag) if o['shape'] == 330) == sorted((o['frame'], tuple(o['stone_target'].values())) for o in stones))
    open_pair(case, bag)
    # Ordinary container drops may stack at the same point. Remove the topmost
    # (last painted) objects first; never bypass the native visibility check.
    for o in reversed(inventory(bag)):
        if o['shape'] == 330 and o['stone_target']['x'] < 1008:
            do('transfer_item', ref=o['ref'], destination=case)
    check('all stones can be reinserted after reload', len(inventory(case)) == 8)
    capture('reloaded-case')
    do('save', slot=1)
    do('load', slot=1)
    case = next(o['ref'] for o in inventory() if o['shape'] == 1601)
    check('loaded case retains all eight marked stones', len(inventory(case)) == 8
          and all(o['stone_target']['x'] == 1000 + o['frame'] and o['stone_target_map'] == o['frame'] % 2 for o in inventory(case)))
    bag = next(o['ref'] for o in inventory() if o['shape'] == 801 and len(inventory(o['ref'])) == 2)
    home_pack = inspect(case)['owner']
    before_transfer = {o['ref']: (o['frame'], o['stone_target']) for o in inventory(case)}
    open_pair(case, bag)
    do('transfer_item', ref=case, destination=bag)
    check('filled case moves into another ordinary container', inspect(case)['owner'] == bag
          and {o['ref']: (o['frame'], o['stone_target']) for o in inventory(case)} == before_transfer)
    do('transfer_item', ref=case, destination=home_pack)
    check('filled case moves back with identical contents', inspect(case)['owner'] == home_pack
          and {o['ref']: (o['frame'], o['stone_target']) for o in inventory(case)} == before_transfer)
    close_gumps()
    avatar = do('inspect_world', radius=2)['avatar']
    do('key', key='1')
    show_party_item(home_pack, avatar)
    do('move_gump', ref=home_pack, x=30, y=220)
    do('move_gump', ref=avatar, x=30, y=30)
    state = d.state()
    dropped = False
    for ox, oy in [(3,2),(4,2),(5,2),(-3,2),(3,4),(-4,4)]:
        try:
            target = dict(x=state['x']+ox, y=state['y']+oy, z=state['z'])
            if do('inspect_tile', **target)['blocked']:
                continue
            do('drop_item', ref=case, **target)
            if inspect(case)['owner'] == 0:
                dropped = True
                break
        except Exception as error:
            records.append(dict(drop_refused=str(error)))
    check('filled case drops into the real world', dropped)
    check('world placement retains each original stone object', all(inspect(ref)['owner'] == case for ref in before_transfer))
    close_gumps()
    do('save', slot=2)
    do('load', slot=2)
    ground = do('find_objects', shape=1601, radius=12)['objects']
    check('world case survives save/load', len(ground) == 1 and ground[0]['owner'] == 0)
    case = ground[0]['ref']
    do('interact_object', ref=case)
    check('world double-click opens specialized gump', any('Case_gump' in g['type'] for g in d.state()['gumps']))
    capture('world-case-eight-stones')
    close_gumps()
    avatar = do('inspect_world', radius=2)['avatar']
    do('key', key='1')
    bag = next(o['ref'] for o in inventory() if o['shape'] == 801 and len(inventory(o['ref'])) == 2)
    show_party_item(bag, avatar)
    do('move_gump', ref=bag, x=220, y=220)
    do('transfer_item', ref=case, destination=bag)
    check('world case picks up normally into ordinary container', inspect(case)['owner'] == bag and len(inventory(case)) == 8)
    check('all destinations survive world storage and pickup', all(o['stone_target']['x'] == 1000+o['frame']
          and o['stone_target_map'] == o['frame'] % 2 for o in inventory(case)))
    do('save', slot=3)
    open_pair(case, bag)
    capture('manual-review-case')
    close_gumps()
    do('exit')
except Exception:
    records.append(dict(error=traceback.format_exc()))
    d.failures += 1
    print(traceback.format_exc(), flush=True)
    if d.p and d.p.poll() is None:
        try:
            capture('failure')
        except Exception:
            pass
finally:
    if d.run:
        (d.run / 'case-checks.json').write_text(json.dumps(records, indent=2))
    result = d.close()
    print(json.dumps(result), flush=True)
sys.exit(0 if result and result['result'] == 'PASS' else 1)
