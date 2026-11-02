"""Exercise real effective BG/Keyring spell Usecode with explicit inventory/stat fixtures.

No quest flags, spell effects, destinations or teleportation are fabricated.
"""
import argparse, json, shutil, subprocess, sys, time, traceback
from pathlib import Path
import xml.etree.ElementTree as ET
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import BASE, Driver, EngineRefusal
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--run',required=True);p.add_argument('--mod',default='',choices=('','Keyring'))
p.add_argument('--checkpoint',type=Path,required=True)
a=p.parse_args();d=Driver();events=[]
old_popen=subprocess.Popen
def launch(args,**kw):
    if '--test-cli' in args:
        cfg=Path(args[args.index('-c')+1]);tree=ET.parse(cfg);video=tree.getroot().find('video')
        for path,value in {'display/width':640,'display/height':400,'game/width':640,'game/height':400,'ui/width':640,'ui/height':400,'ui/fill_mode':'Fit','ui/scale_method':'Point','ui/fill_scaler':'Point','scale':1}.items():
            el=video
            for part in path.split('/'):
                child=el.find(part)
                if child is None:child=ET.SubElement(el,part)
                el=child
            el.text=str(value)
        tree.write(cfg,encoding='unicode')
    return old_popen(args,**kw)
def do(cmd,**kw):
    c=dict(cmd=cmd,**kw);r=d.execute(c);events.append(dict(command=c,response=r));return r
def check(name,value):
    d.assertions+=1;d.failures+=not value;events.append(dict(check=name,passed=bool(value)));print(name,bool(value),flush=True)
    if not value:raise AssertionError(name)
def inv(ref=0):return do('inspect_inventory',ref=ref)['objects']
def obj(ref):return do('inspect_object',ref=ref)['object']
def find_case(ref=0):
    for o in inv(ref):
        if o['shape']==1601:return o['ref']
        if o['class_id']==6:
            found=find_case(o['ref'])
            if found:return found
def close():
    while d.state()['input_owner']=='gump':do('dismiss')
def show(ref):
    owner=obj(ref)['owner'];avatar=do('inspect_world',radius=2)['avatar']
    if owner and owner!=avatar:show(owner)
    do('interact_inventory',ref=ref)
def open_case():
    close();do('key',key='1');show(case);do('move_gump',ref=case,x=310,y=5)
def control(action):
    g=next(g for g in d.state()['gumps'] if 'Case_gump' in g['type'])
    return next(c for c in do('controls')['controls'] if c['gump_id']==g['id'] and c['action']==action)
def capture(name):
    do('screenshot',full=True);shutil.copyfile(d.run/'screenshot.png',d.run/(name+'.png'))
def wait(label,fn,seconds=12):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        if fn():check(label,True);return
        time.sleep(.05)
    check(label,False)
def begin(action):
    open_case();before=do('case_spell_state');c=control(action);do('activate_control',id=c['id'])
    wait('normal '+action+' targeting cursor active',lambda:d.state()['case_targeting'])
    now=do('case_spell_state');check(action+' mana cost applied exactly once before targeting',now['mana']==before['mana']-4)
    required={0,1,3} # Both original fourth-circle Mark and Recall use BP/BM/MR.
    check(action+' correct original reagents consumed exactly once',now['reagents']==[q-(i in required)for i,q in enumerate(before['reagents'])])
    capture(action+'-targeting');return now
try:
    keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n')
    subprocess.Popen=launch;do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys),checkpoint=str(a.checkpoint.resolve()));subprocess.Popen=old_popen
    do('load',slot=3);case=find_case();check('loaded real case with eight original stones',bool(case) and len(inv(case))==8)
    stone=next(o['ref'] for o in inv(case)if o['frame']==2)
    for scenario,label in [(1,'insufficient mana'),(2,'missing reagents'),(3,'unavailable spell'),(4,'missing spellbook')]:
        close();do('test_case_spell_setup',scenario=scenario);open_case();before=do('case_spell_state')
        for action in ['mark','recall']:
            do('activate_control',id=control(action)['id']);after=do('case_spell_state')
            check(label+' blocks '+action+' without costs/targeting',not after['casting'] and not after['targeting'] and after['mana']==before['mana'] and after['reagents']==before['reagents'])
    close();do('test_case_spell_setup',scenario=0)
    begin('mark');before=obj(stone)['stone_target'];do('target_object',ref=stone)
    wait('real Mark intrinsic changes physical stone destination',lambda:obj(stone)['stone_target']!=before)
    wait('Mark animation completes and native request cleans up',lambda:not d.state()['case_casting'])
    marked=obj(stone);at=d.state();check('Mark destination equals current party tile/map',marked['stone_target']==dict(x=at['x'],y=at['y'],z=at['z']) and marked['stone_target_map']==at['map'])
    capture('after-mark')
    # Walk normally before Recall; never teleport or alter a target to make it pass.
    moved=False
    for dx,dy in [(2,0),(0,2),(-2,0),(0,-2)]:
        try:
            tile=dict(x=at['x']+dx,y=at['y']+dy,z=at['z'])
            if do('inspect_tile',**tile)['blocked']:continue
            do('move_to',**tile,timeout=5);wait('party walks away normally',lambda:d.state()['x']!=at['x'] or d.state()['y']!=at['y'],6);moved=True;break
        except EngineRefusal:pass
    check('Recall starts from a different actual party position',moved)
    begin('recall');do('target_object',ref=stone)
    wait('real Recall returns party to saved physical stone destination',lambda:all(d.state()[k]==marked['stone_target'][k]for k in ['x','y','z']),15)
    wait('Recall request cleans up',lambda:not d.state()['case_casting'])
    check('Recall leaves no orphan case gump',not any('Case_gump' in g['type']for g in d.state()['gumps']))
    check('Recall retains original stone and destination',obj(stone)['owner']==case and obj(stone)['stone_target']==marked['stone_target'])
    begin('mark');saved=obj(stone)['stone_target'];do('key',key='escape');wait('normal target cancellation cleans up',lambda:not d.state()['case_casting']);check('canceled Mark leaves destination unchanged',obj(stone)['stone_target']==saved)
    begin('mark');g=next(g for g in d.state()['gumps']if 'Case_gump'in g['type']);do('target_at',x=g['x']+70,y=g['y']+50)
    wait('label/decoration target fails without stale request',lambda:not d.state()['case_casting']);check('invalid target does not mark stone',obj(stone)['stone_target']==saved)
    close();do('exit')
except Exception:
    d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
    if d.p and d.p.poll() is None:
        try:
            if d.state().get('case_targeting'):do('key',key='escape')
            capture('failure')
        except Exception:pass
finally:
    subprocess.Popen=old_popen
    if d.run:(d.run/'spell-checks.json').write_text(json.dumps(events,indent=2))
    result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
