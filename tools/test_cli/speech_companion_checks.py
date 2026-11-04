"""Actual natural Virtue Stone observation uses the same global speech timer."""
from speech_duration_checks import *
try:
 do('init',game='bg',mod=a.mod,run=a.run)
 profile='keyring'if a.mod else'bg';source=Path('C:/Codex/Ultima7 Decomp/test-output/virtue-stage-e-final/human-review/prepared')/profile/'a-iolo/exult08bg.sav'
 for percent in [100,200,300]:
  select(percent);shutil.copy2(source,Path(d.state()['save_path'])/'exult08bg.sav');do('load',slot=8)
  while d.state()['input_owner']=='gump':do('dismiss')
  check('game load retains global engine preference',d.state()['overhead_text_duration']==percent)
  before=do('inspect_inventory')['objects'];end=time.monotonic()+70;state=None
  while time.monotonic()<end:
   state=do('case_breadcrumbs_state')['breadcrumbs']
   if state['a_done']:break
   time.sleep(.08)
  check('natural companion speaks without item manipulation '+str(percent),bool(state and state['a_done'])and state['last_npc']==1)
  effect=next(e for e in do('text_effects')['effects']if e['text']==state['last_text']);check('real companion uses global captured lifetime '+str(percent),effect['lifetime_ticks']==percent//10);check('exact accepted candidate unchanged',effect['text']=='These stones are filling our packs!');after=do('inspect_inventory')['objects'];check('original stones remain in their inventory locations',[(o['ref'],o['owner'])for o in before if o['shape']==330]==[(o['ref'],o['owner'])for o in after if o['shape']==330]);capture('natural-companion-'+str(percent))
 do('exit')
except Exception:
 d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
 try:do('exit')
 except Exception:pass
finally:
 subprocess.Popen=original
 if d.run:(d.run/'speech-companion-checks.json').write_text(json.dumps(events,indent=2))
 result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
