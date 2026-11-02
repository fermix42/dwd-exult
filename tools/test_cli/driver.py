"""JSON Lines client for the ordinary Exult executable (Python stdlib only)."""
import argparse, ctypes, hashlib, json, os, pathlib, queue, shutil, subprocess, sys, threading, time, tempfile
from xml.sax.saxutils import escape
ROOT=pathlib.Path(__file__).resolve().parents[2]
BASE=pathlib.Path(os.environ.get('EXULT_TEST_OUTPUT',str(pathlib.Path(tempfile.gettempdir())/'exult-test-cli'))).resolve()
EXE=pathlib.Path(os.environ.get('EXULT_TEST_EXE',str(ROOT/'exult.exe'))).resolve()
def child_environment():
 env=os.environ.copy()
 if os.environ.get('EXULT_TEST_RUNTIME_DIR'):env['PATH']=os.environ['EXULT_TEST_RUNTIME_DIR']+os.pathsep+env.get('PATH','')
 return env
class EngineRefusal(RuntimeError):
 def __init__(self,response):
  self.detail=response
  super().__init__(response.get('error','engine refused'))
class Driver:
 def __init__(self):
  self.p=None; self.responses=queue.Queue(); self.seq=0; self.assertions=0; self.failures=0; self.run=None; self.last=None; self.exiting=False; self.shutdown=None; self.forced=False
 def init(self,c):
  if self.p: raise ValueError('already initialized; use a fresh session')
  game=c.get('game','bg'); mod=c.get('mod','')
  if game not in ('bg','si') or mod not in ('','Keyring','sifixes'): raise ValueError('unsupported game/mod')
  if (mod=='Keyring' and game!='bg') or (mod=='sifixes' and game!='si'): raise ValueError('mod/base mismatch')
  name=c.get('run',time.strftime('%Y%m%d-%H%M%S'))
  if not name or any(x not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_' for x in name): raise ValueError('invalid run name')
  game_path=os.environ.get('EXULT_TEST_BG_PATH' if game=='bg' else 'EXULT_TEST_SI_PATH')
  data_path=os.environ.get('EXULT_TEST_DATA_PATH')
  if not game_path or not data_path:raise ValueError('Set EXULT_TEST_BG_PATH/EXULT_TEST_SI_PATH and EXULT_TEST_DATA_PATH')
  path=pathlib.Path(game_path).resolve().as_posix();data_path=pathlib.Path(data_path).resolve().as_posix()
  mods_path=pathlib.Path(os.environ.get('EXULT_TEST_MODS_PATH',str(pathlib.Path(path)/'mods'))).resolve()
  if not EXE.is_file():raise ValueError('Set EXULT_TEST_EXE to the normally built Exult executable')
  if mod and not (mods_path/(mod+'.cfg')).is_file():raise ValueError('Selected mod not found; set EXULT_TEST_MODS_PATH')
  self.run=BASE/'runs'/name; self.run.mkdir(parents=True,exist_ok=False)
  (self.run/'.exult-test-run').write_text('isolated developer session\n')
  scale=int(c.get('scale',1))
  if scale<1 or scale>4:raise ValueError('scale must be 1..4')
  g='blackgate' if game=='bg' else 'serpentisle'
  keys_fragment=''
  if c.get('keys_path'):
   keys_path=pathlib.Path(c['keys_path']).resolve()
   if not keys_path.is_file():raise ValueError('keys_path must be a regular bindings file')
   keys_fragment='<keys>'+escape(keys_path.as_posix())+'</keys>'
  patch_path=c.get('patch_path')
  patch_fragment=''
  if patch_path:
   patch_path=pathlib.Path(patch_path).resolve()
   if not (patch_path/'usecode').is_file():raise ValueError('patch_path must contain a usecode resource')
   patch_fragment='<patch>'+escape(patch_path.as_posix())+'</patch>'
  cfg=f'''<config><disk><data_path>{escape(data_path)}</data_path><game><{g}><path>{escape(path)}</path><mods>{escape(mods_path.as_posix())}</mods>{patch_fragment}{keys_fragment}<gamedat_path>{escape(self.run.as_posix())}/gamedat</gamedat_path><savegame_path>{escape(self.run.as_posix())}/saves</savegame_path></{g}></game></disk><gameplay><skip_splash>yes</skip_splash><skip_intro>yes</skip_intro></gameplay><video><fullscreen>no</fullscreen><scale>{scale}</scale><scale_method>point</scale_method><vsync>0</vsync><display><width>640</width><height>480</height></display></video><audio><enabled>no</enabled></audio></config>'''
  (self.run/'exult.cfg').write_text(cfg.replace('><', '>\n<'))
  checkpoint=c.get('checkpoint')
  if checkpoint:
   source=pathlib.Path(checkpoint).resolve()
   if not source.is_file() or source.suffix.lower()!='.sav':raise ValueError('checkpoint must be a regular Exult save archive')
  env=child_environment()
  self.stdout=(self.run/'engine.log').open('w',encoding='utf8');self.stderr=(self.run/'stderr.log').open('w',encoding='utf8')
  args=[str(EXE),'--test-cli','--test-run-dir',str(self.run),'-p','--'+game,'--nomenu','-c',str(self.run/'exult.cfg')]
  if mod: args+=['--mod',mod]
  self.before=self.desktop()
  self.p=subprocess.Popen(args,cwd=self.run,env=env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=self.stderr,text=True,encoding='utf8',errors='replace',creationflags=subprocess.CREATE_NO_WINDOW)
  def reader():
   for line in self.p.stdout:
    self.stdout.write(line);self.stdout.flush()
    if line.startswith('@CLI '):
     try:
      response=json.loads(line[5:])
      if response.get('event') in ('shutdown','cancelled'):self.shutdown=response
      self.responses.put(response)
     except Exception as e:self.responses.put({'ok':False,'error':'malformed engine reply '+str(e)})
  self.reader=threading.Thread(target=reader,daemon=True);self.reader.start()
  ready=self.receive(0,30)
  if checkpoint:
   # Ask the ordinary engine for its selected save directory (mods may nest it).
   target=pathlib.Path(self.raw('state')['state']['save_path']).resolve()
   if self.run.resolve() not in target.parents:raise ValueError('engine save path escapes fresh run')
   target.mkdir(parents=True,exist_ok=True)
   shutil.copyfile(source,target/source.name)
  # Record exact resource identities, read only.
  paths=[pathlib.Path(path)/'static'/'usecode']
  if mod: paths+=list((mods_path/mod).rglob('*usecode*'))
  if patch_path:paths.append(patch_path/'usecode')
  resources=[{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths if p.is_file()]
  return {'ok':True,'category':'lifecycle','ready':ready,'run':str(self.run),'game':game,'mod':mod,'resources':resources,'desktop_before':self.before,'desktop_after':self.desktop(),'executable':str(EXE),'executable_sha256':hashlib.sha256(EXE.read_bytes()).hexdigest(),'checkpoint':{'path':str(source),'sha256':hashlib.sha256(source.read_bytes()).hexdigest()} if checkpoint else None}
 def desktop(self):
  class POINT(ctypes.Structure):_fields_=[('x',ctypes.c_long),('y',ctypes.c_long)]
  p=POINT();u=ctypes.windll.user32;u.GetCursorPos(ctypes.byref(p));u.GetForegroundWindow.restype=ctypes.c_void_p
  windows=[]
  if self.p:
   callback=ctypes.WINFUNCTYPE(ctypes.c_bool,ctypes.c_void_p,ctypes.c_void_p)
   def visit(hwnd,_):
    pid=ctypes.c_ulong();u.GetWindowThreadProcessId(ctypes.c_void_p(hwnd),ctypes.byref(pid))
    if pid.value==self.p.pid:windows.append({'hwnd':hwnd,'visible':bool(u.IsWindowVisible(ctypes.c_void_p(hwnd)))})
    return True
   cb=callback(visit);u.EnumWindows(cb,0)
  return {'foreground':u.GetForegroundWindow(),'cursor':[p.x,p.y],'engine_windows':windows}
 def receive(self,id,timeout):
  end=time.monotonic()+timeout
  while time.monotonic()<end:
   try:r=self.responses.get(timeout=min(.1,max(.001,end-time.monotonic())))
   except queue.Empty:
    if self.p.poll() is not None:raise RuntimeError(f'engine exited {self.p.returncode}; inspect engine.log/stderr.log')
    continue
   if r.get('id')==id:return r
  raise TimeoutError('engine reply timeout')
 def raw(self,op,*args):
  if not self.p or self.p.poll() is not None:raise RuntimeError('engine is not running')
  self.seq+=1;self.p.stdin.write(f'{self.seq} {op} '+ ' '.join(str(a) for a in args)+'\n');self.p.stdin.flush();r=self.receive(self.seq,5)
  if not r.get('ok'):raise EngineRefusal(r)
  if op=='exit':self.exiting=True
  if 'state' in r:self.last=r['state']
  return r
 def state(self):return self.raw('state')['state']
 def execute(self,c):
  op=c['cmd']
  if op=='init':return self.init(c)
  if op in ('wait','run_until'):
   end=time.monotonic()+min(120,max(.01,float(c.get('timeout',10)))); cond=c.get('until',{})
   while True:
    s=self.state()
    if all(s.get(k)==v for k,v in cond.items()) and (cond or time.monotonic()>=end):return {'ok':True,'category':'inspection','state':s}
    if time.monotonic()>=end:raise TimeoutError('condition timeout: '+json.dumps(cond))
    time.sleep(.05)
  if op=='assert':
   s=self.state();expected=c['state'];ok=all(s.get(k)==v for k,v in expected.items());self.assertions+=1
   if not ok:self.failures+=1
   return {'ok':ok,'category':'inspection','assertion':c.get('label',expected),'expected':expected,'state':s}
  if op in ('find_objects','find_actors','inspect_world'):
   return self.raw(op,int(c.get('radius',12)),int(c.get('limit',50)),int(c.get('offset',0)),int(c.get('shape',-359)))
  if op in ('inspect_object','inspect_actor','interact_object','move_to_object','target_object'):
   return self.raw(op,int(c['ref']),0)
  if op in ('inspect_tile','path_query','move_to','target_tile'):
   return self.raw(op,int(c['x']),int(c['y']),int(c.get('z',0)),int(float(c.get('timeout',10))*1000))
  if op=='inspect_usecode':return self.raw(op,int(c['function']))
  if op in ('inspect_inventory','interact_inventory'):return self.raw(op,int(c.get('ref',0)))
  if op=='key':
   names={'w':119,'escape':27,'enter':13,'backspace':8,'left':0x40000050,'right':0x4000004f,'up':0x40000052,'down':0x40000051,'home':0x4000004a,'end':0x4000004d}
   names.update({str(i):ord(str(i))for i in range(1,9)})
   if c['key'].lower()not in names:raise ValueError('unsupported semantic key')
   return self.raw(op,names[c['key'].lower()],3 if c.get('shift',False)else 0)
  if op=='test_light_item':return self.raw(op,int(c['kind']))
  if op=='test_remove_fuel_receiver':return self.raw(op,int(c['ref']))
  if op=='drop_item':return self.raw(op,int(c['ref']),int(c['x']),int(c['y']),int(c.get('z',0)))
  if op=='ready_inventory':return self.raw(op,int(c['ref']),int(c['slot']))
  if op=='move_gump':return self.raw(op,int(c['ref']),int(c['x']),int(c['y']))
  if op=='transfer_item':
   return self.raw(op,int(c['ref']),int(c['destination']),int(c['x']),int(c['y'])) if 'x' in c else self.raw(op,int(c['ref']),int(c['destination']))
  if op=='type_text':
   for ch in c['text']:self.raw('text_character',ord(ch))
   return {'ok':True,'category':'gameplay','characters_dispatched':len(c['text'])}
  if op=='aim_object':return self.raw(op,int(c['ref']))
  if op=='test_case_extra':return self.raw(op)
  if op=='spellbook_cast':return self.raw(op,int(c['ref']),int(c['spell']))
  if op=='test_case_spell_setup':return self.raw(op,int(c.get('scenario',0)))
  if op=='case_spell_state':return self.raw(op)
  if op=='target_at':return self.raw(op,int(c['x']),int(c['y']))
  if op=='test_stall':return self.raw(op,int(c['milliseconds']))
  if op in ('test_exposure','test_orb_chart_item','test_virtue_case','test_virtue_case_labels','test_jawbone_items'):return self.raw(op)
  if op=='test_equipment':return self.raw(op,int(c['kind']))
  if op=='test_stale_event_metadata':return self.raw(op,int(c['ref']))
  if op=='test_calendar':return self.raw(op,int(c['day']),int(c['hour']),int(c['minute']),int(c.get('phase',0)))
  if op=='test_legacy_clock':return self.raw(op,int(c['minutes']))
  if op=='test_time_rate':return self.raw(op,int(c['rate']))
  if op=='test_temporary_effect':return self.raw(op,{'protection':1,'might':2}[c['effect']])
  if op=='native_wait':return self.raw(op,int(c['minutes']))
  if op=='wait_eligibility':return self.raw(op)
  if op=='activate_control':return self.raw(op,int(c['id']),0)
  if op=='choose':
   index=c.get('index')
   if index is None:index=self.state()['choices'].index(c['text'])
   return self.raw(op,int(index))
  if op in ('move','interact','click','control_at'):return self.raw(op,int(c['x']),int(c['y']))
  if op in ('save','load'):return self.raw(op,int(c.get('slot',0)))
  if op=='screenshot':return self.raw(op,1) if c.get('full',False) else self.raw(op)
  if op in ('state','ui_state','controls','continue','dismiss','quit_prompt','movement_status','combat','exit'):return self.raw(op)
  raise ValueError('unknown command '+op)
 def close(self):
  if self.p and self.p.poll() is None:
   try:
    if not self.exiting:self.raw('exit')
    self.p.wait(timeout=3)
   except Exception:
    if self.p.poll() is None:
     self.failures+=1;self.forced=True;self.p.kill();self.p.wait(timeout=3)
    # Native quit confirmation may have completed cleanup before an exit reply.
    # The final shutdown event/status below establishes its actual outcome.
  if self.p:
   self.reader.join(timeout=2)
   if self.p.returncode != 0 or not self.shutdown or not self.shutdown.get('cleanup_complete'):self.failures+=1
  if self.run and self.p:
   evidence={'desktop_before':self.before,'desktop_after':self.desktop(),'assertions':self.assertions,'failures':self.failures,'result':'PASS' if self.assertions and not self.failures else 'FAIL','exit_code':self.p.returncode,'shutdown':self.shutdown,'termination':'forced' if self.forced else 'cancelled' if self.shutdown and self.shutdown.get('event')=='cancelled' else 'normal' if self.shutdown and self.shutdown.get('cleanup_complete') and self.p.returncode==0 else 'failed'}
   (self.run/'result.json').write_text(json.dumps(evidence,indent=2));return evidence

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--script',type=pathlib.Path);a=ap.parse_args();d=Driver();log=None
 try:
  with (a.script.open() if a.script else sys.stdin) as commands:
   for line in commands:
    if not line.strip():continue
    try:c=json.loads(line);r=d.execute(c)
    except Exception as e:d.failures+=1;r={'ok':False,'category':'harness_error' if isinstance(e,(TimeoutError,OSError)) else 'command_error','error':str(e),'detail':getattr(e,'detail',None)}
    record={'command':line.strip(),'response':r};print(json.dumps(record),flush=True)
    if d.run:
     if log is None:log=(d.run/'session.jsonl').open('a')
     log.write(json.dumps(record)+'\n');log.flush()
    if not r['ok'] and a.script:break
 finally:
  result=d.close();print(json.dumps({'result':result}),flush=True)
  if log:log.close()
 return 1 if d.failures or not d.assertions else 0
if __name__=='__main__':sys.exit(main())
