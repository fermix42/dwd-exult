"""Verify ordinary (no -p/test-cli) ZIP storage, relocation and optional content paths."""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

def snapshot(root):
    return {p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else '<DIRECTORY>' for p in sorted(root.rglob('*'))}

def main(a):
    work=a.work.resolve();assert not work.exists();work.mkdir(parents=True)
    unrelated=work/'unrelated-working-directory';unrelated.mkdir()
    protected={str(p.resolve()):snapshot(p.resolve()) for p in a.protect}
    before=snapshot(unrelated);checks=[]
    def check(label,ok):
        checks.append({'check':label,'result':'PASS' if ok else 'FAIL'})
        (work/'results.json').write_text(json.dumps(checks,indent=2)+'\n');assert ok,label
    version=json.loads((a.candidate/'validation.json').read_text())['metadata']['version']
    with zipfile.ZipFile(a.candidate/('DWD-Exult-'+version+'-Windows-x64.zip')) as z:z.extractall(work/'extracted')
    app=work/'extracted/DWDExult';helper=(a.candidate/'installer-input/exconfig.exe').resolve()
    def run(args,label,expect=0):
        r=subprocess.run([str(v) for v in args],cwd=unrelated,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=120)
        (work/(label+'.log')).write_bytes(r.stdout+r.stderr)
        check(label+' exit',r.returncode==expect)
        return (r.stdout+r.stderr).decode(errors='replace')
    forbidden=(a.bg/'DWD-forbidden-install-root').resolve()
    reject=subprocess.run([str(helper),'VerifyDwdRoot',str(forbidden),str(a.bg.resolve()),str(a.si.resolve())],cwd=unrelated,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
    check('overlapping installer root rejected without creation',reject.returncode!=0 and b'outside original game directories' in reject.stdout+reject.stderr and not forbidden.exists())
    run([helper,'SetExultGamePaths',app,a.bg.resolve(),a.si.resolve()],'configure-zip')
    check('ZIP config is inside executable root',(app/'exult.cfg').is_file() and not (unrelated/'exult.cfg').exists())
    version=run([app/'Exult.exe','--version'],'version-from-unrelated-cwd')
    check('ordinary executable version',"Dark Wraith Dragon's Exult" in version)
    for game,archive in [('bg','bgkeyring.zip'),('si','sifixes.zip'),('bg','islefaq.zip')]:
        assert not (a.candidate/archive).exists(), 'Optional archives must stay outside binary packages'
        run([app/'Exult.exe','--'+game,'--installmod',(a.downloads/archive).resolve()],'portable-install-'+archive)
    run([app/'Exult.exe','--bg','--verify-files'],'normal-path-initialization')
    for game,mod in [('blackgate','Keyring'),('serpentisle','sifixes')]:
        check('separate mod save directory '+mod,(app/'saves'/game/'mods'/mod).is_dir())
        check('separate mod gamedat directory '+mod,(app/'gamedat'/game/'mods'/mod).is_dir())
        check('no mod-owned saves directory '+mod,not (app/'mods'/game/mod/'saves').exists())
    # These are metadata-only hostile path fixtures, not modified game/mod packages.
    fixture=app/'mods/blackgate/SaveOverride.cfg'
    doc=ET.Element('mod_info')
    for key,value in [('required_version','1.7.0git'),('patch','__MODS__/Keyring/data'),('savegame_path',str(a.bg/'dwd-forbidden-save')),('gamedat_path',str(a.si/'dwd-forbidden-gamedat'))]:ET.SubElement(doc,key).text=value
    ET.ElementTree(doc).write(fixture,encoding='utf-8')
    run([app/'Exult.exe','--bg','--verify-files'],'mod-save-override-isolation')
    check('mod save override stays under root',(app/'saves/blackgate/mods/SaveOverride').is_dir())
    check('mod gamedat override stays under root',(app/'gamedat/blackgate/mods/SaveOverride').is_dir())
    fixture.unlink()
    cfg=app/'exult.cfg';tree=ET.parse(cfg)
    for game in ['blackgate','serpentisle']:
        for field in ['mods','patch','source','savegame_path','gamedat_path']:tree.getroot().find('disk/game/'+game+'/'+field).text=str(a.bg/'forbidden-dwd-state')
    tree.write(cfg,encoding='utf-8')
    run([app/'Exult.exe','--bg','--verify-files'],'external-config-overrides-rebased')
    check('base writable config rewritten as relocatable',ET.parse(cfg).getroot().findtext('disk/game/blackgate/mods')=='<HOME>/mods/blackgate')
    check('unrelated CWD remains untouched',snapshot(unrelated)==before)
    # Install the real unmodified optional downloads and check every payload file.
    if a.downloads:
        for archive,game in [('exult_audio.zip',None),('Ultima6-1.3.zip','bg'),('Glimmerscape_SI_mod_by_Donfrow.zip','si')]:
            source=(a.downloads/archive).resolve()
            command=[app/'Exult.exe']+(['--'+game,'--installmod',source] if game else ['--installdata',source])
            run(command,'portable-optional-'+archive)
            with zipfile.ZipFile(source) as z:
                count=0
                for info in z.infolist():
                    if info.is_dir():continue
                    root=app/'data' if not game else app/'mods'/('blackgate' if game=='bg' else 'serpentisle')
                    target=root/info.filename
                    # Native mod install follows the original game's filename case.
                    if not target.is_file():
                        matches=[p for p in root.rglob('*') if p.is_file() and p.relative_to(root).as_posix().casefold()==info.filename.casefold()]
                        assert len(matches)==1,info.filename;target=matches[0]
                    assert target.read_bytes()==z.read(info),info.filename
                    count+=1
                check('optional content byte integrity '+archive,count>0)
    sentinel=app/'saves/blackgate/relocation.sav';sentinel.write_bytes(b'portable move sentinel')
    original_cfg=(app/'exult.cfg').read_bytes();oldapp=app
    app=work/'moved installation/DWDExult';app.parent.mkdir();shutil.move(str(oldapp),app)
    run([app/'Exult.exe','--bg','--verify-files'],'moved-folder-normal-startup')
    check('moved config paths remain relocatable',(app/'exult.cfg').read_bytes()==original_cfg)
    check('moved save stays with root',(app/'saves/blackgate/relocation.sav').read_bytes()==b'portable move sentinel')
    check('old root not recreated',not oldapp.exists())
    check('no unrelated CWD output after moving',snapshot(unrelated)==before)
    # Declaring an enclosing directory as game assets must fail before even
    # creating runtime temp/log files. This is a metadata-only fixture.
    cfg=app/'exult.cfg';saved_cfg=cfg.read_bytes();declared=ET.parse(cfg)
    declared.getroot().find('disk/game/blackgate/path').text=str(app.parent)
    declared.write(cfg,encoding='utf-8');before_refusal=snapshot(app)
    refusal=subprocess.run([str(app/'Exult.exe'),'--version'],cwd=unrelated,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=30)
    check('overlapping asset root fails before writes',refusal.returncode!=0 and snapshot(app)==before_refusal)
    cfg.write_bytes(saved_cfg)
    # A malformed archive must not redirect output outside its controlled root.
    bad=work/'escape-audio.zip'
    with zipfile.ZipFile(bad,'w') as z:z.writestr('../../unrelated-working-directory/escape.txt','not allowed')
    r=subprocess.run([str(app/'Exult.exe'),'--installdata',str(bad)],cwd=unrelated,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=30)
    check('archive traversal rejected',r.returncode!=0 and not (unrelated/'escape.txt').exists())
    # Deny writes only on this disposable extracted root, then restore its ACL.
    import csv,io
    identity=subprocess.check_output(['whoami','/user','/fo','csv','/nh'],text=True)
    sid=next(csv.reader(io.StringIO(identity)))[1]
    assert work in app.resolve().parents
    acl=subprocess.run(['icacls',str(app),'/deny','*'+sid+':(WD,AD)'],capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
    check('nonwritable fixture ACL applied',acl.returncode==0)
    try:
        refusal=subprocess.run([str(app/'Exult.exe'),'--version'],cwd=unrelated,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=30)
        check('nonwritable root fails clearly',refusal.returncode!=0 and b'writable installation directory' in refusal.stdout+refusal.stderr)
        check('nonwritable root never falls back to CWD',snapshot(unrelated)==before)
    finally:
        restored=subprocess.run(['icacls',str(app),'/remove:d','*'+sid],capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
        check('disposable ACL restored',restored.returncode==0)
    check('original game and official trees unchanged',all(snapshot(Path(p))==state for p,state in protected.items()))
    print('PASS:',len(checks),'ordinary portable/path assertions')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['candidate','work','bg','si']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--downloads',type=Path)
    p.add_argument('--protect',type=Path,action='append',default=[])
    main(p.parse_args())
