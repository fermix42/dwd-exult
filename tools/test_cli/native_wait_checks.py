"""Semantic Native Wait end-to-end regression in a fresh disposable engine run."""
import json,time,argparse,traceback
from driver import Driver
p=argparse.ArgumentParser();p.add_argument('--run',required=True);p.add_argument('--checkpoint',help='read-only slot-zero BG checkpoint with a nearby lit ground torch');p.add_argument('--game',default='bg');p.add_argument('--mod',default='');p.add_argument('--quick',action='store_true');p.add_argument('--scale',type=int,default=1);a=p.parse_args();
if a.game=='bg' and not a.checkpoint:p.error('BG requires --checkpoint; see docs/game_time_events.md')
d=Driver();log=[]
def do(cmd,**kw):
 c={'cmd':cmd,**kw};v=d.execute(c);log.append({'command':c,'response':v});(d.run/'interface.jsonl').write_text('\n'.join(json.dumps(x)for x in log)+'\n');return v
def check(label,ok,e=None):
 d.assertions+=1;d.failures+=not ok;log.append({'command':{'assert':label},'response':{'ok':bool(ok),'evidence':e}});print(label,bool(ok),flush=True)
 if not ok:raise AssertionError(label)
def state():return do('state')['state']
def preview():return next((g for g in state()['gumps']if 'Wait_gump'in g['type']),None)
def until(fn,timeout=20):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  value=fn()
  if value:return value
  time.sleep(.05)
 raise TimeoutError('native interface transition')
def opening():
 end=time.monotonic()+100
 while time.monotonic()<end:
  s=state()
  if s['input']:do('choose',index=0)if s['choices_active']else do('continue')
  elif s['si_first_scene']and not s['dont_move']and not s['in_usecode']and not s['moving']:return
  time.sleep(.1)
 raise TimeoutError('natural opening')
def open_wait():
 until(lambda:do('wait_eligibility')['reason']=='allow');do('key',key='w',shift=True);return until(preview)
def closed():return until(lambda:state()if not state()['gumps']else None)
def control(action):return next(c for c in do('controls')['controls']if c['action']==action and c['available'])['id']

try:
 kw={}
 if a.game=='bg':kw['checkpoint']=a.checkpoint
 do('init',game=a.game,mod=a.mod,run=a.run,scale=a.scale,**kw)
 if a.game=='si':
  do('key',key='w',shift=True);check('Shift-W respects opening control ownership',preview()is None);opening();until(lambda:do('wait_eligibility')['reason']=='allow');do('save',slot=0)
 else:
  do('load',slot=0)
  lights=do('find_objects',radius=32,shape=701)['objects']
  check('checkpoint has actual registered ground fuel',any(not o['owner'] and o['quality']>30 and len(o['game_time_events'])==1 for o in lights),lights)
 g=open_wait();frozen=state();check('Shift-W opens native one-hour modal',g['hours']==1 and frozen['input_owner']=='modal'and frozen['queue_pause_depth']>0,g);check('current and resulting time agree',g['current_minutes']==frozen['game_time_tick']//25 and g['result_minutes']==g['current_minutes']+60,g)
 do('activate_control',id=control('increase'));check('native arrow increases duration',preview()['hours']==2)
 do('activate_control',id=control('decrease'));check('native arrow decreases duration',preview()['hours']==1)
 do('key',key='end');until(lambda:preview()['hours']==24);g=preview();check('24-hour range and next-day preview',g['result_minutes']-g['current_minutes']==1440 and '+1 day'in g['result_text'],g)
 do('key',key='right');check('upper bound remains24',bool(until(lambda:preview()['hours']==24)))
 do('key',key='home');until(lambda:preview()['hours']==1);do('key',key='left');check('lower bound remains1',preview()['hours']==1)
 check('selection leaves exact calendar unchanged',state()['game_time_tick']==frozen['game_time_tick'])
 do('activate_control',id=control('cancel'));after=closed();check('Cancel returns ownership without advancing selected hours',after['queue_pause_depth']==0 and 0<=after['game_time_tick']-frozen['game_time_tick']<=3,after)
 g=open_wait();frozen=state();do('key',key='escape');after=closed();check('Escape cancels and balances modal pause',after['queue_pause_depth']==0 and 0<=after['game_time_tick']-frozen['game_time_tick']<=3)
 if a.quick:do('exit')
 else:
  do('load',slot=0);until(lambda:do('wait_eligibility')['reason']=='allow');before=state();r=do('native_wait',minutes=60);native=state();native_light=do('find_objects',radius=32,shape=701)['objects'];check('CLI one hour comparison control completes',r['completed_ticks']==1500,r)
  do('load',slot=0);g=open_wait();start=state();do('activate_control',id=control('confirm'));after=closed();ui_light=do('find_objects',radius=32,shape=701)['objects']
  check('Gump confirm invokes equivalent one-hour service',1500<=after['game_time_tick']-start['game_time_tick']<=1503 and after['queue_pause_depth']==0,{'before':start,'after':after,'native':r})
  if a.game=='bg':check('UI and CLI torch consequences match',[(t['quality'],len(t['game_time_events']))for t in ui_light]==[(t['quality'],len(t['game_time_events']))for t in native_light],{'ui':ui_light,'cli':native_light})
  check('hourly hunger does not refill food',after['food']<=start['food'],after)
  do('load',slot=0);do('combat');before=state();do('key',key='w',shift=True);time.sleep(.1);after=state();check('combat refusal cannot open Gump or advance hours',not after['gumps']and after['combat']==1 and after['game_time_tick']-before['game_time_tick']<3);do('combat')
  do('quit_prompt');before=state();do('key',key='w',shift=True);time.sleep(.1);after=state();check('Shift-W does not bypass another modal',not any('Wait_gump'in g['type']for g in after['gumps'])and after['game_time_tick']==before['game_time_tick']);do('dismiss');closed()
  g=open_wait();do('key',key='end');until(lambda:preview()['hours']==24);start=state();do('key',key='enter');after=closed();check('24-hour request interrupts and restores control',0<after['game_time_tick']-start['game_time_tick']<36000 and after['queue_pause_depth']==0,after)
  until(lambda:state()['schedule_reconciliation_pending']==0);reason=do('wait_eligibility')['reason'];check('interruption is the actual party safety boundary',reason in ('hungry-party','exposure','harmful-condition'),{'reason':reason,'state':state()})
  do('save',slot=1);do('load',slot=1);do('wait',timeout=.5);check('post-Wait save-load and ordinary gameplay remain usable',state()['input_owner']=='gameplay'and state()['can_act']==1)
  do('exit')
except Exception:d.failures+=1;print(traceback.format_exc(),flush=True)
finally:print(json.dumps(d.close()),flush=True)
raise SystemExit(1 if d.failures else 0)
