"""Private Stage E review launcher; restores earned, isolated checkpoints."""
import argparse,pathlib,shutil,subprocess
p=argparse.ArgumentParser();p.add_argument('--profile',choices=['BG','Keyring'],required=True);p.add_argument('--state',choices=['a-iolo','a-shamino','b-shamino','b-dupre','deferred-recruit','both-pending','delivered-silent']);p.add_argument('--continue',dest='resume',action='store_true');p.add_argument('--prepare-only',action='store_true');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parent
def owned(relative):
    path=(root/relative).resolve();assert path.is_relative_to(root)and path!=root;return path
if not a.state and not a.resume:
    states=['a-iolo','a-shamino','b-shamino','b-dupre','deferred-recruit','both-pending','delivered-silent']
    print('Disposable companion breadcrumb review: BG / original Keyring')
    for i,state in enumerate(states,1):print(str(i)+': '+state)
    print('C: continue current review game. Selecting 1-7 resets only this review profile.')
    choice=input('Start [1]: ').strip().lower()or'1'
    if choice=='c':a.resume=True
    elif choice in ['1','2','3','4','5','6','7']:a.state=states[int(choice)-1]
    else:raise SystemExit('Unknown choice.')
result=subprocess.run(['tasklist','/FI','IMAGENAME eq Exult.exe','/FO','CSV','/NH'],capture_output=True,text=True)
if '"Exult.exe"'in result.stdout:raise SystemExit('Close Exult before restoring a review checkpoint.')
mod=a.profile=='Keyring';profile='keyring'if mod else'bg'
gd=owned('gamedat/blackgate'+('/mods/Keyring'if mod else''));sv=owned('saves/blackgate'+('/mods/Keyring'if mod else''))
if not a.resume:
    fixture=owned('prepared/'+profile+'/'+a.state)
    contextual=owned('prepared-context/'+profile+'/'+a.state)
    if contextual.is_dir():fixture=contextual
    assert(fixture/'gamedat/identity').is_file()and(fixture/'exult08bg.sav').is_file()
    gd.mkdir(parents=True,exist_ok=True);sv.mkdir(parents=True,exist_ok=True)
    for entry in gd.iterdir():
        if not mod and entry.name=='mods':continue
        assert entry.resolve().is_relative_to(gd)
        if entry.is_dir():shutil.rmtree(entry)
        else:entry.unlink()
    shutil.copytree(fixture/'gamedat',gd,dirs_exist_ok=True)
    shutil.copy2(fixture/'exult08bg.sav',sv/'exult08bg.sav')
    print(a.profile+': '+a.state+' checkpoint restored. S opens normal save/load; prepared slot is 8.')
if not a.prepare_only:
    args=[str(owned('Exult.exe')),'-p','--bg','--nomenu','-c',str(owned('exult-'+profile+'.cfg'))]
    if mod:args+=['--mod','Keyring']
    with owned('normal-'+profile+'.log').open('w',encoding='utf-8')as log:
        raise SystemExit(subprocess.Popen(args,cwd=root,stdout=log,stderr=subprocess.STDOUT).wait())
