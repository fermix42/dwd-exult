"""Private Stage F checkpoint menu; never enables CLI mode or free casting."""
import argparse
from pathlib import Path
import shutil
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--profile', choices=['BG', 'Keyring', 'SI'], required=True)
p.add_argument('--state')
p.add_argument('--prepare-only', action='store_true')
a = p.parse_args()
root = Path(__file__).resolve().parent
if not (root/'.exult-test-run').is_file():
    raise SystemExit('Missing isolated review marker.')
def owned(name):
    path = (root/name).resolve()
    assert path.is_relative_to(root) and path != root
    return path
processes = subprocess.run(['tasklist','/FI','IMAGENAME eq Exult.exe','/FO','CSV','/NH'], capture_output=True, text=True)
if '"Exult.exe"' in processes.stdout:
    raise SystemExit('Close Exult before restoring a review checkpoint.')
states = ['a-iolo', 'b-dupre', 'ready', 'not-started', 'requirements', 'building', 'delivered', 'full-case', 'already-organized']
has_size_review = (root/'prepared'/a.profile.lower()/'size-ready').is_dir()
if has_size_review:
    states += ['size-ready', 'size-table', 'size-backpack', 'size-ground']
if not a.state:
    if a.profile == 'SI':
        a.state = 'continue'
    else:
        print('Closed-case size review:10 delivery,11 tabletop,12 backpack,13 ground.' if has_size_review else 'Private integrated case review.')
        for index, state in enumerate(states, 1):
            print(str(index)+': '+state)
        print('C: continue current game. A number restores only this disposable profile.')
        default = '10' if has_size_review else '1'
        choice = input('Start ['+default+']: ').strip().lower() or default
        a.state = 'continue' if choice == 'c' else states[int(choice)-1] if choice.isdigit() and 1 <= int(choice) <= len(states) else None
if a.state not in states + ['continue']:
    raise SystemExit('Unknown checkpoint.')
if a.profile == 'SI' and a.state != 'continue':
    raise SystemExit('BG checkpoints cannot be restored into Serpent Isle.')
mod = a.profile == 'Keyring'
profile = 'keyring' if mod else 'bg'
gd = owned('gamedat/serpentisle' if a.profile == 'SI' else 'gamedat/blackgate'+('/mods/Keyring' if mod else ''))
sv = owned('saves/serpentisle' if a.profile == 'SI' else 'saves/blackgate'+('/mods/Keyring' if mod else ''))
if a.profile != 'SI' and a.state == 'continue' and not (gd/'identity').is_file():
    a.state = 'a-iolo'
if a.state != 'continue':
    fixture = owned('prepared/'+profile+'/'+a.state)
    assert (fixture/'gamedat/identity').is_file() and (fixture/'exult08bg.sav').is_file()
    gd.mkdir(parents=True, exist_ok=True)
    sv.mkdir(parents=True, exist_ok=True)
    for entry in gd.iterdir():
        if not mod and entry.name == 'mods':
            continue
        assert entry.resolve().is_relative_to(gd)
        if entry.is_dir():
            shutil.rmtree(entry)
        else:
            entry.unlink()
    shutil.copytree(fixture/'gamedat', gd, dirs_exist_ok=True)
    shutil.copy2(fixture/'exult08bg.sav', sv/'exult08bg.sav')
    print(a.profile+': '+a.state+' restored. Shared exult.cfg and other save slots preserved.')
if not a.prepare_only:
    command = [str(owned('Exult.exe')), '-p', '--si' if a.profile == 'SI' else '--bg', '--nomenu', '-c', str(owned('exult.cfg'))]
    if mod:
        command += ['--mod','Keyring']
    with owned('normal-'+a.profile.lower()+'.log').open('w', encoding='utf-8') as log:
        raise SystemExit(subprocess.Popen(command, cwd=root, stdout=log, stderr=subprocess.STDOUT).wait())
