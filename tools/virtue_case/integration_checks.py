"""Stage F focused integration runner; delegates to existing real-engine checks.

Uses the ordinary EXULT_TEST_* environment and fresh named disposable runs.
Retains each suite's assertions, transcripts, captures and failures unchanged.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--prefix', required=True)
p.add_argument('--resume', action='store_true', help='Reuse already passing logs from this prefix')
a = p.parse_args()
source = Path(__file__).resolve().parents[2]
workspace = source.parent
output = Path(os.environ['EXULT_TEST_OUTPUT'])
output.mkdir(parents=True, exist_ok=True)
results = []

def run(script, name, *args):
    name = a.prefix + '-' + name
    log = output / (name + '.log')
    if log.exists() and not a.resume:
        raise RuntimeError('Refusing to overwrite ' + str(log))
    command = [sys.executable, str(source / 'tools' / script), '--run', name, *map(str, args)]
    print('START', name, flush=True)
    code = 0
    if not log.exists():
        with log.open('w', encoding='utf-8') as stream:
            code = subprocess.run(command, cwd=source, stdout=stream, stderr=subprocess.STDOUT).returncode
    lines = log.read_text(encoding='utf-8').splitlines()
    summary = next((json.loads(line) for line in reversed(lines)
                    if line.startswith('{') and '"assertions"' in line), {})
    if summary.get('result') != 'PASS':
        code = code or 1
    results.append(dict(summary, run=name, process_exit_code=code))
    (output / (a.prefix + '-summary.json')).write_text(json.dumps(results, indent=2))
    print('END', name, code, summary, flush=True)
    if code:
        raise RuntimeError('Failed suite; inspect ' + str(log))

for profile in ['bg', 'keyring']:
    mod = ['--mod', 'Keyring'] if profile == 'keyring' else []
    suffix = Path('mods/Keyring/saves') if mod else Path('.')
    bc = workspace / 'test-output/virtue-stage-bc/runs' / ('final-physical-' + profile) / 'saves' / suffix / 'exult03bg.sav'
    accepted = workspace / 'test-output/virtue-stage-d-accepted/runs'
    ready = accepted / ('accepted-delivery-' + profile) / 'saves' / suffix / 'exult03bg.sav'
    commission = accepted / ('accepted-commission-' + profile) / 'saves' / suffix / 'exult03bg.sav'
    for suite in ['checks', 'spell_checks', 'label_checks', 'breadcrumb_checks', 'commission_checks']:
        args = ['--checkpoint', bc] if suite in ['spell_checks', 'label_checks'] else []
        run('virtue_case/' + suite + '.py', suite + '-' + profile, *mod, *args)
    run('virtue_case/transaction_checks.py', 'transactions-' + profile, *mod, '--checkpoint', commission)
    run('virtue_case/delivery_checks.py', 'delivery-' + profile, *mod, '--checkpoint', ready, '--ready-checkpoint')
    run('test_cli/speech_duration_checks.py', 'duration-' + profile, *mod)
run('test_cli/speech_duration_checks.py', 'duration-si', '--game', 'si')
run('virtue_case/collision_check.py', 'shape-collision')
print('TOTAL ASSERTIONS', sum(r.get('assertions', 0) for r in results), flush=True)
