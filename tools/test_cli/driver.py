"""JSON Lines client for the ordinary Exult executable (Python stdlib only)."""
import argparse, ctypes, hashlib, json, os, pathlib, queue, subprocess, sys, threading, time, tempfile
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
  g='blackgate' if game=='bg' else 'serpentisle'
  cfg=f'''<config><disk><data_path>{escape(data_path)}</data_path><game><{g}><path>{escape(path)}</path><mods>{escape(mods_path.as_posix())}</mods><gamedat_path>{escape(self.run.as_posix())}/gamedat</gamedat_path><savegame_path>{escape(self.run.as_posix())}/saves</savegame_path></{g}></game></disk><gameplay><skip_splash>yes</skip_splash><skip_intro>yes</skip_intro></gameplay><video><fullscreen>no</fullscreen><scale>1</scale><scale_method>point</scale_method><vsync>0</vsync><display><width>640</width><height>480</height></display></video><audio><enabled>no</enabled></audio></config>'''
  (self.run/'exult.cfg').write_text(cfg.replace('><', '>\n<'))
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
  # Record exact resource identities, read only.
  paths=[pathlib.Path(path)/'static'/'usecode']
  if mod: paths+=list((mods_path/mod).rglob('*usecode*'))
  resources=[{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths if p.is_file()]
  return {'ok':True,'category':'lifecycle','ready':ready,'run':str(self.run),'game':game,'mod':mod,'resources':resources,'desktop_before':self.before,'desktop_after':self.desktop(),'executable':str(EXE),'executable_sha256':hashlib.sha256(EXE.read_bytes()).hexdigest()}
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
  if op in ('inspect_object','inspect_actor','interact_object','move_to_object'):
   return self.raw(op,int(c['ref']),0)
  if op in ('inspect_tile','path_query','move_to'):
   return self.raw(op,int(c['x']),int(c['y']),int(c.get('z',0)),int(float(c.get('timeout',10))*1000))
  if op=='activate_control':return self.raw(op,int(c['id']),0)
  if op=='choose':
   index=c.get('index')
   if index is None:index=self.state()['choices'].index(c['text'])
   return self.raw(op,int(index))
  if op in ('move','interact','click'):return self.raw(op,int(c['x']),int(c['y']))
  if op in ('save','load'):return self.raw(op,int(c.get('slot',0)))
  if op in ('state','ui_state','controls','continue','dismiss','quit_prompt','movement_status','combat','screenshot','exit'):return self.raw(op)
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
