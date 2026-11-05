"""Read-only Stage F source/resource and previous-review preservation inventory."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
workspace = root.parent
baseline = '7bf0bd8ab2797b3618b41c00a85eb8c8e002a702'
def git(*args):
    return subprocess.check_output(['git', *args], cwd=root)
def sha(data):
    return hashlib.sha256(data).hexdigest()
def entries(blob):
    n = struct.unpack_from('<I', blob, 84)[0]
    return [blob[o:o+s] for o, s in
            (struct.unpack_from('<II', blob, 128+8*i) for i in range(n))]

old_manifest = git('show', baseline + ':data/bg/flx.in').decode().splitlines()
new_manifest = (root/'data/bg/flx.in').read_text().splitlines()
old = [line.strip() for line in old_manifest if line.strip()]
new = [line.strip() for line in new_manifest if line.strip()]
assert new == old + ['virtue_case.shp'], (old, new)
packed = entries((root/'data/exult_bg.flx').read_bytes())
# Manifest's first line is the archive output path; subsequent lines are entries.
assert len(packed) == len(new)-1
def packed_input(name):
    data = (root/'data/bg'/name).read_bytes()
    # Ordinary expack opens recognized text inputs in Windows text mode.
    return data.replace(b'\r\n', b'\n') if name.endswith('.txt') else data
assert all(packed[i] == packed_input(name) for i, name in enumerate(new[1:]))
assert (a.output/'converted/virtue_case.shp').read_bytes() == (root/'data/bg/virtue_case.shp').read_bytes()
changed = git('diff', '--name-only', baseline, 'HEAD').decode().splitlines()
protected = [path for path in git('ls-tree', '-r', '--name-only', baseline).decode().splitlines()
             if path.startswith(('audio/', 'native_wait', 'native_orb', 'packaging/dwd/'))
             or path in ['combat.cc', 'combat.h', 'COPYING', 'AUTHORS', 'README', 'ChangeLog']]
assert not git('diff', baseline, '--', *protected)
previous = {}
for folder in ['virtue-stage-d-final-dialogue/human-review', 'virtue-stage-e-final/human-review',
               'speech-duration-20261104/human-review']:
    directory = workspace/'test-output'/folder
    previous[folder] = {str(path.relative_to(directory)):sha(path.read_bytes())
                        for path in directory.rglob('*') if path.is_file()}
report = dict(baseline=baseline, head=git('rev-parse','HEAD').decode().strip(),
              manifest_append_only=True, archive_entries=len(packed),
              original_resource_entries_unchanged=len(old)-1,
              shp_exact_rebuild=True, protected_files=len(protected),
              changed_from_baseline=changed, previous_review_files=previous,
              release_tag=git('rev-parse','dwd-v1.0.0-rc.1').decode().strip())
a.output.mkdir(parents=True, exist_ok=True)
(a.output/'audit.json').write_text(json.dumps(report, indent=2))
print('PASS', len(packed), 'archive entries;', len(protected), 'protected files;',
      sum(map(len, previous.values())), 'previous review files inventoried')
