"""Smoke the consolidated menu's exact executable/config/checkpoints in-engine."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import traceback

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import Driver
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--root', type=Path, required=True)
p.add_argument('--prefix', required=True)
a = p.parse_args()
root = a.root.resolve()
old = subprocess.Popen
results = []

def launch(args, **kw):
    if '--test-cli' in args:
        args = list(args)
        args[0] = str(root/'Exult.exe')
        args[args.index('-c')+1] = str(root/'exult.cfg')
        args[args.index('--test-run-dir')+1] = str(root)
    return old(args, **kw)

for profile in ['BG', 'Keyring']:
    for state in ['a-iolo','b-dupre','ready','full-case']:
        subprocess.run([sys.executable, str(root/'start_integration_review.py'), '--profile', profile,
                        '--state', state, '--prepare-only'], check=True)
        d = Driver()
        records = []
        def do(cmd, **kw):
            result = d.execute(dict(cmd=cmd, **kw))
            records.append(dict(command=cmd, response=result))
            return result
        def check(label, value):
            d.assertions += 1
            d.failures += not bool(value)
            records.append(dict(check=label, passed=bool(value)))
            if not value:
                raise AssertionError(label)
        try:
            subprocess.Popen = launch
            do('init', game='bg', mod='Keyring' if profile=='Keyring' else '',
               run=a.prefix+'-'+profile.lower()+'-'+state)
            at = d.state()
            check('launcher uses private current-state paths',
                  Path(at['gamedat_path']).resolve().is_relative_to(root) and
                  Path(at['save_path']).resolve().is_relative_to(root))
            check('new private global preference is Normal', at['overhead_text_duration']==100)
            quest = do('case_commission_state')['commission']
            if state == 'ready':
                check('earned ready checkpoint has no automatic reward', quest['stage']==4 and
                      not do('find_objects', shape=1601, radius=32)['objects'])
            elif state in ['a-iolo','b-dupre']:
                q = do('case_breadcrumbs_state')['breadcrumbs']
                check('natural alternative starts unspoken', not q['a_done'] and not q['b_done'])
                check('correct existing inventory eligibility', q['a_pending'] if state=='a-iolo' else q['b_pending'])
            else:
                def contents(ref=0):
                    objects = do('inspect_inventory', ref=ref)['objects']
                    for obj in objects:
                        yield obj
                        if obj['class_id']==6:
                            yield from contents(obj['ref'])
                case = next(obj for obj in contents() if obj['shape']==1601)
                check('full-case fixture has real eight original stones',
                      len(do('inspect_inventory', ref=case['ref'])['objects'])==8)
                check('full-case fixture has ordinary spellbook', any(obj['shape']==761 for obj in contents()))
            do('exit')
        except Exception:
            d.failures += 1
            records.append(dict(error=traceback.format_exc()))
            print(traceback.format_exc(), flush=True)
        finally:
            subprocess.Popen = old
            if d.run:
                (d.run/'review-checks.json').write_text(json.dumps(records, indent=2))
            result = d.close()
            results.append(dict(profile=profile, state=state, **result))
            print(profile, state, json.dumps(result), flush=True)
        if result['result'] != 'PASS':
            break
    if results[-1]['result'] != 'PASS':
        break
(root.parent/(a.prefix+'-summary.json')).write_text(json.dumps(results, indent=2))
raise SystemExit(0 if all(row['result']=='PASS' for row in results) else 1)
