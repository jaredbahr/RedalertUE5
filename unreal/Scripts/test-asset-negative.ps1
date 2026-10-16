param(
    [string]$Blender = 'C:\Program Files\Blender Foundation\Blender 4.5\blender.exe',
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7',
    [ValidateRange(10,600)][int]$ProcessTimeoutSeconds = 120,
    [ValidateRange(30,1200)][int]$OverallTimeoutSeconds = 240
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([IO.Path]::GetPathRoot($repoRoot) -ne 'D:\') { throw 'Use an isolated D: checkout.' }
$output = Join-Path $repoRoot 'generated'
$started = [DateTime]::UtcNow
$clock = [Diagnostics.Stopwatch]::StartNew()
$runDirectory = Join-Path $output ('negative-run\' + $started.ToString('yyyyMMddTHHmmssfffZ') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
$head = (& git -C $repoRoot rev-parse HEAD | Out-String).Trim()
$dirty = @(& git -C $repoRoot status --porcelain)
$result = [ordered]@{ passed=$false; head_sha=$head; dirty=($dirty.Count -gt 0); dirty_paths=$dirty;
    started_utc=$started.ToString('o'); finished_utc=$null; elapsed_seconds=0; stages=@();
    run_id=$null; source_sha256=$null; actual_cases=@(); restoration_verified=$false;
    error=$null; logs=$runDirectory; process_timeout_seconds=$ProcessTimeoutSeconds;
    overall_timeout_seconds=$OverallTimeoutSeconds }
$environmentNames = @('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')
$previous = @{}
foreach ($name in $environmentNames) { $previous[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }

function Invoke-BoundedStage([string]$Name, [string]$Executable, [string[]]$Arguments) {
    $remaining = $OverallTimeoutSeconds - $clock.Elapsed.TotalSeconds
    if ($remaining -le 0) { throw "Overall deadline reached before $Name" }
    if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) { throw "Executable missing: $Executable" }
    # All arguments are known file paths/options. Reject quotes instead of accepting ambiguous command text.
    $quoted = foreach ($argument in $Arguments) {
        if ($argument.Contains('"') -or $argument.Contains("`n") -or $argument.Contains("`r")) { throw 'Unsupported argument character' }
        '"' + $argument + '"'
    }
    $stageStarted = [DateTime]::UtcNow
    $stageClock = [Diagnostics.Stopwatch]::StartNew()
    $stage = [ordered]@{name=$Name; started_utc=$stageStarted.ToString('o'); finished_utc=$null;
        elapsed_seconds=0; pid=$null; exit_code=$null; timed_out=$false;
        stdout=(Join-Path $runDirectory ($Name + '.stdout.log'));
        stderr=(Join-Path $runDirectory ($Name + '.stderr.log'))}
    $process = $null
    try {
        $process = Start-Process -FilePath $Executable -ArgumentList ($quoted -join ' ') -WorkingDirectory $repoRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $stage.stdout -RedirectStandardError $stage.stderr
        $stage.pid = $process.Id
        $ownedStart = $process.StartTime.ToUniversalTime()
        $ownedPath = $process.MainModule.FileName
        $stageDeadlineSeconds = [Math]::Min($ProcessTimeoutSeconds, $remaining)
        while (-not $process.HasExited -and $stageClock.Elapsed.TotalSeconds -lt $stageDeadlineSeconds) {
            Start-Sleep -Milliseconds 200
        }
        if (-not $process.HasExited) {
            $stage.timed_out = $true
            # Kill only this invocation's still-identical process, never by name/tree/project.
            $current = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
            if ($current -and $current.StartTime.ToUniversalTime() -eq $ownedStart -and $current.MainModule.FileName -eq $ownedPath) {
                Stop-Process -Id $current.Id -ErrorAction Stop
                $process.WaitForExit(5000) | Out-Null
            }
            throw "Stage $Name exceeded its bounded deadline; inspect restoration state before reuse"
        }
        # HasExited already guarantees ExitCode is available. Do not wait without
        # a deadline for inherited redirect handles to drain after process exit.
        $stage.exit_code = $process.ExitCode
        if ($process.ExitCode -ne 0) { throw "Stage $Name failed with exit $($process.ExitCode)" }
    } finally {
        $stage.finished_utc = [DateTime]::UtcNow.ToString('o')
        $stage.elapsed_seconds = [Math]::Round($stageClock.Elapsed.TotalSeconds,3)
        $result.stages += [pscustomobject]$stage
        if ($process) { $process.Dispose() }
    }
}

try {
    $cache = Join-Path $output 'ue-cache'
    $temp = Join-Path $output 'ue-temp'
    New-Item -ItemType Directory -Path $cache,$temp -Force | Out-Null
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$cache,'Process')
    [Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process')
    [Environment]::SetEnvironmentVariable('TEMP',$temp,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$temp,'Process')
    foreach ($step in @(@('generation','build_blockout.py'),@('fbx-validation','validate_exports.py'),@('fixture-preparation','prepare_negative_fixtures.py'))) {
        Invoke-BoundedStage $step[0] $Blender @('--background','--factory-startup','--threads','4','--python-exit-code','1','--python',(Join-Path $repoRoot ('tools\art\' + $step[1])),'--','--output',$output)
    }
    $editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    Invoke-BoundedStage 'ue-negative-probe' $editor @((Join-Path $repoRoot 'unreal\RedalertProof.uproject'),'-unattended','-nullrhi','-NoSound','-NoSplash','-NoSourceControl','-NoP4','-CoreLimit=4','-ddc=BlockoutDDC','-stdout','-FullStdOutLogOutput',('-abslog=' + (Join-Path $runDirectory 'ue.log')),'-run=pythonscript',('-script=' + (Join-Path $PSScriptRoot 'probe_negative_assets.py')))
    $reportPath = Join-Path $output 'ue_negative_probe.json'
    if ((Get-Item -LiteralPath $reportPath).LastWriteTimeUtc -lt $started) { throw 'Negative-probe report predates runner' }
    $proof = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    $manifest = Get-Content -LiteralPath (Join-Path $output 'blockout_manifest.json') -Raw | ConvertFrom-Json
    $result.run_id = $manifest.run_id
    $result.source_sha256 = $manifest.source_sha256
    $result.actual_cases = @($proof.cases)
    $result.restoration_verified = ($proof.restored_original_inputs_and_colors -eq $true)
    if ($proof.passed -ne $true -or -not $result.restoration_verified -or $proof.run_id -ne $manifest.run_id -or $proof.source_sha256 -ne $manifest.source_sha256) { throw 'Current proof identity/restoration check failed' }
    $caseNames = @($proof.cases | ForEach-Object { $_.case } | Sort-Object)
    if (($caseNames -join ',') -ne 'palette_change,pivot_z_offset,vertex_z_offset' -or @($proof.cases | Where-Object { $_.passed -ne $true }).Count) { throw 'Expected exactly three successful negative/material cases' }
    if ((& git -C $repoRoot rev-parse HEAD | Out-String).Trim() -ne $head) { throw 'HEAD changed during run' }
    $result.passed = $true
} catch {
    $result.error = $_.Exception.Message
} finally {
    foreach ($name in $environmentNames) { [Environment]::SetEnvironmentVariable($name,$previous[$name],'Process') }
    $result.finished_utc = [DateTime]::UtcNow.ToString('o')
    $result.elapsed_seconds = [Math]::Round($clock.Elapsed.TotalSeconds,3)
    $manifestPath = Join-Path $output 'blockout_manifest.json'
    if (-not $result.run_id -and (Test-Path -LiteralPath $manifestPath)) {
        try {
            $currentManifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            $result.run_id=$currentManifest.run_id; $result.source_sha256=$currentManifest.source_sha256
        } catch { }
    }
    $result | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath (Join-Path $runDirectory 'result.json') -Encoding utf8
}
Write-Output ('ASSET_NEGATIVE_RESULT=' + (Join-Path $runDirectory 'result.json'))
if (-not $result.passed) { throw $result.error }
