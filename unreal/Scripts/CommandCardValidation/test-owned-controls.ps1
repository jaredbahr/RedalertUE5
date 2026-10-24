param([Parameter(Mandatory)][string]$Python,[Parameter(Mandatory)][string]$OutputDirectory,[string]$Git,[switch]$CapacityAdmitted,[switch]$AffectedOnly)
$ErrorActionPreference='Stop'
if(-not $CapacityAdmitted){throw 'Owned-process controls require current capacity admission.'}
if([string]::IsNullOrWhiteSpace($Git) -or -not [IO.Path]::IsPathFullyQualified($Git) -or -not (Test-Path -LiteralPath $Git -PathType Leaf)){throw 'The inert native-call control requires an exact Git executable path.'}
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
$genericCases=if($AffectedOnly){@()}else{@('success','failure','timeout','cancelled','overflow','survivor','short-lease')}
foreach($name in $genericCases){
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
# Negative child exit is expected for refusal. All independent native ownership,
# reservation, cleanup and original timing conditions must still be successful.
function Test-CleanNativeReturn($Outcome,[long]$ReturnedQpc){
    foreach($name in @('resumed','jobAssigned','timedOut','reservationLost','leaseCancelled','terminationRequested','cleanupSucceeded','outputComplete','error','cleanupError','boundSeconds','frequency','deadlineStartQpc','observedExitQpc','completedQpc','elapsedSeconds','cleanupElapsedSeconds','activeOwnedProcessesAfterCleanup')){
        if($null -eq $Outcome.PSObject.Properties[$name]){return $false}
    }
    if($Outcome.resumed -ne $true -or $Outcome.jobAssigned -ne $true -or $Outcome.timedOut -ne $false -or $Outcome.reservationLost -ne $false -or $Outcome.leaseCancelled -ne $false -or $Outcome.terminationRequested -ne $false -or $Outcome.cleanupSucceeded -ne $true -or $Outcome.outputComplete -ne $true -or $null -ne $Outcome.error -or $null -ne $Outcome.cleanupError -or $Outcome.activeOwnedProcessesAfterCleanup -ne 0){return $false}
    if($Outcome.boundSeconds -ne 220 -or $Outcome.frequency -ne [Diagnostics.Stopwatch]::Frequency -or $Outcome.deadlineStartQpc -le 0 -or $Outcome.observedExitQpc -lt $Outcome.deadlineStartQpc -or $Outcome.completedQpc -lt $Outcome.observedExitQpc -or $ReturnedQpc -lt $Outcome.completedQpc){return $false}
    $elapsed=($ReturnedQpc-$Outcome.deadlineStartQpc)/[double]$Outcome.frequency
    if(-not [double]::IsFinite($elapsed) -or $elapsed -lt 0 -or $elapsed -ge 220 -or -not [double]::IsFinite($Outcome.elapsedSeconds) -or $Outcome.elapsedSeconds -lt 0 -or $Outcome.elapsedSeconds -ge 220 -or -not [double]::IsFinite($Outcome.cleanupElapsedSeconds) -or $Outcome.cleanupElapsedSeconds -lt 0 -or $Outcome.cleanupElapsedSeconds -gt 5){return $false}
    return $true
}
# Exercise the actual worker's binding-policy rejection. Its dedicated test host
# mirrors the controller's late caller-only console release, including failures.
$workerDir=Join-Path $OutputDirectory 'worker-refusal'
$workerTemp=Join-Path $workerDir 'temp'
[IO.Directory]::CreateDirectory($workerTemp)|Out-Null
$badBinding=Join-Path $workerDir 'INVALID_BINDINGS.json'
[IO.File]::WriteAllText($badBinding,(@{scope='INERT-INVALID';sourceHead=('1'*40);executables=@{python=@{path=$Python}}}|ConvertTo-Json -Depth 5))
$spec=[ordered]@{mode='interactive';output=$workerDir;bindings=$badBinding;bindingsSha256=(Get-FileHash -LiteralPath $badBinding -Algorithm SHA256).Hash.ToLowerInvariant();review=(Join-Path $workerDir 'ABSENT_REVIEW.json')}
$specPath=Join-Path $workerDir 'SPEC.json'
[IO.File]::WriteAllText($specPath,($spec|ConvertTo-Json))
$workerHost=@'
param([string]$WorkerPath,[string]$SpecPath,[string]$SpecSHA256)
$ErrorActionPreference='Stop'
Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;
public static class CommandCardControlConsole {
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
}
"@
try {
    & $WorkerPath -SpecPath $SpecPath -SpecSHA256 $SpecSHA256
}finally{
    if(-not [CommandCardControlConsole]::FreeConsole()){throw ('Own-console release failed: '+[Runtime.InteropServices.Marshal]::GetLastWin32Error())}
}
'@
$workerHostPath=Join-Path $workerDir 'worker-host.ps1'
[IO.File]::WriteAllText($workerHostPath,$workerHost,[Text.UTF8Encoding]::new($false))
$workerArguments=@('-NoProfile','-File',$workerHostPath,'-WorkerPath',(Join-Path $PSScriptRoot 'stage-worker.ps1'),'-SpecPath',$specPath,'-SpecSHA256',(Get-FileHash -LiteralPath $specPath -Algorithm SHA256).Hash.ToLowerInvariant())
$o=[RA2CommandCardStageSupervisor]::Run((Join-Path $PSHOME 'pwsh.exe'),[string[]]$workerArguments,$OutputDirectory,220,$workerDir,'interactive',$workerTemp,[DateTimeOffset]::UtcNow.AddMinutes(20).ToString('o'),[Diagnostics.Stopwatch]::GetTimestamp(),(Join-Path $workerDir 'CANCEL'))
$workerReturnedQpc=[Diagnostics.Stopwatch]::GetTimestamp()
$workerError=Get-Content -LiteralPath (Join-Path $workerDir 'interactive.stderr.log') -Raw
if($o.passed -or $null -eq $o.exitCode -or $o.exitCode -eq 0 -or -not (Test-CleanNativeReturn $o $workerReturnedQpc) -or $workerError -notmatch 'wrong binding scope' -or $workerError -match 'Own-console release failed'){throw 'Actual worker must reach its intended binding-policy rejection with a clean on-time owned outcome.'}
$results.Add([ordered]@{case='worker-refusal';expectedAccepted=$false;actualAccepted=$o.passed;intendedBindingRejection=$true;zeroSurvivors=$true;acceptanceEvidence=$false})
# Inert PowerShell success coverage for the native-invocation path. Python/Git
# run before caller-only detachment; a final native call represents reconciliation.
$nativeDir=Join-Path $OutputDirectory 'powershell-native-success'
$nativeTemp=Join-Path $nativeDir 'temp'
[IO.Directory]::CreateDirectory($nativeTemp)|Out-Null
$nativeHost=@'
param([string]$Python,[string]$Git,[string]$ReleasedPath)
$ErrorActionPreference='Stop'
Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;
public static class CommandCardNativeSuccessConsole {
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
}
"@
try {
    & $Python -B -c 'print("PR72_NATIVE_FIRST")'
    if($LASTEXITCODE -ne 0){throw 'First native Python call failed.'}
    & $Git --version
    if($LASTEXITCODE -ne 0){throw 'Native Git call failed.'}
    & $Python -B -c 'print("PR72_NATIVE_FINAL")'
    if($LASTEXITCODE -ne 0){throw 'Final native Python call failed.'}
}finally{
    if(-not [CommandCardNativeSuccessConsole]::FreeConsole()){throw ('Own-console release failed: '+[Runtime.InteropServices.Marshal]::GetLastWin32Error())}
    [IO.File]::WriteAllText($ReleasedPath,'caller-only console released after native calls')
}
'@
$nativeHostPath=Join-Path $nativeDir 'native-host.ps1'
$releasedPath=Join-Path $nativeDir 'OWN_CONSOLE_RELEASED.txt'
[IO.File]::WriteAllText($nativeHostPath,$nativeHost,[Text.UTF8Encoding]::new($false))
$nativeArguments=@('-NoProfile','-File',$nativeHostPath,'-Python',$Python,'-Git',$Git,'-ReleasedPath',$releasedPath)
$o=[RA2CommandCardStageSupervisor]::Run((Join-Path $PSHOME 'pwsh.exe'),[string[]]$nativeArguments,$OutputDirectory,220,$nativeDir,'interactive',$nativeTemp,[DateTimeOffset]::UtcNow.AddMinutes(20).ToString('o'),[Diagnostics.Stopwatch]::GetTimestamp(),(Join-Path $nativeDir 'CANCEL'))
$nativeReturnedQpc=[Diagnostics.Stopwatch]::GetTimestamp()
$nativeOutput=Get-Content -LiteralPath (Join-Path $nativeDir 'interactive.stdout.log') -Raw
if($o.passed -ne $true -or $null -eq $o.exitCode -or $o.exitCode -ne 0 -or -not (Test-CleanNativeReturn $o $nativeReturnedQpc) -or $nativeOutput -notmatch '(?s)PR72_NATIVE_FIRST.*git version .*PR72_NATIVE_FINAL' -or -not (Test-Path -LiteralPath $releasedPath) -or (Get-Content -LiteralPath $releasedPath -Raw) -cne 'caller-only console released after native calls'){throw 'Inert PowerShell native-call sequence did not complete on time with late caller-only console release and zero survivors.'}
$results.Add([ordered]@{case='powershell-native-success';expectedAccepted=$true;actualAccepted=$o.passed;nativeCallsBeforeOwnConsoleRelease=$true;zeroSurvivors=$true;acceptanceEvidence=$false})
$tokens=$null;$errors=$null
[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'run-stage.ps1'),[ref]$tokens,[ref]$errors)|Out-Null
if($errors.Count){throw 'Controller parser errors.'}
$prepared=& (Join-Path $PSScriptRoot 'run-stage.ps1')|ConvertFrom-Json
if($prepared.preparedOnly -ne $true -or $prepared.engineInvoked -ne $false){throw 'Default controller did not remain preparation-only.'}
$results|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $OutputDirectory 'CONTROLS.json') -Encoding utf8
Write-Output ('PASS '+$results.Count+' short owned-process controls; no Unreal or acceptance evidence.')
