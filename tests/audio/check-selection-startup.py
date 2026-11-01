"""Normal BG/SI audio startup/restart in a disposable package and SDL dummy devices.

Observes completed initialization and persisted settings, then terminates the
process for test cleanup. Does not claim graceful exit, human listening or play.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import xml.etree.ElementTree as ET
import zipfile


def snapshot(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            if p.is_file() else '<DIRECTORY>' for p in root.rglob('*')}


def main(a):
    work = a.work.resolve()
    assert not work.exists(), 'Use a fresh disposable root'
    work.mkdir(parents=True)
    protected = [a.bg.resolve(), a.si.resolve()]
    before = [snapshot(p) for p in protected]
    with zipfile.ZipFile(next(a.candidate.glob('*-Windows-x64.zip'))) as z:
        z.extractall(work)
    app = work / 'DWDExult'
    for source, dest in [('mt32_ctrl_1_07.rom', 'MT32_CONTROL.ROM'), ('mt32_pcm.rom', 'MT32_PCM.ROM')]:
        shutil.copyfile(a.roms / source, app / 'data' / dest)
    subprocess.run([str(a.candidate.resolve() / 'installer-input/exconfig.exe'),
                    'SetExultGamePaths', str(app), *map(str, protected)], check=True,
                   capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)
    cfg = app / 'exult.cfg'
    checks = []
    def check(label, ok):
        checks.append({'check': label, 'result': 'PASS' if ok else 'FAIL'})
        (work / 'results.json').write_text(json.dumps(checks, indent=2) + '\n')
        assert ok, label
    def configure(driver):
        tree = ET.parse(cfg)
        for path, value in {'audio/enabled': 'yes', 'audio/midi/enabled': 'yes',
                            'audio/midi/driver': driver, 'audio/midi/use_oggs': 'no',
                            'audio/effects/driver': 'FMOpl', 'audio/effects/enabled': 'yes',
                            'gameplay/skip_intro': 'yes', 'gameplay/skip_splash': 'yes',
                            'gameplay/cheat': 'no', 'video/fullscreen': 'no', 'video/scale': '1'}.items():
            node = tree.getroot()
            for tag in path.split('/'):
                child = node.find(tag)
                if child is None:
                    child = ET.SubElement(node, tag)
                node = child
            node.text = value
        tree.write(cfg)
    env = os.environ.copy()
    env.update(SDL_VIDEO_DRIVER='dummy', SDL_AUDIO_DRIVER='dummy',
               SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    for game in ['bg', 'si']:
        for selection in ['OPL3Stereo', 'OriginalOPL2', 'MT32Emu', 'MT32EmuEarly', 'MT32EmuLater', 'MT32EmuCM32L']:
            configure(selection)
            expected = 'MT32Emu' if selection.startswith('MT32Emu') else selection
            for restart in [False, True]:
                label = game + '-' + selection + ('-restart' if restart else '')
                for name in ['stdout.txt', 'stderr.txt']:
                    (app / name).unlink(missing_ok=True)
                stdout = (work / (label + '-stdout.log')).open('wb')
                stderr = (work / (label + '-stderr.log')).open('wb')
                process = subprocess.Popen([str(app / 'Exult.exe'), '--' + game, '--nomenu'],
                                           cwd=work, env=env, stdout=stdout, stderr=stderr,
                                           creationflags=subprocess.CREATE_NO_WINDOW)
                def logs():
                    files = [work / (label + '-stdout.log'), work / (label + '-stderr.log'),
                             app / 'stdout.txt', app / 'stderr.txt']
                    return ''.join(p.read_text(errors='replace') for p in files if p.exists())
                ready = False
                try:
                    end = time.monotonic() + 30
                    while time.monotonic() < end and process.poll() is None:
                        text = logs()
                        ready = ("Trying config specified Midi driver: `" + expected + "'" in text
                                 and 'Success!' in text and 'Game resumed' in text)
                        if ready:
                            break
                        time.sleep(.1)
                    check(label + ' completed normal audio/game initialization', ready)
                finally:
                    if process.poll() is None:
                        process.terminate()  # Only cleanup after the initialization observation.
                    process.wait(timeout=10)
                    stdout.close()
                    stderr.close()
                    (work / (label + '-combined.log')).write_text(logs())
                current = ET.parse(cfg).getroot()
                check(label + ' canonical selection persisted', current.findtext('audio/midi/driver') == expected)
                check(label + ' independent SFX retained', current.findtext('audio/effects/driver') == 'FMOpl')
                print('PASS ' + label, flush=True)
    check('original asset files AND directories unchanged', [snapshot(p) for p in protected] == before)
    print('PASS: ' + str(len(checks)) + ' normal startup/configuration assertions')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['candidate', 'work', 'roms', 'bg', 'si']:
        parser.add_argument('--' + name, type=Path, required=True)
    main(parser.parse_args())
