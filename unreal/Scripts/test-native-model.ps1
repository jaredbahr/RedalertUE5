param(
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7',
    [ValidateRange(15,90)][int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
# These observation/identity helpers are also exercised by pure mock tests.
function Initialize-NativeModelProcessProbe {
    if('NativeModelHandleObservation' -as [type]){return}
    Add-Type @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public sealed class NativeModelHandleObservation {
    public string observedUtc, error, exitTimeUtc;
    public uint processId, waitResult, exitCode;
    public bool exitCodeAvailable, timesAvailable;
    public long creationFileTime, exitFileTime;
    [DllImport("kernel32.dll", SetLastError=true)] static extern uint GetProcessId(IntPtr h);
    [DllImport("kernel32.dll", SetLastError=true)] static extern uint WaitForSingleObject(IntPtr h, uint ms);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr h, out uint code);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetProcessTimes(IntPtr h, out long created, out long exited, out long kernel, out long user);
    public static NativeModelHandleObservation Read(SafeHandle handle, int expectedPid, int waitMilliseconds) {
        var r=new NativeModelHandleObservation(); bool leased=false;
        try {
            if(waitMilliseconds!=0 && waitMilliseconds!=5000) throw new ArgumentOutOfRangeException("waitMilliseconds");
            if(handle==null || handle.IsInvalid || handle.IsClosed) throw new InvalidOperationException("Original pinned process handle unavailable.");
            handle.DangerousAddRef(ref leased);
            IntPtr raw=handle.DangerousGetHandle();
            r.processId=GetProcessId(raw);
            if(r.processId==0) throw new Win32Exception(Marshal.GetLastWin32Error());
            if(r.processId!=expectedPid) throw new InvalidOperationException("Original pinned native PID mismatch.");
            r.waitResult=WaitForSingleObject(raw,(uint)waitMilliseconds);
            if(r.waitResult==0xFFFFFFFF) throw new Win32Exception(Marshal.GetLastWin32Error());
            if(r.waitResult!=0 && r.waitResult!=258) throw new InvalidOperationException("Unexpected process wait result.");
            r.exitCodeAvailable=GetExitCodeProcess(raw,out r.exitCode);
            if(!r.exitCodeAvailable) throw new Win32Exception(Marshal.GetLastWin32Error());
            long kernel,user;
            r.timesAvailable=GetProcessTimes(raw,out r.creationFileTime,out r.exitFileTime,out kernel,out user);
            if(!r.timesAvailable) throw new Win32Exception(Marshal.GetLastWin32Error());
            // An early exit-status observation can precede signaling/timestamp completion.
            if(r.waitResult==0 && r.exitFileTime>0) r.exitTimeUtc=DateTime.FromFileTimeUtc(r.exitFileTime).ToString("o");
        } catch(Exception e) { r.error=e.Message; }
        finally {
            if(leased) { try { handle.DangerousRelease(); } catch(Exception e) { r.error=(r.error==null?"":r.error+"; ")+"SafeHandle lease release: "+e.Message; } }
            r.observedUtc=DateTime.UtcNow.ToString("o");
        }
        return r;
    }
}
'@
}
function Get-NativeModelProcessProbe($Process,[int]$WaitMilliseconds=0) {
    # Always use the original retained SafeHandle, including cleanup via a fresh PID view.
    return [NativeModelHandleObservation]::Read($nativeModelPinnedHandle,$nativeModelPinnedPid,$WaitMilliseconds)
}
function Test-NativeModelKernelExit($Probe) {
    return ($null -ne $Probe -and -not $Probe.error -and $Probe.waitResult -eq 0 -and $Probe.exitCodeAvailable)
}
function Add-NativeModelProcessProbe($Process,[Collections.IDictionary]$Lifecycle,[string]$Phase,[int]$WaitMilliseconds=0) {
    $probe=Get-NativeModelProcessProbe $Process $WaitMilliseconds
    if($null -eq $Lifecycle.nativeObservations){$Lifecycle.nativeObservations=@()}
    if(@($Lifecycle.nativeObservations).Count -lt 8){$Lifecycle.nativeObservations+=@{phase=$Phase;waitMilliseconds=$WaitMilliseconds;sample=$probe}}
    else{$Lifecycle.nativeObservationsTruncated=$true}
    return $probe
}
function Wait-NativeModelCleanupSignal($Process,[Collections.IDictionary]$Result,[string]$Phase) {
    $Result.cleanup.waitStartedUtc=[DateTime]::UtcNow.ToString('o')
    $probe=Add-NativeModelProcessProbe $Process $Result.processLifecycle $Phase 5000
    $Result.cleanup.waitReturnedUtc=[DateTime]::UtcNow.ToString('o')
    $Result.cleanup.waitConfirmedExit=Test-NativeModelKernelExit $probe
    if(-not $Result.cleanup.waitConfirmedExit){throw ('Owned automation kernel exit unconfirmed within the 5000ms cleanup bound: '+$probe.error)}
}
function Get-NativeModelPollDecision([bool]$TimeoutDecided,[bool]$WaitExited,[double]$ElapsedSeconds,[double]$TimeoutSeconds) {
    if($TimeoutDecided){return 'timeout'}
    # Preserve the existing WaitForExit(200) precedence; only an unsuccessful
    # wait reaches the unchanged elapsed-time deadline test.
    if($WaitExited){return 'exited'}
    if($ElapsedSeconds -ge $TimeoutSeconds){return 'timeout'}
    return 'wait'
}
function Get-NativeModelUtcDeadline([string]$StartedUtc,[int]$TimeoutSeconds) {
    return [DateTimeOffset]::Parse($StartedUtc,[Globalization.CultureInfo]::InvariantCulture).UtcDateTime.AddSeconds($TimeoutSeconds).ToString('o')
}
function Test-NativeModelOwnedIdentity($ActualId,$ActualStart,[string]$ActualPath,[string]$CommandLine,$ExpectedId,$ExpectedStart,[string]$ExpectedPath,[string]$ExpectedProject) {
    if($ActualId -ne $ExpectedId -or $ActualStart -isnot [datetime] -or $ExpectedStart -isnot [datetime] -or $ActualStart -ne $ExpectedStart -or -not $ActualPath -or -not $ExpectedPath -or -not $ExpectedProject -or -not $CommandLine){return $false}
    if(-not [string]::Equals($ActualPath.Replace('/','\'),$ExpectedPath.Replace('/','\'),[StringComparison]::OrdinalIgnoreCase)){return $false}
    $projectPattern=[regex]::Escape($ExpectedProject.Replace('/','\'))
    $argument='"'+$projectPattern+'"'
    if($ExpectedProject -notmatch '\s'){$argument='(?:'+$argument+'|'+$projectPattern+')'}
    return [regex]::IsMatch($CommandLine.Replace('/','\'),('(?:^|\s)'+$argument+'(?=\s|$)'),[Text.RegularExpressions.RegexOptions]::IgnoreCase)
}
function Confirm-NativeModelExit($Process,[Collections.IDictionary]$Lifecycle,[string]$Source,[double]$RunnerElapsed,$ProcessElapsed) {
    $Process.Refresh()
    if(-not $Process.HasExited){throw 'Cannot confirm exit of a still-live automation process.'}
    $probe=Add-NativeModelProcessProbe $Process $Lifecycle ('confirm:'+ $Source)
    if(-not (Test-NativeModelKernelExit $probe)){throw ('Cannot kernel-confirm the original automation process exit: '+$probe.error)}
    $Lifecycle.confirmedExitTimeUtc=$probe.exitTimeUtc
    $Lifecycle.confirmedExitCode=$probe.exitCode
    $Lifecycle.kernelExitConfirmed=$true
    $Lifecycle.confirmedExitObservedUtc=[DateTime]::UtcNow.ToString('o')
    $Lifecycle.confirmedExitSource=$Source
    $Lifecycle.confirmedExitRunnerElapsedSeconds=$RunnerElapsed
    $Lifecycle.confirmedExitProcessWatchElapsedSeconds=$ProcessElapsed
    $Lifecycle.confirmedExitElapsedSource='callerObservation'
}
function Set-NativeModelCleanupFailure([Collections.IDictionary]$Result,[string]$Phase,[string]$Message) {
    $Result.cleanup.errors+=@{phase=$Phase;message=$Message}
    $Result.passed=$false
    if(-not $Result.error){$Result.error='Owned-process cleanup failed; see cleanup.errors.'}
}
function Resolve-NativeModelCleanupException($Process,[Collections.IDictionary]$Result,[bool]$ConfirmationStarted,[string]$Failure,[double]$RunnerElapsed,$ProcessElapsed) {
    if(-not $ConfirmationStarted){
        try{
            $Process.Refresh()
            if($Process.HasExited){
                $probe=Add-NativeModelProcessProbe $Process $Result.processLifecycle 'cleanupExitRace'
                if(-not (Test-NativeModelKernelExit $probe)){
                    # A pre-confirmation lookup race can also land in status-before-signal.
                    # Wait once on the original handle; this never masks a prior bounded-wait failure.
                    $ConfirmationStarted=$true
                    Wait-NativeModelCleanupSignal $Process $Result 'cleanupExitRaceWait'
                }
                Confirm-NativeModelExit $Process $Result.processLifecycle 'cleanupExitRace' $RunnerElapsed $ProcessElapsed
                if($ConfirmationStarted){$Result.processLifecycle.confirmedExitElapsedSource='callerBeforeCleanupRaceWait'}
                $Result.cleanup.normalExitRaceObserved=$true
                return
            }
        }catch{$Failure+='; pinned exit recheck: '+$_.Exception.Message}
    }
    Set-NativeModelCleanupFailure $Result 'process' $Failure
}
function Get-NativeModelReportDiagnostic([byte[]]$Bytes,[datetime]$LastWriteTimeUtc,[datetime]$RunStartedUtc) {
    $diagnostic=[ordered]@{diagnosticOnly=$true;fresh=($LastWriteTimeUtc.ToUniversalTime() -ge $RunStartedUtc.ToUniversalTime());lastWriteTimeUtc=$LastWriteTimeUtc.ToUniversalTime().ToString('o');bytes=$Bytes.Length;sha256=$null;testCount=$null;succeeded=$null;failed=$null;tests=@();testsTruncated=$false;error=$null}
    if($Bytes.Length -gt 4MB){$diagnostic.error='Report exceeds the 4MiB diagnostic reading bound; hash and counts skipped.';return $diagnostic}
    $hash=[Security.Cryptography.SHA256]::Create()
    try{$diagnostic.sha256=[BitConverter]::ToString($hash.ComputeHash($Bytes)).Replace('-','')}finally{$hash.Dispose()}
    if(-not $diagnostic.fresh){$diagnostic.error='Existing report is stale; counts not read.';return $diagnostic}
    try{
        $stream=[IO.MemoryStream]::new($Bytes,$false);$reader=[IO.StreamReader]::new($stream,[Text.Encoding]::UTF8,$true)
        try{$text=$reader.ReadToEnd()}finally{$reader.Dispose();$stream.Dispose()}
        $document=$null
        try{$document=[System.Text.Json.JsonDocument]::Parse($text);if($document.RootElement.ValueKind -ne [System.Text.Json.JsonValueKind]::Object){throw 'Diagnostic report must have a JSON object root.'}}finally{if($document){$document.Dispose()}}
        $data=ConvertFrom-Json -InputObject $text -NoEnumerate -ErrorAction Stop
        $tests=if($null -eq $data.tests){@()}else{@($data.tests)}
        $diagnostic.testCount=@($tests).Count
        foreach($field in @('succeeded','failed')){
            $v=$data.$field
            if($v -is [string] -or $v -is [bool] -or $null -eq $v -or [double]$v -lt 0 -or [double]$v -gt 2147483647 -or [double]$v -ne [math]::Truncate([double]$v)){throw "Invalid diagnostic count $field."}
            $diagnostic[$field]=[int]$v
        }
        foreach($test in @($tests)|Select-Object -First 32){$name=[string]$test.fullTestPath;$state=[string]$test.state;$diagnostic.tests+=@{name=$name.Substring(0,[math]::Min(256,$name.Length));state=$state.Substring(0,[math]::Min(32,$state.Length))}}
        $diagnostic.testsTruncated=@($tests).Count -gt 32
    }catch{$diagnostic.error=$_.Exception.Message}
    return $diagnostic
}
$projectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot=(Resolve-Path (Join-Path $projectRoot '..')).Path
$project=Join-Path $projectRoot 'RedalertProof.uproject'
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if ([IO.Path]::GetPathRoot($repoRoot) -ne 'D:\') { throw 'Use a D: checkout.' }
if (-not (Test-Path -LiteralPath $editor)) { throw "Missing installed editor: $editor" }
$run=Join-Path $repoRoot ('generated\native-model-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
$report=Join-Path $run 'report'
New-Item -ItemType Directory -Force -Path $run,$report,(Join-Path $run 'cache'),(Join-Path $run 'temp') | Out-Null
$names=@('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')
$prior=@{}; $process=$null; $started=$null; $processWatch=$null; $nativeModelPinnedHandle=$null; $nativeModelPinnedPid=0
$watch=[Diagnostics.Stopwatch]::StartNew()
$availableMask=[Diagnostics.Process]::GetCurrentProcess().ProcessorAffinity.ToInt64()
$fixtureMask=[int64]0; $cpuCount=0
for($cpu=0; $cpu -lt 63 -and $cpuCount -lt 2; $cpu++) {
    $bit=[int64]1 -shl $cpu
    if(($availableMask -band $bit) -ne 0) { $fixtureMask=$fixtureMask -bor $bit; $cpuCount++ }
}
if($cpuCount -ne 2) { throw 'Native model automation requires two available logical CPUs.' }
$result=[ordered]@{head=(& git -C $repoRoot rev-parse HEAD).Trim();dirtyWorkingTree=[bool](& git -C $repoRoot status --porcelain);startedUtc=[DateTime]::UtcNow.ToString('o');finishedUtc=$null;elapsedSeconds=0;passed=$false; testCount=0; succeeded=0; failed=0; runDirectory=$run; error=$null;cpuPolicy=[ordered]@{affinityMask=$fixtureMask;priority='Normal';polls=0;repairs=0;events=@();peakObservedWorkingSetBytes=[int64]0;lastObservedCpuSeconds=0;pollingIsNotHardConfinement=$true}}
$result.processLifecycle=[ordered]@{pid=$null;launchRequestedUtc=$null;launchRequestedRunnerElapsedSeconds=$null;processStartTimeUtc=$null;processStartTimeObservedUtc=$null;launchReturnedUtc=$null;launchReturnedRunnerElapsedSeconds=$null;processWatchStartedUtc=$null;processWatchStartedRunnerElapsedSeconds=$null;processDeadlineUtc=$null;timeoutSeconds=$TimeoutSeconds;deadlineClock='Stopwatch elapsed; UTC deadline is observational';pollCount=0;lastPoll=$null;timeoutDecided=$false;timeoutDecisionUtc=$null;timeoutDecisionProcessWatchElapsedSeconds=$null;timeoutDecisionRunnerElapsedSeconds=$null;confirmedExitTimeUtc=$null;confirmedExitCode=$null;confirmedExitObservedUtc=$null;confirmedExitSource=$null;confirmedExitRunnerElapsedSeconds=$null;confirmedExitProcessWatchElapsedSeconds=$null}
$result.cleanup=[ordered]@{attempted=$false;identityVerified=$false;stopRequestedUtc=$null;waitStartedUtc=$null;waitReturnedUtc=$null;waitMilliseconds=5000;waitConfirmedExit=$null;normalExitRaceObserved=$false;errors=@()}
$result.processLifecycle.nativeObservations=@();$result.processLifecycle.nativeObservationsTruncated=$false;$result.processLifecycle.kernelExitConfirmed=$false
$result.runtime=[ordered]@{powershell=$PSVersionTable.PSVersion.ToString();dotnet=[Runtime.InteropServices.RuntimeInformation]::FrameworkDescription;processAssembly=[Diagnostics.Process].Assembly.FullName;rootThreadApartmentState=[Threading.Thread]::CurrentThread.GetApartmentState().ToString();returnedStartInfo=$null;returnedStartInfoCaveat='Returned-object data only; blank/default fields do not prove the actual launch branch.'}
$result.reportDiagnostic=$null
try {
    foreach($n in $names){$prior[$n]=[Environment]::GetEnvironmentVariable($n,'Process')}
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',(Join-Path $run 'cache'),'Process')
    [Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process')
    [Environment]::SetEnvironmentVariable('TEMP',(Join-Path $run 'temp'),'Process')
    [Environment]::SetEnvironmentVariable('TMP',(Join-Path $run 'temp'),'Process')
    Initialize-NativeModelProcessProbe
    $launchArguments=@(('"'+$project+'"'),'-unattended','-nullrhi','-nosound','-nosplash','-NoSourceControl','-NoP4','-ddc=BlockoutDDC','-CoreLimit=2','-ExecCmds="Automation RunTests RedalertProof.Commands"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+$report+'"'),('-abslog="'+(Join-Path $run 'automation.log')+'"'))
    $result.processLifecycle.launchRequestedUtc=[DateTime]::UtcNow.ToString('o')
    $result.processLifecycle.launchRequestedRunnerElapsedSeconds=$watch.Elapsed.TotalSeconds
    $process=Start-Process -FilePath $editor -ArgumentList $launchArguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    $result.processLifecycle.launchReturnedUtc=[DateTime]::UtcNow.ToString('o')
    $result.processLifecycle.launchReturnedRunnerElapsedSeconds=$watch.Elapsed.TotalSeconds
    $result.processLifecycle.pid=$process.Id
    $nativeModelPinnedPid=$process.Id;$nativeModelPinnedHandle=$process.SafeHandle
    $started=$process.StartTime
    $result.processLifecycle.processStartTimeUtc=$started.ToUniversalTime().ToString('o')
    $result.processLifecycle.processStartTimeObservedUtc=[DateTime]::UtcNow.ToString('o')
    $processWatch=[Diagnostics.Stopwatch]::StartNew()
    $result.processLifecycle.processWatchStartedUtc=[DateTime]::UtcNow.ToString('o')
    $result.processLifecycle.processWatchStartedRunnerElapsedSeconds=$watch.Elapsed.TotalSeconds
    $result.processLifecycle.processDeadlineUtc=Get-NativeModelUtcDeadline $result.processLifecycle.processWatchStartedUtc $TimeoutSeconds
    $result.runtime.returnedStartInfo=@{useShellExecute=$process.StartInfo.UseShellExecute;fileName=$process.StartInfo.FileName;arguments=$process.StartInfo.Arguments;redirectStandardOutput=$process.StartInfo.RedirectStandardOutput;redirectStandardError=$process.StartInfo.RedirectStandardError;windowStyle=$process.StartInfo.WindowStyle.ToString()}
    $null=Add-NativeModelProcessProbe $process $result.processLifecycle 'launch'
    $applyPolicy={
        if($process.HasExited) { return }
        try {
            $live=Get-Process -Id $process.Id -ErrorAction Stop
            if($live.StartTime -ne $started) { throw 'Automation process start identity changed.' }
            for($pathAttempt=0; $pathAttempt -lt 20 -and -not $live.Path -and -not $live.HasExited; $pathAttempt++) {
                Start-Sleep -Milliseconds 100
                $live.Refresh()
            }
            if($live.HasExited) { return }
            if($live.Path -ne $editor) { throw 'Automation process executable identity could not be verified.' }
            $beforeMask=$live.ProcessorAffinity.ToInt64(); $beforePriority=[string]$live.PriorityClass
            $live.ProcessorAffinity=[IntPtr]$fixtureMask
            $live.PriorityClass='Normal'
            $live.Refresh()
            if($live.ProcessorAffinity.ToInt64() -ne $fixtureMask -or $live.PriorityClass -ne 'Normal') { throw 'Native CPU policy verification failed.' }
            $result.cpuPolicy.polls++
            $result.cpuPolicy.peakObservedWorkingSetBytes=[math]::Max([int64]$result.cpuPolicy.peakObservedWorkingSetBytes,[int64]$live.PeakWorkingSet64)
            $result.cpuPolicy.lastObservedCpuSeconds=$live.CPU
            if($beforeMask -ne $fixtureMask -or $beforePriority -ne 'Normal') {
                $result.cpuPolicy.repairs++
                $result.cpuPolicy.events+=@{utc=[DateTime]::UtcNow.ToString('o');beforeMask=$beforeMask;beforePriority=$beforePriority}
            }
        } catch { if(-not $process.HasExited) { throw } }
    }
    & $applyPolicy
    while($true) {
        $pollStartedUtc=[DateTime]::UtcNow.ToString('o')
        $waitExited=$process.WaitForExit(200)
        $elapsed=$processWatch.Elapsed.TotalSeconds
        $result.processLifecycle.pollCount++
        $result.processLifecycle.lastPoll=@{startedUtc=$pollStartedUtc;returnedUtc=[DateTime]::UtcNow.ToString('o');processWatchElapsedSeconds=$elapsed;runnerElapsedSeconds=$watch.Elapsed.TotalSeconds;waitReturnedExit=$waitExited}
        $decision=Get-NativeModelPollDecision $result.processLifecycle.timeoutDecided $waitExited $elapsed $TimeoutSeconds
        if($decision -eq 'exited') { Confirm-NativeModelExit $process $result.processLifecycle 'normalWait' $watch.Elapsed.TotalSeconds $elapsed; break }
        if($decision -eq 'timeout') {
            $result.processLifecycle.timeoutDecided=$true
            $result.processLifecycle.timeoutDecisionUtc=[DateTime]::UtcNow.ToString('o')
            $result.processLifecycle.timeoutDecisionProcessWatchElapsedSeconds=$elapsed
            $result.processLifecycle.timeoutDecisionRunnerElapsedSeconds=$watch.Elapsed.TotalSeconds
            $null=Add-NativeModelProcessProbe $process $result.processLifecycle 'timeoutDecision'
            throw 'Native model automation timed out.'
        }
        & $applyPolicy
    }
    if ($process.ExitCode -ne 0) { throw "Editor exited $($process.ExitCode)." }
    $index=Join-Path $report 'index.json'
    if (-not (Test-Path -LiteralPath $index)) { throw 'No fresh Automation index.json report.' }
    $data=Get-Content -LiteralPath $index -Raw | ConvertFrom-Json
    $expected=@('RedalertProof.Commands.Validation','RedalertProof.Commands.ResourcesAndRate','RedalertProof.Commands.SimulationAndSnapshots','RedalertProof.Commands.PhaseOrder','RedalertProof.Commands.OwnerAllocation','RedalertProof.Commands.IngressBudget','RedalertProof.Commands.IngressFixture')
    $tests=@($data.tests)
    $result.testCount=$tests.Count; $result.succeeded=[int]$data.succeeded; $result.failed=[int]$data.failed
    if ($tests.Count -ne 7 -or [int]$data.succeeded -ne 7 -or [int]$data.failed -ne 0) { throw 'Expected exactly seven successful tests and zero failures.' }
    foreach($name in $expected){
        $matching=@($tests | Where-Object {$_.fullTestPath -eq $name})
        if ($matching.Count -ne 1 -or $matching[0].state -ne 'Success') { throw "Missing successful exact test $name." }
    }
    $result.passed=$true
} catch { $result.error=$_.Exception.Message }
finally {
    if($process) {
        $cleanupConfirmationStarted=$false
        try {
            $cleanupProbe=Add-NativeModelProcessProbe $process $result.processLifecycle 'cleanupEntry'
            if($process.HasExited -or (-not $cleanupProbe.error -and $cleanupProbe.exitCodeAvailable -and $cleanupProbe.exitCode -ne 259)) {
                if(-not (Test-NativeModelKernelExit $cleanupProbe)){
                    # Status can precede signaling. Wait on the same pinned handle; do not kill for that delay.
                    $result.cleanup.attempted=$true;$cleanupConfirmationStarted=$true
                    Wait-NativeModelCleanupSignal $process $result 'statusExitedCleanupWait'
                }
                if(-not $result.processLifecycle.kernelExitConfirmed){Confirm-NativeModelExit $process $result.processLifecycle 'alreadyExitedBeforeCleanup' $watch.Elapsed.TotalSeconds $(if($processWatch){$processWatch.Elapsed.TotalSeconds}else{$null})}
            } else {
                $result.cleanup.attempted=$true
                $live=Get-Process -Id $process.Id -ErrorAction SilentlyContinue
                if(-not $live){throw 'Automation process disappeared without confirmed exit.'}
                $identity=Get-CimInstance Win32_Process -Filter ('ProcessId='+$process.Id) -OperationTimeoutSec 2 -ErrorAction Stop
                if(-not (Test-NativeModelOwnedIdentity $live.Id $live.StartTime $live.Path ([string]$identity.CommandLine) $process.Id $started $editor $project)){throw 'Automation cleanup refused an unverified PID/start/path/project command line.'}
                $result.cleanup.identityVerified=$true
                $result.cleanup.stopRequestedUtc=[DateTime]::UtcNow.ToString('o')
                # Recheck the same verified object immediately before signalling.
                $live.Refresh()
                if(-not $live.HasExited){Stop-Process -Id $live.Id -Force -ErrorAction Stop}
                $result.cleanup.waitStartedUtc=[DateTime]::UtcNow.ToString('o')
                $cleanupConfirmationStarted=$true
                Wait-NativeModelCleanupSignal $process $result 'stoppedCleanupWait'
                Confirm-NativeModelExit $process $result.processLifecycle 'cleanupWait' $watch.Elapsed.TotalSeconds $(if($processWatch){$processWatch.Elapsed.TotalSeconds}else{$null})
                $remaining=Get-Process -Id $process.Id -ErrorAction SilentlyContinue
                if($remaining -and $remaining.StartTime -eq $started -and -not $remaining.HasExited){throw 'Owned automation process is still live after cleanup confirmation.'}
            }
        }catch{Resolve-NativeModelCleanupException $process $result $cleanupConfirmationStarted $_.Exception.Message $watch.Elapsed.TotalSeconds $(if($processWatch){$processWatch.Elapsed.TotalSeconds}else{$null})}
    }
    foreach($n in $names){if($prior.ContainsKey($n)){try{[Environment]::SetEnvironmentVariable($n,$prior[$n],'Process')}catch{Set-NativeModelCleanupFailure $result ('environment:'+ $n) $_.Exception.Message}}}
    # A late report is diagnostic only. It never updates accepted counts/passed.
    $index=Join-Path $report 'index.json'
    if(Test-Path -LiteralPath $index){
        try{
            $file=Get-Item -LiteralPath $index
            if($file.Length -gt 4MB){$result.reportDiagnostic=@{diagnosticOnly=$true;fresh=($file.LastWriteTimeUtc -ge ([DateTime]::Parse($result.startedUtc)).ToUniversalTime());lastWriteTimeUtc=$file.LastWriteTimeUtc.ToString('o');error='Report exceeds the 4MiB diagnostic reading bound; hash and counts skipped.';bytes=$file.Length;sha256=$null}}
            else{$result.reportDiagnostic=Get-NativeModelReportDiagnostic ([IO.File]::ReadAllBytes($index)) $file.LastWriteTimeUtc ([DateTime]::Parse($result.startedUtc))}
        }catch{$result.reportDiagnostic=@{diagnosticOnly=$true;error=$_.Exception.Message}}
    }
    $result.finishedUtc=[DateTime]::UtcNow.ToString('o'); $result.elapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3)
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8
}
if (-not $result.passed) { throw "$($result.error) See $run\result.json" }
Write-Output (Join-Path $run 'result.json')
