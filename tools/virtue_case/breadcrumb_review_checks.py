"""Headless real-engine smoke checks of the exact private review launcher paths."""
import argparse,json,subprocess,sys,time
import xml.etree.ElementTree as ET
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import Driver
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--prefix',default='review-final');a=p.parse_args();root=a.root.resolve()
original_popen=subprocess.Popen

def launch(args,**kw):
 if '--test-cli'in args:
  args=list(args);args[0]=str(root/'Exult.exe');args[args.index('-c')+1]=str(root/('exult-keyring.cfg'if '--mod'in args else'exult-bg.cfg'));args[args.index('--test-run-dir')+1]=str(root);kw['cwd']=root
 return original_popen(args,**kw)

def config_values(path):
 def walk(element,prefix):
  if not list(element):yield prefix,(element.text or'').strip()
  for child in element:yield from walk(child,prefix+'/'+child.tag)
 return dict(walk(ET.parse(path).getroot(),'config'))

states=['a-iolo','a-shamino','b-shamino','b-dupre','deferred-recruit','both-pending','delivered-silent']
for profile,mod in [('bg',''),('keyring','Keyring')]:
 cfg=root/('exult-'+profile+'.cfg');before=config_values(cfg)
 for name in states:
  subprocess.run([sys.executable,str(root/'start_breadcrumb_review.py'),'--profile',mod or'BG','--state',name,'--prepare-only'],check=True)
  d=Driver();subprocess.Popen=launch
  def check(label,value):
   d.assertions+=1;d.failures+=not value
   if not value:raise AssertionError(label)
  try:
   d.execute(dict(cmd='init',game='bg',mod=mod,run=a.prefix+'-'+profile+'-'+name))
   at=d.state();b=d.execute(dict(cmd='case_breadcrumbs_state'));q=b['breadcrumbs'];quest=d.execute(dict(cmd='case_commission_state'))['commission']
   gd=root/'gamedat/blackgate';sv=root/'saves/blackgate'
   if mod:gd=gd/'mods/Keyring';sv=sv/'mods/Keyring'
   check('canonical review gamedat',Path(at['gamedat_path']).resolve()==gd.resolve());check('canonical review saves',Path(at['save_path']).resolve()==sv.resolve())
   check('correct compiled Keyring selection',(at['overrides']>3)==bool(mod)and('Keyring'in at['patch_usecode_path'])==bool(mod))
   if name=='delivered-silent':
    check('terminal delivery with canceled hints',quest['stage']==5 and not q['a_pending']and not q['b_pending']);time.sleep(7);after=d.execute(dict(cmd='case_breadcrumbs_state'))['breadcrumbs'];check('no late speech in review profile',after['deliveries_since_load']==0)
   elif name=='deferred-recruit':check('pending before original recruitment',q['a_pending']and not q['a_done']and all(6 not in o['flags']for o in b['companions']))
   elif name=='both-pending':check('both real opportunities retained',(q['a_pending']or q['a_done'])and q['b_pending'])
   else:
    hint=name[0];npc={'a-iolo':1,'a-shamino':3,'b-shamino':3,'b-dupre':4}[name]
    check('hint and actual active speaker',bool(q[hint+'_pending']or q[hint+'_done'])and any(o['npc']==npc and 6 in o['flags']for o in b['companions']))
   d.execute(dict(cmd='exit'))
  finally:
   subprocess.Popen=original_popen;result=d.close();print(profile,name,json.dumps(result),flush=True)
  assert result['result']=='PASS',result
 subprocess.run([sys.executable,str(root/'start_breadcrumb_review.py'),'--profile',mod or'BG','--state','a-iolo','--prepare-only'],check=True)
 assert config_values(cfg)==before,'review configuration values unexpectedly changed'
