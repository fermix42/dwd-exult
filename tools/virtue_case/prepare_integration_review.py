"""Assemble a NEW private Stage F runtime from retained earned/test checkpoints."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
source = Path(__file__).resolve().parents[2]
workspace = source.parent
review = a.output.resolve()
assert review.is_relative_to(workspace/'test-output') and not review.exists()
review.mkdir(parents=True)
speech = workspace/'test-output/speech-duration-20261104/human-review'
quest = workspace/'test-output/virtue-stage-d-final-dialogue/human-review'
chatter = workspace/'test-output/virtue-stage-e-final/human-review'
for name in ['data', 'mods']:
    shutil.copytree(speech/name, review/name)
shutil.copy2(source/'data/exult_bg.flx', review/'data/exult_bg.flx')
for file in speech.glob('*.dll'):
    shutil.copy2(file, review/file.name)
shutil.copy2(workspace/'build-exult-fork/bin/Exult.exe', review/'Exult.exe')
shutil.copy2(speech/'exult.cfg', review/'exult.cfg')
cfg = review/'exult.cfg'
cfg.write_bytes(cfg.read_bytes().replace(b'<cheat>yes</cheat>', b'<cheat>no</cheat>'))
shutil.copy2(source/'tools/virtue_case/HUMAN-INTEGRATION-TEST.txt', review/'HUMAN-TEST.txt')
shutil.copy2(source/'tools/virtue_case/start_integration_review.py', review/'start_integration_review.py')
(review/'.exult-test-run').write_text('Private Stage F human review; disposable state.\n')
fixtures = []
for profile in ['bg','keyring']:
    for state in ['a-iolo','b-dupre','already-organized','not-started','requirements','building','ready','delivered']:
        origin = (quest/'prepared'/profile/state if state in ['not-started','requirements','building','ready','delivered'] else
                  chatter/('prepared-natural' if state in ['b-dupre','already-organized'] else 'prepared')/profile/state)
        target = review/'prepared'/profile/state
        shutil.copytree(origin, target)
        fixtures.append(dict(profile=profile, state=state, source=str(origin),
                             sha256=hashlib.sha256((target/'exult08bg.sav').read_bytes()).hexdigest()))
for profile in ['BG','Keyring','SI']:
    (review/('Play '+profile+'.cmd')).write_text('@echo off\nsetlocal\n"'+sys.executable+'" "%~dp0start_integration_review.py" --profile '+profile+' %*\nif errorlevel 1 pause\n')
(review/'BUILD-PROVENANCE.json').write_text(json.dumps(dict(
    purpose='Private review only; not a redistributable release',
    source_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip(),
    executable_sha256=hashlib.sha256((review/'Exult.exe').read_bytes()).hexdigest(),
    fixtures=fixtures), indent=2))
subprocess.run([sys.executable,str(source/'tools/virtue_case/prepare_integration_cases.py'),
                '--root',str(review),'--prefix','integration-full-case'],check=True)
print(review)
