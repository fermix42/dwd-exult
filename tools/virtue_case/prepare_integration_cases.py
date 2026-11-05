"""Prepare filled-case review fixtures through native saved objects and CLI setup."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import Driver
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True)
p.add_argument('--prefix',required=True);a=p.parse_args();root=a.root.resolve()
assert(root/'.exult-test-run').is_file()
provenance=[]
for profile in ['bg','keyring']:
    d=Driver();mod='Keyring'if profile=='keyring'else''
    suffix=Path('mods/Keyring/saves')if mod else Path('.')
    checkpoint=root.parent/'runs'/('final-checks-'+profile)/'saves'/suffix/'exult03bg.sav'
    try:
        d.execute(dict(cmd='init',game='bg',mod=mod,run=a.prefix+'-'+profile,checkpoint=str(checkpoint)))
        d.execute(dict(cmd='load',slot=3))
        # Explicit test provisioning only: normal book/spells, mana and reagents.
        # No free-casting flag or quest state is assigned.
        d.execute(dict(cmd='test_case_spell_setup',scenario=0))
        def items(ref=0):
            for obj in d.execute(dict(cmd='inspect_inventory',ref=ref))['objects']:
                yield obj
                if obj['class_id']==6:yield from items(obj['ref'])
        case=next(o for o in items()if o['shape']==1601)
        stones=d.execute(dict(cmd='inspect_inventory',ref=case['ref']))['objects']
        assert len(stones)==8 and {o['frame']for o in stones}==set(range(8)) and all(o['shape']==330 for o in stones)
        assert any(o['shape']==761 for o in items())
        d.assertions+=2
        d.execute(dict(cmd='save',slot=8));at=d.state()
        target=(root/'prepared'/profile/'full-case').resolve()
        assert target.is_relative_to(root/'prepared')and target!=root/'prepared'
        if target.exists():shutil.rmtree(target)
        shutil.copytree(Path(at['gamedat_path']),target/'gamedat')
        shutil.copy2(Path(at['save_path'])/'exult08bg.sav',target/'exult08bg.sav')
        provenance.append(dict(profile=profile,source=str(checkpoint),sha256=hashlib.sha256((target/'exult08bg.sav').read_bytes()).hexdigest()))
        d.execute(dict(cmd='exit'))
    finally:
        result=d.close();print(profile,json.dumps(result),flush=True)
        assert result['result']=='PASS',result
(root/'filled-case-provenance.json').write_text(json.dumps(provenance,indent=2))
