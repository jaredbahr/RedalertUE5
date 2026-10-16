$ErrorActionPreference='Stop'
$tokens=$null; $errors=$null
$path=Join-Path $PSScriptRoot 'test-input-bindings.ps1'
$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if($errors.Count) { throw ($errors | Out-String) }
$function=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Get-InputStartupEvidence'},$true)
if(-not $function){throw 'Missing parser'}
. ([scriptblock]::Create($function.Extent.Text))
$checks=[Collections.Generic.List[string]]::new()
function Assert([bool]$condition,[string]$name) { if(-not $condition){throw $name}; $checks.Add($name) }
function Expect-Rejection([string]$log,[string]$name) {
    $rejected=$false
    try { $rejected=[bool](Get-InputStartupEvidence $log).collectionError } catch { $rejected=$true }
    Assert $rejected $name
}
function Sample([int]$index,[double]$elapsed,[int]$remaining,[int]$mask) {
    $value=[ordered]@{sample=$index;elapsedSeconds=$elapsed;lastPredicateChangeElapsedSeconds=0.0;failedMask=$mask;owner=1;epoch=1;revision=10;width=1280;height=720;remainingAssets=$remaining;managers=@(@{type='Shader';remaining=$remaining})}
    return 'PROOF_INPUT_BINDING_READINESS '+($value | ConvertTo-Json -Compress -Depth 4)
}
# Synthetic marker-free logs must never acquire invented runtime observations.
$historical=Get-InputStartupEvidence 'LogTemp: Error: fixture timed out before readiness'
Assert (-not $historical.recorded -and $historical.samples.Count -eq 0 -and $null -eq $historical.last) 'marker-free failure remains explicitly unrecorded'
Assert (-not (Get-InputStartupEvidence 'PROOF_INPUT_BINDING_COMPLETE role=client status=pass').recorded) 'legacy success marker does not invent manager observations'
$pending=Sample 1 0.0 6 16384
$ready=Sample 2 1.0 0 0
$result=Get-InputStartupEvidence ($pending+[Environment]::NewLine+$ready)
Assert ($result.recorded -and $result.samples.Count -eq 2 -and $result.last.failedMask -eq 0) 'pending-to-ready observations retained'
$result=Get-InputStartupEvidence ($pending+[Environment]::NewLine+(Sample 2 60.01 6 16384))
Assert ($result.last.failedMask -eq 16384 -and $result.last.remainingAssets -eq 6) 'timeout diagnostic preserves blocked manager'
$bootstrap=Get-InputStartupEvidence (Sample 1 0.0 0 32768)
Assert ($bootstrap.recorded -and $bootstrap.last.failedMask -eq 32768 -and -not $bootstrap.collectionError) 'uninitialized bootstrap mask is retained without becoming ready'
Expect-Rejection ($pending+[Environment]::NewLine+$pending) 'repeated sample index rejected'
Expect-Rejection ((Sample 1 2.0 6 16384)+[Environment]::NewLine+(Sample 2 1.0 0 0)) 'backwards time rejected'
Expect-Rejection 'PROOF_INPUT_BINDING_READINESS {broken' 'malformed JSON rejected'
$prefix=Get-InputStartupEvidence ($pending+[Environment]::NewLine+'PROOF_INPUT_BINDING_READINESS {broken')
Assert ($prefix.samples.Count -eq 1 -and $prefix.last.remainingAssets -eq 6 -and $prefix.collectionError) 'valid prefix survives a truncated final record without becoming a pass'
Expect-Rejection ($ready.Replace('"remainingAssets":0','"remainingAssets":2')) 'inconsistent manager sum rejected'
Expect-Rejection ($ready.Replace('"failedMask":0','"failedMask":-1')) 'negative mask rejected'
Expect-Rejection ($ready.Replace('"lastPredicateChangeElapsedSeconds":0.0','"lastPredicateChangeElapsedSeconds":2.0')) 'change time after sample rejected'
Expect-Rejection ($ready.Replace('"owner":1,','')) 'missing required field rejected'
Expect-Rejection ($ready.Replace('"type":"Shader"','"type":""')) 'missing manager type rejected'
$excess=(1..65 | ForEach-Object { Sample $_ ([double]$_) 0 0 }) -join [Environment]::NewLine
Expect-Rejection $excess 'unbounded sample count rejected'
[pscustomobject]@{passed=$true;classification='PowerShell parser-only checks; synthetic records are not UE runtime evidence';checkCount=$checks.Count;checks=$checks;utc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json -Depth 3
Write-Output ("Parser-only checks passed: "+$checks.Count)

