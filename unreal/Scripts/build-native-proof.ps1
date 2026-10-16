param([string]$EngineRoot = 'D:\Epic Games\UE_5.7')
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path (Join-Path $projectRoot '..')).Path
if ([IO.Path]::GetPathRoot($repoRoot) -ne 'D:\') { throw 'Use an isolated D: checkout.' }
$generated = Join-Path $repoRoot 'generated'
$tempPath = Join-Path $generated 'build-temp'
New-Item -ItemType Directory -Force -Path $tempPath | Out-Null
$priorTemp = $env:TEMP; $priorTmp = $env:TMP
try {
    $env:TEMP = $tempPath; $env:TMP = $tempPath
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') RedalertProofEditor Win64 Development ('-Project=' + (Join-Path $projectRoot 'RedalertProof.uproject')) -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=3 -NoUBA ('-Log=' + (Join-Path $generated 'native-build-ubt.log'))
    if ($LASTEXITCODE -ne 0) { throw "Native editor module build failed with exit $LASTEXITCODE" }
} finally { $env:TEMP = $priorTemp; $env:TMP = $priorTmp }
