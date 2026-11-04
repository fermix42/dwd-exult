"""Actual production overhead layouts and rendered frames; camera-only fixtures."""
import argparse,copy,json,math,shutil,subprocess,sys,time,traceback,xml.etree.ElementTree as ET
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import Driver,BASE
p=argparse.ArgumentParser();p.add_argument('--run',required=True);p.add_argument('--mod',default='',choices=('','Keyring'));p.add_argument('--scale',type=int,default=2);p.add_argument('--world-width',type=int,default=640);p.add_argument('--world-height',type=int,default=400);p.add_argument('--text-width',type=int,default=640);p.add_argument('--text-height',type=int,default=400);p.add_argument('--display-width',type=int);p.add_argument('--display-height',type=int);p.add_argument('--pages',action='store_true');p.add_argument('--native-breadcrumb',action='store_true');a=p.parse_args();d=Driver();events=[]
original=subprocess.Popen

def launch(args,**kw):
 if '--test-cli'in args:
  cfg=Path(args[args.index('-c')+1]);tree=ET.parse(cfg);v=tree.getroot().find('video')
  settings={'game/width':a.world_width,'game/height':a.world_height,'display/width':a.display_width or a.world_width*a.scale,'display/height':a.display_height or a.world_height*a.scale,'ui/width':a.world_width,'ui/height':a.world_height,'ui/text_effect/width':a.text_width,'ui/text_effect/height':a.text_height,'ui/text_effect/fill_mode':'Fit','ui/text_effect/scale_method':'Point','ui/text_effect/fill_scaler':'Point','ui/universal':'no','ui/scale_method':'Point','ui/fill_mode':'Fit','fill_mode':'Fit','fill_scaler':'Point','scale':a.scale,'scale_method':'Point'}
  for key,value in settings.items():
   el=v
   for name in key.split('/'):
    child=el.find(name)
    if child is None:child=ET.SubElement(el,name)
    el=child
   el.text=str(value)
  tree.write(cfg,encoding='unicode')
 return original(args,**kw)
subprocess.Popen=launch

def do(cmd,**kw):
 c=dict(cmd=cmd,**kw);r=d.execute(c);events.append(dict(command=c,response=r));return r

def check(label,value):
 d.assertions+=1;d.failures+=not value;events.append(dict(check=label,passed=bool(value)))
 if not value:raise AssertionError(label)

def capture(name):
 do('screenshot',full=True);shutil.copy2(d.run/'screenshot.png',d.run/(name+'.png'))

def effect(ref):
 for _ in range(20):
  effects=do('text_effects')['effects'];found=next((e for e in effects if e['item']==ref and e['width']>0),None)
  if found:return found
  time.sleep(.03)
 raise RuntimeError('production effect was not laid out')

def fixture(ref,scenario,edge,label):
 do('test_overhead',ref=ref,scenario=scenario,edge=edge);time.sleep(.06);capture(label);e=effect(ref);validate(label,e);return e

def validate(label,e):
 r=e['destination'];v=e['viewport'];anchor=e['anchor'];f=e['scale'];page=e['page'];first=page*e['lines_per_page'];lines=e['lines'][first:first+e['lines_per_page']]
 check(label+' display containment',r['x']>=v['x']and r['y']>=v['y']and r['x']+r['w']<=v['x']+v['w']and r['y']+r['h']<=v['y']+v['h'])
 check(label+' measured line containment',all(w+10<=e['width']for w in e['line_widths'][first:first+len(lines)]))
 check(label+' coherent scaled dimensions',r['w']==math.ceil(e['width']*f)and r['h']==math.ceil(e['height']*f))
 ideal=anchor['x']+(anchor['w']-r['w'])//2
 if v['x']+2<=ideal<=v['x']+v['w']-r['w']-2:check(label+' centered above speaker',abs((r['x']+r['w']/2)-(anchor['x']+anchor['w']/2))<=1)
 if anchor['y']-r['h']-math.ceil(2*f)>=v['y']+2:check(label+' above sprite',r['y']+r['h']<=anchor['y']-math.ceil(2*f))
 elif anchor['y']+anchor['h']+math.ceil(2*f)+r['h']<=v['y']+v['h']-2:check(label+' below top-edge sprite',r['y']>=anchor['y']+anchor['h']+math.ceil(2*f))

try:
 if a.native_breadcrumb:
  profile='keyring'if a.mod else'bg';checkpoint=Path('C:/Codex/Ultima7 Decomp/test-output/virtue-stage-e-final/human-review/prepared')/profile/'a-iolo/exult08bg.sav'
  do('init',game='bg',mod=a.mod,run=a.run,checkpoint=str(checkpoint));do('load',slot=8)
  iolo=next(o for o in do('find_actors',radius=32,limit=200)['objects']if o.get('npc')==1)
  end=time.monotonic()+25
  while time.monotonic()<end:
   state=do('case_breadcrumbs_state')['breadcrumbs']
   if state['a_done']:break
   time.sleep(.03)
  check('actual breadcrumb delivered through ordinary gameplay',state['a_done']and state['last_npc']==1)
  capture('native-iolo-breadcrumb');e=effect(iolo['ref']);validate('native-iolo-breadcrumb',e)
  check('actual spoken text retained in wrapped effect',e['text']==state['last_text']and' '.join(e['lines']).split()==state['last_text'].split());do('exit')
 else:
  do('init',game='bg',mod=a.mod,run=a.run)
  actors=do('find_actors',radius=32,limit=200)['objects'];iolo=next(o for o in actors if o.get('npc')==1);petre=next(o for o in actors if o.get('npc')==11);mayor=next(o for o in actors if o.get('npc')==12)
  before={o['npc']:(o['tile'],o['schedule'],o['frame'])for o in [petre,mayor]}
  short=fixture(iolo['ref'],0,0,'short-centered');check('short remark stays on one line',len(short['lines'])==1)
  for scenario in [1,2,3,4]:
   e=fixture(iolo['ref'],scenario,0,'accepted-line-'+str(scenario));check('approved message text retained '+str(scenario),' '.join(e['lines']).split()==e['text'].split())
   if scenario==1:check('Iolo long message wraps by font metrics',len(e['lines'])>=2)
  for edge,name in [(1,'left'),(2,'right'),(3,'top'),(4,'bottom'),(6,'top-left'),(7,'bottom-right')]:fixture(petre['ref'],1,edge,'edge-'+name)
  initial=fixture(petre['ref'],0,0,'before-scroll');do('test_overhead',ref=petre['ref'],scenario=-1,edge=5);time.sleep(.06);capture('after-scroll');after=effect(petre['ref']);validate('after-scroll',after);check('same speech follows camera projection',after['text']==initial['text']and after['anchor']!=initial['anchor']and after['destination']!=initial['destination'])
  explicit=fixture(petre['ref'],5,0,'explicit-breaks');check('explicit breaks and blank paragraph retained',explicit['lines']==['First line.','Second line.','','Final paragraph.'])
  word=fixture(petre['ref'],6,0,'long-word');check('long word fully retained across measured lines',''.join(word['lines'])=='W'*200+'!')
  small=min(actors,key=lambda o:o['sprite']['height']);big=max(actors,key=lambda o:o['sprite']['height']);check('real actor size coverage',small['sprite']['height']!=big['sprite']['height']);fixture(small['ref'],0,0,'small-actor');fixture(big['ref'],0,0,'large-actor')
  if a.pages:
   e=fixture(petre['ref'],7,0,'oversized-page-0');expected=e['pages'];seen={0};end=time.monotonic()+expected*3+3
   while time.monotonic()<end:
    effects=do('text_effects')['effects'];current=next((e for e in effects if e['item']==petre['ref']),None)
    if current is None:break
    if current['page']not in seen:
     validate('page-'+str(current['page']),current)
     seen.add(current['page']);capture('oversized-page-'+str(current['page']))
    time.sleep(.15)
   check('oversized message uses every page without silent truncation',expected>1 and seen==set(range(expected)))
  do('test_overhead',ref=iolo['ref'],scenario=-2);time.sleep(.1);check('expired or cleared text layers removed',not do('text_effects')['effects'])
  for npc,(tile,schedule,frame)in before.items():
   now=do('inspect_actor',ref=next(o['ref']for o in [petre,mayor]if o['npc']==npc))['object'];check('camera fixture preserves NPC '+str(npc),now['tile']==tile and now['schedule']==schedule and now['frame']==frame)
  do('exit')
except Exception:
 d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
 try:do('exit')
 except Exception:pass
finally:
 subprocess.Popen=original
 if d.run:(d.run/'overhead-checks.json').write_text(json.dumps(events,indent=2))
 result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
