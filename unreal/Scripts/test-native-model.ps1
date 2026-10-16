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
$run=Join-Path $repoRoot ('generated\native-model-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
$report=Join-Path $run 'report'
New-Item -ItemType Directory -Force -Path $run,$report,(Join-Path $run 'cache'),(Join-Path $run 'temp') | Out-Null
$names=@('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')
$prior=@{}; $process=$null; $started=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$result=[ordered]@{head=(& git -C $repoRoot rev-parse HEAD).Trim();dirtyWorkingTree=[bool](& git -C $repoRoot status --porcelain);startedUtc=[DateTime]::UtcNow.ToString('o');finishedUtc=$null;elapsedSeconds=0;passed=$false; testCount=0; succeeded=0; failed=0; runDirectory=$run; error=$null}
try {
    foreach($n in $names){$prior[$n]=[Environment]::GetEnvironmentVariable($n,'Process')}
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',(Join-Path $run 'cache'),'Process')
    [Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process')
    [Environment]::SetEnvironmentVariable('TEMP',(Join-Path $run 'temp'),'Process')
    [Environment]::SetEnvironmentVariable('TMP',(Join-Path $run 'temp'),'Process')
    $launchArguments=@(('"'+$project+'"'),'-unattended','-nullrhi','-nosound','-nosplash','-NoSourceControl','-NoP4','-ddc=BlockoutDDC','-CoreLimit=2','-ExecCmds="Automation RunTests RedalertProof.Commands"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+$report+'"'),('-abslog="'+(Join-Path $run 'automation.log')+'"'))
    $process=Start-Process -FilePath $editor -ArgumentList $launchArguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    $started=$process.StartTime
    if (-not $process.WaitForExit($TimeoutSeconds*1000)) { throw 'Native model automation timed out.' }
    if ($process.ExitCode -ne 0) { throw "Editor exited $($process.ExitCode)." }
    $index=Join-Path $report 'index.json'
    if (-not (Test-Path -LiteralPath $index)) { throw 'No fresh Automation index.json report.' }
    $data=Get-Content -LiteralPath $index -Raw | ConvertFrom-Json
    $expected=@('RedalertProof.Commands.Validation','RedalertProof.Commands.ResourcesAndRate','RedalertProof.Commands.SimulationAndSnapshots','RedalertProof.Commands.PhaseOrder','RedalertProof.Commands.OwnerAllocation')
    $tests=@($data.tests)
    $result.testCount=$tests.Count; $result.succeeded=[int]$data.succeeded; $result.failed=[int]$data.failed
    if ($tests.Count -ne 5 -or [int]$data.succeeded -ne 5 -or [int]$data.failed -ne 0) { throw 'Expected exactly five successful tests and zero failures.' }
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
