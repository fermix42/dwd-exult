"""Fresh ordinary BG/Keyring/SI starts must not automatically receive a case."""
import argparse
import json
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import Driver
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',required=True)
p.add_argument('--game',choices=['bg','si'],default='bg');p.add_argument('--mod',default='');a=p.parse_args()
d=Driver();events=[]
def do(cmd,**kw):
    result=d.execute(dict(cmd=cmd,**kw));events.append(dict(command=cmd,response=result));return result
def check(label,value):
    d.assertions+=1;d.failures+=not bool(value);events.append(dict(check=label,passed=bool(value)))
    if not value:raise AssertionError(label)
try:
    do('init',game=a.game,mod=a.mod,run=a.run)
    def possessions(ref=0):
        for obj in do('inspect_inventory',ref=ref)['objects']:
            yield obj
            if obj['class_id']==6:yield from possessions(obj['ref'])
    check('no case automatically added to initial inventory',not any(o['shape']==1601 for o in possessions()))
    check('no automatic nearby world reward',not do('find_objects',shape=1601,radius=32)['objects'])
    check('commission starts unawarded',do('case_commission_state')['commission']['stage']==0)
    check('new profile retains Normal speech default',d.state()['overhead_text_duration']==100)
    do('exit')
finally:
    if d.run:(d.run/'profile-presence.json').write_text(json.dumps(events,indent=2))
    result=d.close();print(json.dumps(result),flush=True)
raise SystemExit(0 if result['result']=='PASS'else 1)
