"""Native commission dialogue and persistent-state checks via real loaded Usecode."""
import argparse,json,sys,time,traceback
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import Driver,EngineRefusal
p=argparse.ArgumentParser();p.add_argument('--run',required=True);p.add_argument('--mod',default='',choices=('','Keyring'));a=p.parse_args();d=Driver();events=[]
def do(cmd,**kw):
 c=dict(cmd=cmd,**kw);r=d.execute(c);events.append(dict(command=c,response=r));return r
def check(label,value):
 d.assertions+=1;d.failures+=not value;events.append(dict(check=label,passed=bool(value)));print(label,bool(value),flush=True)
 if not value:raise AssertionError(label)
def state():return do('case_commission_state')
heard=[]
def settle():
 for _ in range(160):
  s=d.state()
  text=s.get('dialogue_text','')
  if text and (not heard or heard[-1]!=text):heard.append(text);events.append(dict(dialogue_text=text))
  if 'continue'in s['legal_actions']:do('continue')
  elif s['input_owner']in ['answer_menu','gameplay']:return s
  else:time.sleep(.02)
 raise TimeoutError('dialogue input ownership')
def talk():
 q=state();do('move_to_object',ref=q['xanthia']['ref'])
 end=time.monotonic()+12
 while time.monotonic()<end:
  if do('movement_status')['movement']=='reached':break
  time.sleep(.05)
 do('interact_object',ref=q['xanthia']['ref']);return settle()
def choose(text):heard.clear();do('choose',text=text);return settle()
def leave():
 s=settle()
 if s['input_owner']=='answer_menu':choose('bye')
 settle()
try:
 do('init',game='bg',mod=a.mod,run=a.run);do('test_case_commission_start');do('test_case_commission_items',scenario=0)
 for function in [0x456,0x45d]:
  f=do('inspect_usecode',function=function);check('original NPC effective function '+hex(function)+' present',f['length']>1000)
 s=talk();check('no stones hides special commission', 'Virtue Stone case'not in s['choices']);check('original conversation topics remain',set(['name','job','bye']).issubset(s['choices']));leave()
 for variant,label in [(1,'Avatar'),(2,'active companion'),(3,'nested container')]:
  do('test_case_commission_items',scenario=variant);s=talk();check('one real stone in '+label+' enables offer','Virtue Stone case'in s['choices']);leave()
 s=talk();s=choose('Virtue Stone case');check('introduction begins with attributed direct Avatar speech',bool(heard)and heard[0].lstrip().startswith(chr(34)+'I should like')and'thou sayest'in heard[0]);check('used introduction disappears immediately','Virtue Stone case'not in s['choices']);check('negotiation offers explicit commission','commission case'in s['choices']);check('introduction does not yet accept quest',state()['commission']['stage']==0)
 s=choose('commission case');check('acceptance is persistent state transition',state()['commission']['stage']==1);check('accepted introduction and negotiation removed','Virtue Stone case'not in s['choices']and'commission case'not in s['choices']);check('pending materials explanation appears','case materials'in s['choices']);s=choose('case materials');check('one-time materials explanation disappears','case materials'not in s['choices']);check('ordinary choices survive new topics','bye'in s['choices']);leave()
 do('test_case_commission_items',scenario=0);s=talk();check('active commission survives losing all stones','case materials'in s['choices']and'Virtue Stone case'not in s['choices']);leave()
 do('save',slot=2);do('load',slot=2);check('accepted quest survives normal save/load',state()['commission']['stage']==1);s=talk();check('pending explanation returns only in future conversation','case materials'in s['choices']);check('completed acceptance remains permanently unavailable','commission case'not in s['choices']and'Virtue Stone case'not in s['choices']);leave();do('save',slot=3);do('exit')
except Exception:
 d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
finally:
 if d.run:(d.run/'commission-checks.json').write_text(json.dumps(events,indent=2))
 result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
