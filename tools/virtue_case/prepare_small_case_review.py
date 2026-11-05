"""Add four isolated closed-case size checkpoints per profile; preserve old saves."""
import argparse,json,shutil,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import Driver
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--evidence',type=Path,required=True);a=p.parse_args()
root=a.root.resolve();assert(root/'.exult-test-run').is_file();evidence=a.evidence.resolve();records=[]
for profile in ['bg','keyring']:
    suffix=Path('mods/Keyring/saves')if profile=='keyring'else Path('.')
    for name,run,slot in [('size-ready','delivery-'+profile,3),('size-table','delivery-'+profile,7),('size-backpack','small-final-'+profile+'-2x',8),('size-ground','small-final-'+profile+'-2x',6)]:
        target=root/'prepared'/profile/name;assert not target.exists()
        checkpoint=evidence/'runs'/run/'saves'/suffix/f'exult{slot:02}bg.sav';d=Driver()
        try:
            d.execute(dict(cmd='init',game='bg',mod='Keyring'if profile=='keyring'else'',run='review-'+profile+'-'+name,checkpoint=str(checkpoint)))
            d.execute(dict(cmd='load',slot=slot));q=d.execute(dict(cmd='case_commission_state'))['commission'];assert q['stage']==(4 if name=='size-ready'else 5);d.assertions+=1
            d.execute(dict(cmd='save',slot=8));at=d.state();shutil.copytree(at['gamedat_path'],target/'gamedat');shutil.copy2(Path(at['save_path'])/'exult08bg.sav',target/'exult08bg.sav');records.append(dict(profile=profile,state=name,source=str(checkpoint)));d.execute(dict(cmd='exit'))
        finally:
            result=d.close();print(profile,name,json.dumps(result),flush=True);assert result['result']=='PASS'
(root/'small-case-fixtures.json').write_text(json.dumps(records,indent=2))
