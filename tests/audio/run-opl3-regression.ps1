# Run after the normal Release/x64 fork build. Reuses its objects and dependencies.
# Uses a separate CRT entry point; the application's main is never called.
param([string]$BuildDirectory = '')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$workspace = Split-Path $repo -Parent
if (!$BuildDirectory) { $BuildDirectory = Join-Path $workspace 'build-exult-fork' }
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$objects = Join-Path $repo 'msvcstuff/vs2019/Release/Exult'
$deps = Join-Path $workspace 'build-exult/deps/x64-windows'
$scratch = Join-Path $BuildDirectory 'opl3-regression'
if (Test-Path -LiteralPath $scratch) { throw "Remove the previous regression output first: $scratch" }
New-Item -ItemType Directory -Path $scratch | Out-Null
$originalPath = $env:PATH
$success = $false
try {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
    $compiler = (Get-Command cl.exe).Source
    $linker = Join-Path (Split-Path $compiler) 'link.exe'
    $includeDirs = @('', 'headers','files','audio','audio/midi_drivers','gamemgr','gumps','data','conf','filc','imagewin','objs','pathfinder','shapes','shapes/shapeinf','usecode','server','tools','msvcstuff/vs2019')
    $options = @('/nologo','/c','/O2','/GL','/MD','/EHsc','/std:c++17','/permissive-','/Zc:__cplusplus','/Zc:preprocessor','/DNDEBUG',"/FI$repo/msvcstuff/vs2019/msvc_include.h",("/I"+(Join-Path $deps 'include')))
    foreach ($dir in $includeDirs) { $options += "/I$repo/$dir" }
    & $compiler @options (Join-Path $PSScriptRoot 'opl3_voice_regression.cpp') "/Fo$scratch/check.obj" > "$scratch/compile.log" 2>&1
    if ($LASTEXITCODE) { Get-Content "$scratch/compile.log"; throw 'Regression compilation failed' }
    # Execute the read-only local BG original driver as the OPL2 register oracle.
    & $compiler @options ("/I"+(Join-Path $PSScriptRoot 'original-opl2-reference')) (Join-Path $PSScriptRoot 'original_opl2_reference.cpp') "/Fo$scratch/original-opl2-reference.obj" > "$scratch/reference-compile.log" 2>&1
    if ($LASTEXITCODE) { Get-Content "$scratch/reference-compile.log"; throw 'Original OPL2 reference compilation failed' }
    & $compiler @options ("/I"+(Join-Path $PSScriptRoot 'original-opl2-reference')) ("/I"+(Join-Path $workspace 'ultima7-modern/game/include')) ("/I"+(Join-Path $workspace 'ultima7-modern/game/sound')) (Join-Path $PSScriptRoot 'serpent_opl2_reference.cpp') "/Fo$scratch/serpent-opl2-reference.obj" > "$scratch/serpent-reference-compile.log" 2>&1
    if ($LASTEXITCODE) { Get-Content "$scratch/serpent-reference-compile.log"; throw 'SI OPL2 reference compilation failed' }
    & $compiler @options ("/I"+(Join-Path $PSScriptRoot 'roland-reference')) ("/I"+(Join-Path $PSScriptRoot 'original-opl2-reference')) ("/I"+(Join-Path $workspace 'ultima7-modern/game/include')) ("/I"+(Join-Path $workspace 'ultima7-modern/game/sound')) (Join-Path $PSScriptRoot 'roland_driver_reference.cpp') "/Fo$scratch/roland-reference.obj" > "$scratch/roland-reference-compile.log" 2>&1
    if ($LASTEXITCODE) { Get-Content "$scratch/roland-reference-compile.log"; throw 'Roland reference compilation failed' }
    # Compile the original shared sequencer solely for legacy event-trace comparison.
    & git -C $repo show HEAD:audio/midi_drivers/XMidiSequence.cpp | Set-Content "$scratch/baseline.cpp"
    & $compiler @options "$scratch/baseline.cpp" "/Fo$scratch/baseline.obj" > "$scratch/baseline-compile.log" 2>&1
    if ($LASTEXITCODE) { Get-Content "$scratch/baseline-compile.log"; throw 'Baseline compilation failed' }
    foreach ($mode in @('current','baseline')) {
        $args = @('/NOLOGO','/SUBSYSTEM:CONSOLE','/ENTRY:wmainCRTStartup','/OPT:REF','/OPT:ICF','/LTCG','/MACHINE:X64',"/OUT:`"$scratch/$mode.exe`"", "`"$scratch/check.obj`"")
        $args += "`"$scratch/original-opl2-reference.obj`""
        $args += "`"$scratch/serpent-opl2-reference.obj`""
        $args += "`"$scratch/roland-reference.obj`""
        foreach ($obj in Get-ChildItem -LiteralPath $objects -Filter '*.obj') {
            if ($obj.Name -in @('NukedOpl3.obj','OriginalOpl2MidiDriver.obj')) { continue } # Emulator included in test TU for register checks.
            if ($mode -eq 'baseline' -and $obj.Name -eq 'XMidiSequence.obj') { continue }
            $args += "`"$($obj.FullName)`""
        }
        if ($mode -eq 'baseline') { $args += "`"$scratch/baseline.obj`"" }
        $args += @("/LIBPATH:`"$deps/lib`"", "/LIBPATH:`"$deps/lib/manual-link`"",'winmm.lib','ws2_32.lib','dbghelp.lib','kernel32.lib','user32.lib','gdi32.lib','winspool.lib','comdlg32.lib','advapi32.lib','shell32.lib','ole32.lib','oleaut32.lib','uuid.lib','odbc32.lib','odbccp32.lib')
        foreach ($lib in Get-ChildItem -LiteralPath "$deps/lib" -Filter '*.lib') { $args += "`"$($lib.FullName)`"" }
        $args | Set-Content "$scratch/$mode.rsp"
        & $linker "@$scratch/$mode.rsp" > "$scratch/$mode-link.log" 2>&1
        if ($LASTEXITCODE) { Get-Content "$scratch/$mode-link.log" -Tail 12; throw 'Regression link failed' }
        $duplicates = Select-String -Path "$scratch/$mode-link.log" -Pattern 'LNK4006'
        foreach ($duplicate in $duplicates) { if ($duplicate.Line -notmatch '\bmain\b') { throw "Unexpected duplicate symbol: $duplicate" } }
    }
    $env:PATH = (Join-Path $BuildDirectory 'bin') + ';' + $env:PATH
    & "$scratch/current.exe" > "$scratch/results.log" 2>&1
    if ($LASTEXITCODE) { Get-Content "$scratch/results.log"; throw 'OPL3 regression failed' }
    Get-Content "$scratch/results.log"
    & "$scratch/current.exe" --legacy-trace > "$scratch/current-trace.log" 2>&1
    if ($LASTEXITCODE) { throw 'Current legacy trace failed' }
    & "$scratch/baseline.exe" --legacy-trace > "$scratch/baseline-trace.log" 2>&1
    if ($LASTEXITCODE) { throw 'Baseline legacy trace failed' }
    if ((Get-Content "$scratch/current-trace.log" -Raw) -ne (Get-Content "$scratch/baseline-trace.log" -Raw)) { throw 'Legacy event trace differs from original sequencer' }
    Write-Output 'PASS legacy event trace matches original shared sequencer'
    $success = $true
} finally {
    $env:PATH = $originalPath
    # Keep failed-run evidence; remove only this exact generated directory after success.
    if ($success) {
        $resolvedScratch = (Resolve-Path -LiteralPath $scratch).Path
        if ($resolvedScratch -ne (Join-Path $BuildDirectory 'opl3-regression')) { throw 'Unexpected regression cleanup path' }
        Remove-Item -LiteralPath $resolvedScratch -Recurse -Force
    }
}
