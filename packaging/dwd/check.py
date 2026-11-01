"""Focused source/packaging invariants; no game data required."""
import importlib.util
import json
import tempfile
from pathlib import Path
spec = importlib.util.spec_from_file_location('release', Path(__file__).with_name('release.py'))
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)
m = r.metadata()
assert m['version'] in r.release_notes(m)
for name in ['README', 'ChangeLog', 'COPYING', 'AUTHORS', 'INSTALL', 'FAQ', 'README-DWD.md', 'ChangeLog-DWD.txt']:
    assert (r.ROOT / name).is_file(), name
with tempfile.TemporaryDirectory() as directory:
    p = Path(directory)
    r.installer(p, m)
    text = (p / 'dwd_installer.iss').read_text()
    for required in ['AppId=DWDExult', 'PrivilegesRequired=lowest', 'Software\\DWDExult', 'Uninstall DWD Exult', 'README-DWD.md', 'ChangeLog-DWD.txt', 'InstallMod(', 'SetExultGamePaths(', 'uninsdeletevalue uninsdeletekeyifempty']:
        assert required in text, required
    assert "RegKeyExists(HKEY_LOCAL_MACHINE, 'Software\\Wine')" in text
    assert 'Software\\Exult' not in text
    assert 'DefaultDirName={code:GetExultInstDir|{sd}\\Games\\DWDExult}' in text
    assert 'DisableDirPage=no' in text
    assert "sBGmods := ExpandConstant('{app}\\mods\\blackgate')" in text
    assert "sSImods := ExpandConstant('{app}\\mods\\serpentisle')" in text
    assert "sBGmods := BGPath" not in text and "sSImods := SIPath" not in text
    assert 'CreateCustomPage' in text and 'OnBGBrowseButton' in text
    assert "RaiseException('Could not write DWD configuration" in text

    assert 'DefaultDirName={code:GetExultInstDir|{autopf}\\Exult}' not in text
assert [p.name for p in (r.ROOT / '.github/workflows').glob('*.yml')] == ['dwd-candidate.yml']
print('PASS: provenance, curated notes, required docs, isolated installer identity, inherited automation disabled')
