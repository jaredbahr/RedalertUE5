param(
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [ValidateRange(60,1800)][int]$SessionSeconds = 900,
    [switch]$Skirmish,
    [switch]$CameraPan,
    [switch]$CameraEdgePan,
    [string]$StorageRoot = $env:RA2_STORAGE_ROOT
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'local-interactive-paths.ps1')
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$localPaths=Resolve-ProofInteractivePaths -RepoRoot $repoRoot -EngineRoot $EngineRoot -StorageRoot $StorageRoot
$repoRoot=$localPaths.RepoRoot
$projectRoot=$localPaths.ProjectRoot
$project=Join-Path $projectRoot 'RedalertProof.uproject'
$editor=$localPaths.Editor
foreach($path in @($editor,(Join-Path $projectRoot 'Binaries\Win64\UnrealEditor-RedalertProof.dll'),(Join-Path $projectRoot 'Content\CommandProofGenerated\Maps\CommandProof.umap'))) {
    if(-not(Test-Path -LiteralPath $path)) { throw "Missing prepared local file: $path" }
}
$head=(& git -C $repoRoot rev-parse HEAD).Trim()
if($LASTEXITCODE -ne 0 -or [bool](& git -C $repoRoot status --porcelain)) { throw 'Commit the prepared source before launching the interactive session.' }
$run=Join-Path $repoRoot ('generated\interactive-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
$cache=Join-Path $repoRoot 'generated\controls-cache'
$temp=Join-Path $run 'temp'
New-Item -ItemType Directory -Force -Path $run,$cache,$temp | Out-Null
$available=[Diagnostics.Process]::GetCurrentProcess().ProcessorAffinity.ToInt64()
$mask=[int64]0; $count=0
for($cpu=0; $cpu -lt 63 -and $count -lt 2; $cpu++) {
    $bit=[int64]1 -shl $cpu
    if(($available -band $bit) -ne 0) { $mask=$mask -bor $bit; $count++ }
}
if($count -ne 2) { throw 'Two available logical CPUs are required.' }
$names=@('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')
$prior=@{}; $process=$null; $started=$null
$result=[ordered]@{head=$head;startedUtc=[DateTime]::UtcNow.ToString('o');finishedUtc=$null;mode='standalone local practice';runDirectory=$run;log=(Join-Path $run 'game.log');physicalInputVerified=$false;humanConfirmationRequired=$true;networkTest=$false;sessionLimitSeconds=$SessionSeconds;affinityMask=$mask;priority='Normal';policyPolls=0;affinityRepairs=0;peakObservedWorkingSetBytes=[int64]0;lastObservedCpuSeconds=0;windowTitle=$null;windowHandle=$null;exitCode=$null;expired=$false;error=$null}
try {
    foreach($name in $names) { $prior[$name]=[Environment]::GetEnvironmentVariable($name,'Process') }
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$cache,'Process')
    [Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process')
    [Environment]::SetEnvironmentVariable('TEMP',$temp,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$temp,'Process')
    $launchArguments=@(('"'+$project+'"'),'/Game/CommandProofGenerated/Maps/CommandProof','-game','-windowed','-ResX=1280','-ResY=720','-nosound','-nosplash','-unattended','-NoSourceControl','-NoP4','-ddc=BlockoutDDC','-CoreLimit=2','-ProofInteractive','-ExecCmds="DisableAllScreenMessages"','-ini:GameUserSettings:[/Script/Engine.GameUserSettings]:FrameRateLimit=30','-ini:Engine:[AssetRegistry]:EditorGameScansAR=Async,[ConsoleVariables]:t.MaxFPS=30,[DevOptions.Shaders]:WorkerProcessPriority=0,[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=1,[DevOptions.Shaders]:NumUnusedShaderCompilingThreadsDuringGame=1',('-abslog="'+$result.log+'"'))
    if($Skirmish) { $launchArguments+='-ProofSkirmish' }
    if($CameraPan) { $launchArguments+='-ProofCameraPan' }
    if($CameraEdgePan) { $launchArguments+='-ProofCameraEdgePan' }
    $result.cameraPanEnabled=[bool]$CameraPan
    $result.cameraEdgePanEnabled=[bool]$CameraEdgePan
    Write-Output 'Opening the Red Alert local practice window in 5 seconds. Left-click select, right-click move/attack, P produce, S stop selected mobile units, wheel zoom, Escape exit.'
    if($Skirmish) { Write-Output 'Small match: R refinery at cursor; F factory at cursor; select Harvester/right-click ore; P produces at factory. Destroy enemy HQ.' }
    Write-Output 'T selects visible owned units of the selected generic kind; Shift-click toggles; Ctrl+1-9 saves a group; 1-9 recalls. Group ground orders move mobile units; enemy orders attack with combat units.'
    if($Skirmish) { Write-Output 'Ctrl+Shift+right-click ground: Attack Move with selected Combat units; S stops the route. In-range acquisition only; no pursuit outside weapon range.' }
    if($CameraPan) { Write-Output 'Optional camera pan enabled (-CameraPan): arrow keys while the game viewport is focused and foreground.' }
    if($CameraEdgePan) { Write-Output 'Optional edge pan enabled (-CameraEdgePan): outer 16 viewport pixels; paused over the HUD, while a mouse button is held, or without viewport focus/foreground.' }
    Start-Sleep -Seconds 5
    $process=Start-Process -FilePath $editor -ArgumentList $launchArguments -WorkingDirectory $projectRoot -WindowStyle Normal -PassThru
    $started=$process.StartTime
    if(-not $process.HasExited) {
        $initial=Get-Process -Id $process.Id -ErrorAction Stop
        if($initial.StartTime -ne $started -or $initial.Path -ne $editor) { throw 'Interactive process identity changed at launch.' }
        $initial.ProcessorAffinity=[IntPtr]$mask
        $initial.PriorityClass='Normal'
        $initial.Refresh()
        if($initial.ProcessorAffinity.ToInt64() -ne $mask -or $initial.PriorityClass -ne 'Normal') { throw 'Initial interactive CPU policy failed.' }
        $result.policyPolls++
    }
    $watch=[Diagnostics.Stopwatch]::StartNew()
    $lastStorageCheck=-10.0
    $result.storageGuard=[ordered]@{pollingOnly=$true;maxCacheMiB=2048;maxTempMiB=1024;peakCacheBytes=[int64]0;peakTempBytes=[int64]0}
    while(-not $process.WaitForExit(200)) {
        if($watch.Elapsed.TotalSeconds - $lastStorageCheck -ge 10) {
            $lastStorageCheck=$watch.Elapsed.TotalSeconds
            $cacheBytes=[int64](Get-ChildItem -LiteralPath $cache -File -Recurse -ErrorAction Stop | Measure-Object Length -Sum).Sum
            $tempBytes=[int64](Get-ChildItem -LiteralPath $temp -File -Recurse -ErrorAction Stop | Measure-Object Length -Sum).Sum
            $result.storageGuard.peakCacheBytes=[math]::Max([int64]$result.storageGuard.peakCacheBytes,$cacheBytes)
            $result.storageGuard.peakTempBytes=[math]::Max([int64]$result.storageGuard.peakTempBytes,$tempBytes)
            if($cacheBytes -gt 2048MB -or $tempBytes -gt 1024MB) { throw 'Interactive cache/temp soft limit exceeded.' }
        }
        if($watch.Elapsed.TotalSeconds -ge $SessionSeconds) { $result.expired=$true; break }
        try {
            $live=Get-Process -Id $process.Id -ErrorAction Stop
            if($live.StartTime -ne $started -or $live.Path -ne $editor) { throw 'Interactive process identity changed.' }
            if($live.ProcessorAffinity.ToInt64() -ne $mask -or $live.PriorityClass -ne 'Normal') { $result.affinityRepairs++ }
            $live.ProcessorAffinity=[IntPtr]$mask
            $live.PriorityClass='Normal'
            $live.Refresh()
            if($live.ProcessorAffinity.ToInt64() -ne $mask -or $live.PriorityClass -ne 'Normal') { throw 'Interactive CPU policy verification failed.' }
            $result.policyPolls++
            $result.peakObservedWorkingSetBytes=[math]::Max([int64]$result.peakObservedWorkingSetBytes,[int64]$live.PeakWorkingSet64)
            $result.lastObservedCpuSeconds=$live.CPU
            if($live.MainWindowHandle -ne 0) { $result.windowTitle=$live.MainWindowTitle; $result.windowHandle=$live.MainWindowHandle.ToInt64() }
        } catch { if(-not $process.HasExited) { throw } }
    }
    if($process.HasExited) { $result.exitCode=$process.ExitCode }
} catch { $result.error=$_.Exception.Message }
finally {
    if($process -and -not $process.HasExited) {
        $live=Get-Process -Id $process.Id -ErrorAction SilentlyContinue
        if($live -and $live.StartTime -eq $started -and $live.Path -eq $editor) { Stop-Process -Id $process.Id -Force }
    }
    foreach($name in $names) { if($prior.ContainsKey($name)) { [Environment]::SetEnvironmentVariable($name,$prior[$name],'Process') } }
    $result.finishedUtc=[DateTime]::UtcNow.ToString('o')
    $result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'session.json') -Encoding utf8
}
if($result.error) { throw $result.error }
Write-Output (Join-Path $run 'session.json')
