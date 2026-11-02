# Build this worktree with the ordinary compiler configuration and shared output/
# intermediate tree. Never run concurrently with another Exult build.
$ErrorActionPreference = 'Stop'
$buildTarget = if ($args -contains '-Rebuild') { '/t:Rebuild' } else { '/t:Build' }
$sourceRoot = (Resolve-Path "$PSScriptRoot/../..").Path
$workspace = Split-Path $sourceRoot -Parent
$vs = & "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
# Use the ordinary packer/manifest to regenerate the BG archive and its header.
Push-Location $sourceRoot
try {
    & "$workspace/build-exult-fork/bin/expack.exe" -i data/bg/flx.in
    if ($LASTEXITCODE) { throw "BG resource packing failed: $LASTEXITCODE" }
} finally { Pop-Location }
$oldCL = $env:_CL_
try {
    $env:_CL_ = ("$oldCL /Z7 /DPORTABLE_EXULT_WIN32").Trim()
    & "$vs/MSBuild/Current/Bin/MSBuild.exe" "$sourceRoot/msvcstuff/vs2019/Exult.vcxproj" $buildTarget /m:8 /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 "/p:SolutionDir=$sourceRoot/msvcstuff/vs2019/" "/p:QuizBuildDir=$workspace/exult-fork/msvcstuff/vs2019/Release/Exult/quiz/" "/p:OutDir=$workspace/build-exult-fork/bin/" "/p:VcpkgRoot=$workspace/tools/vcpkg/" "/p:VcpkgInstalledDir=$workspace/build-exult/deps/" /p:VcpkgManifestInstall=false /p:VcpkgLocalAppDataDisabled=true "/p:DirectoryBuildPropsPath=$workspace/build-exult-fork/dependencies.props" "/p:DirectoryBuildTargetsPath=$sourceRoot/tools/virtue_case/build.targets"
    if ($LASTEXITCODE) { throw "Exult build failed: $LASTEXITCODE" }
} finally { $env:_CL_ = $oldCL }
