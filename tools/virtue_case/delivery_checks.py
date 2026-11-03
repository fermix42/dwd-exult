"""Real calendar, food, animated workbench delivery and persistence checks."""
import argparse,json,sys,time,traceback,subprocess,shutil
import xml.etree.ElementTree as ET
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import Driver,EngineRefusal,BASE
p=argparse.ArgumentParser();p.add_argument('--run',required=True);p.add_argument('--mod',default='',choices=('','Keyring'));p.add_argument('--checkpoint',type=Path,required=True);p.add_argument('--ready-checkpoint',action='store_true');a=p.parse_args();d=Driver();events=[]
original_popen=subprocess.Popen
def launch(args,**kw):
 if '--test-cli'in args:
  cfg=Path(args[args.index('-c')+1]);tree=ET.parse(cfg);v=tree.getroot().find('video')
  for path,value in {'display/width':1280,'display/height':800,'game/width':640,'game/height':400,'ui/width':640,'ui/height':400,'ui/fill_mode':'Fit','ui/scale_method':'Point','scale':2}.items():
   el=v
   for part in path.split('/'):
    child=el.find(part)
    if child is None:child=ET.SubElement(el,part)
    el=child
   el.text=str(value)
  tree.write(cfg,encoding='unicode')
 return original_popen(args,**kw)
subprocess.Popen=launch
def do(cmd,**kw):
 c=dict(cmd=cmd,**kw);r=d.execute(c);events.append(dict(command=c,response=r));return r
def check(label,value):
 d.assertions+=1;d.failures+=not value;events.append(dict(check=label,passed=bool(value)));print(label,bool(value),flush=True)
 if not value:raise AssertionError(label)
def q():return do('case_commission_state')['commission']
def settle():
 for _ in range(200):
  s=d.state()
  if 'continue'in s['legal_actions']:do('continue')
  elif s['input_owner']in ['answer_menu','gameplay']:return s
  else:time.sleep(.02)
 raise TimeoutError('input ownership')
def choose(text):do('choose',text=text);return settle()
def leave():
 if settle()['input_owner']=='answer_menu':choose('bye')
 settle()
def arrived(seconds=40):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  m=do('movement_status')
  if m['movement']!='moving':return m['movement']=='reached'
  time.sleep(.05)
 return False
def travel(npc):
 visits={}
 for _ in range(120):
  query=do('case_commission_state');target=query['xanthia'if npc==86 else 'william'];at=d.state();t=target['tile']
  if max(abs(t['x']-at['x']),abs(t['y']-at['y']))<=10:
   if target['interactable']:return target['ref']
   do('move_to_object',ref=target['ref']);check('normal approach reaches NPC '+str(npc),arrived());return target['ref']
  dx=max(-8,min(8,t['x']-at['x']));dy=max(-8,min(8,t['y']-at['y']));candidates=[]
  for ox,oy in [(0,0),(-4,0),(4,0),(0,-4),(0,4),(-8,0),(8,0),(0,-8),(0,8),(-16,0),(16,0),(0,-16),(0,16)]:
   x=at['x']+dx+ox;y=at['y']+dy+oy
   if max(abs(x-at['x']),abs(y-at['y']))>30:continue
   candidates.append((max(abs(x-t['x']),abs(y-t['y']))+visits.get((x,y),0)*100,x,y))
  for ox,oy in [(8,0),(-8,0),(0,8),(0,-8),(8,8),(-8,8),(8,-8),(-8,-8)]:
   x=at['x']+ox;y=at['y']+oy
   candidates.append((max(abs(x-t['x']),abs(y-t['y']))+visits.get((x,y),0)*100,x,y))
  plans=[]
  for score,x,y in sorted(set(candidates)):
   try:
    dest=dict(x=x,y=y,z=at['z']);path=do('path_query',**dest)
    if path['found']and not path['blocked_destination']and path['steps']<=80:
     plans.append((score+path['steps'],path['steps'],dest))
   except EngineRefusal:continue
  moved=False
  for _,steps,dest in sorted(plans,key=lambda plan:plan[0]):
   try:
    timeout=min(45,max(12,steps*.4+5));do('move_to',**dest,timeout=timeout)
    visits[dest['x'],dest['y']]=visits.get((dest['x'],dest['y']),0)+1
    if arrived(timeout+1):moved=True;break
   except EngineRefusal:continue
  if not moved:raise RuntimeError('bounded Minoc navigation cannot find route')
 raise RuntimeError('bounded Minoc navigation exhausted')
def talk(npc=86):
 ref=travel(npc);do('interact_object',ref=ref);return settle()
def capture(name):
 do('screenshot',full=True);shutil.copy2(d.run/'screenshot.png',d.run/(name+'.png'))
def close_gumps():
 while d.state()['input_owner']=='gump':do('dismiss')
def inv(ref=0):return do('inspect_inventory',ref=ref)['objects']
def obj(ref):return do('inspect_object',ref=ref)['object']
def show(ref):
 owner=obj(ref)['owner'];avatar=do('inspect_world',radius=2)['avatar']
 if owner and owner!=avatar:show(owner)
 do('interact_inventory',ref=ref)
def feed():
 close_gumps();members=do('case_commission_state')['party']
 for index,member in enumerate(members):
  if member['food']>9:continue
  before=member['food'];do('key',key='1');time.sleep(.1)
  foods=[o for o in inv()if o['shape']==377 and o['frame']==8]
  if not foods:
   close_gumps();do('test_case_commission_food');do('key',key='1');foods=[o for o in inv()if o['shape']==377 and o['frame']==8]
  food=foods[0]['ref'];show(obj(food)['owner']);do('move_gump',ref=obj(food)['owner'],x=20,y=220)
  do('key',key=str(index+1));time.sleep(.1);do('move_gump',ref=member['ref'],x=400,y=5)
  do('interact_inventory',ref=food)
  end=time.monotonic()+8
  while time.monotonic()<end:
   if d.state()['input_owner']=='target':break
   time.sleep(.03)
  do('target_object',ref=member['ref']);time.sleep(.15);close_gumps()
  end=time.monotonic()+8
  while time.monotonic()<end:
   after=obj(member['ref'])['food']
   if after>before:break
   time.sleep(.05)
  check('ordinary food use feeds party member '+str(member['npc']),after>before)
 close_gumps()
def wait_to(target):
 for _ in range(80):
  now=q()['minutes']
  if now>=target:return
  r=do('native_wait',minutes=min(1440,target-now))
  if r['reason']=='hungry-party':feed()
  elif r['reason']in ['complete','schedule-reconciliation-pending','party-action','party-deferred-sequence']:
   time.sleep(.2)
  else:raise RuntimeError('native Wait interrupted: '+r['reason'])
 raise RuntimeError('bounded normal waiting exhausted')
def cases():return do('find_objects',shape=1601,radius=32)['objects']
try:
 keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n2 INVENTORY 1\n')
 if a.ready_checkpoint:
  do('init',game='bg',mod=a.mod,run=a.run,checkpoint=str(a.checkpoint.resolve()),keys_path=str(keys));do('load',slot=3);check('earned ready checkpoint remains ready',q()['stage']==4)
 else:
  do('init',game='bg',mod=a.mod,run=a.run,checkpoint=str(a.checkpoint.resolve()),keys_path=str(keys));do('load',slot=5);before=q();do('test_case_commission_start');check('approved Minoc start preserves real earned construction state',q()['stage']==3 and q()['deadline']==before['deadline']);do('test_case_commission_food');deadline=q()['deadline'];do('save',slot=1)
  s=talk();check('building has progress but no fulfilled transaction topics','case progress'in s['choices']and not set(['case materials','give cloth','pay Guild fee']).intersection(s['choices']));s=choose('case progress');check('progress disappears within same conversation','case progress'not in s['choices']);leave();s=talk();check('progress may return next conversation','case progress'in s['choices']);leave();check('progress never restarts construction',q()['deadline']==deadline)
  wait_to(deadline-60);check('exactly47 hours after start still building',q()['minutes']==deadline-60 and q()['stage']==3);check('no case automatically created while building',not cases());do('save',slot=2);do('load',slot=2);check('47-hour construction and deadline survive reload',q()['stage']==3 and q()['deadline']==deadline)
  wait_to(deadline);check('exactly48 hours makes case ready',q()['minutes']==deadline and q()['stage']==4);check('time expiry does not automatically create reward',not cases());do('save',slot=3)
 s=talk();check('ready dialogue replaces progress with collection','collect case'in s['choices']and'case progress'not in s['choices']);choose('collect case');check('collection ends dialogue and starts native actor action',d.state()['input_owner']=='gameplay'and q()['delivery_active']);do('save',slot=4);do('load',slot=4);check('load before placement remains retryable with no case',q()['stage']==4 and not cases());s=talk();check('interrupted delivery can be collected again','collect case'in s['choices']);choose('collect case')
 reached=False;placed=False;start=do('case_commission_state')['xanthia']['tile'];end=time.monotonic()+120;retries=0
 while time.monotonic()<end:
  data=do('case_commission_state');quest=data['commission'];npc=data['xanthia']
  if quest['delivery_phase']==2 and not reached:
   check('Xanthia reaches workbench and uses original reach frame',(npc['frame']&15)in (5,8) and abs(npc['tile']['x']-1592)<=3 and abs(npc['tile']['y']-370)<=3);do('save',slot=8);capture('xanthia-reaches-workbench');reached=True
  if quest['stage']==5 and not placed:
   check('one real case appears on visible front of native workbench',len(cases())==1 and cases()[0]['tile']==dict(x=1592,y=370,z=2));capture('case-placed-on-workbench');placed=True
  if placed and not quest['delivery_active']:break
  if quest['stage']==4 and not quest['delivery_active'] and retries<3:
   retries+=1;s=talk();choose('collect case');continue
  if quest['delivery_phase']==0 and max(abs(npc['tile']['x']-d.state()['x']),abs(npc['tile']['y']-d.state()['y']))>7:
   do('move_to_object',ref=npc['ref'])
  time.sleep(.04)
 check('placement animation and reward were both observed',reached and placed);check('delivery becomes terminal',q()['stage']==5 and not q()['delivery_active']);case=cases()[0];check('placed case centre remains visible and reachable after Xanthia steps aside',bool(case['hit_test_target'])and bool(case['reachable']));time.sleep(6);capture('unobstructed-reward-after-step-aside');do('save',slot=7);do('load',slot=7);case=cases()[0];check('tabletop reward and terminal state survive reload before pickup',q()['stage']==5 and len(cases())==1 and case['tile']==dict(x=1592,y=370,z=2));check('reward is original native empty case with eight blank notes',case['class_id']==6 and case['case_labels']==['']*8)
 # Reach and pick up the real world item using normal drag/drop into the backpack.
 do('move_to',x=1592,y=371,z=0,timeout=35);check('Avatar walks to the workbench',arrived());case=cases()[0];do('key',key='1');avatar=do('inspect_world',radius=2)['avatar'];pack=next(e['ref']for e in do('inspect_actor',ref=avatar)['object']['equipment']if e['slot']==1);show(pack);do('move_gump',ref=pack,x=30,y=220);do('move_gump',ref=avatar,x=30,y=5);do('transfer_item',ref=case['ref'],destination=pack);check('real case can be picked up normally',obj(case['ref'])['owner']==pack);show(case['ref']);check('delivered case opens accepted specialized gump',any('Case_gump'in g['type']for g in d.state()['gumps']));check('delivered case is empty',not inv(case['ref']));close_gumps();do('save',slot=5);do('load',slot=5);check('terminal delivery survives normal save/load',q()['stage']==5)
 s=talk();check('all commission topics permanently absent after delivery',not set(['Virtue Stone case','commission case','case materials','give cloth','pay Guild fee','case progress','collect case']).intersection(s['choices']));leave();check('repeated conversation creates no extra case',len([o for o in inv()if o['shape']==1601])==1 and not cases());do('save',slot=6);do('exit')
except Exception:
 d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
 if d.p and d.p.poll()is None:
  try:capture('failure')
  except Exception:pass
finally:
 subprocess.Popen=original_popen
 if d.run:(d.run/'delivery-checks.json').write_text(json.dumps(events,indent=2))
 result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
