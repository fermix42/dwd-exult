"""Private global speech-duration review; one engine config shared by BG/Keyring/SI."""
import argparse,pathlib,shutil,subprocess
p=argparse.ArgumentParser();p.add_argument('--profile',choices=['BG','Keyring','SI'],required=True);p.add_argument('--state',choices=['continue','a-iolo','already-organized','delivered-silent']);p.add_argument('--prepare-only',action='store_true');a=p.parse_args();root=pathlib.Path(__file__).resolve().parent

if not(root/'.exult-test-run').is_file():raise SystemExit('Missing isolated review marker.')

def owned(relative):
 value=(root/relative).resolve();assert value.is_relative_to(root)and value!=root;return value
processes=subprocess.run(['tasklist','/FI','IMAGENAME eq Exult.exe','/FO','CSV','/NH'],capture_output=True,text=True)
if '"Exult.exe"'in processes.stdout:raise SystemExit('Close Exult before starting or restoring this isolated review.')
mod=a.profile=='Keyring';si=a.profile=='SI';profile='keyring'if mod else'bg'
gd=owned('gamedat/serpentisle'if si else'gamedat/blackgate'+('/mods/Keyring'if mod else''));sv=owned('saves/serpentisle'if si else'saves/blackgate'+('/mods/Keyring'if mod else''))
if not a.state:
 if si:a.state='continue'
 else:
  print('Global Overhead Text Duration review. Options -> Game Display.')
  print('C: Continue. R: Restore Iolo bark. A: Organized companion start. D: Delivered case.')
  print('Restoring a disposable checkpoint preserves the shared duration preference.')
  choice=input('Start [C]: ').strip().lower()or'c';a.state={'c':'continue','r':'a-iolo','a':'already-organized','d':'delivered-silent'}.get(choice)
  if a.state is None:raise SystemExit('Unknown choice.')
if not si and(a.state!='continue'or not(gd/'identity').is_file()):
 state=a.state if a.state!='continue'else'a-iolo';fixture=owned('prepared/'+profile+'/'+state)
 assert(fixture/'gamedat/identity').is_file()and(fixture/'exult08bg.sav').is_file()
 gd.mkdir(parents=True,exist_ok=True);sv.mkdir(parents=True,exist_ok=True)
 for entry in gd.iterdir():
  if not mod and entry.name=='mods':continue
  assert entry.resolve().is_relative_to(gd)
  if entry.is_dir():shutil.rmtree(entry)
  else:entry.unlink()
 shutil.copytree(fixture/'gamedat',gd,dirs_exist_ok=True);shutil.copy2(fixture/'exult08bg.sav',sv/'exult08bg.sav')
 print(a.profile+': '+state+' restored; exult.cfg preference is preserved.')
if not a.prepare_only:
 args=[str(owned('Exult.exe')),'-p','--si'if si else'--bg','--nomenu','-c',str(owned('exult.cfg'))]
 if mod:args+=['--mod','Keyring']
 with owned('normal-'+a.profile.lower()+'.log').open('w',encoding='utf-8')as log:raise SystemExit(subprocess.Popen(args,cwd=root,stdout=log,stderr=subprocess.STDOUT).wait())
