"""Both hints remain available in earned commission/material/building states."""
from breadcrumb_checks import *
try:
 keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n2 INVENTORY 1\n')
 profile='keyring'if a.mod else'bg';suffix=Path('mods/Keyring/saves')if a.mod else Path('.')
 old=Path('C:/Codex/Ultima7 Decomp/test-output');prior=old/'virtue-stage-d-accepted/runs'
 archives=[(1,prior/('accepted-commission-'+profile)/'saves'/suffix/'exult03bg.sav'),(2,old/'virtue-stage-d-revision/human-review/prepared'/profile/'requirements/exult08bg.sav'),(3,prior/('accepted-delivery-'+profile)/'saves'/suffix/'exult01bg.sav')]
 do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys))
 for stage,archive in archives:
  shutil.copy2(archive,Path(d.state()['save_path'])/'exult00bg.sav');do('load',slot=0)
  check('earned pre-delivery stage '+str(stage),do('case_commission_state')['commission']['stage']==stage)
  if 6 in companion(1)['flags']:do('test_case_companion_start',npc=1);dismiss_iolo()
  do('test_case_breadcrumb_items',scenario=0);case=prepare_case();stones=inv(case)
  mark(case,stones[0]['ref']);close_gumps();mark(case,stones[1]['ref']);close_gumps();extract_case();acquire_third();close_gumps()
  check('both hints remain eligible before delivery at stage '+str(stage),q()['a_pending']and q()['b_pending']and not q()['a_done']and not q()['b_done']and do('case_commission_state')['commission']['stage']==stage)
  do('save',slot=2);do('load',slot=2);check('both pending states persist at stage '+str(stage),q()['a_pending']and q()['b_pending'])
 do('exit')
except Exception:
 d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
finally:
 subprocess.Popen=original_popen
 if d.run:(d.run/'pre-delivery-checks.json').write_text(json.dumps(events,indent=2))
 result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
