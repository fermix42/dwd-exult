"""Prepare natural B and already-organized overlays through native item handling."""
from breadcrumb_checks import *
try:
 keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n2 INVENTORY 1\n')
 do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys))
 for name in ['b-shamino','b-dupre','both-pending']:
  load_review(name,name.startswith('b-'));extract_case()
  if name.startswith('b-'):
   stones=[o for o in inv()if o['shape']==330];drop(stones[-1]['ref']);close_gumps()
  hold_review(name);check('natural review keeps qualifying real loose stones',q()['marked_stones']>=1 and not q()['case_owned'])
  if name=='both-pending':export_checkpoint('already-organized')
 do('exit')
except Exception:
 d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
 try:do('exit')
 except Exception:pass
finally:
 subprocess.Popen=original_popen
 if d.run:(d.run/'context-review-checks.json').write_text(json.dumps(events,indent=2))
 result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
