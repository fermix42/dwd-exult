"""Exact private review paths and one preference shared across BG/Keyring/SI."""
import argparse,sys,subprocess,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);args,rest=p.parse_known_args();root=args.root.resolve();sys.argv=[sys.argv[0]]+rest
import speech_duration_checks as t
original=t.original

def launch(argv,**kw):
 if '--test-cli'in argv:
  argv=list(argv);argv[0]=str(root/'Exult.exe');argv[argv.index('-c')+1]=str(root/'exult.cfg');argv[argv.index('--test-run-dir')+1]=str(root);kw['cwd']=root
 return original(argv,**kw)

for profile,game,mod in [('BG','bg',''),('Keyring','bg','Keyring'),('SI','si','')]:
 subprocess.run([sys.executable,str(root/'start_speech_duration_review.py'),'--profile',profile,'--state','continue'if game=='si'else'a-iolo','--prepare-only'],check=True)
 t.d=t.Driver();t.events=[];subprocess.Popen=launch
 try:
  t.do('init',game=game,mod=mod,run=t.a.run+'-'+profile.lower());at=t.d.state();t.check('shared config restores global duration in '+profile,at['overhead_text_duration']==(100 if profile=='BG'else 300));t.check('private review paths are canonical',Path(at['gamedat_path']).resolve().is_relative_to(root)and Path(at['save_path']).resolve().is_relative_to(root))
  while t.d.state()['input_owner']=='gump':t.do('dismiss')
  if profile=='BG':t.select(300);t.lifetime(0,30)
  elif profile=='Keyring':t.lifetime(3,30)
  t.do('exit')
 except Exception:
  t.d.failures+=1;t.events.append(dict(error=t.traceback.format_exc()));print(t.traceback.format_exc(),flush=True)
  try:t.do('exit')
  except Exception:pass
 finally:
  subprocess.Popen=original
  if t.d.run:(t.d.run/'review-checks.json').write_text(json.dumps(t.events,indent=2))
  result=t.d.close();print(profile,json.dumps(result),flush=True)
 assert result['result']=='PASS',result
