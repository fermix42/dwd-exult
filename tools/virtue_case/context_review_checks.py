"""Retrieve the existing marked case into disposable B review overlays, natively."""
from breadcrumb_checks import *
try:
 keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n2 INVENTORY 1\n')
 profile='keyring'if a.mod else'bg'
 root=Path('C:/Codex/Ultima7 Decomp/test-output/virtue-stage-e-final/human-review/prepared')/profile
 do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys))
 for name,npc in [('b-shamino',3),('b-dupre',4)]:
  shutil.copy2(root/name/'exult08bg.sav',Path(d.state()['save_path'])/'exult08bg.sav');do('load',slot=8);close_gumps()
  check('original B state is pending with no spontaneous context',q()['b_pending']and not q()['b_context']and not q()['b_done'])
  do('test_case_companion_start',npc=1);case=do('find_objects',shape=1601,radius=32)['objects'][0]['ref'];do('move_to_object',ref=case,timeout=30);check('ordinary approach reaches existing world case',arrived());bag=inventories();do('transfer_item',ref=case,destination=bag)
  check('retrieved case retains both saved marked stones',q()['marked_stones']==2 and not q()['b_context']);close_gumps();do('test_case_companion_start',npc=npc);inventories();show(case);do('move_gump',ref=case,x=310,y=5)
  check('review speaker remains original active companion',6 in companion(npc)['flags']);export_checkpoint(name)
  do('load',slot=9);check('saved overlay is eligible without a queued remark',q()['b_pending']and not q()['b_context']and q()['marked_stones']==2);touch(npc,True);until('overlay marked handling delivers B '+name,lambda:q()['b_done']);check('overlay uses correct original speaker',q()['last_npc']==npc)
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
