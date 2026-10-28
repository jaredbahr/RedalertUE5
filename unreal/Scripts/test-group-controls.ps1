param(
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7',
    [ValidateRange(15,90)][int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
$projectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot=(Resolve-Path (Join-Path $projectRoot '..')).Path
$project=Join-Path $projectRoot 'RedalertProof.uproject'
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if ([IO.Path]::GetPathRoot($repoRoot) -ne 'D:\') { throw 'Use a D: checkout.' }
if (-not (Test-Path -LiteralPath $editor)) { throw "Missing installed editor: $editor" }
$run=Join-Path $repoRoot ('generated\group-controls-model-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
$report=Join-Path $run 'report'
New-Item -ItemType Directory -Force -Path $run,$report,(Join-Path $run 'cache'),(Join-Path $run 'temp') | Out-Null
$names=@('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')
$prior=@{}; $process=$null; $started=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$availableMask=[Diagnostics.Process]::GetCurrentProcess().ProcessorAffinity.ToInt64()
$fixtureMask=[int64]0; $cpuCount=0
for($cpu=0; $cpu -lt 63 -and $cpuCount -lt 2; $cpu++) {
    $bit=[int64]1 -shl $cpu
    if(($availableMask -band $bit) -ne 0) { $fixtureMask=$fixtureMask -bor $bit; $cpuCount++ }
}
if($cpuCount -ne 2) { throw 'Controls model automation requires two available logical CPUs.' }
$result=[ordered]@{head=(& git -C $repoRoot rev-parse HEAD).Trim();dirtyWorkingTree=[bool](& git -C $repoRoot status --porcelain);startedUtc=[DateTime]::UtcNow.ToString('o');finishedUtc=$null;elapsedSeconds=0;passed=$false; testCount=0; succeeded=0; failed=0; runDirectory=$run; error=$null;cpuPolicy=[ordered]@{affinityMask=$fixtureMask;priority='Normal';polls=0;repairs=0;events=@();peakObservedWorkingSetBytes=[int64]0;lastObservedCpuSeconds=0;pollingIsNotHardConfinement=$true}}
try {
    foreach($n in $names){$prior[$n]=[Environment]::GetEnvironmentVariable($n,'Process')}
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',(Join-Path $run 'cache'),'Process')
    [Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process')
    [Environment]::SetEnvironmentVariable('TEMP',(Join-Path $run 'temp'),'Process')
    [Environment]::SetEnvironmentVariable('TMP',(Join-Path $run 'temp'),'Process')
    $launchArguments=@(('"'+$project+'"'),'-unattended','-nullrhi','-nosound','-nosplash','-NoSourceControl','-NoP4','-ddc=BlockoutDDC','-CoreLimit=2','-ExecCmds="Automation RunTests RedalertProof.Controls"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+$report+'"'),('-abslog="'+(Join-Path $run 'automation.log')+'"'))
    $process=Start-Process -FilePath $editor -ArgumentList $launchArguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    $started=$process.StartTime
    $processWatch=[Diagnostics.Stopwatch]::StartNew()
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
            if($live.ProcessorAffinity.ToInt64() -ne $fixtureMask -or $live.PriorityClass -ne 'Normal') { throw 'Controls CPU policy verification failed.' }
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
    while(-not $process.WaitForExit(200)) {
        if($processWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Controls model automation timed out.' }
        & $applyPolicy
    }
    if ($process.ExitCode -ne 0) { throw "Editor exited $($process.ExitCode)." }
    $index=Join-Path $report 'index.json'
    if (-not (Test-Path -LiteralPath $index)) { throw 'No fresh Automation index.json report.' }
    $data=Get-Content -LiteralPath $index -Raw | ConvertFrom-Json
    $expected=@('RedalertProof.Controls.CooperativeContextTargets','RedalertProof.Controls.CameraPanInput','RedalertProof.Controls.CameraPanMovement','RedalertProof.Controls.SelectionOwnershipAndPruning','RedalertProof.Controls.SelectionGroupsAndEpochs','RedalertProof.Controls.SelectionNineGroups','RedalertProof.Controls.SelectionDeterministicPlans','RedalertProof.Controls.SelectionBoundedAndStale','RedalertProof.Controls.QueueCoalescesAndPreservesOrder','RedalertProof.Controls.QueueWallPacing','RedalertProof.Controls.QueueSnapshotRevalidation','RedalertProof.Controls.QueueCapacityIsAtomic','RedalertProof.Controls.ReturnCargoSelection','RedalertProof.Controls.ReturnCargoQueue','RedalertProof.Controls.UnitViewGrounding','RedalertProof.Controls.UnitViewFacing','RedalertProof.Controls.BoxSelectionAtomic','RedalertProof.Controls.BoxSelectionContextAndCapacity','RedalertProof.Controls.BoxSelectionGesture')
    $tests=@($data.tests)
    $result.testCount=$tests.Count; $result.succeeded=[int]$data.succeeded; $result.failed=[int]$data.failed
    if ($tests.Count -ne $expected.Count -or [int]$data.succeeded -ne $expected.Count -or [int]$data.failed -ne 0) { throw 'Expected all exact named controls tests successful and zero failures.' }
    foreach($name in $expected){
        $matching=@($tests | Where-Object {$_.fullTestPath -eq $name})
        if ($matching.Count -ne 1 -or $matching[0].state -ne 'Success') { throw "Missing successful exact test $name." }
    }
    $result.passed=$true
} catch { $result.error=$_.Exception.Message }
finally {
    if ($process -and -not $process.HasExited) {
        $live=Get-Process -Id $process.Id -ErrorAction SilentlyContinue
        if ($live -and $live.StartTime -eq $started -and $live.Path -eq $editor) {
            $identity=Get-CimInstance Win32_Process -Filter ('ProcessId='+$process.Id) -ErrorAction SilentlyContinue
            if ($identity -and $identity.CommandLine.Contains($project)) { Stop-Process -Id $process.Id -Force }
        }
    }
    foreach($n in $names){if($prior.ContainsKey($n)){[Environment]::SetEnvironmentVariable($n,$prior[$n],'Process')}}
    $result.finishedUtc=[DateTime]::UtcNow.ToString('o'); $result.elapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3)
    $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8
}
if (-not $result.passed) { throw "$($result.error) See $run\result.json" }
Write-Output (Join-Path $run 'result.json')


