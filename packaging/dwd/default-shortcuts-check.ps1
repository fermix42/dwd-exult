param([Parameter(Mandatory=$true)][string]$Candidate,[Parameter(Mandatory=$true)][string]$Work,
      [Parameter(Mandatory=$true)][string]$BG,[Parameter(Mandatory=$true)][string]$SI)
$ErrorActionPreference='Stop'
$artifactRoot=(Resolve-Path $Candidate).Path
$expectedRoot=[IO.Path]::GetFullPath("$env:SystemDrive\Games\DWDExult")
if(Test-Path -LiteralPath $expectedRoot){throw 'Preserve existing default installation directory; default-location check NOT TESTED'}
if(Test-Path 'HKCU:\Software\DWDExult'){throw 'Preserve existing DWD install registry'}
if(Test-Path -LiteralPath $Work){throw 'Use a fresh test directory'}
New-Item -ItemType Directory -Path $Work | Out-Null
$proofRoot=(Resolve-Path $Work).Path
$group='DWD Exult Validation '+[guid]::NewGuid().ToString('N')
$programs=[Environment]::GetFolderPath('Programs')
$groupPath=Join-Path $programs $group
$checks=[Collections.Generic.List[object]]::new()
function Check([string]$Label,[bool]$OK){
 $checks.Add([pscustomobject]@{check=$Label;result=$(if($OK){'PASS'}else{'FAIL'})})
 $checks | ConvertTo-Json | Set-Content -Encoding utf8 "$proofRoot/results.json"
 if(!$OK){throw $Label}
}
$setup=(Get-ChildItem -LiteralPath $artifactRoot -Filter '*-Setup.exe' | Select-Object -First 1).FullName
$arguments="/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /COMPONENTS=`"Exult,Docs,GPL,Icons`" /GROUP=`"$group`" /BGDIR=`"$BG`" /SIDIR=`"$SI`" /LOG=`"$proofRoot/install.log`""
$parentRoot=Split-Path $expectedRoot -Parent
$parentExisted=Test-Path -LiteralPath $parentRoot
$created=$false
try {
 $p=Start-Process -FilePath $setup -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
 $created=Test-Path -LiteralPath $expectedRoot
 Check 'default installer exit' ($p.ExitCode -eq 0)
 Check 'actual system-drive default destination' ($created -and (Test-Path "$expectedRoot/Exult.exe"))
 Check 'default remains C Games DWDExult on this host' ($expectedRoot -eq 'C:\Games\DWDExult')
 Check 'own location registry' ((Get-ItemProperty 'HKCU:\Software\DWDExult').Path -eq $expectedRoot)
 $link=Join-Path $groupPath 'Exult.lnk'
 Check 'installer-created Start Menu shortcut' (Test-Path -LiteralPath $link)
 $shell=New-Object -ComObject WScript.Shell
 $shortcut=$shell.CreateShortcut($link)
 Check 'shortcut executable target' ($shortcut.TargetPath -eq "$expectedRoot\Exult.exe")
 Check 'shortcut normal working directory' ($shortcut.WorkingDirectory -eq $expectedRoot)
 $unrelated=Join-Path $proofRoot 'unrelated';New-Item -ItemType Directory $unrelated | Out-Null
 foreach($kind in @('StartMenu','Desktop')){
   $folder=Join-Path $proofRoot $kind;New-Item -ItemType Directory $folder | Out-Null
   $variant=if($kind -eq 'StartMenu'){$link}else{Join-Path $folder 'DWD validation.lnk'}
   if($kind -ne 'StartMenu'){Copy-Item -LiteralPath $link -Destination $variant}
   $copy=$shell.CreateShortcut($variant)
   $copy.Arguments='--bg --verify-files'
   $copy.WorkingDirectory=$unrelated
   $copy.Save()
   $exit=$shell.Run('"'+$variant+'"',0,$true)
   Check "$kind shell shortcut startup without portable flag" ($exit -eq 0)
   if($kind -eq 'StartMenu'){$copy.Arguments='';$copy.WorkingDirectory=$expectedRoot;$copy.Save()}
 }
 $shell.CurrentDirectory=$unrelated
 $exit=$shell.Run('"'+$expectedRoot+'\Exult.exe" --bg --verify-files',0,$true)
 Check 'Explorer-style shell startup without portable flag' ($exit -eq 0)
 Check 'shortcut does not create CWD config' (!(Test-Path "$unrelated/exult.cfg"))
 Check 'configuration beside executable' (Test-Path "$expectedRoot/exult.cfg")
 Check 'base saves inside default root' (Test-Path "$expectedRoot/saves/blackgate")
} finally {
 if($created -and (Test-Path "$expectedRoot/unins000.exe")){
   $p=Start-Process -FilePath "$expectedRoot/unins000.exe" -ArgumentList "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /LOG=`"$proofRoot/uninstall.log`"" -WindowStyle Hidden -Wait -PassThru
   $deadline=[DateTime]::UtcNow.AddSeconds(15)
   while((Test-Path "$expectedRoot/Exult.exe") -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 100}
   Check 'default install uninstall exit' ($p.ExitCode -eq 0 -and !(Test-Path "$expectedRoot/Exult.exe"))
   Check 'Start Menu group removed by uninstall' (!(Test-Path -LiteralPath $groupPath))
   Check 'default config preserved on uninstall' (Test-Path "$expectedRoot/exult.cfg")
 }
 # This directory did not exist before this check and contains only test-created
 # config/empty runtime state. Verify the exact explicitly named target first.
 if($created -and (Test-Path -LiteralPath $expectedRoot)){
   $resolved=(Resolve-Path -LiteralPath $expectedRoot).Path
   if($resolved -ne [IO.Path]::GetFullPath("$env:SystemDrive\Games\DWDExult")){throw 'Unexpected cleanup target'}
   Remove-Item -LiteralPath $resolved -Recurse -Force
   if(!$parentExisted -and (Test-Path -LiteralPath $parentRoot) -and @(Get-ChildItem -LiteralPath $parentRoot -Force).Count -eq 0){Remove-Item -LiteralPath $parentRoot}
 }
}
"PASS: $($checks.Count) default-directory and native shell shortcut assertions"
