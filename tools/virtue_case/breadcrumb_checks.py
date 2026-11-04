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
  if predicate():
   check(label,True)
   if label in ['Iolo delivers exactly one organizing observation','deferred organizer uses Shamino fallback','Shamino delivers preferred destination observation','Dupre delivers destination fallback']:
    capture('short-speech-'+str(q()['last_npc'])+'-'+('A'if q()['a_done']and not q()['b_done']else'B'))
    message=next(e for e in do('text_effects')['effects']if e['text']==q()['last_text'])
    check('short candidate uses one native line',len(message['lines'])==1)
    r=message['destination'];v=message['viewport'];check('short candidate stays inside game viewport',r['x']>=v['x']and r['y']>=v['y']and r['x']+r['w']<=v['x']+v['w']and r['y']+r['h']<=v['y']+v['h'])
   return
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
 close_gumps();bag=inventories()
 owner=obj(ref)['owner'];avatar=do('inspect_world',radius=2)['avatar']
 if owner!=avatar:show(owner)
 if owner not in [avatar,bag]:do('move_gump',ref=owner,x=310,y=5)
 at=d.state()
 offsets=[(-4,2),(-4,4),(-3,-3),(3,2)]if obj(ref)['shape']==1601 else[(3,2),(4,2),(5,2),(-3,2),(3,4),(-4,4)]
 for dx,dy in offsets:
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
def load_review(name,context=False):
 profile='keyring'if a.mod else'bg'
 source=Path('C:/Codex/Ultima7 Decomp/test-output/virtue-stage-e-final/human-review')/('prepared-context'if context else'prepared')/profile/name/'exult08bg.sav'
 shutil.copy2(source,Path(d.state()['save_path'])/'exult08bg.sav');do('load',slot=8)

def extract_case():
 """Remove actual stones and drop the real case; no destination/quest setters."""
 close_gumps();case=next(o['ref']for o in inv()if o['shape']==1601);bag=inventories();show(case);do('move_gump',ref=case,x=310,y=5)
 for stone in inv(case):do('transfer_item',ref=stone['ref'],destination=bag)
 close_gumps();drop(case);close_gumps()
 check('normal extraction retains stones and ends current case ownership',not q()['case_owned'])
 return case

def hold_review(name):
 close_gumps();inventories();export_checkpoint(name)
 check('prepared snapshot contains no spoken hints',not q()['a_done']and not q()['b_done'])

def natural(hint,label,walk=False,seconds=55,capture_line=False):
 close_gumps();end=time.monotonic()+seconds;steps=0;last_move=0;origin=d.state()
 while time.monotonic()<end:
  state=q()
  if state[hint+'_done']:
   check(label,True)
   if walk:check('ordinary exploration issued real pathfinding actions',steps>0)
   if capture_line:
    capture('natural-'+str(state['last_npc'])+'-'+hint)
    effect=next(e for e in do('text_effects')['effects']if e['text']==state['last_text'])
    check('exact candidate displays in one native line',len(effect['lines'])==1)
    r=effect['destination'];v=effect['viewport'];check('native candidate remains in viewport',r['x']>=v['x']and r['y']>=v['y']and r['x']+r['w']<=v['x']+v['w']and r['y']+r['h']<=v['y']+v['h'])
   return state
  if walk and time.monotonic()-last_move>1 and do('movement_status')['movement']!='moving':
   at=d.state()
   for dx,dy in [(6,0),(0,6),(-6,0),(0,-6),(4,4),(-4,-4)]:
    dest=dict(x=origin['x']+dx if steps%2==0 else origin['x'],y=origin['y']+dy if steps%2==0 else origin['y'],z=origin['z'])
    if do('inspect_tile',**dest)['blocked']:continue
    try:
     response=do('move_to',**dest,timeout=6)
     if response['movement']in ['moving','reached']:steps+=1;break
    except EngineRefusal:continue
   last_move=time.monotonic()
  time.sleep(.08)
 check(label,False)

def loose_mark(stone,spell=34):
 close_gumps();do('test_case_spell_setup',scenario=0);bag=inventories();book=next(o['ref']for o in inv()if o['class_id']==8);show(book);do('move_gump',ref=book,x=310,y=5)
 before=do('case_spell_state');do('spellbook_cast',ref=book,spell=spell);do('aim_object',ref=stone)
 return book,before

if __name__=='__main__':
 try:
  keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n2 INVENTORY 1\n')
  do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys))
  load_review('a-iolo');close_gumps();before=inv();check('old organized loose inventory qualifies without handling',q()['loose_colors']>=3 and q()['a_pending']and not q()['natural_due']);time.sleep(3);check('load never produces an instant remark',not q()['a_done'])
  state=natural('a','organized old inventory speaks naturally during exploration',True,capture_line=True);check('Iolo remains preferred',state['last_npc']==1);check('exploration never moved or replaced a stone',[(o['ref'],o['owner'])for o in before if o['shape']==330]==[(o['ref'],o['owner'])for o in inv()if o['shape']==330]);do('save',slot=2);do('load',slot=2);time.sleep(12);check('completed A never repeats on load',q()['a_done']and q()['deliveries_since_load']==0)
  load_review('a-shamino');natural('a','Shamino accumulation fallback is natural',True,capture_line=True);check('fallback remains Shamino',q()['last_npc']==3)
  load_review('a-iolo');close_gumps();do('test_case_breadcrumb_items',scenario=0);time.sleep(12);check('two unmarked colors trigger neither hint',q()['colors']==2 and not q()['a_pending']and not q()['a_done']and not q()['b_pending']);duplicate=do('test_case_breadcrumb_items',scenario=3)['synthetic_world_stone'];bag=inventories();do('transfer_item',ref=duplicate['ref'],destination=bag);close_gumps();time.sleep(3);check('duplicate physical stone does not add a third color',q()['colors']==2 and not q()['a_pending']);acquire_third();close_gumps();natural('a','ordinary third-color acquisition becomes naturally observable');check('one observation only',q()['deliveries_since_load']==1)
  load_review('a-iolo');close_gumps();dismiss_iolo();time.sleep(12);check('absent companions stay silent without consuming A',q()['a_pending']and not q()['a_done']);do('save',slot=3);do('load',slot=3);join(3);natural('a','original recruitment permits later natural A with no stone action');check('joined fallback speaks exactly once',q()['last_npc']==3 and q()['deliveries_since_load']==1)
  for name,npc in [('b-shamino',3),('b-dupre',4)]:
   load_review(name,True);check('older owned case makes both remarks obsolete',q()['case_owned']and not q()['a_pending']and not q()['b_pending']);extract_case();stones=[o for o in inv()if o['shape']==330];drop(stones[-1]['ref']);close_gumps();hold_review(name);do('load',slot=9);check('one existing valid marked object suffices for B',q()['marked_stones']==1 and q()['b_pending']and not q()['a_pending']);before=[(o['ref'],o['stone_target'])for o in inv()if o['shape']==330];natural('b','old marked stone naturally prompts '+name,True,capture_line=True);check('correct destination speaker',q()['last_npc']==npc);check('natural travel retains original saved destination',before==[(o['ref'],o['stone_target'])for o in inv()if o['shape']==330]);do('save',slot=4);do('load',slot=4);time.sleep(12);check('B completion survives load without repeating',q()['b_done']and q()['deliveries_since_load']==0)
  load_review('both-pending');extract_case();hold_review('already-organized');do('load',slot=9);check('already-organized checkpoint contains loose and old marked stones',q()['loose_colors']>=3 and q()['marked_stones']>=1 and q()['a_pending']and q()['b_pending']);natural('a','first natural opportunity delivers A only');check('B remains pending after one opportunity',q()['b_pending']and not q()['b_done']);first=q()['next_minute'];do('save',slot=5);do('load',slot=5);check('saved five-minute separation is preserved',q()['next_minute']==first and not q()['b_done']);do('native_wait',minutes=5);check('Native Wait cannot deliver either pending remark',q()['deliveries_since_load']==0 and not q()['b_done']);natural('b','later ordinary travel delivers B without another event',True);check('both completion flags remain independent',q()['a_done']and q()['b_done']);do('save',slot=6);do('load',slot=6);check('both completions persist',q()['a_done']and q()['b_done'])
  load_review('a-iolo');close_gumps()
  for stone in reversed([o for o in inv()if o['shape']==330]):drop(stone['ref'])
  close_gumps();do('save',slot=7);do('load',slot=7);time.sleep(12);check('old pending hint is revalidated after every stone leaves',not q()['a_pending']and not q()['a_done']and q()['colors']==0)
  # Owning a case cancels both pending hints without falsely completing them.
  load_review('both-pending');check('nested owned case suppresses old pending flags after load',q()['case_owned']and not q()['a_pending']and not q()['b_pending']and not q()['a_done']and not q()['b_done']);time.sleep(12);check('seated stones do not count as loose and no obsolete line speaks',q()['loose_colors']<3 and q()['deliveries_since_load']==0)
  load_review('delivered-silent');check('earned permanent delivery suppresses hints',do('case_commission_state')['commission']['stage']==5 and not q()['a_pending']and not q()['b_pending']);time.sleep(12);check('completed quest stays silent',q()['deliveries_since_load']==0)
  do('exit')
 except Exception:
  d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
  try:do('exit')
  except Exception:pass
 finally:
  subprocess.Popen=original_popen
  if d.run:(d.run/'breadcrumb-checks.json').write_text(json.dumps(events,indent=2))
  result=d.close();print(json.dumps(result),flush=True)
 sys.exit(0 if result and result['result']=='PASS'else 1)
