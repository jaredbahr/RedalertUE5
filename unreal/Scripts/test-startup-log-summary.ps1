#requires -Version 7.0
param()
$ErrorActionPreference = 'Stop'
$parser = Join-Path $PSScriptRoot 'summarize-startup-logs.ps1'
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('proof-startup-summary-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
$assertions = 0
function Assert-Policy([bool]$Condition,[string]$Message) {
    if (-not $Condition) { throw $Message }
    $script:assertions++
}
function Write-Result($Processes) {
    @{processes=@($Processes)} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $fixtureRoot 'result.json') -Encoding utf8
}
try {
    Write-Result @(@{role='host';startUtc='2026-10-17T02:00:00+02:00'},@{role='lead';startUtc='2026-10-17T00:00:01Z'},@{role='owner2';startUtc='2026-10-17T00:00:02Z'})
    @'
untimestamped preamble
[2026.10.17-00.00.02:000][  0]LogInit: first
[2026.10.17-00.00.03:000][  0]LogConfig: step
[2026.10.17-00.00.05:000][  0]LogTemp: Display: PROOF_LIFECYCLE_READY epoch=1
[2026.10.17-00.00.08:000][  0]LogLoad: (Engine Initialization) Total time: 8.00 seconds
[2026.10.17-00.00.09:000][  1]LogTemp: Display: PROOF_LIFECYCLE_JOIN owner=1 epoch=1 seeded=1
[2026.10.17-01.00.00:000][999]LogTemp: quiet-after-startup
'@ | Set-Content -LiteralPath (Join-Path $fixtureRoot 'host.log') -Encoding utf8
    @'
[2026.10.17-00.00.03:000][  0]LogInit: first
[2026.10.17-00.00.06:000][  0]LogLoad: (Engine Initialization) Total time: 5.00 seconds
'@ | Set-Content -LiteralPath (Join-Path $fixtureRoot 'lead.log') -Encoding utf8
    $summary = & $parser -RunDirectory $fixtureRoot -MaxGaps 2 | ConvertFrom-Json
    $hostRole = $summary.roles[0]
    $leadRole = $summary.roles[1]
    Assert-Policy ($hostRole.seconds.processToFirstLog -eq 2) 'Explicit process offset must be converted to UTC.'
    Assert-Policy ($hostRole.firstDatedLog.line -eq 2) 'Preamble must retain original one-based line numbering.'
    Assert-Policy ($hostRole.seconds.firstLogToInit -eq 6) 'First-log to initialization interval is wrong.'
    Assert-Policy ($null -eq $hostRole.seconds.initToEndpoint) 'Readiness before init must not produce a negative duration.'
    Assert-Policy ($hostRole.preInitGaps.Count -eq 2) 'Ranked gap count must honor the requested cap.'
    Assert-Policy ($hostRole.preInitGaps[0].seconds -eq 3) 'Ranking must exclude the later hour of normal quiet.'
    Assert-Policy ($hostRole.preInitGaps[0].beforeLine -eq 4 -and $hostRole.preInitGaps[0].afterLine -eq 5) 'Gap ranking must handle later larger gaps and retain their source lines.'
    Assert-Policy ($hostRole.log.sha256 -eq (Get-FileHash (Join-Path $fixtureRoot 'host.log') -Algorithm SHA256).Hash.ToLowerInvariant()) 'Log provenance hash must describe parsed input bytes.'
    Assert-Policy ($summary.result.sha256 -eq (Get-FileHash (Join-Path $fixtureRoot 'result.json') -Algorithm SHA256).Hash.ToLowerInvariant()) 'Result provenance hash must be retained.'
    Assert-Policy ($leadRole.seconds.initToEndpoint -eq 3 -and $leadRole.endpoint.path -eq (Join-Path $fixtureRoot 'host.log')) 'Client join must correlate the host endpoint log.'
    Assert-Policy ($summary.roles[2].missingFields -contains 'missing-file') 'A missing role log must remain explicit.'
    Assert-Policy ($summary.roles[2].preInitGaps.Count -eq 0) 'Missing initialization cannot produce ranked gaps.'

    Write-Result @(@{role='host';startUtc='2026-10-17T00:00:00'})
    $unqualified = & $parser -RunDirectory $fixtureRoot | ConvertFrom-Json
    Assert-Policy ($unqualified.roles[0].missingFields -contains 'unqualified-process-time-requires-zone') 'Unqualified local process timestamps must not inherit machine timezone.'
    Assert-Policy ($null -eq $unqualified.roles[0].seconds.processToInit) 'Unknown process timezone must leave interval unavailable.'
    $qualified = & $parser -RunDirectory $fixtureRoot -ProcessTimeZoneId UTC | ConvertFrom-Json
    Assert-Policy ($qualified.roles[0].seconds.processToInit -eq 8) 'Explicit timezone must permit local-time correlation.'

    @'
[2026.10.17-00.00.02:000][0]LogInit: first
[2026.10.17-00.00.01:000][0]LogInit: regression
[2026.10.17-00.00.03:000][0]LogLoad: (Engine Initialization) Total time: 3.00 seconds
'@ | Set-Content -LiteralPath (Join-Path $fixtureRoot 'host.log') -Encoding utf8
    $regressed = & $parser -RunDirectory $fixtureRoot -ProcessTimeZoneId UTC | ConvertFrom-Json
    Assert-Policy ($regressed.roles[0].timestampRegressions -eq 1) 'Clock regression must be visible.'
    Assert-Policy ($regressed.roles[0].preInitGaps.Count -eq 1) 'Negative log intervals must not enter ranked gaps.'
    Set-Content -LiteralPath (Join-Path $fixtureRoot 'host.log') -Value '[2026.10.17-00.00.02:000][0]LogInit: incomplete' -Encoding utf8
    $incomplete = & $parser -RunDirectory $fixtureRoot -ProcessTimeZoneId UTC | ConvertFrom-Json
    Assert-Policy ($incomplete.roles[0].missingFields -contains 'engine-init-complete') 'Missing init boundary must be explicit.'
    Assert-Policy ($incomplete.roles[0].preInitGaps.Count -eq 0) 'Incomplete startup must not rank an arbitrary window.'
    Write-Result @(@{role='unmapped-role';startUtc='2026-10-17T00:00:00Z'})
    $unmapped = & $parser -RunDirectory $fixtureRoot | ConvertFrom-Json
    Assert-Policy ($unmapped.roles[0].endpointKind -eq 'unknown' -and $null -eq $unmapped.roles[0].endpoint -and $unmapped.roles[0].missingFields -contains 'role-ready-join-or-rejection') 'Unmapped roles must report an unknown endpoint kind with explicit unavailable evidence.'
    Write-Result @(@{role='../escape';startUtc='2026-10-17T00:00:00Z'})
    $rejected = $false
    try { & $parser -RunDirectory $fixtureRoot | Out-Null } catch { $rejected = $_.Exception.Message -like '*role filename*' }
    Assert-Policy $rejected 'Role traversal must be rejected before reading a log.'
    [ordered]@{passed=$true;assertions=$assertions;engineLaunched=$false} | ConvertTo-Json
} finally {
    $resolvedFixture = [IO.Path]::GetFullPath($fixtureRoot)
    $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if (-not $resolvedFixture.StartsWith($resolvedTemp,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($resolvedFixture) -notlike 'proof-startup-summary-*') { throw 'Refusing cleanup outside the dedicated temporary fixture.' }
    Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
}
