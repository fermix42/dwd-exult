"""Exercise an unrelated native quantity slider through ordinary inventory dragging."""
import argparse
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'test_cli'))
from driver import Driver, EngineRefusal


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', required=True)
    args = parser.parse_args()
    driver = Driver()
    records = []

    def do(cmd, **kw):
        result = driver.execute({'cmd': cmd, **kw})
        records.append({'command': {'cmd': cmd, **kw}, 'response': result})
        return result

    def check(label, condition):
        driver.assertions += 1
        driver.failures += not condition
        print(label, bool(condition), flush=True)
        if not condition:
            raise AssertionError(label)

    def state():
        return do('state')['state']

    def slider():
        return next((g for g in state()['gumps'] if 'Slider_gump' in g['type']), None)

    def until(fn):
        end = time.monotonic() + 10
        while time.monotonic() < end:
            value = fn()
            if value:
                return value
            time.sleep(.05)
        raise TimeoutError('native slider transition')

    try:
        do('init', game='bg', run=args.run)
        inventory = do('inspect_inventory')['objects']
        stack = next(o for o in inventory if o['shape'] == 627 and o['quantity'] > 1)
        pack = next(o for o in inventory if o['ref'] == stack['owner'])
        world = do('inspect_world', radius=4, limit=100)
        do('interact_object', ref=world['avatar'])
        until(lambda: state()['gumps'])
        do('interact_inventory', ref=pack['ref'])
        until(lambda: len(state()['gumps']) >= 2)
        s = state()
        for dx, dy in ((7, 7), (-7, 7), (7, -7), (-7, -7)):
            try:
                do('drop_item', ref=stack['ref'], x=s['x']+dx, y=s['y']+dy, z=0)
                break
            except EngineRefusal:
                continue
        current = until(slider)
        check('unrelated quantity default/range preserved',
              (current['value'], current['minimum'], current['maximum'], current['increment']) ==
              (stack['quantity'], 0, stack['quantity'], 1))
        do('key', key='left')
        changed = until(lambda: slider() if slider()['value'] == current['value']-1 else None)
        check('native left arrow changes selection', changed['value'] == current['value']-1)
        do('key', key='right')
        until(lambda: slider()['value'] == current['value'])
        check('native right arrow restores selection', True)
        do('key', key='escape')
        until(lambda: not slider())
        after = do('inspect_inventory')['objects']
        check('native cancel preserves original stack', any(o['ref'] == stack['ref'] and o['quantity'] == stack['quantity'] for o in after))
        while state()['gumps']:
            do('dismiss')
        do('exit')
    except Exception:
        driver.failures += 1
        raise
    finally:
        result = driver.close()
        if driver.run:
            (driver.run / 'quantity-slider.json').write_text(json.dumps({'events': records, 'result': result}, indent=2))
        print(json.dumps(result), flush=True)
    return 0 if result and result['result'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
