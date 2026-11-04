"""Focused post-delivery suppression using earned native quest checkpoints."""
from breadcrumb_checks import *

def travel(npc):
 visits={}
 for _ in range(120):
  query=do('case_commission_state');target=query['xanthia'if npc==86 else 'william'];at=d.state();t=target['tile']
  if max(abs(t['x']-at['x']),abs(t['y']-at['y']))<=10:
   if target['interactable']:return target['ref']
   do('move_to_object',ref=target['ref']);check('normal approach reaches NPC '+str(npc),arrived());return target['ref']
  dx=max(-8,min(8,t['x']-at['x']));dy=max(-8,min(8,t['y']-at['y']));candidates=[]
  for ox,oy in [(0,0),(-4,0),(4,0),(0,-4),(0,4),(-8,0),(8,0),(0,-8),(0,8),(-16,0),(16,0),(0,-16),(0,16)]:
   x=at['x']+dx+ox;y=at['y']+dy+oy
   if max(abs(x-at['x']),abs(y-at['y']))>30:continue
   candidates.append((max(abs(x-t['x']),abs(y-t['y']))+visits.get((x,y),0)*100,x,y))
  for ox,oy in [(8,0),(-8,0),(0,8),(0,-8),(8,8),(-8,8),(8,-8),(-8,-8)]:
   x=at['x']+ox;y=at['y']+oy
   candidates.append((max(abs(x-t['x']),abs(y-t['y']))+visits.get((x,y),0)*100,x,y))
  plans=[]
  for score,x,y in sorted(set(candidates)):
   try:
    dest=dict(x=x,y=y,z=at['z']);path=do('path_query',**dest)
    if path['found']and not path['blocked_destination']and path['steps']<=80:
     plans.append((score+path['steps'],path['steps'],dest))
   except EngineRefusal:continue
  moved=False
  for _,steps,dest in sorted(plans,key=lambda plan:plan[0]):
   try:
    timeout=min(45,max(12,steps*.4+5));do('move_to',**dest,timeout=timeout)
    visits[dest['x'],dest['y']]=visits.get((dest['x'],dest['y']),0)+1
    if arrived(timeout+1):moved=True;break
   except EngineRefusal:continue
  if not moved:raise RuntimeError('bounded Minoc navigation cannot find route')
 raise RuntimeError('bounded Minoc navigation exhausted')

def quest():return do('case_commission_state')['commission']
def quest_talk():
 ref=travel(86);do('interact_object',ref=ref);return settle()
def silence(label):
 close_gumps();time.sleep(7)
 check(label,not q()['a_pending']and not q()['b_pending']and q()['deliveries_since_load']==0)

def load_archive(path,slot=0):
 shutil.copy2(path,Path(d.state()['save_path'])/('exult%02dbg.sav'%slot));do('load',slot=slot)

def collect_delivery():
 quest_talk();choose('collect case');check('collection request alone does not suppress pending hints',quest()['stage']==4 and (q()['a_pending']or q()['a_done'])and(q()['b_pending']or q()['b_done']))
 end=time.monotonic()+120
 while time.monotonic()<end:
  state=do('case_commission_state');qst=state['commission'];npc=state['xanthia']
  if qst['stage']==5 and not qst['delivery_active']:break
  if qst['stage']==4 and not qst['delivery_active']:quest_talk();choose('collect case')
  elif qst['delivery_phase']==0 and max(abs(npc['tile']['x']-d.state()['x']),abs(npc['tile']['y']-d.state()['y']))>7:do('move_to_object',ref=npc['ref'])
  time.sleep(.05)

try:
 keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n2 INVENTORY 1\n')
 profile='keyring'if a.mod else'bg';suffix=Path('mods/Keyring/saves')if a.mod else Path('.')
 prior=Path('C:/Codex/Ultima7 Decomp/test-output/virtue-stage-d-accepted/runs')/('accepted-delivery-'+profile)/'saves'/suffix
 checkpoint=prior/'exult03bg.sav'
 do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys),checkpoint=str(checkpoint));do('load',slot=3)
 check('earned ready checkpoint has not delivered',quest()['stage']==4)
 do('test_case_companion_start',npc=1);dismiss_iolo();do('test_case_commission_start');do('test_case_breadcrumb_items',scenario=0);case=prepare_case();stones=inv(case)
 mark(case,stones[0]['ref']);close_gumps();mark(case,stones[1]['ref']);close_gumps();acquire_third();close_gumps()
 check('construction completion without delivery leaves both hints pending',quest()['stage']==4 and q()['a_pending']and q()['b_pending']);do('save',slot=1)
 # Original quest conversation, Xanthia walking and physical tabletop handoff.
 collect_delivery()
 check('real Xanthia tabletop delivery completes',quest()['stage']==5 and not quest()['delivery_active'])
 silence('both previously pending hints canceled after successful handoff')
 do('save',slot=2);do('load',slot=2);silence('suppression and canceled pending survive reload')
 join(3);silence('eligible Shamino stays silent after delivery')
 # Reload the earned tabletop checkpoint, then approach and pick up normally.
 do('load',slot=2);case=next(o['ref']for o in inv()if o['shape']==1601);do('move_to',x=1592,y=371,z=0,timeout=35);check('normal approach reaches delivered reward',arrived());reward=next(o for o in do('find_objects',shape=1601,radius=32)['objects']if o['tile']==dict(x=1592,y=370,z=2))
 bag=inventories();do('transfer_item',ref=reward['ref'],destination=bag);check('delivered case is picked up through native inventory',obj(reward['ref'])['owner']==bag)
 close_gumps();drop(case);close_gumps();join(3);do('key',key='1');export_checkpoint('delivered-silent');close_gumps()
 drop(reward['ref']);close_gumps();do('save',slot=4);do('load',slot=4);silence('dropping delivered case never reactivates hints')
 # Legacy delivered save has Stage D terminal state and no Stage E trailer.
 legacy=prior/'exult05bg.sav';load_archive(legacy)
 check('legacy delivered checkpoint recognized',quest()['stage']==5 and not q()['a_pending']and not q()['b_pending'])
 do('test_case_breadcrumb_items',scenario=0);case=prepare_case();stones=inv(case);mark(case,stones[0]['ref']);close_gumps();mark(case,stones[1]['ref']);close_gumps();acquire_third_legacy=do('test_case_breadcrumb_items',scenario=1)['synthetic_world_stone'];bag=inventories();do('transfer_item',ref=acquire_third_legacy['ref'],destination=bag)
 check('legacy completed quest suppresses freshly satisfied A and B triggers',q()['colors']==3 and q()['marked_stones']==2 and not q()['a_pending']and not q()['b_pending'])
 close_gumps();do('save',slot=5);do('load',slot=5);silence('legacy delivered suppression persists after new save')
 # Restore the earned pending/ready branch; speak both normally before handoff.
 do('load',slot=1);join(3);touch(3,True);until('A spoken before second delivery branch',lambda:q()['a_done']);do('native_wait',minutes=5);touch(3,True);until('B spoken before second delivery branch',lambda:q()['b_done'],35)
 check('both actual spoken completions recorded',q()['a_done']and q()['b_done']);do('test_case_commission_start');collect_delivery();check('second branch really delivered',quest()['stage']==5);do('save',slot=6);do('load',slot=6);check('delivery preserves both prior spoken completion bits',q()['a_done']and q()['b_done']and not q()['a_pending']and not q()['b_pending']);do('exit')
except Exception:
 d.failures+=1;events.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
 try:
  if d.p and d.p.poll()is None:capture('failure')
 except Exception:pass
finally:
 subprocess.Popen=original_popen
 if d.run:(d.run/'post-delivery-checks.json').write_text(json.dumps(events,indent=2))
 result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
