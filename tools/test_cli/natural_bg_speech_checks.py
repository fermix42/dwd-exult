"""Observe real BG opening speech and Finnigan's approach without fast-start flags."""
import argparse
import json
import subprocess
import time
import traceback
import xml.etree.ElementTree as ET
from pathlib import Path

from driver import Driver

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--run', required=True)
args = parser.parse_args()
original = subprocess.Popen


def launch(command, **kw):
    if '--test-cli' in command:
        cfg = Path(command[command.index('-c') + 1])
        tree = ET.parse(cfg)
        tree.getroot().find('gameplay/skip_intro').text = 'no'
        tree.write(cfg, encoding='unicode')
    return original(command, **kw)


subprocess.Popen = launch
driver = Driver()
records = []


def do(cmd, **kw):
    result = driver.execute(dict(cmd=cmd, **kw))
    records.append(dict(command=dict(cmd=cmd, **kw), response=result))
    return result


def check(label, value):
    driver.assertions += 1
    driver.failures += not bool(value)
    print(label, bool(value), flush=True)
    records.append(dict(check=label, passed=bool(value)))
    if not value:
        raise AssertionError(label)


def overlaps(a, b):
    return a['x'] < b['x'] + b['w'] and b['x'] < a['x'] + a['w'] and a['y'] < b['y'] + b['h'] and b['y'] < a['y'] + a['h']


try:
    do('init', game='bg', run=args.run)
    check('natural opening enabled', ET.parse(driver.run / 'exult.cfg').getroot().findtext('gameplay/skip_intro') == 'no')
    actors = do('find_actors', radius=32, limit=200)['objects']
    mayor = next(o for o in actors if o['npc'] == 12)
    initial = mayor['tile']
    intro = False
    mayor_dialogue = False
    offscreen_calls = []
    directed_fixture = False
    deadline = time.monotonic() + 100
    while time.monotonic() < deadline:
        state = do('state')['state']
        do('screenshot', full=True)
        for e in do('text_effects')['effects']:
            if e['item'] == mayor['ref'] and e['width'] > 0:
                print('natural mayor bark', json.dumps(e), flush=True)
                check('natural mayor bark remains visible', e['visible'])
                if not overlaps(e['anchor'], e['viewport']):
                    offscreen_calls.append(e)
        text = state['dialogue_text']
        intro |= 'Welcome to Trinsic' in text or 'standing there in the first place' in text
        mayor_dialogue |= 'Town Mayor' in text or 'Welcome, Avatar' in text or 'Hast thou searched the stables' in text
        actor = do('inspect_actor', ref=mayor['ref'])['object']
        # When the unmodified opening gives Finnigan his talk schedule, test its
        # own renderer allowance with camera/sample speech only, without moving him.
        if intro and not directed_fixture and state['can_act'] and not state['input'] and not state['in_usecode'] and actor['schedule'] == 3:
            do('test_overhead', ref=mayor['ref'], scenario=0, edge=8)
            do('screenshot', full=True)
            e = next(e for e in do('text_effects')['effects'] if e['item'] == mayor['ref'])
            check('actual talk-schedule caller allowed beyond ambient margin', not overlaps(e['anchor'], e['viewport']) and e['visible'])
            do('test_overhead', ref=mayor['ref'], scenario=-2)
            directed_fixture = True
        if state['input']:
            if state['choices_active']:
                choices = state['choices']
                choice = next((s for s in ['yes', 'bye'] if s in choices), choices[0])
                do('choose', text=choice)
            else:
                do('continue')
        elif intro and mayor_dialogue and not state['in_usecode'] and not state['faces'] and state['can_act']:
            break
        time.sleep(.02)
    else:
        raise TimeoutError('natural opening and mayor dialogue did not complete')
    check('actual opening dialogue observed', intro)
    check('mayor approached into real conversation', mayor_dialogue and do('inspect_actor', ref=mayor['ref'])['object']['tile'] != initial)
    print('natural off-screen mayor observations', len(offscreen_calls), 'directed renderer check', directed_fixture, flush=True)
    check('offscreen directed summons verified', bool(offscreen_calls) or directed_fixture)
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
    subprocess.Popen = original
    if driver.run:
        (driver.run / 'natural-bg-speech-checks.json').write_text(json.dumps(records, indent=2))
    result = driver.close()
    print(json.dumps(result), flush=True)
raise SystemExit(0 if result and result['result'] == 'PASS' else 1)
