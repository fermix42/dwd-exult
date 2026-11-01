"""Regenerate built-in game-time adapters using the ordinary Usecode compiler."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ucc', default=shutil.which('ucc'), help='ordinary UCC executable')
    args = parser.parse_args()
    if not args.ucc:
        parser.error('specify --ucc or put the ordinary UCC on PATH')
    root = Path(__file__).resolve().parents[1]
    text = '// Generated from data/game_time_{bg,si}.uc by compile_resources.py.\n#pragma once\n'
    with tempfile.TemporaryDirectory(prefix='exult-game-time-') as temporary:
        for game in ('bg', 'si'):
            output = Path(temporary) / (game + '.ucb')
            subprocess.run([args.ucc, '-o', str(output),
                            str(root / 'data' / ('game_time_' + game + '.uc'))], check=True)
            data = output.read_bytes()
            text += 'static constexpr unsigned char game_time_' + game + '[] = {\n'
            for i in range(0, len(data), 20):
                text += '    ' + ','.join(str(n) for n in data[i:i + 20]) + ',\n'
            text += '};\n'
    header = root / 'usecode/game_time_compat.h'
    if not header.exists() or header.read_text() != text:
        header.write_text(text)
    print('Generated', header)


if __name__ == '__main__':
    main()
