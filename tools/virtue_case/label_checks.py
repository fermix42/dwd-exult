"""Real-engine Stage C input, note ownership, persistence and spell interoperability.

Stats/inventory are explicit fixtures; spells still execute installed Usecode.
"""
import argparse,json,shutil,subprocess,sys,time,traceback
from pathlib import Path
import xml.etree.ElementTree as ET
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import BASE,Driver,EngineRefusal
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',required=True);p.add_argument('--mod',default='',choices=('','Keyring'));p.add_argument('--checkpoint',type=Path,required=True)
a=p.parse_args();d=Driver();events=[];original_popen=subprocess.Popen
def launch(args,**kw):
    if '--test-cli'in args:
        cfg=Path(args[args.index('-c')+1]);tree=ET.parse(cfg);video=tree.getroot().find('video')
        for path,value in {'display/width':1280,'display/height':800,'game/width':640,'game/height':400,'ui/width':640,'ui/height':400,'ui/fill_mode':'Fit','ui/scale_method':'Point','ui/fill_scaler':'Point','scale':2,'scale_method':'Point'}.items():
            el=video
            for part in path.split('/'):
                child=el.find(part)
                if child is None:child=ET.SubElement(el,part)
                el=child
            el.text=str(value)
        tree.write(cfg,encoding='unicode')
    return original_popen(args,**kw)
def do(cmd,**kw):
    c=dict(cmd=cmd,**kw);r=d.execute(c);events.append(dict(command=c,response=r));return r
def check(label,value):
    d.assertions+=1;d.failures+=not value;events.append(dict(check=label,passed=bool(value)));print(label,bool(value),flush=True)
    if not value:raise AssertionError(label)
def inv(ref=0):return do('inspect_inventory',ref=ref)['objects']
def obj(ref):return do('inspect_object',ref=ref)['object']
def notes(ref):return obj(ref)['case_labels']
def all_items(ref=0):
    result=[]
    for o in inv(ref):
        result.append(o)
        if o['class_id']==6:result+=all_items(o['ref'])
    return result
def cases():return[o for o in all_items()if o['shape']==1601]
def close():
    while d.state()['input_owner']=='gump':do('dismiss')
def show(ref):
    owner=obj(ref)['owner'];avatar=do('inspect_world',radius=2)['avatar']
    if owner and owner!=avatar:show(owner)
    do('interact_inventory',ref=ref)
def open_case(ref):
    close();do('key',key='1');show(ref);do('move_gump',ref=ref,x=310,y=5)
def control(ref,action):
    # Case is the last opened gump; native control identity is re-read each time.
    g=next(g for g in d.state()['gumps']if 'Case_gump'in g['type'])
    return next(c for c in do('controls')['controls']if c['gump_id']==g['id'] and c['action']==action)
def start_edit(ref,slot):
    if not any('Case_gump'in g['type']for g in d.state()['gumps']):open_case(ref)
    do('activate_control',id=control(ref,'edit_label_'+str(slot))['id'])
    check('label '+str(slot)+' obtains native edit focus',d.state()['case_editing']==slot)
def clear_text():
    for _ in range(12):do('key',key='backspace')
def edit(ref,slot,text):
    start_edit(ref,slot);clear_text();do('type_text',text=text);do('key',key='enter')
    check('label '+str(slot)+' commits '+repr(text),notes(ref)[slot]==text and d.state()['case_editing']==-1)
def capture(name):
    do('screenshot',full=True);shutil.copyfile(d.run/'screenshot.png',d.run/(name+'.png'))
def wait(label,fn,seconds=15):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        if fn():check(label,True);return
        time.sleep(.05)
    check(label,False)
def setup(scenario=0):close();do('test_case_spell_setup',scenario=scenario)
def begin(action):
    open_case(case);before=do('case_spell_state');button=control(case,action);do('activate_control',id=button['id'])
    wait(action+' native targeting begins',lambda:d.state()['case_targeting']);after=do('case_spell_state')
    check(action+' uses exactly4 mana',after['mana']==before['mana']-4)
    check(action+' uses original BP/BM/MR exactly once',after['reagents']==[q-(i in {0,1,3})for i,q in enumerate(before['reagents'])])
    do('aim_object',ref=stone);capture(action+'-targeting');return button
try:
    keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n')
    subprocess.Popen=launch;do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys),checkpoint=str(a.checkpoint.resolve()));subprocess.Popen=original_popen
    mark_meta=do('inspect_usecode',function=0x662);check('effective Mark override matches BG/Keyring profile',bool(mark_meta['has_original'])==bool(a.mod))
    do('load',slot=3);case=cases()[0]['ref'];check('older physical case loads with eight blank compartment notes',notes(case)==['']*8)
    open_case(case);start_edit(case,0);do('type_text',text='Britain Home');check('draft does not change saved note',notes(case)[0]=='');capture('label-being-edited');do('key',key='enter');check('Enter commits a12-character two-line note',notes(case)[0]=='Britain Home')
    start_edit(case,0);clear_text();do('type_text',text='Canceled');do('key',key='escape');check('Escape preserves previous note and leaves case open',notes(case)[0]=='Britain Home' and d.state()['case_editing']==-1 and any('Case_gump'in g['type']for g in d.state()['gumps']))
    start_edit(case,0);clear_text();do('type_text',text='Discard');show(obj(case)['owner']);wait('moving focus cancels uncommitted label edit',lambda:d.state()['case_editing']==-1);check('focus change preserves saved note',notes(case)[0]=='Britain Home');open_case(case)
    start_edit(case,1);do('type_text',text='LB Caste');do('key',key='backspace');do('type_text',text='le');do('key',key='enter');check('Backspace edits ordinary text correctly',notes(case)[1]=='LB Castle')
    start_edit(case,2);do('type_text',text='iiiiiiiiiiiiX');do('key',key='enter');check('12-character cap rejects a13th character',notes(case)[2]=='iiiiiiiiiiii')
    start_edit(case,3);do('type_text',text='WWWWWWWWWWWW');do('type_text',text='é💎');do('key',key='enter');check('pixel limit rejects overflow and unsupported Unicode',notes(case)[3]=='WWWWWWWWWW')
    start_edit(case,4);do('key',key='enter');check('empty strings are safe',notes(case)[4]=='')
    expected=['Britain Home','LB Castle','Destard Ent','Cove Healer','Trinsic Gate','Skara Brae','Shrine Home','Camp Home']
    for i,text in enumerate(expected):edit(case,i,text)
    capture('eight-handwritten-labels');do('save',slot=4);do('load',slot=4);case=cases()[0]['ref'];check('all eight compartment assignments survive normal save/load',notes(case)==expected)
    stone=next(o['ref']for o in inv(case)if o['frame']==2);bag=obj(case)['owner'];open_case(case);do('transfer_item',ref=stone,destination=bag);check('removing a real stone leaves its note behind',obj(stone)['owner']==bag and notes(case)==expected)
    edit(case,0,'Empty Home');check('empty compartment can be edited',not any(o['frame']==2 for o in inv(case)) and notes(case)[0]=='Empty Home')
    # Ordinary spellbook GUI casting, not a case callback, onto the loose stone.
    setup();book=next(o['ref']for o in all_items()if o['class_id']==8);do('key',key='1');show(bag);do('move_gump',ref=bag,x=220,y=220);show(book);do('move_gump',ref=book,x=20,y=20)
    old=obj(stone)['stone_target'];normal_before=do('case_spell_state');do('spellbook_cast',ref=book,spell=34);do('aim_object',ref=stone);normal_cost=do('case_spell_state');check('ordinary open spellbook applies the same4-mana cost',normal_cost['mana']==normal_before['mana']-4);do('target_object',ref=stone)
    wait('loose-stone Mark executes original intrinsic',lambda:obj(stone)['stone_target']!=old)
    wait('ordinary spellbook animation finishes',lambda:do('case_spell_state')['actor_casting']==0)
    check('external Mark never rewrites or prompts for case note',notes(case)[0]=='Empty Home' and d.state()['case_editing']==-1)
    open_case(case);do('transfer_item',ref=stone,destination=case);check('reinsertion retains real mark but does not reveal destination',obj(stone)['owner']==case and notes(case)[0]=='Empty Home')
    # Availability failures, repeat presses and target cancellation preserve notes.
    setup(1);open_case(case);attempts=d.state()['case_cast_attempts'];do('activate_control',id=control(case,'mark')['id']);wait('unavailable native Mark callback processed',lambda:d.state()['case_cast_attempts']>attempts);check('failed Mark leaves note and editor unchanged',notes(case)[0]=='Empty Home' and d.state()['case_editing']==-1 and not d.state()['case_casting'])
    setup();button=begin('mark');cost=do('case_spell_state')
    try:do('activate_control',id=button['id']);check('reentrant spell button rejected',False)
    except EngineRefusal as refusal:events.append(dict(expected_refusal=str(refusal)));check('reentrant spell button rejected',True)
    check('repeated button applies no second cost',do('case_spell_state')['mana']==cost['mana']);do('key',key='escape');wait('canceled Mark request finishes',lambda:not d.state()['case_casting']);check('canceled Mark preserves note without edit prompt',notes(case)[0]=='Empty Home' and d.state()['case_editing']==-1)
    setup();begin('mark');do('target_object',ref=stone);wait('successful real Mark automatically opens its compartment editor',lambda:d.state()['case_editing']==0 and not d.state()['case_casting']);check('Mark does not auto-generate or replace the old note',notes(case)[0]=='Empty Home');capture('successful-mark-editor')
    edit_cost=do('case_spell_state');attempts=d.state()['case_cast_attempts'];do('activate_control',id=control(case,'recall')['id']);wait('native button callback processed while editor owns input',lambda:d.state()['case_cast_attempts']>attempts);check('editing and casting cannot overlap',d.state()['case_editing']==0 and not d.state()['case_casting'] and do('case_spell_state')['mana']==edit_cost['mana'])
    clear_text();do('type_text',text='Britain Home');do('key',key='enter');expected[0]='Britain Home';check('successful-Mark handwritten note commits normally',notes(case)==expected)
    marked=obj(stone);at=d.state();close();moved=False
    for dx,dy in [(2,0),(0,2),(-2,0),(0,-2)]:
        try:
            dest=dict(x=at['x']+dx,y=at['y']+dy,z=at['z'])
            if do('inspect_tile',**dest)['blocked']:continue
            do('move_to',**dest,timeout=5);wait('party walks away before Recall',lambda:d.state()['x']!=at['x'] or d.state()['y']!=at['y'],6);moved=True;break
        except EngineRefusal:pass
    check('Recall begins away from actual saved destination',moved);begin('recall');do('target_object',ref=stone);wait('real Recall returns to marked tile',lambda:all(d.state()[k]==marked['stone_target'][k]for k in ['x','y','z']));wait('Recall cleans session pointers',lambda:not d.state()['case_casting']);check('Recall preserves notes and closes case safely',notes(case)==expected and not any('Case_gump'in g['type']for g in d.state()['gumps']))
    # A second debug-created physical case has independent compartment state.
    close();second=do('test_case_extra')['synthetic_case']['ref'];check('new second case starts entirely blank',notes(second)==['']*8);open_case(second);edit(second,0,'Other Home');check('multiple cases never share their notes',notes(case)==expected and notes(second)[0]=='Other Home')
    do('save',slot=5);do('load',slot=5);items=cases();case=next(o['ref']for o in items if o['case_labels'][0]=='Britain Home');second=next(o['ref']for o in items if o['case_labels'][0]=='Other Home');check('multiple-case isolation survives save/load',notes(case)==expected and notes(second)==['Other Home']+['']*7)
    # Move the filled case between two ordinary containers using native dragging.
    home=obj(case)['owner'];other=next(o['ref']for o in all_items()if o['shape']==801 and o['ref']!=home and o['class_id']==6)
    close();do('key',key='1');show(home);show(other);do('move_gump',ref=home,x=30,y=220);do('move_gump',ref=other,x=220,y=220);do('transfer_item',ref=case,destination=other);check('notes move with physical case into another inventory',obj(case)['owner']==other and notes(case)==expected);do('transfer_item',ref=case,destination=home);check('moving back preserves all notes and real stones',notes(case)==expected and len(inv(case))==8)
    close();avatar=do('inspect_world',radius=2)['avatar'];do('key',key='1');show(home);do('move_gump',ref=home,x=30,y=220);do('move_gump',ref=avatar,x=30,y=30);at=d.state();dropped=False
    for dx,dy in [(3,2),(4,2),(5,2),(-3,2),(3,4),(-4,4)]:
        try:
            dest=dict(x=at['x']+dx,y=at['y']+dy,z=at['z'])
            if do('inspect_tile',**dest)['blocked']:continue
            do('drop_item',ref=case,**dest)
            if obj(case)['owner']==0:dropped=True;break
        except EngineRefusal:pass
    check('notes stay with case dropped into real world',dropped and notes(case)==expected);close();do('save',slot=6);do('load',slot=6);case=do('find_objects',shape=1601,radius=12)['objects'][0]['ref'];check('world storage notes survive normal save/load',notes(case)==expected)
    do('interact_object',ref=case);capture('saved-labels-world-case');close();home=next(o['ref']for o in all_items()if o['shape']==801 and o['class_id']==6 and any(c['shape']==644 for c in inv(o['ref'])))
    do('key',key='1');show(home);do('move_gump',ref=home,x=220,y=220);do('transfer_item',ref=case,destination=home);check('world pickup retains notes and eight physical stones',obj(case)['owner']==home and notes(case)==expected and len(inv(case))==8)
    do('save',slot=7);open_case(case);show(home);do('move_gump',ref=home,x=220,y=220);show(case);do('move_gump',ref=case,x=310,y=5);capture('saved-labels-and-backpack');close();do('exit')
except Exception:
    d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
    if d.p and d.p.poll()is None:
        try:
            if d.state().get('case_targeting'):do('key',key='escape')
            capture('failure')
        except Exception:pass
finally:
    subprocess.Popen=original_popen
    if d.run:(d.run/'label-checks.json').write_text(json.dumps(events,indent=2))
    result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
