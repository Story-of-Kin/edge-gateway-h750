param([string]$KeilRoot='D:\Keil_v5',[switch]$Flash)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$project=Join-Path $root 'Projects\MDK-ARM\edge_gateway_h750.uvprojx'
$out=Join-Path $root 'artifacts'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$mode=if($Flash){'-f'}else{'-r'}
$log=Join-Path $out $(if($Flash){'flash.txt'}else{'build.txt'})
$arguments=$mode+' "'+$project+'" -t "gateway_h750" -o "'+$log+'" -j0'
$process=Start-Process -FilePath (Join-Path $KeilRoot 'UV4\UV4.exe') -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
Get-Content -LiteralPath $log | Where-Object {$_ -notmatch 'LIC=|License Information'}
if($process.ExitCode -ge 2 -or -not $Flash -and -not((Get-Content -LiteralPath $log -Raw) -match ' - 0 Error\(s\)')){throw "Keil failed: $($process.ExitCode). See $log"}
if($Flash -and -not((Get-Content -LiteralPath $log -Raw) -match 'Verify OK')){throw 'Flash verification was not confirmed'}
