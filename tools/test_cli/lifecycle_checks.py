"""Nonintrusive unified binary and cancellation checks, no visible startup."""
import argparse,json,os,pathlib,subprocess,time,hashlib
from driver import Driver,BASE,EXE,child_environment
p=argparse.ArgumentParser();p.add_argument('--prefix',required=True);a=p.parse_args();out=[]
def record(label,condition,evidence):
 out.append({'assertion':label,'ok':bool(condition),'category':'inspection','evidence':evidence});print(json.dumps(out[-1]),flush=True)
env=child_environment();exe=EXE;proof=BASE/'runs'/(a.prefix+'-startup');proof.mkdir(parents=True)
for flag in ['--help','--version']:
 r=subprocess.run([str(exe),'-p',flag],cwd=proof,env=env,text=True,capture_output=True,timeout=10,creationflags=subprocess.CREATE_NO_WINDOW);(proof/(flag[2:]+'.log')).write_text(r.stdout+r.stderr)
 record('same binary normal '+flag+' has no controller event',r.returncode==(1 if flag=='--help' else 0) and '@CLI' not in r.stdout+r.stderr,{'exit_code':r.returncode,'bytes':len(r.stdout+r.stderr)})
r=subprocess.run([str(exe),'--test-cli'],cwd=proof,env=env,text=True,capture_output=True,timeout=10,creationflags=subprocess.CREATE_NO_WINDOW);record('unisolated CLI startup fails closed',r.returncode==64 and 'requires' in r.stderr,{'exit_code':r.returncode,'stderr':r.stderr})
# A rejected command is a failure to a script, while idle cleanup still completes.
script=proof/'negative.jsonl';script.write_text(json.dumps({'cmd':'init','game':'bg','run':a.prefix+'-negative'})+'\n'+json.dumps({'cmd':'choose','index':99})+'\n')
r=subprocess.run([os.sys.executable,str(pathlib.Path(__file__).with_name('driver.py')),'--script',str(script)],env=env,text=True,capture_output=True,timeout=40,creationflags=subprocess.CREATE_NO_WINDOW);(proof/'negative.log').write_text(r.stdout+r.stderr);record('public script failure is nonzero and actionable',r.returncode==1 and 'not awaiting input' in r.stdout,{'exit_code':r.returncode})
d=Driver()
try:
 init=d.execute({'cmd':'init','game':'bg','run':a.prefix+'-cancel'});w=d.execute({'cmd':'inspect_world','radius':12,'limit':100});d.execute({'cmd':'save','slot':0});saved=list((d.run/'saves').glob('*.sav'));before={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in saved};sign=min((o for o in w['objects'] if o['name']=='sign' and o['interactable']),key=lambda o:o['distance']);d.execute({'cmd':'interact_object','ref':sign['ref']})
 end=time.monotonic()+5
 while time.monotonic()<end:
  s=d.state()
  if s['input_owner']=='text':break
  time.sleep(.05)
 record('nested cancellation starts in genuine sign input',s['input_owner']=='text',{'input_owner':s['input_owner'],'text':s['text_page']})
 d.execute({'cmd':'exit'})
finally:result=d.close()
record('nested session never reported successful',result['exit_code']==2 and result['result']=='FAIL' and result['termination']=='cancelled' and not result['shutdown']['cleanup_complete'],result)
record('completed checkpoint survives cancellation',bool(before) and all(pathlib.Path(p).exists() and hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()==h for p,h in before.items()),before)
# Confirm the actual native quit prompt, then observe ordinary cleanup.
d=Driver();modal_observed=False
try:
 d.execute({'cmd':'init','game':'bg','run':a.prefix+'-confirm'});d.execute({'cmd':'quit_prompt'});end=time.monotonic()+5
 while time.monotonic()<end:
  s=d.state()
  if s['input_owner']=='modal':modal_observed=True;break
  time.sleep(.03)
 controls=d.execute({'cmd':'controls'})['controls'];confirm=next(c for c in controls if c['action']=='confirm' and c['available']);d.execute({'cmd':'activate_control','id':confirm['id']})
finally:confirmed=d.close()
record('native modal confirmation exits through ordinary cleanup',modal_observed and confirmed['exit_code']==0 and confirmed['termination']=='normal' and confirmed['shutdown']['cleanup_complete'],confirmed)
(proof/'checks.json').write_text(json.dumps(out,indent=2));raise SystemExit(0 if all(x['ok'] for x in out) else 1)
