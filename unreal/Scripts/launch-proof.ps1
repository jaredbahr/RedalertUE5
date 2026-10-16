param(
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7',
    [switch]$Import,
    [switch]$NullRHI,
    [switch]$NativeSetup
)
$ErrorActionPreference = 'Stop'
$Import = $Import -or $NativeSetup
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path (Join-Path $projectRoot '..')).Path
if ([IO.Path]::GetPathRoot($repoRoot) -ne 'D:\') { throw 'Use a D: checkout for this proof.' }
$cache = Join-Path $repoRoot 'generated\ue-cache'
$tempPath = Join-Path $repoRoot 'generated\ue-temp'
New-Item -ItemType Directory -Force -Path $cache,$tempPath | Out-Null
$variables = @('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')
$previous = @{}
foreach ($name in $variables) { $previous[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $cache, 'Process')
    [Environment]::SetEnvironmentVariable('UE-SharedDataCachePath', 'None', 'Process')
    [Environment]::SetEnvironmentVariable('TEMP', $tempPath, 'Process')
    [Environment]::SetEnvironmentVariable('TMP', $tempPath, 'Process')
    $arguments = @((Join-Path $projectRoot 'RedalertProof.uproject'), '-ddc=BlockoutDDC', '-NoSplash', '-NoSourceControl', '-NoP4', '-stdout', '-FullStdOutLogOutput', '-NoSound', '-CoreLimit=4', ('-abslog=' + (Join-Path $repoRoot 'generated\ue-proof.log')))
    if ($NullRHI) { $arguments += '-nullrhi' }
    if ($Import) {
        $editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
        $scriptName = if ($NativeSetup) { 'setup_native_scene.py' } else { 'import_scene.py' }
        $arguments += @('-unattended', '-run=pythonscript', ('-script=' + (Join-Path $PSScriptRoot $scriptName)))
    } else { $editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe' }
    if (-not (Test-Path -LiteralPath $editor)) { throw "Editor not found: $editor" }
    $reportPath = Join-Path $repoRoot 'generated\ue_import_report.json'
    if ($Import -and (Test-Path -LiteralPath $reportPath)) { Remove-Item -LiteralPath $reportPath }
    $nativeReportPath = Join-Path $repoRoot 'generated\native_map_report.json'
    if ($NativeSetup -and (Test-Path -LiteralPath $nativeReportPath)) { Remove-Item -LiteralPath $nativeReportPath }
    & $editor @arguments
    if ($LASTEXITCODE -ne 0) { throw "Unreal exited with code $LASTEXITCODE" }
    if ($Import) {
        if (-not (Test-Path -LiteralPath $reportPath)) { throw 'Import produced no fresh completion report; inspect generated/ue-proof.log.' }
        $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
        if ($NativeSetup -and -not (Test-Path -LiteralPath $nativeReportPath)) { throw 'No fresh native map report.' }
        if ($report.assets.Count -ne 3) { throw 'Import report must contain three mesh bounds.' }
        if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'Content\BlockoutGenerated\Maps\VisualProof.umap'))) { throw 'Generated map is missing.' }
    }
} finally {
    foreach ($name in $variables) { [Environment]::SetEnvironmentVariable($name, $previous[$name], 'Process') }
}
