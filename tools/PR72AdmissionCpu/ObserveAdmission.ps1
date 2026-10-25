[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourceReviewSHA256,[Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Probe.Common.ps1')
$expectedOutput='D:\UserTemp\PR72-Admission-CPU-Attribution-20261025\observation'
if([IO.Path]::GetFullPath($OutputDirectory) -ine $expectedOutput -or -not [IO.Directory]::Exists($OutputDirectory)){throw 'Exact owned observation directory required.'}
$script:markerError=$null
$script:markerCount=0
function Write-Phase([int]$Index,[string]$Phase){
 if($script:markerError){return}
 try{
  if($Index -ne $script:markerCount -or $Index -lt 0 -or $Index -gt 3){throw 'Marker sequence mismatch.'}
  $begin=[Diagnostics.Stopwatch]::GetTimestamp()
  $self=[Diagnostics.Process]::GetCurrentProcess()
  try{$rootId=$self.Id;$birth=$self.StartTime.ToUniversalTime().ToFileTimeUtc();$cpu=$self.TotalProcessorTime.Ticks}finally{$self.Dispose()}
  $row=[ordered]@{phase=$Phase;index=$Index;pid=$rootId;creationFileTime=$birth;utc=[DateTimeOffset]::UtcNow.ToString('o');qpcStart=$begin;qpcEnd=[Diagnostics.Stopwatch]::GetTimestamp();frequency=[Diagnostics.Stopwatch]::Frequency;rootTotalCpu100ns=$cpu}
  $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($row|ConvertTo-Json -Compress))
  if($bytes.Length -gt 2048){throw 'Marker exceeds 2 KiB.'}
  $path=Join-Path $OutputDirectory ('phase-'+$Index+'.json')
  $stream=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
  try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
  $script:markerCount++
 }catch{$script:markerError='Phase marker capture/write failed; observation incomplete.'}
}
function Write-MarkerStatus{
 try{
  $value=[ordered]@{complete=($script:markerCount -eq 4 -and $null -eq $script:markerError);error=$script:markerError;count=$script:markerCount;acceptanceEvidence=$false}
  $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($value|ConvertTo-Json -Compress))
  if($bytes.Length -gt 4096){throw 'Marker status cap.'}
  $stream=[IO.FileStream]::new((Join-Path $OutputDirectory 'marker-status.json'),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
  try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
 }catch{# Missing status is a diagnostic failure. Do not replace the sampler's decision.
 }
}
$consoleTypeReady=$false
try{
 Write-Phase 0 'before_preflight'
 Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class PR72AdmissionProbeOwnConsole {
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool FreeConsole();
}
'@
 $consoleTypeReady=$true
 $bindings=Assert-ProbeSource $SourceReviewSHA256 -OwnedPayload
 $gameBinding=$bindings.gameBinding.path
 $gameReview=$bindings.gameReview.path
 $game=Read-ProbeJson $gameBinding
 $python=$game.executables.python.path
 $checker=Join-Path $game.sourceRoot 'unreal\Scripts\CommandCardValidation\policy.py'
 $checkerPin=@($game.files|Where-Object relative -CEQ 'unreal/Scripts/CommandCardValidation/policy.py')
 if($checkerPin.Count -ne 1 -or (Get-ProbeHash $checker) -cne $checkerPin[0].sha256 -or (Get-ProbeHash $python) -cne $game.executables.python.sha256){throw 'Exact existing binding checker required.'}
 $checkedText=& $python -B $checker bindings $gameBinding --sha256 $bindings.gameBinding.sha256 --review $gameReview --head $bindings.parentGameSourceHead
 if($LASTEXITCODE -ne 0){throw 'Existing complete game binding checker failed.'}
 $checked=$checkedText|ConvertFrom-Json -DateKind String
 if($checked.valid -isnot [bool] -or -not $checked.valid -or $checked.runtimeAcceptance -ne $false){throw 'Actual checker completion required.'}
 [IO.File]::WriteAllText((Join-Path $OutputDirectory 'BINDING_PREFLIGHT.json'),($checked|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
 Write-Phase 1 'after_preflight'
 if((Get-ProbeHash $bindings.sampler.path) -cne 'f891ebaf39f09a738578ce0da9d94fa47352b27e84d8ce0fb56d447e49c5c353'){throw 'Original sampler changed.'}
 Write-Phase 2 'before_sampler'
 $sampler=$bindings.sampler.path
 & $sampler -OutputPath (Join-Path $OutputDirectory 'ADMISSION.json')
 $samplerExit=$LASTEXITCODE
 Write-Phase 3 'after_sampler'
 if($samplerExit -ne 0){throw 'Original sampler execution failed.'}
 # eligible=false is retained as observed data, never promoted to work acceptance.
}finally{
 Write-MarkerStatus
 if($consoleTypeReady -and -not [PR72AdmissionProbeOwnConsole]::FreeConsole()){throw 'Own console release failed.'}
}
