# Pure mocks only: load selected helper declarations, never the runner body.
# No Start-Process/Get-Process/Stop-Process/CIM calls or Unreal execution here.
$ErrorActionPreference='Stop'
$tokens=$null;$errors=$null
$runner=Join-Path $PSScriptRoot 'test-native-model.ps1'
$ast=[Management.Automation.Language.Parser]::ParseFile($runner,[ref]$tokens,[ref]$errors)
if($errors){throw ($errors.Message -join '; ')}
foreach($name in @('Get-NativeModelPollDecision','Get-NativeModelUtcDeadline','Test-NativeModelOwnedIdentity','Test-NativeModelKernelExit','Add-NativeModelProcessProbe','Wait-NativeModelCleanupSignal','Confirm-NativeModelExit','Set-NativeModelCleanupFailure','Resolve-NativeModelCleanupException','Get-NativeModelReportDiagnostic')){
    $definition=$ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name},$true)
    if(-not $definition){throw "Missing actual runner helper $name"}
    Invoke-Expression $definition.Extent.Text
}
$checks=0
function Check([bool]$Condition,[string]$Name){if(-not $Condition){throw "Pure lifecycle check failed: $Name"};$script:checks++}
function Throws([scriptblock]$Action,[string]$Name){$didThrow=$false;try{$null=& $Action}catch{$didThrow=$true};Check $didThrow $Name}
Check ((Get-NativeModelPollDecision $false $false 89.999 90) -eq 'wait') 'unfinished before unchanged deadline waits'
Check ((Get-NativeModelPollDecision $false $false 90 90) -eq 'timeout') 'unfinished at90timesout'
Check ((Get-NativeModelPollDecision $false $false 90.2 90) -eq 'timeout') 'unfinished after90timesout'
Check ((Get-NativeModelPollDecision $false $true 12 90) -eq 'exited') 'confirmed ordinary exit'
Check ((Get-NativeModelPollDecision $true $true 91 90) -eq 'timeout') 'late confirmed exit cannot clear timeout latch'
Check ((Get-NativeModelPollDecision $true $false 10 90) -eq 'timeout') 'timeout latch is monotonic even on later clock observation'
foreach($offset in @('2026-10-17T06:00:00Z','2026-10-17T01:00:00-05:00','2026-10-17T08:00:00+02:00')){
    Check ((Get-NativeModelUtcDeadline $offset 90) -ceq '2026-10-17T06:01:30.0000000Z') ('deadline preserves UTC instant from '+$offset)
}
$start=[datetime]'2026-10-17T06:00:00Z';$exe='D:\UE\UnrealEditor-Cmd.exe';$project='D:\Proof\RedalertProof.uproject';$command='"D:\UE\UnrealEditor-Cmd.exe" "D:\Proof\RedalertProof.uproject" -nullrhi'
function Identity($Id=101,$At=$start,$Path=$exe,$Cmd=$command,$ExpectedId=101,$ExpectedAt=$start,$ExpectedExe=$exe,$ExpectedProject=$project){Test-NativeModelOwnedIdentity $Id $At $Path $Cmd $ExpectedId $ExpectedAt $ExpectedExe $ExpectedProject}
Check (Identity) 'exact PID/start/path/quotedproject tuple'
Check (Identity -Path 'd:/ue/UnrealEditor-Cmd.exe' -Cmd '"d:/UE/UnrealEditor-Cmd.exe" "d:/Proof/RedalertProof.uproject" -nullrhi') 'Windows case/slash normalization'
Check (Identity -Cmd 'D:\UE\UnrealEditor-Cmd.exe D:\Proof\RedalertProof.uproject -nullrhi') 'whole unquoted project argument without spaces'
Check (-not (Identity -Id 102)) 'PID reuse refused'
Check (-not (Identity -At $start.AddSeconds(1))) 'same PID wrong start refused'
Check (-not (Identity -Path 'D:\Other\UnrealEditor-Cmd.exe')) 'wrong executable refused'
Check (-not (Identity -Cmd '"D:\UE\UnrealEditor-Cmd.exe" "D:\Other\RedalertProof.uproject"')) 'other project refused'
Check (-not (Identity -Cmd '"D:\UE\UnrealEditor-Cmd.exe" -Other="D:\Proof\RedalertProof.uproject"')) 'project substring in another argument refused'
Check (-not (Identity -Cmd '"D:\UE\UnrealEditor-Cmd.exe" "D:\Proof\RedalertProof.uproject.backup"')) 'project suffix lookalike refused'
Check (-not (Identity -Cmd '')) 'missing commandline refused'
Check (-not (Identity -At $null)) 'missing start identity refused'
Check (Identity -ExpectedProject 'D:\Proof Space\RedalertProof.uproject' -Cmd '"D:\UE\UnrealEditor-Cmd.exe" "D:\Proof Space\RedalertProof.uproject"') 'quoted project with spaces'
Check (-not (Identity -ExpectedProject 'D:\Proof Space\RedalertProof.uproject' -Cmd 'D:\UE\UnrealEditor-Cmd.exe D:\Proof Space\RedalertProof.uproject')) 'unquoted split project argument refused'
function Fake-NativeProcess([bool]$Exited,[int]$Code=0){
    $probe=[pscustomobject]@{error=$null;waitResult=$(if($Exited){0}else{258});exitCodeAvailable=$true;exitCode=$(if($Exited){$Code}else{259});exitTimeUtc=$start.AddSeconds(92).ToUniversalTime().ToString('o')}
    $fake=[pscustomobject]@{HasExited=$Exited;ExitTime=[datetime]::FromFileTimeUtc(0);ExitCode=$Code;Probe=$probe}
    $fake|Add-Member -MemberType ScriptMethod -Name Refresh -Value {} -PassThru
}
# Replace only the native observation boundary. Real runner policy helpers above
# execute against these status/signal samples; no process or P/Invoke is launched.
$waitRequests=[Collections.Generic.List[int]]::new()
function Get-NativeModelProcessProbe($Process,[int]$WaitMilliseconds=0){
    $waitRequests.Add($WaitMilliseconds)
    if($WaitMilliseconds -eq 5000 -and $Process.WaitProbe){$Process.Probe=$Process.WaitProbe}
    return $Process.Probe
}
$lifecycle=[ordered]@{timeoutDecided=$true;confirmedExitTimeUtc=$null;confirmedExitCode=$null}
Confirm-NativeModelExit (Fake-NativeProcess $true) $lifecycle 'cleanupWait' 93 91
Check ($lifecycle.confirmedExitTimeUtc -eq $start.AddSeconds(92).ToUniversalTime().ToString('o') -and $lifecycle.confirmedExitCode -eq 0) 'actual mock exit time and code recorded separately from observation'
Check ($lifecycle.timeoutDecided) 'cleanup exit0 leaves timeout latched'
Check ($lifecycle.kernelExitConfirmed) 'same-handle signal is explicitly kernel-confirmed'
Check ($lifecycle.confirmedExitTimeUtc -ne [datetime]::FromFileTimeUtc(0).ToString('o')) 'cached Process.ExitTime1601 is not trusted'
Throws {Confirm-NativeModelExit (Fake-NativeProcess $false) ([ordered]@{confirmedExitTimeUtc=$null}) 'cleanupWait' 93 91} 'live process cannot gain confirmed exit timestamp'
$statusOnly=Fake-NativeProcess $true;$statusOnly.Probe.waitResult=258;$statusOnly.Probe.exitTimeUtc=$null
Check (-not (Test-NativeModelKernelExit $statusOnly.Probe)) 'exit0 plus raw timeout is not kernel confirmation'
$statusLifecycle=[ordered]@{timeoutDecided=$true;confirmedExitTimeUtc=$null;nativeObservations=@()}
Throws {Confirm-NativeModelExit $statusOnly $statusLifecycle 'statusOnly' 93 91} 'status-only HasExited cannot gain confirmed exit metadata'
Check ($null -eq $statusLifecycle.confirmedExitTimeUtc -and -not $statusLifecycle.kernelExitConfirmed) 'status-only confirmation failure leaves timestamps and kernel flag unset'
$cleanupResult=[ordered]@{passed=$false;error='Native model automation timed out.';processLifecycle=$statusLifecycle;cleanup=[ordered]@{waitConfirmedExit=$null}}
$waitRequests.Clear()
Throws {Wait-NativeModelCleanupSignal $statusOnly $cleanupResult 'statusOnlyWait'} 'status-exited unsignaled cleanup fails after bounded wait'
Check ($waitRequests.Count -eq 1 -and $waitRequests[0] -eq 5000 -and -not $cleanupResult.cleanup.waitConfirmedExit) 'cleanup signal wait uses exactly existing5000bound'
$statusOnly.Probe.waitResult=0;$statusOnly.Probe.exitTimeUtc=$start.AddSeconds(94).ToUniversalTime().ToString('o')
Wait-NativeModelCleanupSignal $statusOnly $cleanupResult 'lateSignalWait'
Confirm-NativeModelExit $statusOnly $statusLifecycle 'lateSignal' 95 94
Check ($cleanupResult.cleanup.waitConfirmedExit -and $statusLifecycle.kernelExitConfirmed) 'late kernel signal can confirm cleanup'
Check ($statusLifecycle.timeoutDecided -and -not $cleanupResult.passed -and $cleanupResult.error -eq 'Native model automation timed out.') 'late kernel signal never clears failed test gate or timeout'
$probeFailure=Fake-NativeProcess $true;$probeFailure.Probe.error='GetProcessId failed'
Check (-not (Test-NativeModelKernelExit $probeFailure.Probe)) 'native failure is not kernel confirmation even with zero wait result'
$probeFailureLifecycle=[ordered]@{confirmedExitTimeUtc=$null;nativeObservations=@()}
Throws {Confirm-NativeModelExit $probeFailure $probeFailureLifecycle 'nativeFailure' 1 1} 'native failure cannot promote exit confirmation'
Check ($probeFailureLifecycle.nativeObservations[0].sample.error -eq 'GetProcessId failed') 'native failure retained as bounded diagnostic observation'
$noTime=Fake-NativeProcess $true;$noTime.Probe.exitTimeUtc=$null
$noTimeLifecycle=[ordered]@{confirmedExitTimeUtc=$null}
Confirm-NativeModelExit $noTime $noTimeLifecycle 'noValidTime' 2 2
Check ($noTimeLifecycle.kernelExitConfirmed -and $null -eq $noTimeLifecycle.confirmedExitTimeUtc) 'kernel signal with unavailable valid timestamp stays timestamp-null'
$signaled259=Fake-NativeProcess $true 259;$signaled259Lifecycle=[ordered]@{}
Confirm-NativeModelExit $signaled259 $signaled259Lifecycle 'signaledNonzero' 2 2
Check ($signaled259Lifecycle.kernelExitConfirmed -and $signaled259Lifecycle.confirmedExitCode -eq 259) 'signaled exit259 is cleanup-confirmed but remains nonzero for unchanged acceptance check'
$bounded=[ordered]@{nativeObservations=@()}
for($i=0;$i -lt 12;$i++){$null=Add-NativeModelProcessProbe $noTime $bounded 'bounded'}
Check ($bounded.nativeObservations.Count -eq 8 -and $bounded.nativeObservationsTruncated) 'native observation storage bounded8'
$expected=@('Validation','ResourcesAndRate','SimulationAndSnapshots','PhaseOrder','OwnerAllocation','IngressBudget','IngressFixture')
$tests=@($expected|ForEach-Object{[ordered]@{fullTestPath=('RedalertProof.Commands.'+$_);state='Success'}})
$bytes=[Text.Encoding]::UTF8.GetBytes(([ordered]@{succeeded=7;failed=0;tests=$tests}|ConvertTo-Json -Depth 6 -Compress))
$failedRun=[ordered]@{passed=$false;testCount=0;succeeded=0;failed=0;error='Native model automation timed out.';cleanup=[ordered]@{errors=@()};reportDiagnostic=$null}
$failedRun.reportDiagnostic=Get-NativeModelReportDiagnostic $bytes $start.AddSeconds(91) $start
Check ($failedRun.reportDiagnostic.fresh -and $failedRun.reportDiagnostic.testCount -eq 7 -and $failedRun.reportDiagnostic.succeeded -eq 7) 'late fresh7Success is captured diagnostically'
Check (-not $failedRun.passed -and $failedRun.testCount -eq 0 -and $failedRun.succeeded -eq 0 -and $failedRun.error -eq 'Native model automation timed out.') 'late successful report never salvages failed gate or accepted counts'
Check ($failedRun.reportDiagnostic.sha256 -cmatch '^[A-F0-9]{64}$') 'diagnostic records raw report SHA256'
$changedBytes=[byte[]]($bytes+[byte]10)
$other=Get-NativeModelReportDiagnostic $changedBytes $start.AddSeconds(91) $start
Check ($other.sha256 -ne $failedRun.reportDiagnostic.sha256 -and $other.testCount -eq 7) 'hash binds bytes rather than normalized JSON'
$stale=Get-NativeModelReportDiagnostic $bytes $start.AddSeconds(-1) $start
Check (-not $stale.fresh -and $null -eq $stale.testCount -and $stale.error) 'stale existing report has no diagnostic success counts'
$mixedZone=Get-NativeModelReportDiagnostic $bytes $start.ToUniversalTime().AddSeconds(-1) $start
Check (-not $mixedZone.fresh -and $null -eq $mixedZone.testCount) 'freshness compares UTC instants across local/UTC DateTime kinds'
$sameOffset=Get-NativeModelReportDiagnostic $bytes ([datetime]::Parse('2026-10-17T08:00:00+02:00')) ([datetime]::Parse('2026-10-17T01:00:00-05:00'))
Check ($sameOffset.fresh -and $sameOffset.testCount -eq 7) 'opposite offsets at same instant are equally fresh independent of host timezone'
$staleOffset=Get-NativeModelReportDiagnostic $bytes ([DateTimeOffset]::Parse('2026-10-17T07:59:59+02:00').UtcDateTime) ([datetime]::Parse('2026-10-17T01:00:00-05:00'))
Check (-not $staleOffset.fresh -and $null -eq $staleOffset.testCount) 'stale opposite-offset report cannot gain success counts'
$bad=Get-NativeModelReportDiagnostic ([Text.Encoding]::UTF8.GetBytes('{broken')) $start.AddSeconds(91) $start
Check ($bad.error -and $null -eq $bad.testCount) 'torn report produces diagnostic error'
$large=Get-NativeModelReportDiagnostic ([byte[]]::new(4MB+1)) $start.AddSeconds(91) $start
Check ($large.error -and $null -eq $large.testCount -and $null -eq $large.sha256 -and $large.bytes -eq 4MB+1) 'diagnostic read/hash/parse are bounded'
foreach($root in @('[]','[{}]','true','7','"report"')){
    $wrongRoot=Get-NativeModelReportDiagnostic ([Text.Encoding]::UTF8.GetBytes($root)) $start.AddSeconds(91) $start
    Check ($wrongRoot.error -and $null -eq $wrongRoot.testCount) ('nonobject report root refused: '+$root)
}
$race=[ordered]@{passed=$true;error=$null;processLifecycle=[ordered]@{confirmedExitTimeUtc=$null};cleanup=[ordered]@{errors=@();normalExitRaceObserved=$false}}
Resolve-NativeModelCleanupException (Fake-NativeProcess $true) $race $false 'Get-Process or CIM raced normal exit' 93 91
Check ($race.passed -and $race.cleanup.normalExitRaceObserved -and $race.cleanup.errors.Count -eq 0 -and $race.processLifecycle.confirmedExitTimeUtc) 'pre-confirmation exit/PIDreuse race confirms original pinned exit without false cleanup failure'
$raceDelayed=Fake-NativeProcess $true;$raceDelayed.Probe.waitResult=258
$raceDelayed | Add-Member -NotePropertyName WaitProbe -NotePropertyValue (Fake-NativeProcess $true).Probe
$delayedResult=[ordered]@{passed=$false;error='Native model automation timed out.';processLifecycle=[ordered]@{timeoutDecided=$true};cleanup=[ordered]@{errors=@();normalExitRaceObserved=$false}}
$waitRequests.Clear()
Resolve-NativeModelCleanupException $raceDelayed $delayedResult $false 'CIM raced exit-status update' 93 91
Check ($delayedResult.processLifecycle.kernelExitConfirmed -and $delayedResult.cleanup.normalExitRaceObserved -and $delayedResult.cleanup.errors.Count -eq 0 -and @($waitRequests|Where-Object{$_ -eq 5000}).Count -eq 1) 'pre-confirmation status-only race waits once then kernel-confirms original handle'
Check (-not $delayedResult.passed -and $delayedResult.processLifecycle.timeoutDecided) 'pre-confirmation late signal retains failed timeout gate'
Check ($delayedResult.processLifecycle.confirmedExitElapsedSource -eq 'callerBeforeCleanupRaceWait' -and $lifecycle.confirmedExitElapsedSource -eq 'callerObservation') 'elapsed metadata labels caller sampling before bounded race wait while UTC observations remain separate'
$lateExit=[ordered]@{passed=$false;error='Native model automation timed out.';processLifecycle=[ordered]@{timeoutDecided=$true};cleanup=[ordered]@{errors=@();normalExitRaceObserved=$false}}
Resolve-NativeModelCleanupException (Fake-NativeProcess $true) $lateExit $true 'WaitForExit5000 timed out' 96 94
Check (-not $lateExit.passed -and -not $lateExit.cleanup.normalExitRaceObserved -and $lateExit.cleanup.errors.Count -eq 1 -and $lateExit.error -eq 'Native model automation timed out.') 'post-bound late exit never masks actual cleanup timeout'
$unverified=[ordered]@{passed=$true;error=$null;processLifecycle=[ordered]@{};cleanup=[ordered]@{errors=@();normalExitRaceObserved=$false}}
Resolve-NativeModelCleanupException (Fake-NativeProcess $false) $unverified $false 'Project commandline unverified' 15 13
Check (-not $unverified.passed -and $unverified.cleanup.errors.Count -eq 1) 'unverified still-live process fails instead of being signalled'
Set-NativeModelCleanupFailure $failedRun 'process' 'Owned process exceeded5000ms'
Check (-not $failedRun.passed -and $failedRun.error -eq 'Native model automation timed out.' -and $failedRun.cleanup.errors.Count -eq 1) 'cleanup failure preserves original timeout error'
$previouslyPassed=[ordered]@{passed=$true;error=$null;cleanup=[ordered]@{errors=@()}}
Set-NativeModelCleanupFailure $previouslyPassed 'process' 'Unverified commandline'
Check (-not $previouslyPassed.passed -and $previouslyPassed.error -and $previouslyPassed.cleanup.errors.Count -eq 1) 'cleanup failure cannot leave a passed gate'
Write-Output "PASS $checks pure-mock native lifecycle/timeout checks; no process or Unreal launched."
