"""Real inn bed, native wake-time modal, sleep/wake and later natural chatter."""
from breadcrumb_checks import *
def sleep_state():
 s=d.state()
 if 'continue'in s['legal_actions']:do('continue');s=d.state()
 return s
try:
 keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n2 INVENTORY 1\n')
 do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys));load_review('b-dupre',True);close_gumps();beds=do('find_objects',shape=1011,radius=32)['objects']+do('find_objects',shape=696,radius=32)['objects'];bed=next(o for o in beds if o['tile']['z']==1 and o['tile']['x']==229)
 do('move_to_object',ref=bed['ref'],timeout=20);check('normal approach reaches the actual unoccupied inn bed',arrived());extract_case();inventories();do('save',slot=1);close_gumps();check('real save observes eligible marked inventory before sleeping',q()['b_pending']);do('interact_object',ref=bed['ref']);until('original bed puts Avatar to sleep and asks wake time',lambda:sleep_state()['input_owner']=='modal'and not d.state()['can_act']);before=d.state();events.append(dict(sleep_controls=do('controls')));time.sleep(13);check('eligible B remains pending while Avatar really sleeps',q()['b_pending']and not q()['b_done']and not d.state()['can_act']);do('key',key='right');do('key',key='right');do('key',key='enter');until('normal native wake-time choice returns control',lambda:sleep_state()['input_owner']=='gameplay'and bool(d.state()['can_act']),45);after=d.state();check('actual sleep advances the normal calendar',after['day']*1440+after['hour']*60+after['minute']>before['day']*1440+before['hour']*60+before['minute']);check('sleep never falsely consumes the pending observation',not q()['b_done']);feed();natural('b','normal waking exploration delivers the retained destination observation');check('actual nearby Dupre speaks once after waking',q()['last_npc']==4 and q()['deliveries_since_load']==1);do('exit')
except Exception:
 d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
 try:do('exit')
 except Exception:pass
finally:
 subprocess.Popen=original_popen
 if d.run:(d.run/'sleep-checks.json').write_text(json.dumps(events,indent=2))
 result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
