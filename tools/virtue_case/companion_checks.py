"""Multiple real companions gathered by normal paid MARK/RECALL, never NPC fixtures."""
from breadcrumb_checks import *

def cast(stone,spell):
 if spell==36:
  at=d.state();moved=False
  for dx,dy in [(2,0),(0,2),(-2,0),(0,-2)]:
   target=dict(x=at['x']+dx,y=at['y']+dy,z=at['z'])
   if do('inspect_tile',**target)['blocked']:continue
   do('move_to',**target,timeout=8)
   if arrived():moved=True;break
  check('party walks away before normal RECALL',moved)
 book,before=loose_mark(stone,spell);do('target_object',ref=stone);until('ordinary spell animation completes '+str(spell),lambda:do('case_spell_state')['actor_casting']==0);after=do('case_spell_state');check('normal spell pays mana '+str(spell),after['mana']==before['mana']-4);close_gumps()
 if spell==36:until('real RECALL returns Avatar to saved destination',lambda:all(d.state()[k]==obj(stone)['stone_target'][k]for k in ['x','y','z']))

try:
 keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n2 INVENTORY 1\n')
 do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys))
 load_review('a-iolo');close_gumps();case=do('test_case_extra')['synthetic_case']['ref'];join(3);stone=next(o['ref']for o in reversed(inv())if o['shape']==330 and o['owner']==pack()[1]);cast(stone,34);cast(stone,36)
 check('native RECALL gathers actual Iolo and Shamino without assigning membership or positions',all(6 in companion(n)['flags']and companion(n)['distance']<=8 for n in [1,3]));drop(case);close_gumps();natural('a','both eligible accumulation companions produce one observation');check('Iolo preferred with Shamino actually nearby',q()['last_npc']==1);do('native_wait',minutes=5);natural('b','Shamino destination observation follows naturally');check('A and B each spoken once with multiple companions',q()['last_npc']==3 and q()['deliveries_since_load']==2);do('save',slot=1);do('load',slot=1);time.sleep(13);check('alternative companions cannot repeat either completed hint',q()['a_done']and q()['b_done']and q()['deliveries_since_load']==0)
 load_review('b-dupre',True);join(3);close_gumps();case=next(o['ref']for o in inv()if o['shape']==1601);bag=inventories();show(case);do('move_gump',ref=case,x=310,y=5)
 for stone in inv(case):do('transfer_item',ref=stone['ref'],destination=bag)
 stone=next(o['ref']for o in reversed(inv(bag))if o['shape']==330);close_gumps();cast(stone,34);cast(stone,36);check('native RECALL gathers actual Shamino and Dupre',all(6 in companion(n)['flags']and companion(n)['distance']<=8 for n in [3,4]));drop(case);close_gumps();natural('b','one destination remark with both eligible companions');check('Shamino preferred over nearby Dupre',q()['last_npc']==3 and q()['deliveries_since_load']==1);do('save',slot=2);do('load',slot=2);time.sleep(13);check('Dupre alternative cannot repeat completed B',q()['b_done']and q()['deliveries_since_load']==0);do('exit')
except Exception:
 d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
 try:do('exit')
 except Exception:pass
finally:
 subprocess.Popen=original_popen
 if d.run:(d.run/'companion-checks.json').write_text(json.dumps(events,indent=2))
 result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
