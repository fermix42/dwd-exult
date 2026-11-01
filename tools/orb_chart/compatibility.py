"""Inspect effective Orb bytecode and verify safe refusal of an unknown override."""
import argparse
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'test_cli'))
from driver import Driver, BASE
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'quiz'))
from verify import functions

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--bg-usecode', required=True)
p.add_argument('--run', required=True)
p.add_argument('--checkpoint')
p.add_argument('--keyring-usecode',help='Check a modified Keyring outer-container helper instead of the Orb literal')
a=p.parse_args()
BASE.mkdir(parents=True,exist_ok=True)
patch=BASE/(a.run+'-patch')
patch.mkdir(exist_ok=False)
effective=functions(Path(a.bg_usecode).read_bytes())
number=0x311
if a.keyring_usecode:
    effective.update(functions(Path(a.keyring_usecode).read_bytes()))
    number=0x945
    unknown=effective[number][0]+bytes([0x32]) # Unreachable RETZ; original behavior preserved.
    entries={n:effective[n] for n in (0x311,0x821,0x822,0x823,0x824,0x825,0x826,0x93e,0x945)}
    entries[number]=(unknown,False)
else:
    body,extended=effective[number]
    assert not extended and b'@How odd!@' in body
    unknown=body.replace(b'@How odd!@',b'@How odd?@')
    entries={number:(unknown,False)}
assert all(not extended for body,extended in entries.values())
(patch/'usecode').write_bytes(b''.join(n.to_bytes(2,'little')+len(body).to_bytes(2,'little')+body for n,(body,_) in sorted(entries.items())))
keys=patch/'keys.txt'
keys.write_text('w ORB_CHART\n1 INVENTORY 0\n')
d=Driver()
records=[]
def check(label,value):
    d.assertions+=1
    d.failures+=not value
    records.append(dict(check=label,passed=bool(value)))
    print(label,bool(value),flush=True)
    if not value:raise AssertionError(label)
try:
    init=d.execute(dict(cmd='init',game='bg',run=a.run,patch_path=str(patch),keys_path=str(keys),**({'checkpoint':a.checkpoint} if a.checkpoint else {})))
    if a.checkpoint:d.execute(dict(cmd='load',slot=int(Path(a.checkpoint).stem[5:7])))
    records.append(dict(init=init))
    s=d.execute(dict(cmd='state'))['state']
    check('unknown effective Orb is not assigned a layout',s['orb_chart_layout']==0)
    check('override retains its own complete bytecode',bytes.fromhex(d.execute(dict(cmd='inspect_usecode',function=number))['bytes'])==unknown)
    d.execute(dict(cmd='test_orb_chart_item'))
    d.execute(dict(cmd='key',key='w'))
    time.sleep(.1)
    check('unsupported implementation cannot open selector',d.execute(dict(cmd='state'))['state']['input_owner']=='gameplay')
    check('chart attempt does not replace active Orb function',bytes.fromhex(d.execute(dict(cmd='inspect_usecode',function=number))['bytes'])==unknown)
    if a.checkpoint:
        d.execute(dict(cmd='key',key='1'))
        time.sleep(.05)
        inventory=d.execute(dict(cmd='inspect_inventory'))['objects']
        orb=next(o for o in inventory if o['shape']==785)
        owner=next((o for o in inventory if o['ref']==orb['owner']),None)
        if owner and owner['type']=='container':d.execute(dict(cmd='interact_inventory',ref=owner['ref']))
        d.execute(dict(cmd='interact_inventory',ref=orb['ref']))
        s=d.execute(dict(cmd='state'))['state']
        check('unknown Orb retains its real manual target prompt',s['input_owner']=='target')
        d.execute(dict(cmd='key',key='escape'))
        time.sleep(.1)
        check('manual override leaves no chart intercept',not d.execute(dict(cmd='state'))['state']['target_intercept_pending'])
        while d.execute(dict(cmd='state'))['state']['input_owner']=='gump':d.execute(dict(cmd='dismiss'))
    d.execute(dict(cmd='exit'))
except Exception as error:
    if not d.failures: d.failures += 1
    records.append(dict(error=str(error)))
    raise
finally:
    result=d.close()
    if d.run:(d.run/'compatibility-checks.json').write_text(json.dumps(dict(records=records,result=result),indent=2))
    print(json.dumps(result),flush=True)
if result['result'] != 'PASS':raise SystemExit(1)
