# Source-only fixtures. Run only in an admitted bounded slot after preparing the
# exact watchdog; no real editor, project payload or engine executable is used.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Python,[Parameter(Mandatory)][string]$OutputDirectory,[switch]$CapacityAdmitted)
$ErrorActionPreference='Stop'
if(-not $CapacityAdmitted -or -not ('RA2CommandCardStageSupervisor' -as [type])){throw 'Admitted capacity and an already prepared watchdog are required.'}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Fresh synthetic output directory required.'}
[IO.Directory]::CreateDirectory($OutputDirectory)|Out-Null
$fixture=Join-Path $OutputDirectory 'controller-fixture.py'
$code=@'
import ctypes, hashlib, json, subprocess, sys, time
from pathlib import Path
if not ctypes.WinDLL('kernel32',use_last_error=True).FreeConsole():
    raise OSError('Own-console release failed')
mode, directory, cancel = sys.argv[1:]
root = Path(directory)
def write(name, value):
    raw=json.dumps(value).encode()
    (root/name).write_bytes(raw)
    return hashlib.sha256(raw).hexdigest()
identity=dict(head='1'*40,bindingsSha256='b'*64,cppTree='2'*40)
write('SETUP_STARTED.json',dict(mode=mode))
if mode=='setup-stall': time.sleep(10)
proof=write('PAYLOAD_RESULT.json',dict(identity,automatedStageValidated=True,error=None,acceptanceEvidence=False))
inner=write('INNER_RESULT.json',dict(identity,passed=True,error=None,proofSha256=proof,acceptanceEvidence=False))
if mode=='post-inner-stall': time.sleep(10)
if mode=='nonzero-after-inner': sys.exit(7)
write('CONTROLLER_COMPLETE.json',dict(identity,passed=True,error=None,proofSha256=proof,innerOutcomeSha256=inner,acceptanceEvidence=False))
if mode in ('cancel-running-descendant','survivor'):
    child=subprocess.Popen([sys.executable,'-c','import time; time.sleep(10)'],creationflags=subprocess.CREATE_NO_WINDOW)
    write('DESCENDANT_STARTED.json',dict(rootPid=__import__('os').getpid(),childPid=child.pid))
    if mode=='cancel-running-descendant':
        Path(cancel).write_text('Cancel only this owned fixture after its child starts')
        time.sleep(10)
'@
[IO.File]::WriteAllText($fixture,$code,[Text.UTF8Encoding]::new($false))
$results=[Collections.Generic.List[object]]::new()
foreach($name in @('success','setup-stall','post-inner-stall','nonzero-after-inner','cancel-running-descendant','survivor')){
    $directory=Join-Path $OutputDirectory $name
    $temp=Join-Path $directory 'temp'
    $cancel=Join-Path $directory 'CANCEL'
    $origin=[Diagnostics.Stopwatch]::GetTimestamp()
    # Artificial earlier origin tests the original fixed 220-second deadline;
    # production dispatch never shifts its origin and no functional bound changes.
    if($name -in @('setup-stall','post-inner-stall')){$origin-=[long](217*[Diagnostics.Stopwatch]::Frequency)}
    $outcome=[RA2CommandCardStageSupervisor]::Run($Python,[string[]]@('-B',$fixture,$name,$directory,$cancel),$OutputDirectory,220,$directory,'interactive',$temp,[DateTimeOffset]::UtcNow.AddMinutes(20).ToString('o'),$origin,$cancel,('1'*40),('b'*64),('2'*40))
    [RA2CommandCardStageSupervisor]::ExportReturnedOutcome((Join-Path $directory 'RETURNED_OUTCOME.json'),$outcome)
    $expected=$name -eq 'success'
    if($outcome.passed -ne $expected -or $outcome.wholeController -ne $expected){throw "Unexpected terminal whole-controller result: $name"}
    if(-not $outcome.cleanupSucceeded -or $outcome.activeOwnedProcessesAfterCleanup -ne 0 -or $outcome.cleanupElapsedSeconds -gt 5){throw "Shared cleanup failed: $name"}
    if(-not (Test-Path -LiteralPath (Join-Path $directory 'SETUP_STARTED.json'))){throw 'Fixture did not reach its intended observation point.'}
    if($name -eq 'setup-stall' -and (Test-Path -LiteralPath (Join-Path $directory 'PAYLOAD_RESULT.json'))){throw 'Setup-stall control ran payload unexpectedly.'}
    if($name -in @('setup-stall','post-inner-stall') -and -not $outcome.timedOut){throw 'Whole-controller stall was not rejected at the fixed deadline.'}
    if($name -in @('post-inner-stall','nonzero-after-inner') -and -not (Test-Path -LiteralPath (Join-Path $directory 'INNER_RESULT.json'))){throw 'Control did not reach the post-inner-receipt point.'}
    if($name -eq 'cancel-running-descendant' -and (-not $outcome.leaseCancelled -or -not (Test-Path -LiteralPath (Join-Path $directory 'DESCENDANT_STARTED.json')))){throw 'Cancellation did not cover a running root with its descendant.'}
    if($name -eq 'survivor' -and (-not $outcome.originalSurvivorDecision -or -not $outcome.terminationRequested)){throw 'Surviving-child rejection was weakened.'}
    $results.Add([ordered]@{case=$name;accepted=$outcome.passed;wholeController=$outcome.wholeController;cleanupSeconds=$outcome.cleanupElapsedSeconds;zeroSurvivors=$true;acceptanceEvidence=$false})
}
$results|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $OutputDirectory 'CONTROLS.json') -Encoding utf8
Write-Output ('PASS '+$results.Count+' whole-controller synthetic controls; no Unreal or physical-input evidence.')
