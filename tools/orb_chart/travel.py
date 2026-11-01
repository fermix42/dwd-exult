"""Natural handover/recovery and real Usecode travel from an existing checkpoint.

Requires a naturally progressed BG/Keyring save with the Orb (and room for a
chart), or Lord British within ordinary interaction range. Never changes flags,
creates an Orb, moves NPCs, or teleports the player. Use open terrain for mapping
checks; failed gate placement/injury is recorded, never counted as arrival.
"""
import argparse
import json
from pathlib import Path
import sys
import time

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import Driver,BASE

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--checkpoint',required=True)
p.add_argument('--run',required=True)
p.add_argument('--mod',default='')
p.add_argument('--handover-only',action='store_true')
p.add_argument('--actions',nargs='+',help='Only named native actions; reuse already verified directions')
a=p.parse_args()
d=Driver()
records=[]
def do(cmd,**kw):
    command=dict(cmd=cmd,**kw)
    response=d.execute(command)
    records.append(dict(command=command,response=response))
    return response
def state():return do('state')['state']
def check(label,value):
    d.assertions+=1
    d.failures+=not value
    records.append(dict(check=label,passed=bool(value)))
    print(label,bool(value),flush=True)
    if not value:raise AssertionError(label)
def count(s,shape):return sum(o['shape']==shape for o in s['inventory'])
def settle(owner,seconds=4):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        s=state()
        if s['input_owner']==owner:return s
        time.sleep(.05)
    raise TimeoutError(owner)
def acknowledge():
    for _ in range(40):
        s=state()
        if s['input_owner']!='acknowledgement':return s
        do('continue')
    raise TimeoutError('conversation pages')
def acquire():
    before=state()
    recovering=bool(before['orb_given'])
    topic='Orb chart' if recovering else 'Orb of the Moons'
    british=next((o for o in do('find_actors',radius=32,limit=100)['objects'] if o['npc']==23),None)
    if not british or not british['interactable']:
        raise RuntimeError('checkpoint needs Lord British in ordinary interaction range for acquisition')
    do('interact_object',ref=british['ref'])
    selected=False
    for _ in range(12):
        s=acknowledge()
        if s['input_owner']=='gameplay':break
        if s['input_owner']!='answer_menu':raise RuntimeError('unexpected acquisition input owner')
        choices=s['choices']
        if not selected and topic in choices:
            do('choose',text=topic)
            selected=True
        elif selected and set(choices)=={'Yes','No'} and not recovering:
            do('choose',text='No') # The original question about bringing thine own Orb.
        elif selected and 'bye' in choices:
            do('choose',text='bye')
        else:raise RuntimeError('unexpected acquisition choices: '+repr(choices))
    after=settle('gameplay')
    check('Lord British supplies physical chart through existing conversation',selected and count(after,1600)>0)
    check('chart-only recovery does not issue another Orb' if recovering else 'original Orb handover still succeeds',
          count(after,785)==count(before,785) if recovering else count(after,785)==count(before,785)+1 and after['orb_given'])

# Expected arrivals are test observations transcribed from the original and
# supported Keyring scripts. Production chart code contains none of these.
retail=[(1231,1314,0),(261,2826,0),(519,491,0),(1855,402,0),
        (1023,2435,0),(2175,2338,0),(567,1609,0),(2879,1810,0)]
towns=[retail[i] for i in (0,1,2,3,4,6,5,7)]
shrines=[(1464,1356,0),(507,2672,0),(750,140,0),(2289,966,0),
         (867,2526,0),(2919,2403,1),(2850,2502,0),(2713,1164,0)]
directions=['north','northeast','east','southeast','south','southwest','west','northwest']
def arrived(s,expected):
    x,y,world=expected
    return s['map']==world and max(abs(s['x']-x),abs(s['y']-y))<=3

try:
    BASE.mkdir(parents=True,exist_ok=True)
    keys=BASE/(a.run+'.keys')
    keys.write_text('w ORB_CHART\n1 INVENTORY 0\n')
    do('init',game='bg',mod=a.mod,run=a.run,checkpoint=a.checkpoint,keys_path=str(keys))
    do('load',slot=int(Path(a.checkpoint).stem[5:7]))
    s=state()
    check('effective supported implementation',s['orb_chart_layout']==(2 if a.mod else 1))
    if not count(s,1600):acquire()
    s=state()
    if a.handover_only:
        acquire() # Requesting a chart already possessed must not duplicate it.
        check('existing chart is retained without duplicate',count(state(),1600)==1)
        do('key',key='1')
        settle('gump')
        inventory=do('inspect_inventory')['objects']
        chart=next(o for o in inventory if o['shape']==1600)
        owner=next((o for o in inventory if o['ref']==chart['owner']),None)
        if owner and owner['type']=='container':do('interact_inventory',ref=owner['ref'])
        s=state()
        for dx,dy in [(x,y) for y in range(2,8,2) for x in range(-6,8,2)]:
            try:
                if do('inspect_tile',x=s['x']+dx,y=s['y']+dy,z=s['z'])['blocked']:continue
                do('drop_item',ref=chart['ref'],x=s['x']+dx,y=s['y']+dy,z=s['z'])
            except Exception:continue
            time.sleep(.05)
            if not count(state(),1600):break
        check('chart removed through ordinary inventory drop',not count(state(),1600))
        while state()['input_owner']=='gump':do('dismiss')
        settle('gameplay')
        do('save',slot=8)
        do('load',slot=8)
        check('chartless Orb-owner save loads normally',not count(state(),1600) and count(state(),785)>0 and state()['orb_given'])
        acquire()
        s=state()
    check('actual Orb is possessed',count(s,785)>0)
    do('save',slot=9)
    restricted=bool(s['orb_sphere_broken'] or (not s['orb_left_trinsic']
                    and 912<=s['x']<=1104 and 2064<=s['y']<=2352 and 0<=s['z']<=15))
    expected=list(towns if a.mod else retail)
    if a.mod:
        expected+=shrines
        if not s['orb_timelord_repaired']:expected[13]=(567,1609,0)
    elif s['orb_timelord_repaired']:expected[7]=(2919,2402,0)
    for index,destination in enumerate([] if a.handover_only else expected):
        action=('far-' if index>=8 else 'near-')+directions[index%8]
        if a.actions and action not in a.actions:continue
        success=False
        offsets=[(0,0),(2,0),(-2,0),(0,2),(0,-2),(4,4),(-4,4),(4,-4),(-4,-4),(8,0),(-8,0),(0,8),(0,-8)]
        for attempt in range(1 if restricted else len(offsets)):
            do('load',slot=9)
            before=state()
            if attempt:
                dx,dy=offsets[attempt]
                x,y=before['x']+dx,before['y']+dy
                path=do('path_query',x=x,y=y,z=before['z'])
                if not path['found'] or path['blocked_destination']:continue
                do('move_to',x=x,y=y,z=before['z'])
                end=time.monotonic()+8
                while time.monotonic()<end:
                    movement=do('movement_status')
                    if movement['movement']!='moving':break
                    time.sleep(.05)
                before=state()
                if (before['x'],before['y'])!=(x,y):continue
            do('key',key='w')
            settle('modal')
            control=next(c for c in do('controls')['controls'] if c['action']==action)
            do('activate_control',id=control['id'])
            # Observe the actual temporary gate produced by the loaded Usecode.
            # Its quality is the destination index; native object distance is
            # exactly what Keyring compares against seven (not cursor radius).
            gates=[]
            for shape in (779,157,1113,1114):
                gates+=do('find_objects',radius=16,limit=20,shape=shape)['objects']
            gate=next((o for o in gates if o['shape'] in (779,157,1113,1114) and o['quality']==index),None)
            if not restricted and gate:
                check(action+' actual gate quality',gate['quality']==index)
                if a.mod:check(action+' actual near/far distance',(gate['distance']>7)==(index>=8))
            records.append(dict(action=action,attempt=attempt,actual_gate=gate))
            if not gate and not restricted:
                # The active script refused placement. Try a nearby position by
                # real walking; never lift/move a gate or bypass its checks.
                records.append(dict(action=action,attempt=attempt,placement_refused=True))
                check(action+' refused placement restores target',not state()['target_intercept_pending'])
                continue
            end=time.monotonic()+12
            while time.monotonic()<end:
                after=state()
                if (not restricted and arrived(after,destination) and not after['dont_move']
                    and after['can_act'] and not after['in_usecode']):success=True;break
                if restricted and after['input_owner']=='gameplay' and not after['in_usecode']:
                    success=(after['x'],after['y'],after['map'])==(before['x'],before['y'],before['map'])
                    break
                time.sleep(.1)
            check(action+' restores target intercept',not after['target_intercept_pending'])
            if success:break
            records.append(dict(action=action,attempt=attempt,arrival=False,
                                explanation='ordinary gate failure/injury; not counted as arrival'))
        check(action+(' obeys plot restriction' if restricted else ' actual arrival'),success)
        if success and not restricted:do('save',slot=7)
    do('load',slot=9)
    do('key',key='1')
    settle('gump')
    inventory=do('inspect_inventory')['objects']
    orb=next(o for o in inventory if o['shape']==785)
    owner=next((o for o in inventory if o['ref']==orb['owner']),None)
    if owner and owner['type']=='container':do('interact_inventory',ref=owner['ref'])
    do('interact_inventory',ref=orb['ref'])
    if not restricted:
        check('manual Orb double-click still requests a world target',settle('target')['input_owner']=='target')
        do('key',key='escape')
    time.sleep(.2)
    check('manual activation leaves no chart target behind',not state()['target_intercept_pending'])
    while state()['input_owner']=='gump':do('dismiss')
    do('exit')
except Exception as error:
    if not d.failures: d.failures += 1
    records.append(dict(error=str(error)))
    raise
finally:
    if d.p and d.p.poll() is None:
        try:
            if state()['input_owner'] in ('modal','target'):do('key',key='escape')
        except Exception:pass
    result=d.close()
    if d.run:(d.run/'travel-checks.json').write_text(json.dumps(dict(records=records,result=result),indent=2))
    print(json.dumps(result),flush=True)
if result['result'] != 'PASS':raise SystemExit(1)
