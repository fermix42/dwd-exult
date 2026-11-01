"""Exercise the real Inno optional-download path with real BG/SI asset sources."""
import argparse,hashlib,importlib.util,json,subprocess,time,winreg,zipfile
from pathlib import Path
spec=importlib.util.spec_from_file_location('installer_check',Path(__file__).with_name('installer-check.py'))
common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)

def main(a):
    work=a.work.resolve();assert not work.exists();work.mkdir(parents=True)
    app=work/'installed';candidate=a.candidate.resolve();checks=[]
    assert common.registry(winreg.HKEY_CURRENT_USER,r'Software\\DWDExult') is None,'Preserve existing DWD installation'
    protected={str(p.resolve()):common.tree(p.resolve()) for p in a.protect}
    def check(label,ok):
        checks.append({'check':label,'result':'PASS' if ok else 'FAIL'})
        (work/'results.json').write_text(json.dumps(checks,indent=2)+'\n');assert ok,label
    setup=next(candidate.glob('*-Setup.exe'))
    components='Exult,Docs,GPL,Paths,downloads/audio,downloads/mods/keyring,downloads/mods/sfisland,downloads/mods/sifixes,downloads/3rdpartymods/ultima6,downloads/3rdpartymods/glimmerscape'.replace('/','\\')
    args=[str(setup),'/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS','/DIR='+str(app),'/TYPE=custom','/COMPONENTS='+components,'/BGDIR='+str(a.bg.resolve()),'/SIDIR='+str(a.si.resolve()),'/LOG='+str(work/'setup.log')]
    try:
        r=subprocess.run(args,cwd=work,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=600)
        (work/'setup-command.log').write_bytes(r.stdout+r.stderr)
        check('real Inno optional download installation exit',r.returncode==0)
        log=(work/'setup.log').read_text(encoding='utf-8-sig',errors='replace')
        for archive,game in [('exult_audio.zip',None),('Ultima6-1.3.zip','blackgate'),('Glimmerscape_SI_mod_by_Donfrow.zip','serpentisle'),('bgkeyring.zip','blackgate'),('sifixes.zip','serpentisle'),('islefaq.zip','blackgate')]:
            source=(a.downloads/archive) if game is None or archive in ['Ultima6-1.3.zip','Glimmerscape_SI_mod_by_Donfrow.zip'] else candidate/archive
            root=app/'data' if game is None else app/'mods'/game
            files={p.relative_to(root).as_posix().casefold():p for p in root.rglob('*') if p.is_file()}
            with zipfile.ZipFile(source) as z:
                infos=[i for i in z.infolist() if not i.is_dir()]
                check('optional content installed '+archive,bool(infos) and all(i.filename.casefold() in files and files[i.filename.casefold()].read_bytes()==z.read(i) for i in infos))
        check('audio download succeeded through wizard','Successfully downloaded file' in log and 'exult_audio.zip' in log)
    finally:
        if (app/'unins000.exe').exists():
            r=subprocess.run([str(app/'unins000.exe'),'/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/LOG='+str(work/'uninstall.log')],capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=60)
            end=time.monotonic()+15
            while (app/'Exult.exe').exists() and time.monotonic()<end:time.sleep(.1)
            check('download-test uninstall',r.returncode==0 and not (app/'Exult.exe').exists())
        check('original assets and official installation unchanged',all(common.tree(Path(p))==state for p,state in protected.items()))
    print('PASS:',len(checks),'real installer download assertions')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['candidate','work','downloads','bg','si']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--protect',type=Path,action='append',default=[])
    main(p.parse_args())
