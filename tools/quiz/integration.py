"""Verify effective built-in helpers and untouched mod overrides in the real engine."""
import argparse
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'test_cli'))
from driver import Driver
from verify import embedded, functions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', choices=('bg', 'si'), required=True)
    parser.add_argument('--mod', default='')
    parser.add_argument('--header', required=True)
    parser.add_argument('--run', required=True)
    args = parser.parse_args()
    driver = Driver()
    records = []

    def check(label, condition, evidence=None):
        driver.assertions += 1
        driver.failures += not condition
        records.append({'check': label, 'pass': bool(condition), 'evidence': evidence})
        print(label, bool(condition), flush=True)
        if not condition:
            raise AssertionError(label)

    def state():
        return driver.execute({'cmd': 'state'})['state']

    try:
        init = driver.execute({'cmd': 'init', 'game': args.game, 'mod': args.mod, 'run': args.run})
        records.append({'init': init})
        expected = embedded(args.header, args.game)

        def verify_helpers():
            for number, (body, _) in expected.items():
                live = driver.execute({'cmd': 'inspect_usecode', 'function': number})
                check('effective quiz ' + hex(number), bytes.fromhex(live['bytes']) == body and live['has_original'] == 1)

        verify_helpers()
        basepath = Path(next(r['path'] for r in init['resources'] if 'static' in r['path'].lower()))
        original = functions(basepath.read_bytes())
        mod_functions = {}
        s = state()
        check('headless native engine', s['video'] == 'dummy' and s['studio_listener_active'] == 0)
        if args.mod:
            modpath = next(r['path'] for r in init['resources'] if args.mod in r['path'] and Path(r['path']).name == 'usecode')
            overrides = functions(Path(modpath).read_bytes())
            mod_functions = overrides
            for number, (body, _) in overrides.items():
                live = driver.execute({'cmd': 'inspect_usecode', 'function': number})
                if bytes.fromhex(live['bytes']) != body:
                    raise AssertionError('mod override changed: ' + hex(number))
            check('all installed mod functions retain authority', True, {'functions': len(overrides)})
        for number in ((0x622, 0x84b) if args.game == 'bg' else (0x622, 0x821)):
            live = driver.execute({'cmd': 'inspect_usecode', 'function': number})
            expected_body = mod_functions.get(number, original[number])[0]
            check('unrelated numeric helper unchanged ' + hex(number), bytes.fromhex(live['bytes']) == expected_body)
        # SI must finish its real opening dialogue before ordinary save/reload.
        if args.game == 'si':
            deadline = time.monotonic() + 100
            pages = 0
            while time.monotonic() < deadline:
                s = state()
                if s['input']:
                    driver.execute({'cmd': 'choose', 'index': 0} if s['choices_active'] else {'cmd': 'continue'})
                    pages += 1
                elif s['si_first_scene'] and not s['dont_move'] and not s['in_usecode'] and not s['moving']:
                    break
                time.sleep(.05)
            else:
                raise TimeoutError('natural SI opening')
            check('natural opening completed', pages > 0 and s['si_first_scene'] and not s['dont_move'])
            # The real Iolo conversation exercises SI Fixes' NPC override.
            actors = driver.execute({'cmd': 'find_actors', 'radius': 20, 'limit': 100})['objects']
            iolo = next(actor for actor in actors if actor['npc'] == 3)
            driver.execute({'cmd': 'interact_object', 'ref': iolo['ref']})
            deadline = time.monotonic() + 30
            answered = False
            while time.monotonic() < deadline:
                s = state()
                if s['input']:
                    if s['choices_active']:
                        driver.execute({'cmd': 'choose', 'text': 'bye' if 'bye' in s['choices'] else s['choices'][0]})
                        answered = True
                    else:
                        driver.execute({'cmd': 'continue'})
                elif answered and not s['in_usecode'] and not s['faces']:
                    break
                time.sleep(.05)
            check('real companion conversation returned', answered and not s['in_usecode'] and not s['faces'])
        driver.execute({'cmd': 'wait', 'until': {'moving': 0, 'input': 0, 'in_usecode': 0, 'dont_move': 0}, 'timeout': 15})
        before = state()
        driver.execute({'cmd': 'save', 'slot': 0})
        driver.execute({'cmd': 'load', 'slot': 0})
        after = state()
        check('save/reload preserves player state', all(before[k] == after[k] for k in ('x', 'y', 'z', 'map', 'health', 'dont_move')))
        verify_helpers()
        driver.execute({'cmd': 'exit'})
    except Exception:
        driver.failures += 1
        raise
    finally:
        result = driver.close()
        if driver.run:
            (driver.run / 'quiz-integration.json').write_text(json.dumps({'checks': records, 'result': result}, indent=2))
        print(json.dumps(result), flush=True)
    return 0 if result and result['result'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
