param([Parameter(Mandatory)][string]$Python,[Parameter(Mandatory)][string]$OutputDirectory,[switch]$CapacityAdmitted)
$ErrorActionPreference='Stop'
if(-not $CapacityAdmitted){throw 'Owned-process controls require current capacity admission.'}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Fresh owned control directory required.'}
[IO.Directory]::CreateDirectory($OutputDirectory)|Out-Null
$source=Join-Path $PSScriptRoot 'CommandCardStageSupervisor.cs'
if(-not ('RA2CommandCardStageSupervisor' -as [type])){Add-Type -Path $source}
$fixture=Join-Path $OutputDirectory 'fixture.py'
$code=@'
import subprocess, sys, time
from pathlib import Path
namespace={'__name__':'inert_console_control'}
exec(compile(Path(sys.argv[2]).read_bytes(),sys.argv[2],'exec'),namespace)
namespace['release_own_console']()
mode=sys.argv[1]
if mode=='success': print('owned fixture'); sys.exit(0)
if mode=='failure': sys.exit(7)
if mode=='wait': time.sleep(3)
if mode=='overflow': sys.stdout.write('x'*(1024*1024+1)); sys.stdout.flush()
if mode=='survivor':
    subprocess.Popen([sys.executable,'-c','import time; time.sleep(3)'], creationflags=subprocess.CREATE_NO_WINDOW)
    sys.exit(0)
'@
[IO.File]::WriteAllText($fixture,$code,[Text.UTF8Encoding]::new($false))
$results=[Collections.Generic.List[object]]::new()
foreach($name in @('success','failure','timeout','cancelled','overflow','survivor','short-lease')){
    $dir=Join-Path $OutputDirectory $name;$temp=Join-Path $dir 'temp'
    [IO.Directory]::CreateDirectory($temp)|Out-Null
    $cancel=Join-Path $dir 'CANCEL'
    $origin=[Diagnostics.Stopwatch]::GetTimestamp()
    $mode=if($name -eq 'timeout'){'wait'}elseif($name -in @('cancelled','short-lease')){'success'}else{$name}
    if($name -eq 'timeout'){$origin-=[long](219.7*[Diagnostics.Stopwatch]::Frequency)}
    if($name -eq 'cancelled'){[IO.File]::WriteAllText($cancel,'inert control cancellation')}
    $end=if($name -eq 'short-lease'){[DateTimeOffset]::UtcNow.AddSeconds(30)}else{[DateTimeOffset]::UtcNow.AddMinutes(20)}
    $o=[RA2CommandCardStageSupervisor]::Run($Python,[string[]]@('-B',$fixture,$mode,(Join-Path $PSScriptRoot 'policy.py')),$OutputDirectory,220,$dir,'interactive',$temp,$end.ToString('o'),$origin,$cancel)
    $expected=$name -eq 'success'
    if($o.passed -ne $expected){throw "Unexpected owned control outcome: $name"}
    if(-not $o.cleanupSucceeded -or $o.activeOwnedProcessesAfterCleanup -ne 0){throw "Owned control cleanup failed: $name"}
    if($name -eq 'timeout' -and -not $o.timedOut){throw 'Timeout control did not observe the unchanged absolute deadline.'}
    if($name -eq 'cancelled' -and -not $o.leaseCancelled){throw 'Cancellation control failed.'}
    if($name -eq 'survivor' -and -not $o.terminationRequested){throw 'Deliberate surviving-child rejection was lost.'}
    $results.Add([ordered]@{case=$name;expectedAccepted=$expected;actualAccepted=$o.passed;zeroSurvivors=$true;acceptanceEvidence=$false})
}
# Exercise the actual PowerShell worker's own-console release and fail-closed
# prerequisite path. Deliberately invalid bindings reject before Git or Unreal.
$workerDir=Join-Path $OutputDirectory 'worker-refusal'
$workerTemp=Join-Path $workerDir 'temp'
[IO.Directory]::CreateDirectory($workerTemp)|Out-Null
$badBinding=Join-Path $workerDir 'INVALID_BINDINGS.json'
[IO.File]::WriteAllText($badBinding,(@{scope='INERT-INVALID';sourceHead=('1'*40);executables=@{python=@{path=$Python}}}|ConvertTo-Json -Depth 5))
$spec=[ordered]@{mode='interactive';output=$workerDir;bindings=$badBinding;bindingsSha256=(Get-FileHash -LiteralPath $badBinding -Algorithm SHA256).Hash.ToLowerInvariant();review=(Join-Path $workerDir 'ABSENT_REVIEW.json')}
$specPath=Join-Path $workerDir 'SPEC.json'
[IO.File]::WriteAllText($specPath,($spec|ConvertTo-Json))
$workerArguments=@('-NoProfile','-File',(Join-Path $PSScriptRoot 'stage-worker.ps1'),'-SpecPath',$specPath,'-SpecSHA256',(Get-FileHash -LiteralPath $specPath -Algorithm SHA256).Hash.ToLowerInvariant())
$o=[RA2CommandCardStageSupervisor]::Run((Join-Path $PSHOME 'pwsh.exe'),[string[]]$workerArguments,$OutputDirectory,220,$workerDir,'interactive',$workerTemp,[DateTimeOffset]::UtcNow.AddMinutes(20).ToString('o'),[Diagnostics.Stopwatch]::GetTimestamp(),(Join-Path $workerDir 'CANCEL'))
if($o.passed -or $o.exitCode -eq 0 -or $o.terminationRequested -or -not $o.outputComplete -or -not $o.cleanupSucceeded -or $o.activeOwnedProcessesAfterCleanup -ne 0){throw 'Actual worker refusal/own-console control failed.'}
$results.Add([ordered]@{case='worker-refusal';expectedAccepted=$false;actualAccepted=$o.passed;zeroSurvivors=$true;acceptanceEvidence=$false})
$tokens=$null;$errors=$null
[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'run-stage.ps1'),[ref]$tokens,[ref]$errors)|Out-Null
if($errors.Count){throw 'Controller parser errors.'}
$prepared=& (Join-Path $PSScriptRoot 'run-stage.ps1')|ConvertFrom-Json
if($prepared.preparedOnly -ne $true -or $prepared.engineInvoked -ne $false){throw 'Default controller did not remain preparation-only.'}
$results|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $OutputDirectory 'CONTROLS.json') -Encoding utf8
Write-Output ('PASS '+$results.Count+' short owned-process controls; no Unreal or acceptance evidence.')
