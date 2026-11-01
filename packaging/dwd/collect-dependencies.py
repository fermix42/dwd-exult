"""Collect hash-verified corresponding dependency sources and exact vcpkg recipes."""
import argparse
import hashlib
import json
import shutil
import subprocess
import urllib.request
from pathlib import Path
COMPONENTS=['sdl3','fluidsynth','libmt32emu','libpng','libogg','libvorbis','zlib','gcem','dirent']
DLLS={'sdl3':['SDL3.dll'],'fluidsynth':['libfluidsynth-3.dll'],'libmt32emu':['mt32emu-2.dll'],'libpng':['libpng16.dll'],'libogg':['ogg.dll'],'libvorbis':['vorbis.dll','vorbisenc.dll','vorbisfile.dll'],'zlib':['z.dll'],'gcem':[],'dirent':[]}

def collect(installed,vcpkg,output):
    installed=Path(installed).resolve();vcpkg=Path(vcpkg).resolve();output=Path(output).resolve()
    output.mkdir(parents=True,exist_ok=True)
    inventory={}
    for name in COMPONENTS:
        spdx=installed/'share'/name/'vcpkg.spdx.json'
        doc=json.loads(spdx.read_text())
        port,binary,upstream=doc['packages'][:3]
        location=upstream['downloadLocation']
        assert location.startswith('git+https://github.com/')
        repo,ref=location.removeprefix('git+https://github.com/').rsplit('@',1)
        url='https://github.com/'+repo+'/archive/'+ref+'.tar.gz'
        expected=next(x['checksumValue'] for x in upstream['checksums'] if x['algorithm']=='SHA512')
        archive=output/(name+'-source.tar.gz')
        if not archive.exists():
            print('Downloading '+name,flush=True)
            with urllib.request.urlopen(url,timeout=90) as response, archive.open('wb') as stream:
                shutil.copyfileobj(response,stream)
        assert hashlib.sha512(archive.read_bytes()).hexdigest()==expected, name+' source SHA512 mismatch'
        hashes={}
        for dll in DLLS[name]:
            f=installed/'bin'/dll
            digest=hashlib.sha256(f.read_bytes()).hexdigest()
            record=next(x for x in doc['files'] if x['fileName'].replace('\\','/').lower()=='./bin/'+dll.lower())
            assert digest==next(x['checksumValue'] for x in record['checksums'] if x['algorithm']=='SHA256'),dll+' installed binary does not match SPDX'
            hashes[dll]=digest
        recipe=output/'recipes'/name
        shutil.copytree(vcpkg/'ports'/name,recipe,dirs_exist_ok=True)
        # Verify installed SPDX's recipe checksums against the archived recipes.
        for item in doc['files']:
            if item['SPDXID'].startswith('SPDXRef-port-file-'):
                f=recipe/item['fileName'].removeprefix('./')
                assert f.is_file(),f
                for check in item['checksums']:
                    if check['algorithm']=='SHA256':assert hashlib.sha256(f.read_bytes()).hexdigest()==check['checksumValue'],str(f)+' differs from installed recipe'
        shutil.copy2(spdx,output/(name+'-installed.spdx.json'))
        inventory[name]={'version':port['versionInfo'],'license':port['licenseConcluded'],'source_url':url,'source_sha512':expected,'archive':archive.name,'binary_abi':binary['versionInfo'],'runtime_sha256':hashes}
    revision=subprocess.check_output(['git','-C',str(vcpkg),'rev-parse','HEAD'],text=True).strip()
    archive=output/'vcpkg-build-recipes.zip'
    if not archive.exists():subprocess.run(['git','-C',str(vcpkg),'archive','--format=zip','--prefix=vcpkg/','-o',str(archive),'HEAD'],check=True)
    shutil.copy2(installed.parent/'vcpkg/status',output/'installed-vcpkg-status.txt')
    (output/'dependency-sources.json').write_text(json.dumps({'vcpkg_revision':revision,'components':inventory},indent=2)+'\n')
    (output/'BUILDING.txt').write_text('These archives match installed SPDX source hashes and recipe files.\nExtract vcpkg-build-recipes.zip, bootstrap vcpkg, and build with the DWD source\nmsvcstuff/vs2019/vcpkg.json manifest on Visual Studio v143 x64 Release.\nSupply these hash-verified archives to vcpkg downloads/offline asset cache.\nPorts include all patches and CMake options; installed-vcpkg-status records features\nand ABIs. The source archive includes both vcpkg source and helper scripts.\nDLLs remain dynamically linked and replaceable; no ROM, soundfont or game data\nis contained in these dependency archives. Windows OS APIs and MSVC compiler/CRT\nare separate system/toolchain prerequisites, not third-party DLL source archives.\n')
    print('PASS: dependency archives, DLL-to-SPDX hashes and exact recipes verified',flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for arg in ['installed','vcpkg','output']:p.add_argument('--'+arg,required=True)
    a=p.parse_args();collect(a.installed,a.vcpkg,a.output)
