"""Focused final-review regressions; no desktop automation or game fixtures."""
import argparse,hashlib,json,os,pathlib,subprocess,time
from driver import Driver,BASE,EXE,EngineRefusal,child_environment
p=argparse.ArgumentParser();p.add_argument('--run',required=True);a=p.parse_args()
D=Driver();events=[];evidence=BASE/'review-evidence'/a.run;evidence.mkdir(parents=True,exist_ok=False)
def record(label,ok,detail=None):
 events.append({'assertion':label,'category':'inspection','ok':bool(ok),'evidence':detail})
 if not ok:raise AssertionError(label)
def refuse(op,*args,expected):
 try:D.raw(op,*args)
 except EngineRefusal as e:record('refuse '+op+' '+repr(args),expected in str(e),e.detail);return
 raise AssertionError('accepted malformed/redirected command')
try:
 init=D.execute({'cmd':'init','game':'bg','run':a.run});before=D.state();world=D.execute({'cmd':'inspect_world','radius':4,'limit':100});ref=world['avatar']
 for op,args,msg in [('load',(), 'missing_argument'),('load',('bad',),'invalid_integer_argument'),('load',('0junk',),'invalid_integer_argument'),('load',('9999999999999999999',),'invalid_integer_argument'),('combat',(1,),'invalid_argument_count'),('move_to',(1081,2214,'bad'),'invalid_integer_argument')]:refuse(op,*args,expected=msg)
 after=D.state();record('malformed commands preserved live world/control',all(before[k]==after[k] for k in ['x','y','z','combat','dont_move','health']) and D.execute({'cmd':'inspect_object','ref':ref})['object']['ref']==ref)
 D.execute({'cmd':'save','slot':0});D.execute({'cmd':'load','slot':0});refuse('inspect_object',ref,0,expected='stale_object_reference');record('shared read replacement refreshes references',D.execute({'cmd':'inspect_world','radius':4,'limit':100})['avatar']!=ref)
 canary=evidence/'outside-canary.txt';canary.write_text('review canary, must remain unchanged\n');original=canary.read_bytes();link=D.run/'saves'/'redirected-file';os.link(canary,link)
 try:refuse('save',0,expected='redirected_run_path');record('hard-link write refusal preserved external file',canary.read_bytes()==original)
 finally:link.unlink()
 junction=D.run/'redirected-directory'
 r=subprocess.run(['powershell','-NoProfile','-NonInteractive','-Command',f"$ErrorActionPreference='Stop'; New-Item -ItemType Junction -Path '{junction}' -Target '{evidence}' | Out-Null"],capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
 if r.returncode:raise RuntimeError('junction fixture creation: '+r.stderr)
 try:refuse('screenshot',expected='redirected_run_path');record('junction refusal preserved external file',canary.read_bytes()==original)
 finally:os.rmdir(junction)
 D.execute({'cmd':'screenshot'});record('offscreen capture stayed inside run',(D.run/'screenshot.png').is_file())
 D.assertions=len(events);D.execute({'cmd':'exit'})
finally:
 result=D.close();(evidence/'targeted-checks.json').write_text(json.dumps({'checks':events,'session':result},indent=2));print(json.dumps({'checks':len(events),'all_passed':all(e['ok'] for e in events),'session':result}),flush=True)
# Check startup rejects an outside configured write directory before creating it.
cfg=(D.run/'exult.cfg').read_text();outside=evidence/'must-not-be-created';escape=D.run/'escape.cfg';escape.write_text(cfg.replace(str(D.run/'gamedat').replace('\\','/'),outside.as_posix()))
env=child_environment()
r=subprocess.run([str(EXE),'--test-cli','--test-run-dir',str(D.run),'-p','--bg','--nomenu','-c',str(escape)],cwd=D.run,env=env,capture_output=True,text=True,timeout=30,creationflags=subprocess.CREATE_NO_WINDOW)
record('outside configured directory rejected before mkdir',r.returncode==64 and not outside.exists(),{'exit':r.returncode,'tail':r.stdout[-300:],'stderr':r.stderr[-300:]})
(evidence/'targeted-checks.json').write_text(json.dumps({'checks':events,'session':result},indent=2));print(json.dumps({'final_checks':len(events),'all_passed':all(e['ok'] for e in events)}))
