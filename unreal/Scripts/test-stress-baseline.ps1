param(
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7',
    [ValidateRange(15,120)][int]$TimeoutSeconds = 120
)
$ErrorActionPreference = 'Stop'
$projectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot=(Resolve-Path (Join-Path $projectRoot '..')).Path
$project=Join-Path $projectRoot 'RedalertProof.uproject'
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if ([IO.Path]::GetPathRoot($repoRoot) -ne 'D:\') { throw 'Use a D: checkout.' }
if (-not (Test-Path -LiteralPath $editor)) { throw "Missing installed editor: $editor" }
$run=Join-Path $repoRoot ('generated\stress-baseline-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
$report=Join-Path $run 'report'
New-Item -ItemType Directory -Force -Path $run,$report,(Join-Path $run 'cache'),(Join-Path $run 'temp') | Out-Null
$names=@('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')
$prior=@{}; $process=$null; $started=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$storageLimitBytes=[int64]1GB
$cachePath=Join-Path $run 'cache'; $tempPath=Join-Path $run 'temp'
$cleanupErrors=@()
$getFolderBytes={ param([string]$Path)
    if(-not (Test-Path -LiteralPath $Path)) { return [int64]0 }
    $files=Get-ChildItem -LiteralPath $Path -File -Recurse -Force -ErrorAction SilentlyContinue
    $sum=($files | Measure-Object -Property Length -Sum).Sum
    if($null -eq $sum) { return [int64]0 }; return [int64]$sum
}
$sampleStorage={
    $now=[DateTime]::UtcNow
    if($result.storage.lastSampleUtc -and ($now-[DateTime]$result.storage.lastSampleUtc).TotalSeconds -lt 1.0) { return }
    $result.storage.lastSampleUtc=$now.ToString('o')
    $cacheBytes=& $getFolderBytes $cachePath
    $tempBytes=& $getFolderBytes $tempPath
    $result.storage.cacheBytes=[int64]$cacheBytes
    $result.storage.tempBytes=[int64]$tempBytes
    $result.storage.peakCacheBytes=[math]::Max([int64]$result.storage.peakCacheBytes,[int64]$cacheBytes)
    $result.storage.peakTempBytes=[math]::Max([int64]$result.storage.peakTempBytes,[int64]$tempBytes)
    $result.storage.samples++
    if($cacheBytes -gt $storageLimitBytes -or $tempBytes -gt $storageLimitBytes) { throw 'Sampled D: cache or temp storage exceeded its 1024 MiB soft limit.' }
}
$availableMask=[Diagnostics.Process]::GetCurrentProcess().ProcessorAffinity.ToInt64()
$fixtureMask=[int64]0; $cpuCount=0
for($cpu=0; $cpu -lt 63 -and $cpuCount -lt 2; $cpu++) {
    $bit=[int64]1 -shl $cpu
    if(($availableMask -band $bit) -ne 0) { $fixtureMask=$fixtureMask -bor $bit; $cpuCount++ }
}
if($cpuCount -ne 2) { throw 'Stress baseline automation requires two available logical CPUs.' }
$engineVersion=$null
$versionFile=Join-Path $EngineRoot 'Engine\Build\Build.version'
if(Test-Path -LiteralPath $versionFile) { $engineVersion=Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json }
$dirtyFiles=@(& git -C $repoRoot status --porcelain)
$result=[ordered]@{head=(& git -C $repoRoot rev-parse HEAD).Trim();dirtyWorkingTree=($dirtyFiles.Count -gt 0);dirtyFiles=$dirtyFiles;engineRoot=$EngineRoot;engineVersion=$engineVersion;startedUtc=[DateTime]::UtcNow.ToString('o');finishedUtc=$null;wallSeconds=0;processCpuSeconds=$null;passed=$false;testCount=0;succeeded=0;failed=0;runDirectory=$run;automationLog=(Join-Path $run 'automation.log');reportDirectory=$report;error=$null;cleanupErrors=@();stressMetrics=@();storage=[ordered]@{limitBytes=$storageLimitBytes;limitMiB=1024;cacheBytes=[int64]0;tempBytes=[int64]0;peakCacheBytes=[int64]0;peakTempBytes=[int64]0;samples=0;lastSampleUtc=$null};cpuPolicy=[ordered]@{affinityMask=$fixtureMask;priority='Normal';polls=0;repairs=0;events=@();peakObservedWorkingSetBytes=[int64]0;finalWorkingSetBytes=$null;lastObservedCpuSeconds=0;pollingIsNotHardConfinement=$true}}
try {
    & $sampleStorage
    foreach($n in $names){$prior[$n]=[Environment]::GetEnvironmentVariable($n,'Process')}
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',(Join-Path $run 'cache'),'Process')
    [Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process')
    [Environment]::SetEnvironmentVariable('TEMP',(Join-Path $run 'temp'),'Process')
    [Environment]::SetEnvironmentVariable('TMP',(Join-Path $run 'temp'),'Process')
    $launchArguments=@(('"'+$project+'"'),'-unattended','-nullrhi','-nosound','-nosplash','-NoSourceControl','-NoP4','-ddc=BlockoutDDC','-CoreLimit=2','-ExecCmds="Automation RunTests RedalertProof.Stress.ModelBaseline"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+$report+'"'),('-abslog="'+$result.automationLog+'"'))
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
            if($live.ProcessorAffinity.ToInt64() -ne $fixtureMask -or $live.PriorityClass -ne 'Normal') { throw 'Stress CPU policy verification failed.' }
            & $sampleStorage
            $result.cpuPolicy.polls++
            $result.cpuPolicy.peakObservedWorkingSetBytes=[math]::Max([int64]$result.cpuPolicy.peakObservedWorkingSetBytes,[int64]$live.PeakWorkingSet64)
            $result.cpuPolicy.finalWorkingSetBytes=[int64]$live.WorkingSet64
            $result.cpuPolicy.lastObservedCpuSeconds=$live.CPU
            if($beforeMask -ne $fixtureMask -or $beforePriority -ne 'Normal') {
                $result.cpuPolicy.repairs++
                $result.cpuPolicy.events+=@{utc=[DateTime]::UtcNow.ToString('o');beforeMask=$beforeMask;beforePriority=$beforePriority}
            }
        } catch { if(-not $process.HasExited) { throw } }
    }
    & $applyPolicy
    while(-not $process.WaitForExit(200)) {
        if($processWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Stress baseline automation timed out.' }
        & $applyPolicy
    }
    if($processWatch.Elapsed.TotalSeconds -gt $TimeoutSeconds) { throw 'Stress baseline automation exceeded its deadline.' }
    $process.Refresh()
    try { $result.processCpuSeconds=$process.CPU } catch { $result.cleanupErrors+=@('Process CPU after exit: '+$_.Exception.Message) }
    if ($process.ExitCode -ne 0) { throw "Editor exited $($process.ExitCode)." }
    $index=Join-Path $report 'index.json'
    if (-not (Test-Path -LiteralPath $index)) { throw 'No fresh stress Automation index.json report.' }
    $data=Get-Content -LiteralPath $index -Raw | ConvertFrom-Json
    $expected='RedalertProof.Stress.ModelBaseline'
    $tests=@($data.tests)
    $result.testCount=$tests.Count; $result.succeeded=[int]$data.succeeded; $result.failed=[int]$data.failed
    $matching=@($tests | Where-Object {$_.fullTestPath -eq $expected})
    if ($tests.Count -ne 1 -or [int]$data.succeeded -ne 1 -or [int]$data.failed -ne 0 -or $matching.Count -ne 1 -or $matching[0].state -ne 'Success') { throw 'Expected exactly one successful stress baseline test and zero failures.' }
    $logText=Get-Content -LiteralPath $result.automationLog -Raw
    $cohortMatches=[regex]::Matches($logText,'STRESS_BASELINE (?<json>\{[^\r\n]+\})')
    if($cohortMatches.Count -ne 3) { throw "Expected exactly three structured STRESS_BASELINE cohort records; found $($cohortMatches.Count)." }
    $metrics=@($cohortMatches | ForEach-Object { $_.Groups['json'].Value | ConvertFrom-Json })
    $expectedSizes=@(64,128,256)
    $actualSizes=@($metrics | ForEach-Object { [int]$_.armySize } | Sort-Object)
    if(($actualSizes -join ',') -ne ($expectedSizes -join ',')) { throw "Expected cohort sizes 64, 128, and 256; found $($actualSizes -join ',')." }
    foreach($metric in $metrics) {
        if($metric.fixture -ne 'legacy_proof_command_model' -or $metric.rendered -or $metric.networked -or [int]$metric.steps -ne 100 -or -not $metric.repeatDigestMatched -or [int]$metric.acceptedAttackCommands -ne [int]$metric.armySize -or [int]$metric.activeCombatUnits -ne [int]$metric.armySize -or [int]$metric.movedUnits -ne [int]$metric.armySize -or [int]$metric.damagedUnits -ne [int]$metric.armySize) { throw "Invalid stress workload evidence for cohort $($metric.armySize)." }
        if([string]$metric.finalDigest -notmatch '^[0-9a-fA-F]{32}$') { throw "Invalid final digest in cohort $($metric.armySize)." }
        foreach($value in @($metric.advanceMs.median,$metric.advanceMs.p95,$metric.advanceMs.max,$metric.snapshotAndComputeDigestMs.median,$metric.snapshotAndComputeDigestMs.p95,$metric.snapshotAndComputeDigestMs.max)) {
            $number=[double]$value
            if([double]::IsNaN($number) -or [double]::IsInfinity($number) -or $number -lt 0) { throw "Nonfinite or negative timing metric in cohort $($metric.armySize)." }
        }
    }
    $result.stressMetrics=$metrics
    $result.passed=$true
} catch { $result.error=$_.Exception.Message }
finally {
    try {
        if ($process -and -not $process.HasExited) {
            $live=Get-Process -Id $process.Id -ErrorAction SilentlyContinue
            if ($live -and $live.StartTime -eq $started -and $live.Path -eq $editor) {
                $identity=Get-CimInstance Win32_Process -Filter ('ProcessId='+$process.Id) -ErrorAction SilentlyContinue
                if ($identity -and $identity.CommandLine.Contains($project)) { Stop-Process -Id $process.Id -Force }
            }
        }
    } catch { $cleanupErrors+=@('Owned process cleanup: '+$_.Exception.Message) }
    if($process) {
        try { $process.Refresh(); $result.processCpuSeconds=$process.CPU } catch { $cleanupErrors+=@('Process CPU after exit: '+$_.Exception.Message) }
        try { $process.Refresh(); $result.cpuPolicy.finalWorkingSetBytes=[int64]$process.WorkingSet64 } catch { $cleanupErrors+=@('Working set after exit: '+$_.Exception.Message) }
    }
    foreach($n in $names) {
        try { if($prior.ContainsKey($n)){[Environment]::SetEnvironmentVariable($n,$prior[$n],'Process')} }
        catch { $cleanupErrors+=@("Restore process environment $n`: "+$_.Exception.Message) }
    }
    $result.finishedUtc=[DateTime]::UtcNow.ToString('o'); $result.wallSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3)
    $result.cleanupErrors=$cleanupErrors
    try { $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8 }
    catch { Write-Error "Could not write result.json; retained automation log and report at $run. $($_.Exception.Message)" -ErrorAction Continue }
}
if (-not $result.passed) { throw "$($result.error) See $run\result.json and $($result.automationLog)" }
if($result.cleanupErrors.Count -gt 0) { Write-Warning ($result.cleanupErrors -join '; ') }
Write-Output (Join-Path $run 'result.json')
