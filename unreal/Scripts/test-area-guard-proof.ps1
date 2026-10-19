param(
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7',
    [ValidateRange(20000,60000)][int]$Port = 28969,
    [ValidateRange(45,120)][int]$TimeoutSeconds = 120,
    [ValidateRange(30,300)][int]$WarmupTimeoutSeconds = 300,
    [ValidateRange(1,8192)][int]$MaxCacheMiB = 2048,
    [ValidateRange(1,8192)][int]$MaxTempMiB = 1024
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'AreaGuardObservation.ps1')
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path (Join-Path $projectRoot '..')).Path
$project = Join-Path $projectRoot 'RedalertProof.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$workerExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\ShaderCompileWorker.exe'
$dll = Join-Path $projectRoot 'Binaries\Win64\UnrealEditor-RedalertProof.dll'
if ([IO.Path]::GetPathRoot($repoRoot) -ne 'D:\') { throw 'AreaGuard requires a D: checkout.' }
foreach ($path in @($project,$editor,$workerExe,$dll,(Join-Path $projectRoot 'Content\CommandProofGenerated\Maps\CommandProof.umap'))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing exact fixture input: $path" }
}
$probe = [Net.Sockets.UdpClient]::new()
try { $probe.Client.ExclusiveAddressUse=$true; $probe.Client.Bind([Net.IPEndPoint]::new([Net.IPAddress]::Loopback,$Port)) }
finally { $probe.Dispose() }
$run = Join-Path $repoRoot ('generated\area-guard-proof-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
if (Test-Path -LiteralPath $run) { throw 'Fresh run directory already exists.' }
$cache = Join-Path $repoRoot 'generated\area-guard-proof-cache'
$temp = Join-Path $run 'temp'
New-Item -ItemType Directory -Force -Path $run,$cache,$temp | Out-Null
$cvars = Join-Path $run 'literal-client-cvars.ini'
"[Startup]`nnet.IpConnectionDisableResolution=1`n" | Set-Content -LiteralPath $cvars -Encoding ascii
$token = [Guid]::NewGuid().ToString('N')
$head = (& git -C $repoRoot rev-parse HEAD).Trim()
$source = (& git -C $repoRoot rev-parse 'HEAD:unreal/Source').Trim()
$dirty = [bool](& git -C $repoRoot status --porcelain)
if ($LASTEXITCODE -ne 0 -or $dirty -or $source -cnotmatch '^[0-9a-f]{40}$') { throw 'Fixture requires a clean, hashable source checkout.' }
$mask = [int64]68
$invokerMask = [Diagnostics.Process]::GetCurrentProcess().ProcessorAffinity.ToInt64()
if (($invokerMask -band $mask) -ne $mask) { throw 'Required logical CPUs 2 and 6 are unavailable.' }
$watch = [Diagnostics.Stopwatch]::StartNew()
$scenarioWatch = $null
$processes = [Collections.Generic.List[object]]::new()
$workers = [Collections.Generic.List[object]]::new()
$environmentNames = @('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')
$prior = @{}
$result = [ordered]@{passed=$false;head=$head;sourceTree=$source;dllSHA256=(Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash;
    runToken=$token;runDirectory=$run;dirtyWorkingTree=$dirty;startedUtc=[DateTime]::UtcNow.ToString('o');finishedUtc=$null;
    warmupTimeoutSeconds=$WarmupTimeoutSeconds;scenarioTimeoutSeconds=$TimeoutSeconds;elapsedSeconds=0;scenarioElapsedSeconds=0;
    topology='dedicated NullRHI host, offscreen rendered owner1, NullRHI owner2 observer';physicalInputVerified=$false;
    cpuPolicy=[ordered]@{mask='0x44';priority='Normal';admission=@();polls=0;repairs=0;owned=@();workerOwnershipVerified=$false};
    storage=[ordered]@{cache=$cache;temp=$temp;maxCacheMiB=$MaxCacheMiB;maxTempMiB=$MaxTempMiB;checks=0;peakCacheBytes=0;peakTempBytes=0};
    engine=@{root=$EngineRoot;port=$Port;literalClientCvars=$cvars};logs=@{};evidence=$null;error=$null;cleanupErrors=@()}
$lastStorage = -10.0; $lastPolicy = -1.0
function Admit-AreaGuardCpu([string]$Stage) {
    $samples = @()
    for($index=0;$index -lt 3;$index++) {
        $rows = @(Get-CimInstance Win32_PerfFormattedData_PerfOS_Processor -ErrorAction Stop |
            Where-Object { $_.Name -in @('_Total','2','6') })
        $reading = @{}
        foreach($row in $rows) { $reading[[string]$row.Name] = [double]$row.PercentProcessorTime }
        if($reading.Count -ne 3 -or @($reading.Values|Where-Object { -not [double]::IsFinite($_) -or $_ -ge 80 -or $_ -lt 0 }).Count) {
            throw "Fresh CPU admission failed for $Stage sample $index."
        }
        $samples += [ordered]@{utc=[DateTime]::UtcNow.ToString('o');aggregate=$reading._Total;cpu2=$reading['2'];cpu6=$reading['6']}
        if($index -lt 2) { Start-Sleep -Seconds 1 }
    }
    $conflicts = @(Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
        $_.Name -in @('UnrealEditor.exe','UnrealEditor-Cmd.exe','ShaderCompileWorker.exe','UnrealBuildTool.exe','MSBuild.exe') -and
        $_.ProcessId -notin @($processes|ForEach-Object{$_.Process.Id}) -and
        $_.ProcessId -notin @($workers|ForEach-Object{$_.Process.Id})
    })
    if($conflicts.Count) { throw "Competing UE/build process during $Stage admission." }
    $result.cpuPolicy.admission += [ordered]@{stage=$Stage;samples=$samples}
}
function Get-OwnedAreaGuardProcess($entry) {
    $live = Get-Process -Id $entry.Process.Id -ErrorAction SilentlyContinue
    if(-not $live) { return $null }
    try { if($live.StartTime -eq $entry.Started -and [string]::Equals($live.Path,$entry.Path,[StringComparison]::OrdinalIgnoreCase)) { return $live } }
    catch { if($live.HasExited) { return $null }; throw }
    return $null
}
function Maintain-AreaGuardPolicy([switch]$Force) {
    if(-not $Force -and $watch.Elapsed.TotalSeconds-$script:lastPolicy -lt 1) { return }
    $script:lastPolicy = $watch.Elapsed.TotalSeconds
    $result.cpuPolicy.polls++
    $parents = @{}
    foreach($entry in $processes) {
        $live = Get-OwnedAreaGuardProcess $entry
        if(-not $live) { continue }
        $parents[$live.Id]=$entry
        $beforeMask=$live.ProcessorAffinity.ToInt64(); $beforePriority=$live.PriorityClass.ToString()
        if($beforeMask -ne $mask){$live.ProcessorAffinity=[IntPtr]$mask}
        if($beforePriority -ne 'Normal'){$live.PriorityClass=[Diagnostics.ProcessPriorityClass]::Normal}
        $live.Refresh()
        if($live.ProcessorAffinity.ToInt64() -ne $mask -or $live.PriorityClass -ne 'Normal'){throw "Owned $($entry.Role) CPU policy failure."}
        if($beforeMask -ne $mask -or $beforePriority -ne 'Normal'){$result.cpuPolicy.repairs++}
    }
    if($parents.Count) {
        $candidates = @(Get-CimInstance Win32_Process -Filter "Name='ShaderCompileWorker.exe'" -ErrorAction Stop)
        foreach($candidate in $candidates) {
            $parentId=[int]$candidate.ParentProcessId
            if(-not $parents.ContainsKey($parentId)){continue}
            $child=Get-Process -Id ([int]$candidate.ProcessId) -ErrorAction SilentlyContinue
            if(-not $child){continue}
            if(-not [string]::Equals($child.Path,$workerExe,[StringComparison]::OrdinalIgnoreCase) -or
                $child.StartTime -lt $parents[$parentId].Started -or
                -not $candidate.CommandLine -or $candidate.CommandLine.Replace('/','\').IndexOf($project,[StringComparison]::OrdinalIgnoreCase) -lt 0) {
                throw 'Cannot verify owned shader worker metadata.'
            }
            if(-not @($workers|Where-Object{$_.Process.Id -eq $child.Id -and $_.Started -eq $child.StartTime}).Count) {
                $workers.Add([pscustomobject]@{Process=$child;Started=$child.StartTime;Role='shader-worker';Path=$workerExe})
                $result.cpuPolicy.owned += @{role='shader-worker';pid=$child.Id;parentPid=$parentId;startUtc=$child.StartTime.ToUniversalTime().ToString('o')}
            }
        }
        $result.cpuPolicy.workerOwnershipVerified=$true
    }
    foreach($entry in $workers) {
        $live=Get-OwnedAreaGuardProcess $entry
        if(-not $live){continue}
        if($live.ProcessorAffinity.ToInt64() -ne $mask){$live.ProcessorAffinity=[IntPtr]$mask;$result.cpuPolicy.repairs++}
        if($live.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::Normal){$live.PriorityClass=[Diagnostics.ProcessPriorityClass]::Normal;$result.cpuPolicy.repairs++}
        $live.Refresh()
        if($live.ProcessorAffinity.ToInt64() -ne $mask -or $live.PriorityClass -ne 'Normal'){throw 'Owned worker CPU policy failure.'}
    }
}
function Check-AreaGuardStorage([switch]$Force) {
    if(-not $Force -and $watch.Elapsed.TotalSeconds-$script:lastStorage -lt 10){return}
    $script:lastStorage=$watch.Elapsed.TotalSeconds
    $cacheBytes=[int64](Get-ChildItem -LiteralPath $cache -File -Recurse -Force|Measure-Object Length -Sum).Sum
    $tempBytes=[int64](Get-ChildItem -LiteralPath $temp -File -Recurse -Force|Measure-Object Length -Sum).Sum
    $result.storage.checks++
    $result.storage.peakCacheBytes=[math]::Max($result.storage.peakCacheBytes,$cacheBytes)
    $result.storage.peakTempBytes=[math]::Max($result.storage.peakTempBytes,$tempBytes)
    if($cacheBytes -gt $MaxCacheMiB*1MB -or $tempBytes -gt $MaxTempMiB*1MB){throw 'D: cache/temp soft guard exceeded.'}
}
function Start-AreaGuardProcess([string]$Role,[string]$Destination,[string[]]$Mode,[int]$CoreLimit=2) {
    $log=Join-Path $run ($Role+'.log')
    $arguments=@(('"'+$project+'"'),$Destination)+$Mode+@('-unattended','-nosound','-nosplash','-NoSourceControl','-NoP4',
        '-ini:GameUserSettings:[/Script/Engine.GameUserSettings]:FrameRateLimit=30','-ddc=BlockoutDDC',
        ('-CoreLimit='+$CoreLimit),('-port='+$Port),('-abslog="'+$log+'"'),
        '-ini:Engine:[DevOptions.Shaders]:WorkerProcessPriority=0,[ConsoleVariables]:t.MaxFPS=30')
    if($Role -in @('rendered','observer')){$arguments+=('-cvarsini="'+$cvars+'"')}
    $process=Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    $entry=[pscustomobject]@{Process=$process;Started=$process.StartTime;Role=$Role;Path=$editor;Log=$log}
    $processes.Add($entry)
    $process.ProcessorAffinity=[IntPtr]$mask
    $process.PriorityClass=[Diagnostics.ProcessPriorityClass]::Normal
    $process.Refresh()
    if($process.ProcessorAffinity.ToInt64() -ne $mask -or $process.PriorityClass -ne 'Normal'){throw "Cannot apply $Role CPU policy."}
    $result.cpuPolicy.owned += @{role=$Role;pid=$process.Id;startUtc=$entry.Started.ToUniversalTime().ToString('o')}
    return $entry
}
function Read-AreaGuardLog($entry) { if(Test-Path -LiteralPath $entry.Log){return (Get-Content -LiteralPath $entry.Log -Raw)};return '' }
function Check-AreaGuardProgress([switch]$AllowExit) {
    Maintain-AreaGuardPolicy
    Check-AreaGuardStorage
    $limit=if($scenarioWatch){$TimeoutSeconds}else{$WarmupTimeoutSeconds}
    $elapsed=if($scenarioWatch){$scenarioWatch.Elapsed.TotalSeconds}else{$watch.Elapsed.TotalSeconds}
    if($elapsed -ge $limit){throw 'AreaGuard bounded stage timed out.'}
    foreach($entry in $processes) {
        $log=Read-AreaGuardLog $entry
        if($log -match 'PROOF_AREA_GUARD_(FIXTURE|OBSERVATION)_ERROR|Fatal error:|Assertion failed:|Unhandled Exception:'){
            throw "$($entry.Role) reported a runtime error."
        }
        if($entry.Process.HasExited -and $entry.Process.ExitCode -ne 0){throw "$($entry.Role) exited $($entry.Process.ExitCode)."}
        if(-not $AllowExit -and $entry.Process.HasExited){throw "$($entry.Role) exited early."}
    }
}
function Wait-AreaGuardMarker($entry,[string]$Pattern) {
    while($true) {
        Check-AreaGuardProgress
        if((Read-AreaGuardLog $entry) -match $Pattern){return}
        Start-Sleep -Milliseconds 200
    }
}
try {
    foreach($name in $environmentNames){$prior[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$cache,'Process')
    [Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process')
    [Environment]::SetEnvironmentVariable('TEMP',$temp,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$temp,'Process')
    Check-AreaGuardStorage -Force
    Admit-AreaGuardCpu 'warmup'
    $warmup=Start-AreaGuardProcess 'warmup' '/Game/CommandProofGenerated/Maps/CommandProof' @('-game','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ProofGraphicsWarmup') 4
    while(-not $warmup.Process.WaitForExit(200)) { Check-AreaGuardProgress -AllowExit }
    if($warmup.Process.ExitCode -ne 0 -or (Read-AreaGuardLog $warmup) -notmatch 'PROOF_GRAPHICS_WARMUP_COMPLETE status=pass'){
        throw 'Graphics warmup failed or lacked completion evidence.'
    }
    $scenarioWatch=[Diagnostics.Stopwatch]::StartNew()
    Admit-AreaGuardCpu 'fixture'
    $common=@('-ProofSkirmish','-ProofAreaGuardObserve',('-ProofAreaGuardRun='+$token),
        ('-ProofAreaGuardSource='+$source),('-ProofAreaGuardRunDirectory="'+$run+'"'))
    $hostEntry=Start-AreaGuardProcess 'host' '/Game/CommandProofGenerated/Maps/CommandProof' (@('-server','-nullrhi','-MULTIHOME=127.0.0.1','-ProofAreaGuardFixtureHost')+$common)
    Wait-AreaGuardMarker $hostEntry 'PROOF_SKIRMISH_READY\b'
    $renderedEntry=Start-AreaGuardProcess 'rendered' ('127.0.0.1:'+$Port) (@('-game','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ProofAreaGuardFixture')+$common)
    Wait-AreaGuardMarker $hostEntry 'PROOF_SKIRMISH_PLAYER owner=1\b'
    $observerEntry=Start-AreaGuardProcess 'observer' ('127.0.0.1:'+$Port) (@('-game','-nullrhi','-ProofAreaGuardFixture')+$common)
    while(@(@($hostEntry,$renderedEntry,$observerEntry) | Where-Object { -not $_.Process.HasExited }).Count -gt 0) {
        Check-AreaGuardProgress -AllowExit
        Start-Sleep -Milliseconds 200
    }
    if($scenarioWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds){throw 'Completion first observed at strict scenario timeout.'}
    $logs=@{host=(Read-AreaGuardLog $hostEntry);rendered=(Read-AreaGuardLog $renderedEntry);observer=(Read-AreaGuardLog $observerEntry)}
    foreach($role in @('host','rendered','observer')){$result.logs[$role]=Join-Path $run ($role+'.log')}
    $result.evidence=Get-AreaGuardObservation -Logs $logs -RunId $token -SourceTree $source -RequireRuntimeMarkers
    if(-not $result.evidence.passed){throw 'AreaGuard strict evidence did not pass.'}
    $result.passed=$true
} catch { $result.error=$_.Exception.Message }
finally {
    foreach($entry in @($processes)+@($workers)) {
        try {
            $live=Get-OwnedAreaGuardProcess $entry
            if($live) { Stop-Process -Id $live.Id -Force -ErrorAction Stop; [void]$live.WaitForExit(5000) }
        } catch { $result.cleanupErrors+=@{role=$entry.Role;pid=$entry.Process.Id;message=$_.Exception.Message};$result.passed=$false }
    }
    foreach($name in $environmentNames){if($prior.ContainsKey($name)){[Environment]::SetEnvironmentVariable($name,$prior[$name],'Process')}}
    $result.finishedUtc=[DateTime]::UtcNow.ToString('o')
    $result.elapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3)
    if($scenarioWatch){$result.scenarioElapsedSeconds=[math]::Round($scenarioWatch.Elapsed.TotalSeconds,3)}
    $result | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8
}
if(-not $result.passed){throw "$($result.error) See $run\result.json"}
Write-Output (Join-Path $run 'result.json')
