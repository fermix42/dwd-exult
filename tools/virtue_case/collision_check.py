"""A third-party shape at 1601 must prevent native case registration."""
import argparse
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'test_cli'))
from driver import BASE, Driver, EngineRefusal

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--run', required=True)
a = p.parse_args()
patch = BASE / (a.run + '-patch')
patch.mkdir(parents=True, exist_ok=False)
# An original one-pixel test sprite in a conventional sparse FLEX patch.
frame = struct.pack('<hhhhHhhBH', 0, 0, 0, 0, 2, 0, 0, 3, 0)
shape = struct.pack('<II', 8 + len(frame), 8) + frame
count = 1602
resource = bytearray(128 + count*8)
struct.pack_into('<III', resource, 80, 0xffff1a00, count, 0xcc)
struct.pack_into('<II', resource, 128 + 1601*8, len(resource), len(shape))
resource += shape
(patch / 'shapes.vga').write_bytes(resource)
# One unused RET helper; overrides no existing function. An empty Usecode file
# does not have a valid format for this engine's loader.
(patch / 'usecode').write_bytes(struct.pack('<HHHHHHB', 0xefff, 9, 0, 0, 0, 0, 0x25))
d = Driver()
events = []
try:
    events.append(d.execute(dict(cmd='init', game='bg', run=a.run, patch_path=str(patch))))
    before = d.state()['inventory']
    try:
        d.execute(dict(cmd='test_virtue_case'))
        raise AssertionError('engine claimed the third-party shape')
    except EngineRefusal as error:
        assert 'native_case_shape_unavailable' in str(error)
        events.append(error.detail)
    assert d.state()['inventory'] == before
    assert (patch / 'shapes.vga').read_bytes() == resource
    d.assertions = 3
    d.execute(dict(cmd='exit'))
finally:
    result = d.close()
    if d.run:
        (d.run / 'collision-check.json').write_text(json.dumps(dict(events=events, result=result), indent=2))
    print(json.dumps(result), flush=True)
sys.exit(0 if result and result['result'] == 'PASS' else 1)
