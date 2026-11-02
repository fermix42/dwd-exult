"""Remaining native case failure/target edges, using real spell Usecode."""
import argparse,json,shutil,subprocess,sys,time,traceback
from pathlib import Path
import xml.etree.ElementTree as ET
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import BASE,Driver
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',required=True);p.add_argument('--mod',default='',choices=('','Keyring'));p.add_argument('--checkpoint',type=Path,required=True);a=p.parse_args();d=Driver();records=[];old=subprocess.Popen
def launch(args,**kw):
    if '--test-cli'in args:
        cfg=Path(args[args.index('-c')+1]);tree=ET.parse(cfg);v=tree.getroot().find('video')
        for path,val in {'display/width':640,'display/height':400,'game/width':640,'game/height':400,'ui/width':640,'ui/height':400,'ui/fill_mode':'Fit','ui/scale_method':'Point','ui/fill_scaler':'Point','scale':1}.items():
            el=v
            for part in path.split('/'):
                child=el.find(part)
                if child is None:child=ET.SubElement(el,part)
                el=child
            el.text=str(val)
        tree.write(cfg,encoding='unicode')
    return old(args,**kw)
def do(cmd,**kw):
    c=dict(cmd=cmd,**kw);r=d.execute(c);records.append(dict(command=c,response=r));return r
def check(name,v):
    d.assertions+=1;d.failures+=not v;records.append(dict(check=name,passed=bool(v)));print(name,bool(v),flush=True)
    if not v:raise AssertionError(name)
def inv(ref=0):return do('inspect_inventory',ref=ref)['objects']
def obj(ref):return do('inspect_object',ref=ref)['object']
def find(ref=0):
    for o in inv(ref):
        if o['shape']==1601:return o['ref']
        if o['class_id']==6:
            r=find(o['ref'])
            if r:return r
def find_book(ref=0):
    for o in inv(ref):
        if o['class_id']==8:return o['ref']
        if o['class_id']==6:
            found=find_book(o['ref'])
            if found:return found
def close():
    while d.state()['input_owner']=='gump':do('dismiss')
def show(ref):
    owner=obj(ref)['owner'];avatar=do('inspect_world',radius=2)['avatar']
    if owner and owner!=avatar:show(owner)
    do('interact_inventory',ref=ref)
def open_case():close();do('key',key='1');show(case);do('move_gump',ref=case,x=310,y=5)
def control(action):
    g=next(g for g in d.state()['gumps']if 'Case_gump'in g['type'])
    return next(c for c in do('controls')['controls']if c['gump_id']==g['id']and c['action']==action)
def wait(name,fn):
    end=time.monotonic()+15
    while time.monotonic()<end:
        if fn():check(name,True);return
        time.sleep(.05)
    check(name,False)
def setup(n):close();do('test_case_spell_setup',scenario=n)
def begin(action):
    open_case();before=do('case_spell_state');do('activate_control',id=control(action)['id']);wait(action+' target cursor active',lambda:d.state()['case_targeting']);after=do('case_spell_state');check('one original spell cost only',after['mana']==before['mana']-4);return after
try:
    keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n');subprocess.Popen=launch;do('init',game='bg',mod=a.mod,run=a.run,checkpoint=str(a.checkpoint.resolve()),keys_path=str(keys));subprocess.Popen=old;do('load',slot=3);case=find();stone=next(o['ref']for o in inv(case)if o['frame']==2);bag=obj(case)['owner'];original=obj(stone)['stone_target']
    setup(5);open_case();before=do('case_spell_state');check('fixture level is below fourth circle',before['level']<4)
    for action in ['mark','recall']:
        attempts=d.state()['case_cast_attempts'];do('activate_control',id=control(action)['id']);wait('native '+action+' rejection callback processed',lambda:d.state()['case_cast_attempts']>attempts);after=do('case_spell_state');check('insufficient level blocks '+action,not after['casting']and after['mana']==before['mana']and after['reagents']==before['reagents'])
    setup(0);open_case();do('activate_control',id=control('edit_label_0')['id']);do('type_text',text='Keep Home');do('key',key='enter');check('edge-test note committed',obj(case)['case_labels'][0]=='Keep Home')
    do('transfer_item',ref=stone,destination=bag);check('Honesty compartment is genuinely empty',obj(stone)['owner']==bag and not any(o['frame']==2 for o in inv(case)))
    begin('mark');g=next(g for g in d.state()['gumps']if 'Case_gump'in g['type']);do('target_at',x=g['x']+44,y=g['y']+52);wait('empty compartment target finishes without a cast effect',lambda:not d.state()['case_casting']);check('empty-slot target does not mark or edit',obj(stone)['stone_target']==original and obj(case)['case_labels'][0]=='Keep Home'and d.state()['case_editing']==-1)
    open_case();do('transfer_item',ref=stone,destination=case)
    begin('mark');g=next(g for g in d.state()['gumps']if 'Case_gump'in g['type']);do('target_at',x=g['x']+72,y=g['y']+51);wait('parchment target exits original spell normally',lambda:not d.state()['case_casting']);check('targeting a label does not open editing',d.state()['case_editing']==-1 and obj(case)['case_labels'][0]=='Keep Home')
    begin('mark');cost=do('case_spell_state');g=next(g for g in d.state()['gumps']if 'Case_gump'in g['type']);do('target_at',x=g['x']+53,y=g['y']+20);wait('repeated physical MARK click terminates only the original targeting request',lambda:not d.state()['case_casting']);check('physical repeat click cannot apply a second cost',do('case_spell_state')['mana']==cost['mana'] and obj(stone)['stone_target']==original)
    setup(0);begin('recall');at=d.state();do('key',key='escape');wait('Recall cancellation cleans up request',lambda:not d.state()['case_casting']);after=d.state();check('canceled Recall does not teleport or change notes',(after['x'],after['y'],after['z'])==(at['x'],at['y'],at['z']) and obj(case)['case_labels'][0]=='Keep Home')
    setup(0);do('key',key='1');book=find_book();show(book);normal_before=do('case_spell_state');do('spellbook_cast',ref=book,spell=34);normal_cost=do('case_spell_state');do('key',key='escape');wait('ordinary spellbook canceled-cast animation ends',lambda:do('case_spell_state')['actor_casting']==0);check('cancellation cost matches ordinary spellbook exactly',normal_cost['mana']==normal_before['mana']-4 and d.state()['case_editing']==-1)
    setup(6);check('original magic-storm failure condition is active',do('case_spell_state')['weather']==3);begin('mark');do('target_object',ref=stone);wait('original Usecode storm failure completes',lambda:not d.state()['case_casting']);check('Usecode failure neither marks nor prompts or changes note',obj(stone)['stone_target']==original and d.state()['case_editing']==-1 and obj(case)['case_labels'][0]=='Keep Home')
    setup(0);check('temporary fixture weather removed',do('case_spell_state')['weather']==0);do('exit')
except Exception:
    d.failures+=1;records.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
    if d.p and d.p.poll()is None:
        try:
            if d.state().get('case_targeting'):do('key',key='escape')
        except Exception:pass
finally:
    subprocess.Popen=old
    if d.run:(d.run/'edge-checks.json').write_text(json.dumps(records,indent=2))
    result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
