"""Create a new isolated Stage E review without changing any prior review."""
import argparse,hashlib,json,shutil
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',type=Path,required=True)
p.add_argument('--bg-delivery-run',default='final-delivery-bg-04')
p.add_argument('--keyring-delivery-run',default='final-delivery-keyring-04')
a=p.parse_args()
source=Path(__file__).resolve().parents[2];workspace=source.parent
base=workspace/'test-output/virtue-stage-d-revision/human-review'
runs=workspace/'test-output/virtue-stage-e-final/runs';root=a.root.resolve()
assert root.is_relative_to(workspace/'test-output/virtue-stage-e-final')and not root.exists(),root
root.mkdir(parents=True)
for entry in base.iterdir():
 if entry.suffix.lower()=='.dll' or entry.name in ['exult-bg.cfg','exult-keyring.cfg']:
  shutil.copy2(entry,root/entry.name)
for name in ['data','mods']:shutil.copytree(base/name,root/name)
shutil.copy2(workspace/'build-exult-fork/bin/Exult.exe',root/'Exult.exe')
shutil.copy2(source/'data/exult_bg.flx',root/'data/exult_bg.flx')
shutil.copy2(source/'tools/virtue_case/start_breadcrumb_review.py',root/'start_breadcrumb_review.py')
shutil.copy2(source/'tools/virtue_case/HUMAN-BREADCRUMB-TEST.md',root/'HUMAN-TEST.md')
(root/'.exult-test-run').write_text('isolated Stage E human review; not a player profile\n')
provenance=[]
for profile,delivery in [('bg',a.bg_delivery_run),('keyring',a.keyring_delivery_run)]:
 breadcrumb='final-breadcrumb-'+profile
 for name in ['a-iolo','a-shamino','b-shamino','b-dupre','deferred-recruit','both-pending','delivered-silent']:
  run=delivery if name=='delivered-silent'else breadcrumb
  result=json.loads((runs/run/'result.json').read_text());assert result['result']=='PASS'and not result['failures'],run
  fixture=runs/run/'review-checkpoints'/name;target=root/'prepared'/profile/name
  shutil.copytree(fixture,target)
  provenance.append(dict(profile=profile,state=name,source=str(fixture),sha256=hashlib.sha256((target/'exult08bg.sav').read_bytes()).hexdigest()))
 label='BG'if profile=='bg'else'Keyring'
 (root/('Play '+label+'.cmd')).write_text('@echo off\ncd /d "%~dp0"\npython "%~dp0start_breadcrumb_review.py" --profile '+label+'\nif errorlevel 1 pause\n')
(root/'fixture-provenance.json').write_text(json.dumps(provenance,indent=2))
(root/'BUILD-PROVENANCE.json').write_text(json.dumps(dict(executable_sha256=hashlib.sha256((root/'Exult.exe').read_bytes()).hexdigest(),baseline='fc391bb48ae7bd13db9a3c9a9c258dede94a1e4f',purpose='private Stage E gameplay review, not a release'),indent=2))
print(root)
