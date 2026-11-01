param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force $Destination | Out-Null
$download=Join-Path $Destination 'innosetup-6.7.4.exe'
Invoke-WebRequest 'https://github.com/jrsoftware/issrc/releases/download/is-6_7_4/innosetup-6.7.4.exe' -OutFile $download
if ((Get-FileHash $download -Algorithm SHA256).Hash -ne 'A549929319F9120716677C685535CA4671E4B937B5FDC87E2585F32D8EB30E8C') {throw 'Inno compiler download checksum mismatch'}
if ((Get-AuthenticodeSignature $download).Status -ne 'Valid') {throw 'Inno compiler download signature invalid'}
$compiler=Join-Path $Destination 'compiler'
$p=Start-Process -FilePath $download -ArgumentList "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CURRENTUSER /NOICONS /DIR=`"$compiler`"" -WindowStyle Hidden -Wait -PassThru
if ($p.ExitCode -ne 0) {throw "Inno tool install failed: $($p.ExitCode)"}
if ($env:GITHUB_ENV) {"DWD_ISCC=$compiler/ISCC.exe" | Out-File -Append -Encoding utf8 $env:GITHUB_ENV}
