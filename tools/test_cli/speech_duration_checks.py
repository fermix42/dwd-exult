"""Global speech preference through native Options handlers and real queue entries."""
import argparse,json,subprocess,sys,time,traceback,shutil,xml.etree.ElementTree as ET
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from driver import Driver,BASE,EngineRefusal
p=argparse.ArgumentParser();p.add_argument('--run',required=True);p.add_argument('--game',choices=['bg','si'],default='bg');p.add_argument('--mod',default='');p.add_argument('--startup');p.add_argument('--relaunch-cfg',type=Path);p.add_argument('--startup-only',action='store_true');a=p.parse_args();d=Driver();events=[]
original=subprocess.Popen

def launch(args,**kw):
 if '--test-cli'in args:
  args=list(args);cfg=Path(args[args.index('-c')+1])
  if a.relaunch_cfg:
   cfg=a.relaunch_cfg.resolve();args[args.index('-c')+1]=str(cfg);args[args.index('--test-run-dir')+1]=str(cfg.parent)
  else:
   tree=ET.parse(cfg);gameplay=tree.getroot().find('gameplay')
   if a.startup is not None:ET.SubElement(gameplay,'overhead_text_duration').text=a.startup
   tree.write(cfg,encoding='unicode')
 return original(args,**kw)
subprocess.Popen=launch

def do(cmd,**kw):
 c=dict(cmd=cmd,**kw);r=d.execute(c);events.append(dict(command=c,response=r));return r

def check(label,value):
 d.assertions+=1;d.failures+=not bool(value);events.append(dict(check=label,passed=bool(value)));print(label,bool(value),flush=True)
 if not value:raise AssertionError(label)

def until(label,predicate,seconds=8):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  value=predicate()
  if value:return value
  time.sleep(.025)
 raise TimeoutError(label)

def controls():return [c for c in do('controls')['controls']if c['available']]
def duration_button():return next(c for c in controls()if c['label'].split('(')[0].strip()in ['Short','Normal','Longer','Double','Triple'])
def open_options():
 do('key',key='escape');until('Options menu opens',lambda:any('Gamemenu'in g['type']for g in d.state()['gumps']));menu=next(c for c in controls()if c['label']=='Game Display');do('activate_control',id=menu['id']);until('Game Display opens',lambda:any('GameDisplayOptions'in g['type']for g in d.state()['gumps']))
def close_options(save=True,keyboard=False):
 if keyboard:do('key',key='enter'if save else'escape')
 else:do('activate_control',id=next(c for c in controls()if c['label']==('OK'if save else'CANCEL'))['id'])
 until('Game Display closes',lambda:not any('GameDisplayOptions'in g['type']for g in d.state()['gumps']));do('key',key='escape');until('Options closes',lambda:d.state()['input_owner']=='gameplay')
def select(percent,mouse=False):
 open_options()
 for _ in range(5):
  button=duration_button()
  if '('+str(percent)+'%)'in button['label']:break
  old=button['label']
  if mouse:do('activate_control',id=button['id'])
  else:do('key',key='right')
  until('native duration selection updates',lambda:duration_button()['label']!=old)
 check('native Options selects '+str(percent),'('+str(percent)+'%)'in duration_button()['label']);close_options(keyboard=not mouse);check('runtime preference applies without restart',d.state()['overhead_text_duration']==percent)
def values():
 tree=ET.parse(a.relaunch_cfg or d.run/'exult.cfg')
 def walk(el,path):
  if not list(el):yield path,(el.text or'').strip()
  for child in el:yield from walk(child,path+'/'+child.tag)
 return dict(walk(tree.getroot(),'config'))
def lifetime(kind,limit):
 r=do('test_text_lifetime',kind=kind);check('genuine queue lifetime kind '+str(kind),r['normal_queue_dispatch']and r['limit']==limit and len(r['samples'])==limit+1)
 for sample in r['samples']:
  expected=sample['step']<limit;check('visibility at update '+str(sample['step']),bool(sample['alive'])==expected);check('one callback or complete cleanup '+str(sample['step']),sample['entries']==int(expected))
 check('expired layer/effect cleanup completes',not do('text_effects')['effects'])
def capture(name):
 do('screenshot',full=True);shutil.copy2(d.run/'screenshot.png',d.run/(name+'.png'))
if __name__=='__main__':
 try:
  do('init',game=a.game,mod=a.mod,run=a.run)
  expected=int(a.startup)if a.startup in ['50','100','150','200','300']else 100
  if a.relaunch_cfg:expected=300
  check('startup default/validated preference',d.state()['overhead_text_duration']==expected)
  if a.startup_only:
   lifetime(0,expected//10);do('exit')
  else:
   lifetime(0,10);open_options();check('Normal appears by default','100%'in duration_button()['label']);do('key',key='left');until('Left cycles backwards',lambda:'(50%)'in duration_button()['label']);check('native Left selects Short','(50%)'in duration_button()['label']);do('key',key='right');until('Right returns to Normal',lambda:'(100%)'in duration_button()['label']);capture('game-display-normal');do('key',key='right');until('unsaved Longer selection',lambda:'(150%)'in duration_button()['label']);close_options(save=False,keyboard=True);check('Escape cancels an unsaved duration change',d.state()['overhead_text_duration']==100);open_options();close_options();baseline=values()
   for index,percent in enumerate([50,100,150,200,300]):
    select(percent,mouse=index%2==0);open_options();check('reopening Options retains chosen value','('+str(percent)+'%)'in duration_button()['label']);close_options(save=False)
    current=values();check('other normalized settings remain unchanged',{k:v for k,v in current.items()if not k.endswith('/overhead_text_duration')}=={k:v for k,v in baseline.items()if not k.endswith('/overhead_text_duration')})
    lifetime(0,percent//10);lifetime(3,percent//10);lifetime(4,percent//10);lifetime(1,10);lifetime(2,10)
   open_options();capture('game-display-triple');close_options(save=False)
   protected_si=a.game=='si'and(d.state()['dont_move']or not d.state()['can_act'])
   if protected_si:
    events.append(dict(not_tested='SI active-bark retention/moving-actor visual scenario: original opening owns player control; BG/Keyring cover these checks.'))
    check('SI original protected opening remains intact',bool(d.state()['dont_move']or not d.state()['can_act']))
   else:
    # Visible speech captures its original timer even when the global preference changes.
    avatar=do('inspect_world',radius=2)['avatar'];do('test_overhead',ref=avatar,scenario=0,edge=0);old=do('text_effects')['effects'][0];check('new bark captures Triple',old['lifetime_ticks']==30);select(50,mouse=True);retained=next(e for e in do('text_effects')['effects']if e['item']==avatar);check('visible bark retains original expiration',retained['lifetime_ticks']==30);do('test_overhead',ref=avatar,scenario=0,edge=0);check('subsequent bark uses Short',do('text_effects')['effects'][0]['lifetime_ticks']==5)
    select(300);do('test_overhead',ref=avatar,scenario=0,edge=0);before=do('text_effects')['effects'][0];at=d.state();moved=False
    for dx,dy in [(2,0),(0,2),(-2,0),(0,-2)]:
     target=dict(x=at['x']+dx,y=at['y']+dy,z=at['z'])
     if do('inspect_tile',**target)['blocked']:continue
     do('move_to',**target,timeout=5)
     if until('normal movement finishes',lambda:do('movement_status')['movement']!='moving',7):
      moved=(d.state()['x'],d.state()['y'])!=(at['x'],at['y'])
      if moved:break
    check('long-lived spoken actor moves through actual gameplay',moved);after=next(e for e in do('text_effects')['effects']if e['item']==avatar);check('longer bark follows moving actor',after['anchor']!=before['anchor']);capture('moving-actor-triple');do('test_overhead',ref=avatar,scenario=-2,edge=0);check('scene clear removes longer text and layers',not do('text_effects')['effects'])
   check('Triple persisted through ordinary config save',values()['config/gameplay/overhead_text_duration']=='300');do('exit')
 except Exception:
  d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
  try:do('exit')
  except Exception:pass
 finally:
  subprocess.Popen=original
  if d.run:(d.run/'speech-duration-checks.json').write_text(json.dumps(events,indent=2))
  result=d.close();print(json.dumps(result),flush=True)
 sys.exit(0 if result and result['result']=='PASS'else 1)
