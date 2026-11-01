"""Installer-only optional-content catalog and policy; no engine/mod changes."""
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def catalog(stage, tools):
    distribution = json.loads((ROOT / 'packaging/dwd/mods.json').read_text())
    packages = [distribution['audio'], *distribution['packages']]
    downloads = json.loads((ROOT / 'packaging/dwd/downloads.json').read_text())
    lines = ['[packages]', 'count=' + str(len(packages))]
    for index, p in enumerate(packages):
        archive = p.get('package', p.get('download'))
        if p.get('package'):
            with zipfile.ZipFile(Path(tools) / archive) as z:
                files = {i.filename: hashlib.sha256(z.read(i)).hexdigest() for i in z.infolist() if not i.is_dir()}
        else:
            manifest = json.loads((ROOT / 'packaging/dwd/content-manifests' / (archive + '.json')).read_text())
            assert manifest['archive_sha256'] == downloads[archive]['sha256'], 'Refresh the verified archive manifest: ' + archive
            files = manifest['files']
        assert files and len(files) == len({n.casefold() for n in files})
        for n in files:
            assert not Path(n).is_absolute() and '..' not in Path(n).parts and ':' not in n
        identity = hashlib.sha256(json.dumps(sorted((n.casefold(), h) for n, h in files.items()), separators=(',', ':')).encode()).hexdigest()
        values = dict(id=p['id'], name=p['name'], version=p['version'], release=p['release'],
                      component=p['component'].replace('/', '\\'), archive=archive,
                      root=('mods\\' + p['game']) if p['game'] else 'data',
                      anchor=p['installed_id'], game=p['game'], identity=identity,
                      url=downloads.get(archive, {}).get('url', ''),
                      sha256=downloads.get(archive, {}).get('sha256', ''), count=len(files))
        lines += ['[' + str(index) + ']'] + [str(k) + '=' + str(v) for k, v in values.items()]
        for i, (name, digest) in enumerate(sorted(files.items())):
            lines += ['file' + str(i) + '=' + name.replace('/', '\\'), 'hash' + str(i) + '=' + digest]
    (stage / 'dwd-content.ini').write_text('\n'.join(lines) + '\n', encoding='utf-16')


def policy(text):
    code = (ROOT / 'packaging/dwd/optional-content.iss').read_text()
    text = text.replace('Description: "Download and install"', 'Description: "Optional content (existing content is preserved)"')
    text = text.replace('  PrevItemAChecked: Boolean;\n', '')
    text = text.replace('procedure InitializeWizard;', code + '\nprocedure InitializeWizard;')
    text = text.replace('  DownloadPage := CreateDownloadPage(', '  DwdInitContentPage();\n  DownloadPage := CreateDownloadPage(')
    start = text.index('// Download page:')
    end = text.index('//\n// Skip page', start)
    text = text[:start] + '''  end else if CurPageID = wpReady then begin
    Result := DwdPrepareContent();
  end else Result := True;
end;

''' + text[end:]
    start = text.index('    // Installing the downloads')
    # Replace the full inherited optional install block (the final two ends close
    # ssPostInstall and CurStepChanged and must remain).
    text = text[:start] + '    DwdInstallContent();\n  end;\nend;\n' + text[text.index('// Preserve the same download page', start):]
    # Validate sources only for packages actually scheduled for installation.
    start = text.index("  if (Result = '') and ((WizardIsComponentSelected('downloads\\mods\\keyring'))")
    end = text.index('\nend;', start)
    text = text[:start] + text[end:]
    text = text.replace("    SetExultGamePaths(ExpandConstant('{app}'), BGPath, SIPath );", '    DwdSetPathsIfChanged();')
    # Failing extraction must never produce a successful content receipt.
    text = text.replace("MsgBox('Error Installing mod '+modname+sNewLine+sNewLine+ stderr,mbError, MB_OK)", "RaiseException('Error installing mod '+modname+': '+stderr)")
    text = text.replace("MsgBox('Error Installing '+name+sNewLine+sNewLine+ stderr,mbError, MB_OK)", "RaiseException('Error installing '+name+': '+stderr)")
    text = text.replace('    if Success then begin', "    if not Success then RaiseException('Could not execute optional-content installer');\n    if Success then begin")
    text = text.replace('[Files]', '[Files]\nSource: dwd-content.ini; Flags: dontcopy')
    return text
