# Build the ordinary UCC sources as an MSVC host tool for embedded quiz resources.
param([Parameter(Mandatory=$true)][string]$OutputDirectory,
      [string]$VsInstallPath, [string]$WinFlexBisonDirectory=$env:WIN_FLEX_BISON)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$out=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $out | Out-Null
$compiler=Join-Path $repo 'tools/compiler'
if (!$WinFlexBisonDirectory) {
    $bundled=Join-Path (Split-Path $repo -Parent) 'tools/win-flex-bison-2.5.24'
    if (Test-Path "$bundled/win_bison.exe") {$WinFlexBisonDirectory=$bundled}
}
$bison=if ($WinFlexBisonDirectory) {Join-Path $WinFlexBisonDirectory 'win_bison.exe'} else {'win_bison.exe'}
$flex=if ($WinFlexBisonDirectory) {Join-Path $WinFlexBisonDirectory 'win_flex.exe'} else {'win_flex.exe'}
if (!$VsInstallPath) {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VsInstallPath=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
Import-Module (Join-Path $VsInstallPath 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $VsInstallPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
& $bison -d -o "$out/ucparse.cc" "--defines=$out/ucparse.h" "$compiler/ucparse.yy"
if ($LASTEXITCODE) {throw 'UCC Bison generation failed'}
& $flex --wincompat -o "$out/uclex.cc" "$compiler/uclex.ll"
if ($LASTEXITCODE) {throw 'UCC Flex generation failed'}
$sources=@("$out/ucparse.cc","$out/uclex.cc","$repo/usecode/ucsymtbl.cc")
foreach ($name in @('ucmain','ucclass','ucexpr','ucfun','ucloc','ucstmt','ucsym')) {$sources+="$compiler/$name.cc"}
$options=@('/nologo','/EHsc','/std:c++17','/MD','/O2','/D_CRT_SECURE_NO_WARNINGS',
    "/FI$compiler/msvc_compat.h","/I$out","/I$compiler","/I$repo/headers",
    "/I$repo/usecode","/I$repo/files","/I$repo/msvcstuff/vs2019","/I$repo")
# Engine-only options inherited from the developer build must not affect UCC.
$previousCL=$env:_CL_
try {
    $env:_CL_=''
    Push-Location $out
    try { & cl.exe @options @sources "/Fe:$out/ucc.exe"
        if ($LASTEXITCODE) {throw 'UCC host-tool build failed'}
    } finally {Pop-Location}
} finally {$env:_CL_=$previousCL}
