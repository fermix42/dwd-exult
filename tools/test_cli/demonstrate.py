"""Run real gameplay demonstrations. Never creates synthetic game state."""
import argparse,json,time,traceback
from driver import Driver,BASE
ap=argparse.ArgumentParser();ap.add_argument('game',choices=['bg','si']);ap.add_argument('--mod',default='');ap.add_argument('--run',required=True);a=ap.parse_args()
d=Driver();records=[]
def record(c,r):
 records.append({'command':c,'response':r});print(json.dumps(records[-1]),flush=True)
 if d.run:(d.run/'demonstration.jsonl').write_text('\n'.join(json.dumps(x) for x in records)+'\n')
 return r
def do(cmd,**kw):
 c={'cmd':cmd,**kw};return record(c,d.execute(c))
def check(label,condition,evidence=None):
 d.assertions+=1
 if not condition:d.failures+=1
 record({'assert':label},{'ok':bool(condition),'category':'inspection','evidence':evidence})
 if not condition:raise AssertionError(label)
def state():return do('state')['state']
try:
 init=do('init',game=a.game,mod=a.mod,run=a.run);s=state();check('dummy video and zero native engine windows',s['video']=='dummy' and not d.desktop()['engine_windows'],d.desktop());check('mod overrides actually loaded',(s['overrides']>0)==bool(a.mod),s['overrides'])
 if a.game=='si':
  check('real opening owns player',s['dont_move']==1 and s['si_first_scene']==0)
  end=time.monotonic()+90;ack=0;choices=0;text_pages=[]
  while time.monotonic()<end:
   s=state()
   if s['input']:
    if s['choices_active']:do('choose',index=0);choices+=1
    else:
     text_pages.append(s['dialogue_text']);do('continue');ack+=1
   elif s['si_first_scene'] and not s['dont_move'] and not s['in_usecode'] and not s['moving']:break
   time.sleep(.15)
  else:raise TimeoutError('natural SI opening timed out')
  check('opening dialogue text observed on multiple pages',len(text_pages)>1 and all(text_pages),{'pages':len(text_pages),'sample':text_pages[0] if text_pages else ''})
  check('opening completes through real Usecode dialogue acknowledgements',ack>0 and s['si_first_scene']==1 and s['dont_move']==0,{'acknowledgements':ack,'choices':choices})
  # Initiate a real companion conversation using the same hit test as double-click.
  actors=do('find_actors',radius=20,limit=100)['objects'];target=next(n for n in actors if n['npc']==3)
  do('interact_object',ref=target['ref']);end=time.monotonic()+25;selected=[];refusal_checked=False
  while time.monotonic()<end:
   s=state()
   if s['input']:
    if s['choices_active']:
     if not refusal_checked:
      try:do('dismiss');raise AssertionError('dismiss incorrectly chose a response')
      except RuntimeError as e:record({'cmd':'dismiss','expected_refusal':True},{'ok':False,'category':'inspection','reason':str(e)})
      unchanged=state();check('dismiss refused at answer menu without changing choices',unchanged['choices']==s['choices'] and unchanged['choices_active']==1);refusal_checked=True
     text='bye' if 'bye' in s['choices'] else s['choices'][0];do('choose',text=text);selected.append(text)
    else:do('continue')
   elif selected and not s['in_usecode'] and not s['faces']:break
   time.sleep(.1)
  check('real companion dialogue response selected and returned',bool(selected) and not s['in_usecode'] and not s['faces'],selected)
 else:
  initial=(s['x'],s['y'],s['z']);do('combat');do('assert',state={'combat':1});do('combat');do('assert',state={'combat':0})
  do('move',x=180,y=100);do('wait',timeout=1);do('wait',until={'moving':0},timeout=10);s=state();check('ordinary pathfinding changes Avatar position',(s['x'],s['y'],s['z'])!=initial,{'before':initial,'after':[s['x'],s['y'],s['z']]})
 # Snapshot/reload is verified against observed state, rather than process exit.
 do('wait',until={'moving':0,'input':0,'in_usecode':0,'dont_move':0},timeout=10)
 before=state();do('save',slot=0);do('load',slot=0);after=state();keys=['x','y','z','map','health','day','hour','minute','dont_move','can_act','si_first_scene','overrides']
 check('real save reload preserves player and clock',{k:before[k] for k in keys}=={k:after[k] for k in keys},{'before':{k:before[k] for k in keys},'after':{k:after[k] for k in keys}})
 do('screenshot');check('offscreen PNG exists',(d.run/'screenshot.png').stat().st_size>100)
 check('engine never owns a native desktop window at checks',not d.desktop()['engine_windows'],d.desktop());do('exit')
except Exception as e:
 d.failures+=1;record({'event':'failure'},{'ok':False,'error':str(e),'traceback':traceback.format_exc()})
finally:
 result=d.close();print(json.dumps({'result':result}),flush=True)
raise SystemExit(0 if result and result['result']=='PASS' else 1)
