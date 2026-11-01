import argparse,json,time
from driver import Driver
p=argparse.ArgumentParser();p.add_argument('--run',required=True);a=p.parse_args();d=Driver();out=[]
def do(cmd,**kw):
 r=d.execute({'cmd':cmd,**kw});out.append({'command':{'cmd':cmd,**kw},'response':r});return r
def check(label,ok,evidence=None):
 d.assertions+=1
 if not ok:d.failures+=1
 out.append({'assertion':label,'category':'inspection','ok':bool(ok),'evidence':evidence})
 if not ok:raise AssertionError(label)
def completed():
 end=time.monotonic()+12
 while time.monotonic()<end:
  r=do('movement_status')
  if r['movement']!='moving':return r
  time.sleep(.05)
 raise TimeoutError('movement')
try:
 do('init',game='bg',run=a.run);w=do('inspect_world',radius=12,limit=100);r=do('move_to_object',ref=w['avatar']);check('already at object destination is reached',r['movement']=='reached',r)
 actor=next(o for o in w['objects'] if o['type']=='actor' and o['npc']==1);do('move_to_object',ref=actor['ref']);r=completed();check('native movement toward observed actor snapshot reaches proximity',r['movement']=='reached',r)
 s=do('state')['state'];dest=None
 for dx,dy in [(3,0),(0,3),(-3,0),(0,-3)]:
  t={'x':s['x']+dx,'y':s['y']+dy,'z':s['z']};q=do('path_query',**t)
  if q['found'] and not q['blocked_destination']:dest=t;break
 check('timeout test has real valid native route',dest is not None,dest)
 do('move_to',**dest,timeout=.001);time.sleep(.02);r=completed();check('movement timeout is distinct from success',r['movement']=='timed_out',r)
 do('wait',until={'moving':0},timeout=10);check('timeout observation preserves normal engine continuation',do('state')['state']['can_act']==1)
 do('exit')
finally:
 result=d.close();(d.run/'navigation.jsonl').write_text('\n'.join(json.dumps(e) for e in out)+'\n');print(json.dumps({'result':result}),flush=True)
raise SystemExit(0 if result['result']=='PASS' else 1)
