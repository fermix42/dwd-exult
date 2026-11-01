"""Focused accepted-engine tests against ACL-enforced disposable asset copies.

Never changes permissions on supplied games. Evidence stays in a fresh work root.
"""
import argparse
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile


def snapshot(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            if p.is_file() else '<DIRECTORY>' for p in root.rglob('*')}


def main(a):
    work = a.work.resolve()
    assert not work.exists(), 'Use a fresh disposable work root'
    work.mkdir(parents=True)
    sources = [a.bg.resolve(), a.si.resolve()]
    before = [snapshot(p) for p in sources]
    checks = []
    def check(label, ok):
        checks.append({'check': label, 'result': 'PASS' if ok else 'FAIL'})
        (work / 'results.json').write_text(json.dumps(checks, indent=2) + '\n')
        assert ok, label
    def run(args, label, env=None):
        result = subprocess.run([str(x) for x in args], cwd=work, env=env,
                                capture_output=True, timeout=180,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        (work / (label + '.log')).write_bytes(result.stdout + result.stderr)
        check(label, result.returncode == 0)
    candidate = a.candidate.resolve()
    with zipfile.ZipFile(next(candidate.glob('*-Windows-x64.zip'))) as z:
        z.extractall(work)
    app = work / 'DWDExult'
    fixtures = [work / 'assets' / name for name in ['blackgate', 'serpentisle']]
    for source, dest in zip(sources, fixtures):
        # Only the engine's original static assets are needed; no player state.
        assert not any(p.is_symlink() or p.is_junction() for p in (source / 'static').rglob('*'))
        shutil.copytree(source / 'static', dest / 'static', copy_function=shutil.copyfile)
    copies_before = [snapshot(p) for p in fixtures]
    sid = next(csv.reader(io.StringIO(subprocess.check_output(
        ['whoami', '/user', '/fo', 'csv', '/nh'], text=True))))[1]
    locked = []
    try:
        for dest in fixtures:
            assert work in dest.parents
            locked.append(dest)
            run(['icacls', dest, '/deny', '*' + sid + ':(OI)(CI)(WD,AD,WEA,WA,DE,DC)'], 'lock-' + dest.name)
            for path, mode in [(dest / 'write-probe', 'xb'), (dest / 'static' / 'write-probe', 'xb'),
                               (dest / 'static' / 'usecode', 'r+b')]:
                denied = False
                try:
                    with path.open(mode):
                        pass
                except PermissionError:
                    denied = True
                check('ACL denies write ' + dest.name + '/' + str(path.relative_to(dest)), denied)
        run([candidate / 'installer-input/exconfig.exe', 'SetExultGamePaths', app, *fixtures], 'configure-copies')
        for game, archive in [('bg', 'bgkeyring.zip'), ('si', 'sifixes.zip')]:
            run([app / 'Exult.exe', '--' + game, '--installmod', candidate / archive], 'install-' + game + '-mod')
        env = os.environ.copy()
        env.update(EXULT_TEST_EXE=str(app / 'Exult.exe'), EXULT_TEST_RUNTIME_DIR=str(app),
                   EXULT_TEST_DATA_PATH=str(app / 'data'), EXULT_TEST_BG_PATH=str(fixtures[0]),
                   EXULT_TEST_SI_PATH=str(fixtures[1]), EXULT_TEST_OUTPUT=str(work / 'runtime'))
        for game, mod in [('bg', ''), ('si', ''), ('bg', 'Keyring'), ('si', 'sifixes')]:
            env['EXULT_TEST_MODS_PATH'] = str(app / 'mods' / ('blackgate' if game == 'bg' else 'serpentisle'))
            args = [sys.executable, Path(__file__).resolve().parents[2] / 'tools/quiz/integration.py',
                    '--game', game, '--header', a.header.resolve(), '--run', 'readonly-' + game + '-' + (mod or 'vanilla')]
            if mod:
                args += ['--mod', mod]
            run(args, 'runtime-' + game + '-' + (mod or 'vanilla'), env)
        check('ACL-locked copy files AND directories unchanged', [snapshot(p) for p in fixtures] == copies_before)
    finally:
        for dest in reversed(locked):
            run(['icacls', dest, '/remove:d', '*' + sid], 'unlock-' + dest.name)
        check('owner game files AND directories unchanged', [snapshot(p) for p in sources] == before)
    runtime = [json.loads(p.read_text()) for p in (work / 'runtime/runs').glob('*/result.json')]
    assert len(runtime) == 4 and all(r['result'] == 'PASS' and r['failures'] == 0 for r in runtime)
    report = {'candidate_revision': json.loads((app / 'release-metadata.json').read_text())['dwd_revision'],
              'checks': len(checks), 'runtime_assertions': sum(r['assertions'] for r in runtime),
              'result': 'PASS', 'source_permissions': 'Never modified; only disposable copies ACL-locked'}
    (work / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['candidate', 'work', 'bg', 'si', 'header']:
        parser.add_argument('--' + name, type=Path, required=True)
    main(parser.parse_args())
