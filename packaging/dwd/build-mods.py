"""Build the three upstream installer mods from committed sources using normal UCC/expack."""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]

def build(output, ucc, expack, exconfig):
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    records = {}
    for mod in ['bgkeyring', 'sifixes', 'islefaq']:
        source = output / 'source' / mod
        if (ROOT/'.git').exists():
            paths = subprocess.check_output(['git','-C',str(ROOT),'ls-files','content/'+mod],text=True).splitlines()
            revision = subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
        else:
            paths = [n for n in json.loads((ROOT/'source-manifest.json').read_text()) if n.startswith('content/'+mod+'/')]
            revision = json.loads((ROOT/'source-provenance.json').read_text())['dwd_revision']
        for name in paths:
            relative = Path(name).relative_to('content/'+mod)
            target = source / relative
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(subprocess.check_output(['git','-C',str(ROOT),'show','HEAD:'+name]) if (ROOT/'.git').exists() else (ROOT/name).read_bytes())
        targetdir = 'patch' if mod == 'islefaq' else 'data'
        (source / targetdir).mkdir(exist_ok=True)
        log = output / (mod+'-build.log')
        with log.open('w') as stream:
            subprocess.run([str(ucc),'-I',str(source/'src'),'-o',str(source/targetdir/'usecode'),str(source/'src/usecode.uc')],cwd=source,stdout=stream,stderr=subprocess.STDOUT,check=True)
            for inp in sorted((source/'src/graphics').glob('*.in')):
                subprocess.run([str(expack),'-i',inp.as_posix()],cwd=source,stdout=stream,stderr=subprocess.STDOUT,check=True)
        install = (source/'Makefile.mingw').read_text().split('install: all',1)[1].split('uninstall:',1)[0]
        staged = output / 'staged' / mod
        for src,dst in re.findall(r'^\s*cp (\S+) \$\('+mod+r'dir\)/(\S+)\s*$',install,re.M):
            srcpath = ROOT/'COPYING' if src == './../../COPYING' else source/src
            assert srcpath.is_file(),srcpath
            target=staged/dst
            target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(srcpath,target)
        assert (staged/('Keyring.cfg' if mod=='bgkeyring' else mod+'.cfg')).is_file()
        archive=output/(mod+'.zip')
        with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
            for f in sorted(staged.rglob('*')):
                z.write(f,f.relative_to(staged))
        records[mod]={'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'files':len(list(staged.rglob('*'))),'source':'content/'+mod,'revision':revision}
    shutil.copy2(exconfig,output/'exconfig.exe')
    (output/'mod-build-manifest.json').write_text(json.dumps(records,indent=2)+'\n')
    print(json.dumps(records,indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for arg in ['output','ucc','expack','exconfig']:p.add_argument('--'+arg,required=True,type=Path)
    a=p.parse_args();build(a.output,a.ucc.resolve(),a.expack.resolve(),a.exconfig.resolve())
