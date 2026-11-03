"""Materialize five real, earned commission checkpoints per private review profile."""
import hashlib, json, shutil, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import Driver, BASE

root=BASE/'human-review'
root.mkdir(exist_ok=True)
provenance=[]
for mod in ['', 'Keyring']:
    profile='keyring' if mod else 'bg'
    suffix=Path('mods/Keyring/saves') if mod else Path('.')
    delivery='d3-delivery-keyring-01' if mod else 'd3-delivery-bg-final'
    for state,stage,source,slot in [
        ('not-started',0,None,None),
        ('requirements',2,f'd2-transactions-{profile}-04',4),
        ('building',3,delivery,1),('ready',4,delivery,3),('delivered',5,delivery,5)]:
        target=root/'prepared'/profile/state
        if target.exists():
            provenance.append(dict(profile=profile,state=state,expected_stage=stage,fixture=str(target),archive_sha256=hashlib.sha256((target/'exult08bg.sav').read_bytes()).hexdigest()))
            continue
        d=Driver()
        try:
            args=dict(cmd='init',game='bg',mod=mod,run=f'd4-review-v2-{profile}-{state}')
            checkpoint=BASE/'runs'/source/'saves'/suffix/f'exult{slot:02}bg.sav' if source else None
            if checkpoint:args['checkpoint']=str(checkpoint)
            d.execute(args)
            if checkpoint:d.execute(dict(cmd='load',slot=slot))
            else:
                d.execute(dict(cmd='test_case_commission_start'))
                d.execute(dict(cmd='test_case_commission_items',scenario=1))
            q=d.execute(dict(cmd='case_commission_state'))
            assert q['commission']['stage']==stage, q
            d.assertions+=1
            d.execute(dict(cmd='save',slot=8))
            # A normal save has just written current gamedat and its matching archive.
            gd=Path(d.state()['gamedat_path'])
            sv=d.run/'saves'/suffix/'exult08bg.sav'
            target=root/'prepared'/profile/state
            assert not target.exists(),target
            shutil.copytree(gd,target/'gamedat')
            shutil.copy2(sv,target/'exult08bg.sav')
            provenance.append(dict(profile=profile,state=state,source=str(checkpoint),commission=q['commission'],fixture=str(target)))
            d.execute(dict(cmd='exit'))
        finally:
            result=d.close()
            assert result['result']=='PASS',result
        print(profile,state,'PASS',flush=True)
(root/'fixture-provenance.json').write_text(json.dumps(provenance,indent=2))
