"""Natural state-based deferral through actual gameplay/UI and ordinary spells."""
from breadcrumb_checks import *
try:
 keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n2 INVENTORY 1\n')
 do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys))
 load_review('a-iolo');close_gumps();inventories();time.sleep(13);check('open inventory defers eligible natural A',q()['a_pending']and not q()['a_done']);close_gumps();natural('a','inventory deferral is followed by natural speech without handling')
 load_review('a-iolo');close_gumps();do('combat');time.sleep(13);check('combat never delivers or consumes natural A',q()['a_pending']and not q()['a_done']);do('combat');natural('a','combat deferral retains current eligible circumstances')
 load_review('a-iolo');close_gumps();talk(1);time.sleep(13);check('original NPC dialogue protects natural A',q()['a_pending']and not q()['a_done']);leave();natural('a','original conversation resumes with natural pending A')
 load_review('a-iolo');close_gumps();before=q()['deliveries_since_load'];result=do('native_wait',minutes=20);check('Native Wait executes without delivering chatter',q()['deliveries_since_load']==before and q()['a_pending']);natural('a','ordinary play after Native Wait needs no stone action')
 load_review('a-iolo');close_gumps();npc=companion(1)['ref']
 for _ in range(5):do('test_overhead',ref=npc,scenario=0,edge=0);time.sleep(3)
 check('existing overhead speech prevents overlap',q()['a_pending']and not q()['a_done']);natural('a','speech resumes only after other overhead expires')
 load_review('a-iolo');close_gumps();inventories();do('save',slot=1);do('load',slot=1);close_gumps();time.sleep(3);check('rapid save/load does not skip natural startup delay',q()['a_pending']and not q()['a_done']);natural('a','saved pending state resumes naturally');do('save',slot=2);do('load',slot=2);time.sleep(13);check('saved completion never repeats',q()['a_done']and q()['deliveries_since_load']==0)
 # Successful and failed ordinary spellbook MARK onto original loose stones.
 load_review('a-iolo');close_gumps();do('test_case_breadcrumb_items',scenario=0);join(3);stone=acquire_third();close_gumps()
 book,before=loose_mark(stone);time.sleep(13);check('ordinary MARK targeting protects otherwise eligible A',q()['a_pending']and not q()['a_done']and not q()['b_pending']);do('target_object',ref=stone);until('ordinary MARK animation finishes',lambda:do('case_spell_state')['actor_casting']==0);after=do('case_spell_state');check('ordinary successful MARK pays native mana and reagents',after['mana']==before['mana']-4 and after['reagents']==[n-(i in {0,1,3})for i,n in enumerate(before['reagents'])]);check('one successful original loose MARK qualifies B',q()['marked_stones']==1 and q()['b_pending']);inventories();time.sleep(16);check('eligibility survives beyond the removed 15-second event window',q()['a_pending']and q()['b_pending']and not q()['a_done']and not q()['b_done']);close_gumps();natural('a','A remains natural after a long protected spell/inventory interval');do('native_wait',minutes=5);natural('b','B follows later normal play without recasting or dragging')
 load_review('a-iolo');close_gumps();do('test_case_breadcrumb_items',scenario=0);do('test_case_spell_setup',scenario=1);bag=inventories();book=next(o['ref']for o in inv()if o['class_id']==8);show(book);do('move_gump',ref=book,x=310,y=5);before=do('case_spell_state')
 try:do('spellbook_cast',ref=book,spell=34)
 except EngineRefusal as refusal:events.append(dict(expected_refusal=str(refusal)))
 time.sleep(.3);after=do('case_spell_state');check('failed ordinary MARK cannot create a marked destination or B',q()['marked_stones']==0 and not q()['b_pending']and after['mana']==before['mana']);close_gumps()
 load_review('a-iolo');close_gumps();stone=next(o['ref']for o in reversed(inv())if o['shape']==330 and o['owner']==pack()[1]);book,before=loose_mark(stone,36);time.sleep(13);check('ordinary RECALL targeting defers eligible A',q()['a_pending']and not q()['a_done']);do('key',key='escape');until('ordinary RECALL cancellation completes',lambda:do('case_spell_state')['actor_casting']==0);natural('a','natural speech resumes after ordinary RECALL cancellation')
 # Actual case acquisition suppresses both without marking observations completed.
 load_review('a-iolo');close_gumps();do('test_case_extra');check('alternate real case ownership suppresses pending A',q()['case_owned']and not q()['a_pending']and not q()['a_done']);do('save',slot=3);do('load',slot=3);time.sleep(13);check('old-save case ownership stays silent across reload',q()['deliveries_since_load']==0 and not q()['a_pending']);case=next(o['ref']for o in inv()if o['shape']==1601);drop(case);close_gumps();natural('a','nonquest ownership suppression is derived rather than a fabricated permanent quest flag');check('alternate case never changed commission lifecycle',do('case_commission_state')['commission']['stage']==0)
 do('exit')
except Exception:
 d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
 try:do('exit')
 except Exception:pass
finally:
 subprocess.Popen=original_popen
 if d.run:(d.run/'context-checks.json').write_text(json.dumps(events,indent=2))
 result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
