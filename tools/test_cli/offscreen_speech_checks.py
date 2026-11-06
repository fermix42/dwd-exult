"""Verify world speech culling through the production renderer and camera fixtures."""
import argparse
import json
import shutil
import time
import traceback

from driver import Driver

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--run', required=True)
parser.add_argument('--game', choices=['bg', 'si'], default='bg')
parser.add_argument('--mod', default='')
args = parser.parse_args()
driver = Driver()
records = []


def do(cmd, **kw):
    response = driver.execute(dict(cmd=cmd, **kw))
    records.append(dict(command=dict(cmd=cmd, **kw), response=response))
    return response


def check(label, condition):
    driver.assertions += 1
    driver.failures += not bool(condition)
    records.append(dict(check=label, passed=bool(condition)))
    print(label, bool(condition), flush=True)
    if not condition:
        raise AssertionError(label)


def effect(ref, capture=None):
    # Flush the ordinary renderer; assertions use its semantic observations.
    do('screenshot', full=True)
    if capture:
        shutil.copy2(driver.run / 'screenshot.png', driver.run / (capture + '.png'))
    return next(e for e in do('text_effects')['effects'] if e['item'] == ref)


def intersects(a, b):
    return a['x'] < b['x'] + b['w'] and b['x'] < a['x'] + a['w'] and a['y'] < b['y'] + b['h'] and b['y'] < a['y'] + a['h']


try:
    do('init', game=args.game, mod=args.mod, run=args.run)
    if args.game == 'si':
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            state = do('state')['state']
            if state['input']:
                do('choose', index=0) if state['choices_active'] else do('continue')
            elif state['si_first_scene'] and not state['dont_move'] and not state['in_usecode'] and not state['moving']:
                break
            time.sleep(.05)
        else:
            raise TimeoutError('natural SI opening')
    ref = do('inspect_world', radius=2)['avatar']
    for edge in [8, 9, 10, 11]:
        before = do('inspect_actor', ref=ref)['object']['tile']
        do('test_overhead', ref=ref, scenario=0, edge=0)
        initial = effect(ref, 'visible' if edge == 8 else None)
        check('centered speaker renders ' + str(edge), initial['visible'] and intersects(initial['anchor'], initial['viewport']))
        do('test_overhead', ref=ref, scenario=-1, edge=edge)
        hidden = effect(ref, 'hidden-left' if edge == 8 else None)
        check('speaker fully offscreen ' + str(edge), not intersects(hidden['anchor'], hidden['viewport']))
        check('existing bark layer removed ' + str(edge), not hidden['visible'] and hidden['destination']['w'] == 0)
        check('hiding preserves text and captured duration ' + str(edge), hidden['text'] == initial['text'] and hidden['lifetime_ticks'] == initial['lifetime_ticks'] and hidden['elapsed_ticks'] >= initial['elapsed_ticks'])
        do('test_overhead', ref=ref, scenario=-1, edge=0)
        returned = effect(ref)
        check('same unexpired bark returns with speaker ' + str(edge), returned['visible'] and returned['text'] == initial['text'] and returned['elapsed_ticks'] >= hidden['elapsed_ticks'])
        do('test_overhead', ref=ref, scenario=0, edge=edge)
        fresh = effect(ref)
        check('new offscreen bark stays hidden ' + str(edge), not fresh['visible'] and fresh['destination']['w'] == 0)
        check('camera fixture preserves actor position ' + str(edge), do('inspect_actor', ref=ref)['object']['tile'] == before)
    for edge in [1, 2, 3, 4, 6, 7]:
        do('test_overhead', ref=ref, scenario=1, edge=edge)
        visible = effect(ref)
        check('edge speaker remains readable ' + str(edge), intersects(visible['anchor'], visible['viewport']) and visible['visible'] and visible['destination']['w'] > 0)
    for edge in [12, 13, 14, 15]:
        do('test_overhead', ref=ref, scenario=0, edge=edge)
        near = effect(ref)
        check('nearby offscreen caller remains readable ' + str(edge), not intersects(near['anchor'], near['viewport']) and near['visible'] and near['destination']['w'] > 0)
    do('test_overhead', ref=ref, scenario=0, edge=8)
    check('expiry starts with hidden layer', not effect(ref)['visible'])
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if not any(e['item'] == ref for e in do('text_effects')['effects']):
            break
        time.sleep(.05)
    else:
        raise TimeoutError('hidden speech did not expire')
    check('hidden speech expires through normal queue', not any(e['item'] == ref for e in do('text_effects')['effects']))
    do('test_overhead', ref=ref, scenario=-2)
    do('exit')
except Exception:
    driver.failures += 1
    records.append(dict(error=traceback.format_exc()))
    print(traceback.format_exc(), flush=True)
    try:
        do('exit')
    except Exception:
        pass
finally:
    if driver.run:
        (driver.run / 'offscreen-speech-checks.json').write_text(json.dumps(records, indent=2))
    result = driver.close()
    print(json.dumps(result), flush=True)
raise SystemExit(0 if result and result['result'] == 'PASS' else 1)
