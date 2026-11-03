"""Real-engine companion breadcrumb, native MARK and persistence checks."""
import argparse,json,sys,time,traceback,subprocess,shutil
import xml.etree.ElementTree as ET
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import Driver,EngineRefusal,BASE
p=argparse.ArgumentParser();p.add_argument('--run',required=True);p.add_argument('--mod',default='',choices=('','Keyring'));p.add_argument('--checkpoint',type=Path);p.add_argument('--capture-review',action='store_true');a=p.parse_args();d=Driver();events=[]
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
def capture(name):
 do('screenshot',full=True);shutil.copy2(d.run/'screenshot.png',d.run/(name+'.png'))
def close_gumps():
 while d.state()['input_owner']=='gump':do('dismiss')
def inv(ref=0):return do('inspect_inventory',ref=ref)['objects']
def obj(ref):return do('inspect_object',ref=ref)['object']
def show(ref):
 avatar=do('inspect_world',radius=2)['avatar']
 if ref==avatar:return
 owner=obj(ref)['owner']
 if owner and owner!=avatar:show(owner)
 do('interact_inventory',ref=ref)
def q():return do('case_breadcrumbs_state')['breadcrumbs']
def companion(npc):return next(o for o in do('case_breadcrumbs_state')['companions']if o['npc']==npc)
def until(label,predicate,seconds=25):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if predicate():check(label,True);return
  time.sleep(.08)
 check(label,False)
def talk(npc):
 actor=companion(npc);do('interact_object',ref=actor['ref']);return settle()
def dismiss_iolo():
 s=talk(1);check('Iolo original leave topic available','leave'in s['choices']);s=choose('leave')
 for _ in range(6):
  choices=s.get('choices',[]);answers={v.lower():v for v in choices}
  if 'yes'in answers:s=choose(answers['yes'])
  elif 'wait here'in choices:s=choose('wait here')
  elif 'bye'in choices:s=choose('bye')
  else:break
 check('Iolo leaves through original dialogue',6 not in companion(1)['flags']);leave()
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
def wait_minutes(minutes):
 target=None
 for _ in range(30):
  feed()
  at=d.state();now=at['day']*1440+at['hour']*60+at['minute']
  if target is None:target=now+minutes
  if now>=target:return
  result=do('native_wait',minutes=target-now)
  if result['reason']not in ['complete','hungry-party','schedule-reconciliation-pending','party-action','party-deferred-sequence']:
   raise RuntimeError('normal wait deferred: '+result['reason'])
  time.sleep(.2)
 raise TimeoutError('normal companion wake-up wait')

def review_checkpoint(name):
 if not a.capture_review:return
 if d.state()['input_owner']!='gump':do('key',key='1')
 state=q();check('review checkpoint remains undelivered '+name,not state['a_done']and not state['b_done'])
 export_checkpoint(name)
 close_gumps()

def export_checkpoint(name):
 do('save',slot=9);at=d.state();target=d.run/'review-checkpoints'/name
 target.mkdir(parents=True,exist_ok=False)
 shutil.copytree(Path(at['gamedat_path']),target/'gamedat')
 shutil.copy2(Path(at['save_path'])/'exult09bg.sav',target/'exult08bg.sav')
 (target/'provenance.json').write_text(json.dumps(dict(source_run=a.run,source_slot=9,breadcrumbs=q(),commission=do('case_commission_state')['commission']),indent=2))

def join(npc,review_name=None):
 for _ in range(4):
  do('test_case_companion_start',npc=npc)
  if review_name=='a-shamino' and a.capture_review and not(d.run/'review-checkpoints/deferred-recruit').exists():review_checkpoint('deferred-recruit')
  s=talk(npc)
  if s['input_owner']!='gameplay':break
  wait_minutes(180)
 if 'join'not in s.get('choices',[])and'name'in s.get('choices',[]):s=choose('name')
 if 'join'not in s.get('choices',[])and'job'in s.get('choices',[]):s=choose('job')
 check('original companion join topic '+str(npc),'join'in s.get('choices',[]));s=choose('join')
 if s['input_owner']=='answer_menu'and'bye'in s.get('choices',[]):
  before=q()['deliveries_since_load'];time.sleep(6);check('existing conversation defers companion speech',q()['deliveries_since_load']==before);choose('bye')
 check('companion joins through original Usecode '+str(npc),6 in companion(npc)['flags']);feed()
 if review_name:review_checkpoint(review_name)
def pack():
 avatar=do('inspect_world',radius=2)['avatar'];return avatar,next(e['ref']for e in do('inspect_actor',ref=avatar)['object']['equipment']if e['slot']==1)
def inventories():
 do('key',key='1');avatar,bag=pack();do('move_gump',ref=avatar,x=30,y=5);show(bag);do('move_gump',ref=bag,x=30,y=220);return bag
def acquire_third():
 world=do('test_case_breadcrumb_items',scenario=1)['synthetic_world_stone'];bag=inventories();do('transfer_item',ref=world['ref'],destination=bag);check('third distinct colour acquired by actual world pickup',obj(world['ref'])['owner']==bag and q()['colors']==3);check('acquisition latches pending while UI owns gameplay',q()['a_pending']and not q()['a_done']);return world['ref']
def drop(ref):
 close_gumps();inventories()
 owner=obj(ref)['owner'];avatar=do('inspect_world',radius=2)['avatar']
 if owner!=avatar:show(owner)
 at=d.state()
 for dx,dy in [(3,2),(4,2),(5,2),(-3,2),(3,4),(-4,4)]:
  try:
   dest=dict(x=at['x']+dx,y=at['y']+dy,z=at['z'])
   if do('inspect_tile',**dest)['blocked']:continue
   do('drop_item',ref=ref,**dest)
   if not obj(ref)['owner']:return
  except EngineRefusal:pass
 raise RuntimeError('no visible reachable native drop')
def prepare_case():
 case=do('test_case_extra')['synthetic_case']['ref'];bag=inventories();show(case);do('move_gump',ref=case,x=310,y=5)
 for stone in [o for o in inv()if o['shape']==330]:
  show(obj(stone['ref'])['owner']);do('transfer_item',ref=stone['ref'],destination=case)
 check('two original unmarked objects inserted into real case',len(inv(case))==2 and q()['marked_stones']==0);close_gumps();return case
def mark(case,stone):
 close_gumps();do('test_case_spell_setup',scenario=0);inventories();show(case);do('move_gump',ref=case,x=310,y=5)
 g=next(g for g in d.state()['gumps']if'Case_gump'in g['type']);button=next(c for c in do('controls')['controls']if c['gump_id']==g['id']and c['action']=='mark');before=do('case_spell_state');do('activate_control',id=button['id']);until('ordinary MARK target begins',lambda:d.state()['case_targeting']);do('target_object',ref=stone);until('ordinary MARK completes',lambda:not d.state()['case_casting']);after=do('case_spell_state');check('MARK pays normal mana and reagents',after['mana']==before['mana']-4 and after['reagents']==[n-(i in {0,1,3})for i,n in enumerate(before['reagents'])]);check('stone has actual saved native destination',obj(stone)['stone_target']['x']>0 or obj(stone)['stone_target']['y']>0)
 if d.state()['case_editing']>=0:do('key',key='escape')
if __name__=='__main__':
 try:
  keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n2 INVENTORY 1\n')
  if a.checkpoint:
   do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys),checkpoint=str(a.checkpoint.resolve()));do('load',slot=7)
  else:
   do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys));do('test_case_breadcrumb_items',scenario=0);check('two unmarked colours trigger neither observation',q()['colors']==2 and q()['marked_stones']==0 and q()['flags']==0);do('save',slot=0)
   duplicate=do('test_case_breadcrumb_items',scenario=3)['synthetic_world_stone'];bag=inventories();do('transfer_item',ref=duplicate['ref'],destination=bag);check('three objects but only two colours do not qualify A',q()['colors']==2 and not q()['a_pending']);close_gumps()
   do('combat');acquire_third();close_gumps();time.sleep(6);check('combat defers eligible observation',not q()['a_done']);do('combat');inventories();do('save',slot=1);do('load',slot=1);inventories();check('pending survives reload',q()['a_pending']and not q()['a_done']);review_checkpoint('a-iolo');[drop(o['ref'])for o in inv()if o['shape']==330];check('pending survives actual removal of every held stone',q()['colors']==0 and q()['a_pending']);close_gumps();until('Iolo delivers exactly one organizing observation',lambda:q()['a_done']);check('Iolo is primary and line is atmospheric',q()['last_npc']==1 and'case'not in q()['last_text'].lower());do('save',slot=2);do('load',slot=2);time.sleep(6);check('completed organizing observation never repeats after reload',q()['a_done']and q()['deliveries_since_load']==0)
   # Neither eligible organizer: actual dismissal, followed by actual Shamino recruitment.
   do('load',slot=0);dismiss_iolo();do('save',slot=7);acquire_third();close_gumps();time.sleep(6);check('absent eligible companions never speak',q()['a_pending']and q()['deliveries_since_load']==0);do('save',slot=9);do('load',slot=9);join(3,'a-shamino');until('deferred organizer uses Shamino fallback',lambda:q()['a_done']);check('organizer fallback speaker is Shamino',q()['last_npc']==3)
  # Native MARKs of two physical objects inside accepted case, with nobody eligible.
  do('load',slot=7);case=prepare_case();stones=inv(case);mark(case,stones[0]['ref']);check('one successful marked object does not qualify B',q()['marked_stones']==1 and not q()['b_pending']and not q()['b_done']);close_gumps();mark(case,stones[0]['ref']);check('re-marking one physical object does not qualify B',q()['marked_stones']==1 and not q()['b_pending']);close_gumps();mark(case,stones[1]['ref']);check('second distinct successfully marked object latches B',q()['marked_stones']==2 and q()['b_pending']and not q()['b_done']);do('save',slot=3);drop(case);close_gumps();check('pending marking observation survives stones leaving party',q()['marked_stones']==0 and q()['b_pending']);do('save',slot=4);do('load',slot=4);time.sleep(6);check('no absent marking speaker, pending survives load',q()['b_pending']and q()['deliveries_since_load']==0);join(3,'b-shamino');until('Shamino delivers preferred destination observation',lambda:q()['b_done']);check('preferred destination speaker is Shamino',q()['last_npc']==3);do('save',slot=5);do('load',slot=5);time.sleep(6);check('destination observation completed state survives reload',q()['b_done']and q()['deliveries_since_load']==0);do('test_case_companion_start',npc=1);worldcase=do('find_objects',shape=1601,radius=32)['objects'][0];do('move_to_object',ref=worldcase['ref'],timeout=30);check('player walks into real pickup reach of stored case',arrived());bag=inventories();do('transfer_item',ref=worldcase['ref'],destination=bag);close_gumps();mark(worldcase['ref'],inv(worldcase['ref'])[0]['ref']);close_gumps();time.sleep(6);check('additional successful MARK cannot repeat completed B',q()['b_done']and not q()['b_pending']and q()['deliveries_since_load']==0)
  do('load',slot=4);join(4,'b-dupre');until('Dupre delivers destination fallback',lambda:q()['b_done']);check('destination fallback speaker is Dupre',q()['last_npc']==4);do('save',slot=6);do('load',slot=6);check('fallback completion persists',q()['b_done'])
  # Two different physical stones of the SAME colour still count separately for B.
  do('load',slot=7);do('test_case_breadcrumb_items',scenario=2);twins=[o['ref']for o in inv()if o['shape']==330];containers=[]
  for ref in twins:
   close_gumps();c=do('test_case_extra')['synthetic_case']['ref'];inventories();show(c);do('move_gump',ref=c,x=310,y=5);owner=obj(ref)['owner'];avatar=do('inspect_world',radius=2)['avatar']
   if owner!=avatar:show(owner)
   do('transfer_item',ref=ref,destination=c);containers.append(c);close_gumps()
  mark(containers[0],twins[0]);check('remark B does not count one object twice',q()['marked_stones']==1 and not q()['b_pending']);mark(containers[1],twins[1]);check('two physical marked objects of one colour qualify B',q()['marked_stones']==2 and q()['colors']==1 and q()['b_pending']);close_gumps()
  # Both pending: one observation, then a persisted game-minute separation.
  do('load',slot=3);acquire_third();close_gumps();join(3,'both-pending');until('first of two pending observations is delivered',lambda:q()['a_done']);check('second observation is still pending',q()['b_pending']and not q()['b_done']);first=q()['next_minute'];do('save',slot=8);do('load',slot=8);time.sleep(2);check('reload does not bypass observation separation',q()['next_minute']==first and not q()['b_done']);do('native_wait',minutes=5);until('second observation is deferred to a later quiet opportunity',lambda:q()['b_done'],35);check('both complete exactly once',q()['a_done']and q()['b_done'])
  # Old ordinary-engine archive, no Stage E trailer; destinations are supplied
  # by the retained Stage A state fixture, NOT asserted as player MARK history.
  legacy=Path(r'C:/Codex/Ultima7 Decomp/test-output/virtue-stage-bc/runs')/('final-physical-keyring'if a.mod else'final-physical-bg')/'saves'/('mods/Keyring/saves'if a.mod else'.')/'exult03bg.sav';shutil.copy2(legacy,Path(d.state()['save_path'])/'exult09bg.sav');do('load',slot=9);check('old native save contains two or more valid distinct destinations',q()['marked_stones']>=2);until('old save acquires pending B without new MARK or metadata',lambda:q()['b_pending']);do('save',slot=6);do('load',slot=6);check('migrated old-save eligibility persists',q()['b_pending'])
  do('test_case_commission_start');npc=do('case_commission_state')['xanthia'];do('move_to_object',ref=npc['ref']);arrived();do('interact_object',ref=npc['ref']);settle();choose('Virtue Stone case');choose('commission case');leave();check('optional observations never gate real commission acceptance',do('case_commission_state')['commission']['stage']==1);do('save',slot=5);do('load',slot=5);check('Stage D and breadcrumb optional trailers coexist',do('case_commission_state')['commission']['stage']==1 and q()['b_pending']);do('exit')
 except Exception:
  d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
  try:
   if d.p and d.p.poll()is None:capture('failure')
  except Exception:pass
 finally:
  subprocess.Popen=original_popen
  if d.run:(d.run/'breadcrumb-checks.json').write_text(json.dumps(events,indent=2))
  result=d.close();print(json.dumps(result),flush=True)
 sys.exit(0 if result and result['result']=='PASS'else 1)
