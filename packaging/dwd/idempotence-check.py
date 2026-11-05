"""Targeted real Inno upgrades using verified archive bytes and an instrumented
loopback download fixture. Does not run gameplay or modify the production catalog.
"""
import argparse
import configparser
import hashlib
import http.server
import importlib.util
import json
import shutil
import subprocess
import threading
import time
import winreg
import zipfile
from pathlib import Path

spec = importlib.util.spec_from_file_location('installer_common', Path(__file__).with_name('installer-check.py'))
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)


def main(a):
    candidate = a.candidate.resolve()
    work = a.work.resolve()
    assert not work.exists(), 'Use a fresh disposable root'
    assert common.registry(winreg.HKEY_CURRENT_USER, r'Software\DWDExult') is None, 'Preserve existing DWD install'
    assert common.registry(winreg.HKEY_CURRENT_USER, r'Software\Microsoft\Windows\CurrentVersion\Uninstall\DWDExult_is1') is None
    before = {str(p.resolve()): common.tree(p.resolve()) for p in a.protect}
    work.mkdir(parents=True)
    checks = []
    def check(name, ok):
        checks.append({'check': name, 'result': 'PASS' if ok else 'FAIL'})
        (work / 'results.json').write_text(json.dumps(checks, indent=2) + '\n')
        assert ok, name
    requests = []
    responses = {}
    downloads = a.downloads.resolve()
    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            name = self.path.removeprefix('/')
            requests.append(name)
            if responses.get(name) == 'unavailable':
                self.send_error(503)
                return
            if isinstance(responses.get(name), bytes):
                data = responses[name]
                self.send_response(200)
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)
                return
            path = downloads / name
            if name not in ['exult_audio.zip', 'Ultima6-1.3.zip', 'Glimmerscape_SI_mod_by_Donfrow.zip', 'bgkeyring.zip', 'sifixes.zip', 'islefaq.zip']:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header('Content-Length', str(path.stat().st_size))
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            with path.open('rb') as stream:
                shutil.copyfileobj(stream, self.wfile)
        def log_message(self, *args):
            pass
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    stage = work / 'fixture'
    shutil.copytree(candidate / 'installer-input', stage)
    ini = configparser.ConfigParser(interpolation=None)
    ini.optionxform = str
    ini.read(stage / 'dwd-content.ini', encoding='utf-16')
    count = int(ini['packages']['count'])
    for i in range(count):
        p = ini[str(i)]
        if p['url']:
            p['url'] = 'http://127.0.0.1:' + str(server.server_port) + '/' + p['archive']
    with (stage / 'dwd-content.ini').open('w', encoding='utf-16') as stream:
        ini.write(stream)
    def compile_fixture(label):
        r = subprocess.run([str(a.iscc.resolve()), '/DNO_i686=1', '/DNO_ARM64=1', str(stage / 'dwd_installer.iss')],
                           capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW, timeout=120)
        (work / (label + '-compile.log')).write_bytes(r.stdout + r.stderr)
        check(label + ' fixture compiled', r.returncode == 0)
        target = work / (label + '-Setup.exe')
        shutil.copy2(next(stage.glob('*-Setup.exe')), target)
        return target
    setup = compile_fixture('original')
    app = work / 'installed'
    all_components = ['Exult', 'Docs', 'GPL', 'Paths'] + [ini[str(i)]['component'] for i in range(count)]
    initial_components = ['Exult', 'Docs', 'GPL', 'Paths', 'downloads\\audio', 'downloads\\mods\\keyring']
    def run(label, components=all_components, extra=(), program=None, success=True):
        r = subprocess.run([str(program or setup), '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/NOICONS',
                            '/DIR=' + str(app), '/COMPONENTS=' + ','.join(components),
                            '/BGDIR=' + str(a.bg.resolve()), '/SIDIR=' + str(a.si.resolve()),
                            '/LOG=' + str(work / (label + '.log')), *extra], cwd=work,
                           capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW, timeout=180)
        (work / (label + '-command.log')).write_bytes(r.stdout + r.stderr)
        check(label + ' exit', (r.returncode == 0) == success)
        return (work / (label + '.log')).read_text(encoding='utf-8-sig', errors='replace')
    def paths(i):
        p = ini[str(i)]
        return [app / p['root'] / p['file' + str(j)] for j in range(int(p['count']))]
    def verified(i):
        p = ini[str(i)]
        return all(f.is_file() and common.digest(f) == p['hash' + str(j)] for j, f in enumerate(paths(i)))
    def state():
        roots = ['mods', 'saves', 'content-state']
        result = {r: common.tree(app / r) for r in roots}
        result['audio'] = {str(p.relative_to(app)): common.digest(p) for p in paths(0) if p.exists()}
        watched = [p for r in roots for p in (app / r).rglob('*') if p.is_file()] + [p for p in paths(0) if p.exists()] + [app / 'exult.cfg']
        result['mtimes'] = {str(p.relative_to(app)): p.stat().st_mtime_ns for p in watched}
        result['cfg'] = (app / 'exult.cfg').read_bytes().hex()
        return result
    try:
        base_components = ['Exult', 'Docs', 'GPL', 'Paths']
        mainapp = app
        for index in [None, 1, 2, 3]:
            app = work / ('fresh-' + str(index))
            requests.clear()
            run('fresh-selection-' + str(index), base_components + ([] if index is None else [ini[str(index)]['component']]))
            check('fresh independent selection ' + str(index), requests == ([] if index is None else [ini[str(index)]['archive']]) and (index is None or verified(index)))
            subprocess.run([str(app / 'unins000.exe'), '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART'], creationflags=subprocess.CREATE_NO_WINDOW, check=True, timeout=60)
            end = time.monotonic() + 15
            while (app / 'Exult.exe').exists() and time.monotonic() < end: time.sleep(.1)
        app = mainapp
        requests.clear()
        run('fresh', initial_components)
        check('fresh selected audio and Keyring exact bytes', verified(0) and verified(1))
        check('fresh only selected remote content requested', requests == ['exult_audio.zip', 'bgkeyring.zip'])
        cfg = app / 'exult.cfg'
        cfg.write_bytes(cfg.read_bytes() + b'\n<!-- preserve owner formatting -->\n')
        keycfg = app / 'mods/blackgate/Keyring.cfg'
        keycfg.write_bytes(keycfg.read_bytes() + b'\n<!-- customized mod -->\n')
        extra = app / 'mods/blackgate/Keyring/custom-file.txt'
        extra.write_bytes(b'custom content')
        save = app / 'saves/blackgate/mods/Keyring/keep.sav'
        save.parent.mkdir(parents=True, exist_ok=True)
        save.write_bytes(b'player save sentinel')
        saved = state()
        requests.clear()
        log = run('same-version', initial_components)
        check('same-version no HTTP requests or native reinstalls', requests == [] and 'network requests scheduled: 0' in log and 'InstallMod:' not in log and 'InstallData:' not in log)
        check('same-version preserves customized content config saves receipts', state() == saved)
        run('new-selection')
        check('new selection downloads only new remote packages', sorted(requests) == sorted(['Glimmerscape_SI_mod_by_Donfrow.zip', 'Ultima6-1.3.zip', 'sifixes.zip', 'islefaq.zip']))
        check('newly selected optional packages verified', all(verified(i) for i in range(2, count)))
        check('new selection preserves existing customization/config/save', keycfg.read_bytes().endswith(b'<!-- customized mod -->\n') and cfg.read_bytes().hex() == saved['cfg'] and save.read_bytes() == b'player save sentinel')
        # A receiptless independently managed third-party installation is never guessed away.
        u6 = app / 'mods/blackgate/Ultima6v1.3.cfg'
        u6.write_bytes(u6.read_bytes() + b'\n<!-- independently managed -->\n')
        (app / 'content-state/ultima6-original.ini').unlink()
        missing = next(p for p in paths(1) if p.name.lower() == 'usecode' or p.suffix.lower() in ['.flx', '.vga'])
        missing.unlink()
        damaged = next(p for p in paths(0) if p.suffix.lower() == '.ogg')
        damaged.write_bytes(damaged.read_bytes() + b'customized audio')
        saved = state()
        requests.clear()
        log = run('damaged-and-unmanaged')
        check('damaged and unknown packages preserve by default without HTTP', not requests and state() == saved)
        check('repair and unknown status reported', 'repair available' in log and 'version unverified' in log)
        run('repair-not-approved', extra=['/REINSTALLCONTENT=keyring-upstream'], success=False)
        check('unapproved explicit replacement changed no content/config/saves', state() == saved and not requests)
        log = run('approved-repair', extra=['/REINSTALLCONTENT=keyring-upstream,audio-original', '/ALLOWCONTENTREPLACEMENT=yes'])
        check('explicit repair only requested audio and Keyring transfers', requests == ['exult_audio.zip', 'bgkeyring.zip'])
        check('explicit repair restores exact package files', verified(0) and verified(1))
        backups = list((app / 'content-backups').rglob('*'))
        check('custom mod and audio backups retained', any(p.is_file() and p.name == 'custom-file.txt' and p.read_bytes() == b'custom content' for p in backups) and any(p.is_file() and p.name == damaged.name and p.read_bytes().endswith(b'customized audio') for p in backups))
        check('repair leaves separate saves and config unchanged', save.read_bytes() == b'player save sentinel' and cfg.read_bytes().hex() == saved['cfg'])
        saved = state()
        for label, response in [('network-failure', 'unavailable'), ('checksum-failure', b'tampered archive')]:
            requests.clear()
            responses['bgkeyring.zip'] = response
            log = run(label, extra=['/REINSTALLCONTENT=keyring-upstream', '/ALLOWCONTENTREPLACEMENT=yes'])
            check(label + ' preserves content and completes main install', state() == saved and requests == ['bgkeyring.zip'] and 'DWD OPTIONAL SKIPPED keyring-upstream' in log and (app / 'Exult.exe').is_file())
        responses.clear()
        # Test-only SHA approves a deliberately incomplete ZIP so extraction/manifest
        # failure, rather than the production SHA check, exercises real rollback.
        partial = work / 'partial.zip'
        with zipfile.ZipFile(downloads / 'bgkeyring.zip') as source, zipfile.ZipFile(partial, 'w') as target:
            for info in source.infolist():
                if info.is_dir() or info.filename.lower().endswith(('keyring.cfg', 'initgame.dat')):
                    target.writestr(info, source.read(info))
        production_sha = ini['1']['sha256']
        ini['1']['sha256'] = common.digest(partial)
        with (stage / 'dwd-content.ini').open('w', encoding='utf-16') as stream: ini.write(stream)
        malformed = compile_fixture('partial-archive')
        responses['bgkeyring.zip'] = partial.read_bytes()
        log = run('extraction-rollback', extra=['/REINSTALLCONTENT=keyring-upstream', '/ALLOWCONTENTREPLACEMENT=yes'], program=malformed)
        check('partial extraction restores prior custom package receipt config saves', {k:v for k,v in state().items() if k != 'mtimes'} == {k:v for k,v in saved.items() if k != 'mtimes'} and 'DWD OPTIONAL INSTALL FAILED keyring-upstream' in log)
        app = work / 'fresh-partial'
        log = run('fresh-partial-rejected', components=base_components + [ini['1']['component']], program=malformed)
        check('fresh partial removed without success receipt main install completes', not (app / 'mods/blackgate/Keyring').exists() and not (app / 'mods/blackgate/Keyring.cfg').exists() and not (app / 'content-state/keyring-upstream.ini').exists() and 'DWD OPTIONAL INSTALL FAILED keyring-upstream' in log)
        subprocess.run([str(app / 'unins000.exe'), '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART'], creationflags=subprocess.CREATE_NO_WINDOW, check=True, timeout=60)
        time.sleep(2)
        app = mainapp
        responses.clear()
        ini['1']['sha256'] = production_sha
        # Metadata-only future package fixture: update is offered, never inferred
        # from the engine version or installed by a leftover component checkbox.
        ini['1']['release'] = '2'
        ini['1']['version'] = 'future-version-fixture'
        with (stage / 'dwd-content.ini').open('w', encoding='utf-16') as stream:
            ini.write(stream)
        keycfg.write_bytes(keycfg.read_bytes() + b'\n<!-- customized again -->\n')
        script = stage / 'dwd_installer.iss'
        text = script.read_text(encoding='utf-8')
        script.write_text(text.replace('AppVersion='+json.loads((candidate/'validation.json').read_text())['metadata']['version'], 'AppVersion=9.0.0'), encoding='utf-8')
        # Force a real binary replacement, rather than relying on unchanged mtime.
        if a.previous_engine:
            shutil.copy2(a.previous_engine, app / 'Exult.exe')
            check('previous accepted engine differs from rebuilt candidate', common.digest(app / 'Exult.exe') != common.digest(stage / 'Exult/x86_64/Exult.exe'))
        else:
            (app / 'Exult.exe').write_bytes(b'replace this old engine fixture')
        future = compile_fixture('newer-version')
        saved = state()
        requests.clear()
        server.shutdown()
        server.server_close()
        log = run('offline-newer-engine', program=future)
        check('offline newer installer replaces old engine fixture', common.digest(app / 'Exult.exe') == common.digest(stage / 'Exult/x86_64/Exult.exe'))
        check('newer installer with unchanged package bytes is offline/idempotent', not requests and 'network requests scheduled: 0' in log and state() == saved)
        check('independent future optional update presented but not applied', 'Update available: future-version-fixture' in log)
        check('unknown third-party package remains independently managed', not (app / 'content-state/ultima6-original.ini').exists() and u6.read_bytes().endswith(b'<!-- independently managed -->\n'))
        log = run('offline-engine-only', components=['Exult', 'Docs', 'GPL'], program=future)
        check('engine-only offline upgrade preserves all content state', state() == saved and 'network requests scheduled: 0' in log)
        run('approved-independent-update', extra=['/REINSTALLCONTENT=keyring-upstream', '/ALLOWCONTENTREPLACEMENT=yes'], program=future)
        receipt = configparser.ConfigParser()
        receipt.read(app / 'content-state/keyring-upstream.ini')
        check('offline explicit update failure preserves previous receipt and files', receipt['Package']['Release'] == '1' and state() == saved)
        check('independent update retains existing saved game', save.read_bytes() == b'player save sentinel')

    finally:
        server.shutdown()
        server.server_close()
        if (app / 'unins000.exe').exists():
            r = subprocess.run([str(app / 'unins000.exe'), '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART'],
                               capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW, timeout=60)
            end = time.monotonic() + 15
            while (app / 'Exult.exe').exists() and time.monotonic() < end:
                time.sleep(.1)
            check('test uninstall', r.returncode == 0 and not (app / 'Exult.exe').exists())
            check('uninstall retains owner configuration and save', (app / 'exult.cfg').exists() and save.exists())
        check('original game and official files AND directories unchanged', all(common.tree(Path(p)) == s for p, s in before.items()))
    (work / 'scope.json').write_text(json.dumps({'result': 'PASS', 'assertions': len(checks),
        'downloads': 'Real Inno HTTP transfers of pinned original bytes from an instrumented loopback server; no manual install substitute',
        'newer_version': 'Synthetic newer installer/package version metadata; previous accepted engine binary replaced when --previous-engine supplied, otherwise sentinel. Release version/mod payloads unchanged',
        'gameplay': 'Not rerun'}, indent=2) + '\n')
    print('PASS:', len(checks), 'targeted installer idempotence assertions')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['candidate', 'work', 'downloads', 'bg', 'si', 'iscc']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--previous-engine', type=Path)
    parser.add_argument('--protect', type=Path, action='append', default=[])
    main(parser.parse_args())
