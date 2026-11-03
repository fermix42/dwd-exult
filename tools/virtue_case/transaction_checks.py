"""Real NPC transaction and construction-start checks; no quest flags are forced."""
import argparse,json,sys,time,traceback,subprocess
import xml.etree.ElementTree as ET
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import Driver,EngineRefusal
p=argparse.ArgumentParser();p.add_argument('--run',required=True);p.add_argument('--mod',default='',choices=('','Keyring'));p.add_argument('--checkpoint',type=Path,required=True);a=p.parse_args();d=Driver();events=[]
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
heard=[]
def settle():
 for _ in range(200):
  s=d.state()
  text=s.get('dialogue_text','')
  if text and (not heard or heard[-1]!=text):heard.append(text);events.append(dict(dialogue_text=text))
  if 'continue'in s['legal_actions']:do('continue')
  elif s['input_owner']in ['answer_menu','gameplay']:return s
  else:time.sleep(.02)
 raise TimeoutError('input ownership')
def choose(text):heard.clear();do('choose',text=text);return settle()
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
try:
 do('init',game='bg',mod=a.mod,run=a.run,checkpoint=str(a.checkpoint.resolve()));do('load',slot=3);check('checkpoint is accepted commission from real conversation',q()['stage']==1);do('save',slot=1)
 do('test_case_commission_items',scenario=5);s=talk();before=q();s=choose('give cloth');check('missing cloth changes no inventory or requirement',q()['cloth']==0 and q()['requirements']==0 and q()['gold']==before['gold']);check('failed cloth topic suppressed this conversation','give cloth'not in s['choices']);leave();s=talk();check('unfinished cloth topic returns next conversation','give cloth'in s['choices']);leave()
 do('test_case_commission_items',scenario=4);s=talk();s=choose('pay Guild fee');check('insufficient Guild fee deducts nothing and leaves quest pending',q()['gold']==0 and q()['requirements']==0);check('failed fee topic suppressed','pay Guild fee'not in s['choices']);leave();s=talk();check('unfinished fee returns next conversation','pay Guild fee'in s['choices']);leave()
 do('test_case_commission_items',scenario=2);s=talk();before=q();s=choose('give cloth');check('companion cloth handoff consumes exactly one',q()['cloth']==before['cloth']-1 and q()['requirements']==2);check('successful cloth topic immediately disappears','give cloth'not in s['choices']);leave();do('save',slot=2);do('load',slot=2);check('cloth completion survives normal reload',q()['requirements']==2)
 s=talk();before=q();s=choose('pay Guild fee');check('Guild fee consumes exactly100 gold',q()['gold']==before['gold']-100 and q()['requirements']==6);check('completed fee and cloth topics removed','pay Guild fee'not in s['choices']and'give cloth'not in s['choices']);leave();do('save',slot=4)
 do('test_case_commission_items',scenario=4);s=talk(93);check('William original topics preserved','job'in s['choices']and'bye'in s['choices']);s=choose('timber delivery');check('William discusses timber and Guild without knowing the case',bool(heard)and not any(w in' '.join(heard).lower()for w in ['xanthia','virtue','case','two days','construction']));check('William quotes price before purchase',q()['gold']==0 and'buy timber'in s['choices']);s=choose('buy timber');check('insufficient timber payment changes nothing',q()['gold']==0 and q()['requirements']==6);check('unchanged failed timber purchase suppressed','buy timber'not in s['choices']and'timber delivery'not in s['choices']);leave()
 do('test_case_commission_items',scenario=1);s=talk(93);choose('timber delivery');before=q();s=choose('buy timber');after=q();check('William final timber receipt never announces construction or duration',bool(heard)and not any(w in' '.join(heard).lower()for w in ['xanthia','virtue','case','two days','construction']));check('timber costs20 gold exactly once',after['gold']==before['gold']-20);check('last requirement starts construction for exactly48 hours',after['requirements']==7 and after['stage']==3 and after['deadline']==after['minutes']+2880);check('William timber topics permanently removed','timber delivery'not in s['choices']and'buy timber'not in s['choices']);leave();deadline=q()['deadline'];do('save',slot=5);do('load',slot=5);check('construction deadline survives normal save/load',q()['deadline']==deadline and q()['stage']==3)
 # Progress/ready dialogue is covered by the dedicated construction suite.
 do('load',slot=1);do('test_case_commission_items',scenario=1);talk();choose('pay Guild fee');check('fee can be first requirement',q()['requirements']==4);choose('give cloth');check('cloth can follow fee before timber',q()['requirements']==6);leave();talk(93);choose('timber delivery');choose('buy timber');leave();check('alternate payment/cloth order also begins construction',q()['requirements']==7 and q()['stage']==3);do('save',slot=7);do('load',slot=1);do('test_case_commission_items',scenario=1);talk(93);choose('timber delivery');choose('buy timber');leave();check('timber alone may be the first fulfilled requirement',q()['requirements']==1 and q()['stage']==2);do('save',slot=8);do('exit')
except Exception:
 d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
finally:
 subprocess.Popen=original_popen
 if d.run:(d.run/'transaction-checks.json').write_text(json.dumps(events,indent=2))
 result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
