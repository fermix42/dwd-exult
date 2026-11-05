"""Bounded real-world construction travel/bed probe from an earned building save.

Unsupported navigation/bed access is recorded NOT TESTED, never fabricated.
No clock assignment, actor teleport, quest flags or NPC schedule overrides.
"""
import argparse
import json
from pathlib import Path
import sys
import time
import traceback
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import Driver, EngineRefusal
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--run',required=True)
p.add_argument('--checkpoint',type=Path,required=True)
p.add_argument('--mod',default='')
p.add_argument('--skip-travel',action='store_true')
a=p.parse_args();d=Driver();events=[];coverage={}
def do(cmd,**kw):
    result=d.execute(dict(cmd=cmd,**kw));events.append(dict(command=cmd,args=kw,response=result));return result
def check(label,value):
    d.assertions+=1;d.failures+=not bool(value);events.append(dict(check=label,passed=bool(value)))
    if not value:raise AssertionError(label)
def q():return do('case_commission_state')['commission']
def walk(target,seconds=100):
    end=time.monotonic()+seconds;visited={}
    while time.monotonic()<end:
        at=d.state();distance=max(abs(at['x']-target['x']),abs(at['y']-target['y']))
        if distance<=3:return True
        candidates=[]
        for dx,dy in [(0,0),(-8,0),(8,0),(0,-8),(0,8),(-8,-8),(8,8),(-8,8),(8,-8)]:
            x=at['x']+max(-14,min(14,target['x']-at['x']))+dx
            y=at['y']+max(-14,min(14,target['y']-at['y']))+dy
            if (x,y)==(at['x'],at['y']):continue
            dest=dict(x=x,y=y,z=at['z'])
            try:
                path=do('path_query',**dest)
                if path['found'] and not path['blocked_destination'] and path['steps']<=80:
                    score=max(abs(x-target['x']),abs(y-target['y']))+visited.get((x,y),0)*100+path['steps']/10
                    candidates.append((score,x,y,dest))
            except EngineRefusal:pass
        if not candidates:return False
        _,x,y,dest=min(candidates);visited[x,y]=visited.get((x,y),0)+1
        do('move_to',**dest,timeout=25)
        for _ in range(500):
            state=do('movement_status')['movement']
            if state!='moving':break
            time.sleep(.05)
    return False
try:
    do('init',game='bg',mod=a.mod,run=a.run,checkpoint=str(a.checkpoint.resolve()))
    do('load',slot=5);before=q();home=d.state();check('earned construction checkpoint',before['stage']==3)
    beds=do('find_objects',shape=696,radius=32)['objects']+do('find_objects',shape=1011,radius=32)['objects']
    events.append(dict(nearby_original_beds=beds))
    coverage['sleep_48h']='NOT TESTED: no complete 48-hour sequence through an accessible ordinary bed established'
    if not a.skip_travel:
        # Probe a southbound route beyond Minoc using ordinary bounded pathfinding.
        if walk(dict(x=1592,y=600,z=0)):
            check('ordinary walking travels south beyond Minoc during construction',d.state()['y']>=597 and q()['deadline']==before['deadline'] and q()['stage']==3)
            result=do('native_wait',minutes=60)
            check('away time retains the earned construction deadline',q()['deadline']==before['deadline'] and q()['stage']==3)
            if walk(dict(x=home['x'],y=home['y'],z=home['z'])):
                check('ordinary return retains construction and exact deadline',q()['stage']==3 and q()['deadline']==before['deadline'])
                do('save',slot=6);do('load',slot=6)
                check('travelled construction state survives reload',q()['stage']==3 and q()['deadline']==before['deadline'])
                coverage['leave_return']='PASS'
            else:coverage['leave_return']='NOT TESTED: bounded return navigation exhausted; outbound movement recorded'
        else:coverage['leave_return']='NOT TESTED: bounded outbound navigation exhausted'
    # Revisit the earlier sleep limitation only when the original world supplies
    # a nearby reachable bed. Use its ordinary native wake-time modal repeatedly.
    do('load',slot=5);before=q()
    beds=do('find_objects',shape=696,radius=32)['objects']+do('find_objects',shape=1011,radius=32)['objects']
    if beds:
        bed=next((b for b in beds if b['tile']['z']==1),beds[0])
        do('move_to_object',ref=bed['ref'],timeout=20)
        end=time.monotonic()+22
        while time.monotonic()<end and do('movement_status')['movement']=='moving':time.sleep(.05)
        for cycle in range(9):
            if q()['stage']!=3:break
            do('interact_object',ref=bed['ref']);end=time.monotonic()+12
            while time.monotonic()<end:
                at=d.state()
                if 'continue'in at['legal_actions']:do('continue')
                elif at['input_owner']=='modal' and not at['can_act']:break
                else:time.sleep(.05)
            if at['input_owner']!='modal' or at['can_act']:
                coverage['sleep_48h']='NOT TESTED: nearby original bed did not enter ordinary sleep/wake modal'
                break
            do('key',key='right');do('key',key='right');do('key',key='enter')
            end=time.monotonic()+45
            while time.monotonic()<end:
                at=d.state()
                if 'continue'in at['legal_actions']:do('continue')
                elif at['input_owner']=='gameplay' and at['can_act']:break
                else:time.sleep(.05)
            if at['input_owner']!='gameplay' or not at['can_act']:break
            check('ordinary sleep keeps original construction deadline',q()['deadline']==before['deadline'])
        if q()['minutes']>=before['deadline'] and q()['stage']==4:
            check('sleeping through deadline makes ready without automatic reward',not do('find_objects',shape=1601,radius=32)['objects'])
            coverage['sleep_48h']='PASS'
            do('save',slot=7);do('load',slot=7)
            check('slept construction deadline survives reload',q()['stage']==4 and q()['deadline']==before['deadline'])
    do('exit')
except Exception:
    d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
finally:
    if d.run:(d.run/'construction-boundary.json').write_text(json.dumps(dict(coverage=coverage,events=events),indent=2))
    result=d.close();print(json.dumps(dict(result,coverage=coverage)),flush=True)
raise SystemExit(0 if result['result']=='PASS' else 1)
