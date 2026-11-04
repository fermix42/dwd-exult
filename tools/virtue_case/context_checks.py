"""New contextual opportunities, expiry and old-save migration through real input."""
from breadcrumb_checks import *
try:
 keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n2 INVENTORY 1\n')
 profile='keyring'if a.mod else'bg';suffix=Path('mods/Keyring/saves')if a.mod else Path('.')
 review=Path('C:/Codex/Ultima7 Decomp/test-output/virtue-stage-e-final/human-review/prepared')/profile/'a-iolo/exult08bg.sav'
 do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys),checkpoint=str(review));do('load',slot=8)
 check('qualifying old save is eligible without contextual opportunity',q()['a_pending']and not q()['a_context']and not q()['b_context']);time.sleep(7);check('loading alone never delivers A',not q()['a_done'])
 do('test_case_extra');time.sleep(6);check('unrelated container grant never provides context',not q()['a_context']and not q()['a_done'])
 touch();do('key',key='1');time.sleep(16);check('context expires while inventory owns input',not q()['a_context']and q()['a_pending']and not q()['a_done']);close_gumps();time.sleep(6);check('old handling never produces a stale late comment',not q()['a_done']);do('save',slot=1);do('load',slot=1);check('eligibility persists but stale context is never saved',q()['a_pending']and not q()['a_context']);bag=inventories();stone=next(o['ref']for o in reversed(inv(bag))if o['shape']==330);do('transfer_item',ref=stone,destination=bag);check('native rearrangement within backpack supplies fresh context',q()['a_context']);close_gumps();until('fresh handling renews expired opportunity',lambda:q()['a_done']);do('save',slot=2);do('load',slot=2);time.sleep(6);check('completion still prevents duplicates across reload',q()['a_done']and not q()['a_pending']and q()['deliveries_since_load']==0)
 do('load',slot=8);do('combat');touch();time.sleep(6);check('recent context cannot interrupt combat',q()['a_pending']and not q()['a_done']);do('combat');until('brief combat deferral remains contextual',lambda:q()['a_done'])
 # Earned archive contains two actual successful MARKs and no eligible B speaker.
 earned=Path('C:/Codex/Ultima7 Decomp/test-output/virtue-stage-e-final/runs')/('final-breadcrumb-'+profile)/'saves'/suffix/'exult03bg.sav'
 shutil.copy2(earned,Path(d.state()['save_path'])/'exult07bg.sav');do('load',slot=7);check('existing two marked objects qualify without new history fields',q()['marked_stones']==2 and q()['b_pending']and not q()['b_context']);join(3);time.sleep(6);check('recruitment and quiet time cannot create B context',not q()['b_done']and not q()['b_context'])
 unmarked=do('test_case_breadcrumb_items',scenario=3)['synthetic_world_stone'];bag=inventories();do('transfer_item',ref=unmarked['ref'],destination=bag);close_gumps();check('unmarked duplicate-color handling does not trigger B',not q()['b_context']and not q()['a_pending']);time.sleep(6);check('unrelated quiet time leaves B pending',q()['b_pending']and not q()['b_done'])
 case=next(o['ref']for o in inv()if o['shape']==1601);do('test_case_spell_setup',scenario=1);inventories();show(case);do('move_gump',ref=case,x=310,y=5);g=next(g for g in d.state()['gumps']if'Case_gump'in g['type']);button=next(c for c in do('controls')['controls']if c['gump_id']==g['id']and c['action']=='mark');before=do('case_spell_state');do('activate_control',id=button['id']);time.sleep(.3);after=do('case_spell_state');check('failed MARK never creates a context or consumes mana',not q()['b_context']and q()['b_pending']and after['mana']==before['mana']and not d.state()['case_targeting']);close_gumps()
 # A real successful re-MARK is sufficient context once eligibility already exists.
 stone=next(o['ref']for o in inv(case)if o['shape']==330);mark(case,stone);check('successful MARK creates destination context',q()['b_context']);close_gumps();until('successful MARK is followed by contextual B',lambda:q()['b_done']);do('load',slot=7);join(3);touch(3,True);until('later deliberate marked-stone handling works for existing saves',lambda:q()['b_done']);do('exit')
except Exception:
 d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
 try:do('exit')
 except Exception:pass
finally:
 subprocess.Popen=original_popen
 if d.run:(d.run/'context-checks.json').write_text(json.dumps(events,indent=2))
 result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
