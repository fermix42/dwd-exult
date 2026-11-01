"""Semantic acceptance: no screenshots, fixtures, or desktop input."""
import argparse,json,time,traceback,subprocess,ctypes
from driver import Driver,BASE
p=argparse.ArgumentParser();p.add_argument('--run',required=True);args=p.parse_args();d=Driver();records=[]
def log(c,r):
 records.append({'command':c,'response':r});print(json.dumps(records[-1]),flush=True)
 if d.run:(d.run/'acceptance.jsonl').write_text('\n'.join(json.dumps(e) for e in records)+'\n')
 return r
def do(cmd,**kw):return log({'cmd':cmd,**kw},d.execute({'cmd':cmd,**kw}))
def check(label,condition,evidence=None):
 d.assertions+=1
 if not condition:d.failures+=1
 log({'assert':label},{'ok':bool(condition),'category':'inspection','evidence':evidence})
 if not condition:raise AssertionError(label)
def refused(cmd,contains,**kw):
 try:do(cmd,**kw)
 except RuntimeError as e:
  log({'cmd':cmd,'expected_refusal':True},{'ok':False,'category':'inspection','detail':getattr(e,'detail',None),'error':str(e)});check('refusal: '+contains,contains in str(e));return
 raise AssertionError('command should have been refused: '+cmd)
def state():return do('state')['state']
def until(predicate,timeout=10):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  s=state()
  if predicate(s):return s
  time.sleep(.08)
 raise TimeoutError('engine state wait')
def movement(timeout=10):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  r=do('movement_status')
  if r['movement']!='moving':return r
  time.sleep(.08)
 raise TimeoutError('movement status wait')
try:
 do('init',game='bg',run=args.run);s=state();initial=(s['x'],s['y'],s['z']);w=do('inspect_world',radius=12,limit=100);avatar=w['avatar'];objects=w['objects'];actor=next(o for o in objects if o['type']=='actor' and o['npc']!=0)
 check('semantic actor identity and position',bool(actor['name']) and actor['map']==s['map'] and 'tile' in actor,actor)
 check('stable session reference',do('inspect_actor',ref=actor['ref'])['object']['ref']==actor['ref'])
 paged=do('find_objects',radius=12,limit=2,offset=1);check('bounded object query and pagination',len(paged['objects'])<=2 and paged['offset']==1)
 check('shape filtered semantic query',all(o['shape']==actor['shape'] for o in do('find_objects',radius=12,limit=100,shape=actor['shape'])['objects']))
 blocked=None
 for o in objects:
  if o['solid'] and o['tile']['z']==0 and o['type']!='actor':
   t=do('inspect_tile',**o['tile']);r=do('path_query',**o['tile'])
   if t['blocked'] and not r['found']:blocked=o['tile'];break
 check('real world obstacle and failed native route found',blocked is not None,blocked)
 obstacle=do('inspect_tile',**blocked);check('obstruction resolves to real object reference',obstacle['obstacle']>0,do('inspect_object',ref=obstacle['obstacle']))
 dest=None
 for dx,dy in [(2,0),(0,2),(-2,0),(0,-2)]:
  t={'x':initial[0]+dx,'y':initial[1]+dy,'z':initial[2]};r=do('path_query',**t)
  if r['found'] and not r['blocked_destination'] and r['steps']>0:dest=t;break
 check('valid native route found',dest is not None,dest);s=state();check('world and route inspection do not reposition Avatar',(s['x'],s['y'],s['z'])==initial)
 do('move_to',**dest);r=movement();check('movement reached actual requested destination',r['movement']=='reached' and r['actual']==dest,r)
 before=state();r=do('move_to',**blocked);check('blocked movement is not reported as success',r['movement']=='blocked',r);after=state();check('blocked destination preserved actual position',(before['x'],before['y'],before['z'])==(after['x'],after['y'],after['z']))
 combat=do('inspect_actor',ref=actor['ref'])['object'];check('combat facts use actual alignment target action range and line of fire',all(k in combat for k in ['hostile_to_avatar','alignment','target','action','effective_range','line_of_fire_to_avatar','equipment']),combat)
 do('combat');do('assert',state={'combat':1});do('combat');do('assert',state={'combat':0})
 w=do('inspect_world',radius=12,limit=100);sign=next(o for o in w['objects'] if o['name']=='sign' and o['interactable']);do('interact_object',ref=sign['ref']);s=until(lambda s:s['input_owner']=='text');check('native sign text observable',bool(s['text_page']),s['text_page']);refused('choose','invalid dialogue choice',index=0);do('dismiss');until(lambda s:s['input_owner']=='gameplay');check('sign dismissal returns gameplay control',True)
 do('interact_object',ref=w['avatar']);s=until(lambda s:bool(s['gumps']));check('native inventory Gump identified',not s['gumps'][0]['modal'] and s['gumps'][0]['id']>0,s['gumps']);controls=do('controls')['controls'];close=next(c for c in controls if c['action']=='close' and c['available']);do('activate_control',id=close['id']);until(lambda s:s['input_owner']=='gameplay');check('semantic close control restores gameplay',True);refused('activate_control','unsupported_ui_action_or_stale_control',id=close['id'])
 do('quit_prompt');s=until(lambda s:s['input_owner']=='modal');check('native modal owns input',any(g['modal'] for g in s['gumps']),s['gumps']);controls=do('controls')['controls'];check('modal confirm and cancel semantics reported',{'confirm','cancel'}<=set(c['action'] for c in controls),controls);cancel=next(c for c in controls if c['action']=='cancel' and c['available']);do('activate_control',id=cancel['id']);until(lambda s:s['input_owner']=='gameplay');check('native modal cancellation restores ownership',True)
 s=state();old=do('inspect_world',radius=4,limit=100)['avatar'];do('save',slot=0);do('load',slot=0);refused('inspect_object','stale_object_reference',ref=old);fresh=do('inspect_world',radius=4,limit=100)['avatar'];check('load assigns fresh references',fresh!=old);after=state();keys=['x','y','z','health','day','hour','minute'];check('native save reload preserves player state',{k:s[k] for k in keys}=={k:after[k] for k in keys})
 refused('dismiss','unsupported_ui_action_or_stale_control');do('assert',state={'input_owner':'gameplay','video':'dummy'})
 check('Studio listener remains inactive',state()['studio_listener_active']==0)
 check('dummy backend owns no native windows',not d.desktop()['engine_windows'],d.desktop());do('exit')
except Exception as e:
 d.failures+=1;log({'event':'failure'},{'ok':False,'category':'inspection','error':str(e),'traceback':traceback.format_exc()})
finally:
 result=d.close();print(json.dumps({'result':result}),flush=True)
raise SystemExit(0 if result and result['result']=='PASS' else 1)
