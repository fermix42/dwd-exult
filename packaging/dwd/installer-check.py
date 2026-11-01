"""Silent host-isolated installer regression. No VM or interactive wizard claim."""
import argparse
import hashlib
import json
import os
import sys
import subprocess
import time
import winreg
import xml.etree.ElementTree as ET
from pathlib import Path

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def tree(root): return {p.relative_to(root).as_posix():digest(p) for p in sorted(root.rglob('*')) if p.is_file()}
def registry(hive,path):
    try:
        with winreg.OpenKey(hive,path) as key:
            result={};i=0
            while True:
                try:name,value,kind=winreg.EnumValue(key,i);result[name]=[value,kind];i+=1
                except OSError:return result
    except FileNotFoundError:return None

def main(a):
    candidate=a.candidate.resolve();work=a.work.resolve();app=work/'installed'
    assert not work.exists(),'Use a fresh test directory'
    dwd=r'Software\DWDExult';uninstall=r'Software\Microsoft\Windows\CurrentVersion\Uninstall\DWDExult_is1'
    assert registry(winreg.HKEY_CURRENT_USER,dwd) is None and registry(winreg.HKEY_CURRENT_USER,uninstall) is None,'Preserve existing DWD installations'
    official_keys=[(h,p) for h in [winreg.HKEY_CURRENT_USER,winreg.HKEY_LOCAL_MACHINE] for p in [r'Software\Exult',r'Software\Microsoft\Windows\CurrentVersion\Uninstall\Exult_is1',r'Software\WOW6432Node\Exult']]
    before_registry=[registry(h,p) for h,p in official_keys]
    before_files={str(p):tree(p) for p in a.protect}
    work.mkdir(parents=True)
    checks=[]
    def check(label,condition):
        checks.append({'check':label,'result':'PASS' if condition else 'FAIL'})
        (work/'results.json').write_text(json.dumps(checks,indent=2)+'\n')
        assert condition,label
    def run(args,label):
        r=subprocess.run([str(v) for v in args],cwd=work,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=120)
        (work/(label+'-command.log')).write_bytes(r.stdout+r.stderr)
        check(label+' exit',r.returncode==0)
        return (r.stdout+r.stderr).decode(errors='replace')
    setup=next(candidate.glob('*-Setup.exe'))
    args=[setup,'/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS','/DIR='+str(app),'/COMPONENTS=Exult,Docs,GPL','/LOG='+str(work/'install.log')]
    installed=False
    try:
        run(args,'fresh-install');installed=True
        manifest=json.loads((candidate/'payload-manifest.json').read_text())
        for name,expected in manifest.items():
            rel=Path(name)
            if rel.parts[0]=='mods':continue # embedded dontcopy optional packages
            if rel.parts[0]=='x86_64':rel=Path(*rel.parts[1:])
            check('installed file '+rel.as_posix(),(app/rel).is_file() and digest(app/rel)==expected)
        meta=json.loads((app/'release-metadata.json').read_text())
        version=run([app/'Exult.exe','--version'],'installed-version')
        check('exact installed identity',all(x in version for x in [meta['name'],meta['version'],meta['dwd_revision'],meta['upstream_revision']]))
        check('independent uninstall entry',registry(winreg.HKEY_CURRENT_USER,uninstall)['DisplayName'][0].startswith("Dark Wraith Dragon's Exult"))
        helper=candidate/'installer-input/exconfig.exe'
        run([helper,'SetExultGamePaths',app,a.bg,a.si],'set-game-paths')
        paths=run([helper,'GetExultGamePaths',app],'get-game-paths')
        check('path helper roundtrip',str(a.bg) in paths and str(a.si) in paths)
        cfg=app/'exult.cfg';doc=ET.parse(cfg)
        for game in ['blackgate','serpentisle']:
            g=doc.getroot().find('disk/game/'+game)
            ET.SubElement(g,'mods').text=(work/'mods').as_posix()
            ET.SubElement(g,'savegame_path').text=(work/'saves').as_posix()
        doc.write(cfg,encoding='utf-8')
        save=app/'saves/preserve.sav';save.parent.mkdir();save.write_bytes(b'DWD disposable preservation sentinel\n')
        external=work/'saves/external.sav';external.parent.mkdir();external.write_bytes(b'external disposable save\n')
        for mod,game in [('bgkeyring','bg'),('sifixes','si'),('islefaq','bg')]:
            run([app/'Exult.exe','-p','-c',cfg,'--'+game,'--installmod',candidate/(mod+'.zip')],'install-'+mod)
            import zipfile
            with zipfile.ZipFile(candidate/(mod+'.zip')) as archive:
                for info in archive.infolist():
                    if info.is_dir():continue
                    # Native installer normalizes names to the selected game's case.
                    target=work/'mods'/info.filename
                    matches=[p for p in (work/'mods').rglob('*') if p.is_file() and p.relative_to(work/'mods').as_posix().lower()==info.filename.lower()]
                    check('mod file '+mod+'/'+info.filename,len(matches)==1 and matches[0].read_bytes()==archive.read(info))
        if a.quiz_header:
            env=os.environ.copy()
            env.update(EXULT_TEST_EXE=str(app/'Exult.exe'),EXULT_TEST_RUNTIME_DIR=str(app),EXULT_TEST_DATA_PATH=str(app/'data'),EXULT_TEST_BG_PATH=str(a.bg),EXULT_TEST_SI_PATH=str(a.si),EXULT_TEST_MODS_PATH=str(work/'mods'),EXULT_TEST_OUTPUT=str(work/'runtime'))
            for game,mod in [('bg',''),('si',''),('bg','Keyring'),('si','sifixes')]:
                command=[sys.executable,str(Path(__file__).resolve().parents[2]/'tools/quiz/integration.py'),'--game',game,'--header',str(a.quiz_header.resolve()),'--run','installed-'+game+'-'+(mod or 'vanilla')]
                if mod:command+=['--mod',mod]
                result=subprocess.run(command,env=env,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=120)
                (work/('runtime-'+game+'-'+(mod or 'vanilla')+'.log')).write_bytes(result.stdout+result.stderr)
                check('installed runtime '+game+'/'+(mod or 'vanilla'),result.returncode==0)
        def config_values():
            def walk(node,prefix=''):
                path=prefix+'/'+node.tag
                if not len(node):return {path:(node.text or '').strip()}
                return {k:v for child in node for k,v in walk(child,path).items()}
            return walk(ET.parse(cfg).getroot())
        cfg_tree=config_values()
        saves_before={str(p):digest(p) for p in [save,external]}
        run(args[:-1]+['/LOG='+str(work/'upgrade.log')],'upgrade')
        check('upgrade retains config values',config_values()==cfg_tree)
        check('upgrade retains saves',all(Path(p).is_file() and digest(Path(p))==h for p,h in saves_before.items()))
        # Paths-only uses the existing valid paths and does not install the engine.
        run([setup,'/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS','/DIR='+str(app),'/TYPE=pathsonly','/COMPONENTS=GPL,Paths','/LOG='+str(work/'paths-only.log')],'paths-only')
        check('paths-only retains binary',digest(app/'Exult.exe')==manifest['x86_64\\Exult.exe'] if 'x86_64\\Exult.exe' in manifest else digest(app/'Exult.exe')==manifest['x86_64/Exult.exe'])
    finally:
        if installed and (app/'unins000.exe').is_file():
            run([app/'unins000.exe','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/LOG='+str(work/'uninstall.log')],'uninstall')
            end=time.monotonic()+10
            while ((app/'Exult.exe').exists() or registry(winreg.HKEY_CURRENT_USER,uninstall) is not None) and time.monotonic()<end:time.sleep(.1)
            check('installed executable removed',not (app/'Exult.exe').exists())
            check('DWD uninstall identity removed',registry(winreg.HKEY_CURRENT_USER,uninstall) is None)
            check('DWD install registry cleaned',registry(winreg.HKEY_CURRENT_USER,dwd) is None)
            check('config and saves survive uninstall',(app/'exult.cfg').is_file() and save.is_file() and external.is_file())
        check('official registry preserved',[registry(h,p) for h,p in official_keys]==before_registry)
        check('protected game/official files unchanged',all(tree(Path(p))==files for p,files in before_files.items()))
    print('PASS:',len(checks),'installer assertions; host-isolated, not a clean VM')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for x in ['candidate','work','bg','si']:p.add_argument('--'+x,type=Path,required=True)
    p.add_argument('--quiz-header',type=Path)
    p.add_argument('--protect',type=Path,action='append',default=[])
    main(p.parse_args())
