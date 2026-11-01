param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path "$PSScriptRoot/../..").Path
$vs = & "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ tools are required' }
if (!$env:VCPKG_INSTALLATION_ROOT) { throw 'Set VCPKG_INSTALLATION_ROOT to the pinned vcpkg checkout' }
$baseline = (Get-Content "$sourceRoot/msvcstuff/vs2019/vcpkg.json" -Raw | ConvertFrom-Json).'builtin-baseline'
$actual = & git -C $env:VCPKG_INSTALLATION_ROOT rev-parse HEAD
if ($actual -ne $baseline) { throw "Expected vcpkg baseline $baseline; got $actual" }
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$outputPath = (Resolve-Path $OutputDirectory).Path.Replace('\','/') + '/'
& "$env:VCPKG_INSTALLATION_ROOT/vcpkg.exe" integrate install
if ($LASTEXITCODE) { throw 'vcpkg integration failed' }
& "$vs/MSBuild/Current/Bin/MSBuild.exe" "$sourceRoot/msvcstuff/vs2019/Exult.sln" '/t:Exult;expack;exconfig' /m:4 /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 "/p:OutDir=$outputPath"
if ($LASTEXITCODE) { throw 'MSBuild failed' }
New-Item -ItemType Directory -Force "$outputPath/data" | Out-Null
Copy-Item "$sourceRoot/data/exult*.flx" "$outputPath/data"
