"""Read-only artifact/source audit; optional startup uses a fresh extracted copy."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def audit(a):
    root = a.candidate.resolve()
    validation = json.loads((root / 'validation.json').read_text())
    meta = validation['metadata']
    assert meta['dwd_revision'] == a.revision and not meta['dirty'], 'Wrong/dirty source identity'
    assert not validation['blockers'], validation['blockers']
    sums = (root / 'SHA256SUMS.txt').read_text().splitlines()
    for line in sums:
        expected, name = line.split('  ', 1)
        assert Path(name).name == name, 'Checksum entry escapes artifact root'
        assert digest((root / name).read_bytes()) == expected, name + ' checksum mismatch'
    version = meta['version']
    setup = root / ('DWD-Exult-' + version + '-Setup.exe')
    assert setup.read_bytes()[:2] == b'MZ', 'Missing Windows installer'
    payload = json.loads((root / 'payload-manifest.json').read_text())
    portable = root / ('DWD-Exult-' + version + '-Windows-x64.zip')
    with zipfile.ZipFile(portable) as z:
        assert z.testzip() is None, 'Corrupt portable archive'
        assert all(n.startswith('DWDExult/') and '..' not in Path(n).parts for n in z.namelist())
        for name, expected in payload.items():
            rel = name.replace('\\', '/')
            if rel.startswith('x86_64/'):
                rel = rel[len('x86_64/'):]
            assert digest(z.read('DWDExult/' + rel)) == expected, rel + ' payload mismatch'
        for name in ['README.txt', 'ChangeLog.txt', 'COPYING.txt', 'AUTHORS.txt', 'INSTALL.txt',
                     'FAQ.txt', 'README-DWD.md', 'ChangeLog-DWD.txt', 'data/exult.flx',
                     'data/exult_bg.flx', 'data/exult_si.flx', 'licenses/sdl3-copyright.txt']:
            assert 'DWDExult/' + name in z.namelist(), 'Missing resource/document: ' + name
        assert json.loads(z.read('DWDExult/release-metadata.json')) == meta
        for name in ['bgkeyring.zip', 'sifixes.zip', 'islefaq.zip']:
            assert z.read('DWDExult/mods/' + name) == (root / name).read_bytes()
        if a.smoke_root:
            assert not a.smoke_root.exists(), 'Use a fresh smoke directory'
            z.extractall(a.smoke_root)
    with zipfile.ZipFile(root / ('DWD-Exult-' + version + '-Source.zip')) as z:
        assert z.comment.decode() == a.revision, 'Source archive commit mismatch'
        assert z.testzip() is None, 'Corrupt source archive'
        assert json.loads(z.read('dwd-exult/source-provenance.json')) == meta
        for name, expected in json.loads(z.read('dwd-exult/source-manifest.json')).items():
            assert digest(z.read('dwd-exult/' + name)) == expected, name + ' source mismatch'
        deps = json.loads(z.read('dwd-exult/dependency-sources/dependency-sources.json'))
        assert deps == json.loads((root / 'dependency-sources.json').read_text())
        for info in deps['components'].values():
            data = z.read('dwd-exult/dependency-sources/' + info['archive'])
            assert hashlib.sha512(data).hexdigest() == info['source_sha512']
            for name, expected in info['runtime_sha256'].items():
                key = 'x86_64\\' + name if 'x86_64\\' + name in payload else 'x86_64/' + name
                assert payload[key] == expected, name + ' dependency mapping mismatch'
    if a.smoke_root:
        executable = a.smoke_root.resolve() / 'DWDExult/Exult.exe'
        result = subprocess.run([str(executable), '--version'], cwd=a.smoke_root,
                                capture_output=True, timeout=60,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        output = (result.stdout + result.stderr).decode(errors='replace')
        assert result.returncode == 0, output
        assert all(str(meta[k]) in output for k in ['version', 'dwd_revision', 'upstream_version', 'upstream_revision']), output
    print(json.dumps({'result': 'PASS', 'revision': a.revision, 'version': version,
                      'checksum_entries': len(sums), 'payload_files': len(payload),
                      'packaged_startup': 'PASS --version' if a.smoke_root else 'NOT TESTED',
                      'scope': 'Integrity/source audit; no gameplay or clean-OS claim'}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--smoke-root', type=Path)
    audit(parser.parse_args())
