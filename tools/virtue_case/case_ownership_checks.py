"""Actual alternate/earned case transfer to active companion nested inventory."""
from breadcrumb_checks import *

def companion_pack():
 close_gumps();inventories();member=do('case_commission_state')['party'][1];do('key',key='2');do('move_gump',ref=member['ref'],x=400,y=5);bag=next(e['ref']for e in do('inspect_actor',ref=member['ref'])['object']['equipment']if e['slot']==1);do('interact_inventory',ref=bag);do('move_gump',ref=bag,x=310,y=220);return member['ref'],bag

def drop_from_companion(case):
 companion_pack();at=d.state()
 for dx,dy in [(-4,2),(-4,4),(4,2),(-3,-3)]:
  try:
   dest=dict(x=at['x']+dx,y=at['y']+dy,z=at['z'])
   if do('inspect_tile',**dest)['blocked']:continue
   do('drop_item',ref=case,**dest)
   if not obj(case)['owner']:close_gumps();return
  except EngineRefusal:pass
 raise RuntimeError('no native companion case drop')
try:
 keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n2 INVENTORY 1\n')
 do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys))
 for name in ['a-iolo','delivered-silent']:
  load_review(name);close_gumps()
  if name=='a-iolo':case=do('test_case_extra')['synthetic_case']['ref']
  else:case=next(o['ref']for o in inv()if o['shape']==1601)
  owner,bag=companion_pack();do('transfer_item',ref=case,destination=bag);check('real case transfers into actual active companion backpack '+name,obj(case)['owner']==bag);check('nested companion case suppresses both observations '+name,q()['case_owned']and not q()['a_pending']and not q()['b_pending']);close_gumps();do('save',slot=1);do('load',slot=1);time.sleep(13);check('nested ownership survives save/load without late chatter '+name,q()['case_owned']and q()['deliveries_since_load']==0);owner,bag=companion_pack();case=next(o['ref']for o in inv(bag)if o['shape']==1601);drop_from_companion(case)
  if name=='delivered-silent':
   do('save',slot=2);do('load',slot=2);time.sleep(13);check('dropping transferred earned case never re-enables hints',do('case_commission_state')['commission']['stage']==5 and not q()['a_pending']and not q()['b_pending']and q()['deliveries_since_load']==0)
  else:natural('a','dropping alternate case restores only currently justified undelivered A')
 do('exit')
except Exception:
 d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
 try:do('exit')
 except Exception:pass
finally:
 subprocess.Popen=original_popen
 if d.run:(d.run/'case-ownership-checks.json').write_text(json.dumps(events,indent=2))
 result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
