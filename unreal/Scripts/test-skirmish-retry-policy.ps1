# Synthetic ownership-retry policy only; no Unreal or OS process work.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'SkirmishRetryEvidence.ps1')
function New-RetryLog {
    $live=@{role='client';viewer=1;epoch=1;revision=9;phase=1;units=@(@{id=20;owner=1;hp=2.5})}|ConvertTo-Json -Depth 5 -Compress
    $dead=@{role='client';viewer=1;epoch=1;revision=12;phase=1;units=@(@{id=20;owner=1;hp=0})}|ConvertTo-Json -Depth 5 -Compress
    return (@(
        'PROOF_REFINERY_STATE '+$live
        'PROOF_SKIRMISH_OWNERSHIP_REJECTION owner=1 sequence=96 unit=20 exists=1 unitOwner=1 hp=0 revision=10 epoch=1 type=Attack reason=ownership'
        'PROOF_SNAPSHOT role=host owner=0 epoch=1 revision=12 digest=0123456789abcdef0123456789abcdef'
        'PROOF_SNAPSHOT role=client owner=1 epoch=1 revision=12 digest=0123456789abcdef0123456789abcdef'
        'PROOF_REFINERY_STATE '+$dead
        'PROOF_SKIRMISH_RETRY reason=ownership unit=20 sequence=96 snapshotRevision=12 replicatedUnit=owned-dead'
    ) -join [Environment]::NewLine)
}
$checks=0
if((Get-SkirmishOwnershipRetryEvidence -Log 'No retry').Count -ne 0){throw 'No-retry case failed'};$checks++
$r=Get-SkirmishOwnershipRetryEvidence -Log (New-RetryLog) -RequireSemanticEvidence
if($r.Count -ne 1 -or $r[0].unit -ne 20 -or -not $r[0].semanticEvidenceRequired){throw 'Valid semantic case failed'};$checks++
$r=Get-SkirmishOwnershipRetryEvidence -Log ((New-RetryLog) -split '\r?\n'|Where-Object {$_ -notmatch '^PROOF_REFINERY_STATE'}|Join-String -Separator ([Environment]::NewLine))
if($r.Count -ne 1 -or $r[0].semanticEvidenceRequired){throw 'Ordinary no-semantic mode failed'};$checks++
function Reject([string]$Name,[scriptblock]$Edit) {
    $text=& $Edit (New-RetryLog);$bad=$false
    try{Get-SkirmishOwnershipRetryEvidence -Log $text -RequireSemanticEvidence|Out-Null}catch{$bad=$true}
    if(-not $bad){throw "Incorrect acceptance: $Name"};$script:checks++
}
Reject 'live at validation' {param($x)$x.Replace('unitOwner=1 hp=0','unitOwner=1 hp=2.5')}
Reject 'foreign validation unit' {param($x)$x.Replace('unitOwner=1 hp=0','unitOwner=2 hp=0')}
Reject 'missing validation unit' {param($x)$x.Replace('exists=1','exists=0')}
Reject 'wrong source id' {param($x)$x.Replace('sequence=96 unit=20','sequence=96 unit=21')}
Reject 'wrong receipt id' {param($x)$x.Replace('sequence=96 unit=20','sequence=97 unit=20')}
Reject 'malformed diagnostic' {param($x)$x.Replace('type=Attack','type=Move')}
Reject 'before rejection revision' {param($x)$x.Replace('snapshotRevision=12','snapshotRevision=8')}
Reject 'duplicate diagnostic' {param($x)$x+[Environment]::NewLine+'PROOF_SKIRMISH_OWNERSHIP_REJECTION owner=1 sequence=96 unit=20 exists=1 unitOwner=1 hp=0 revision=10 epoch=1 type=Attack reason=ownership'}
Reject 'duplicate retry' {param($x)$x+[Environment]::NewLine+'PROOF_SKIRMISH_RETRY reason=ownership unit=20 sequence=96 snapshotRevision=12 replicatedUnit=owned-dead'}
Reject 'missing death publication' {param($x)($x -split '\r?\n'|Where-Object {$_ -notmatch '^PROOF_REFINERY_STATE.*"revision":12'}) -join [Environment]::NewLine}
Reject 'no prior positive client unit' {param($x)($x -split '\r?\n'|Where-Object {$_ -notmatch '^PROOF_REFINERY_STATE.*"revision":9'}) -join [Environment]::NewLine}
Reject 'terminal teardown cannot prove death' {param($x)$x.Replace('"phase":1','"phase":2')}
Reject 'cross epoch evidence' {param($x)$x.Replace('"epoch":1','"epoch":2')}
Reject 'ordinary host/client mismatch' {param($x)$x.Replace('role=client owner=1 epoch=1 revision=12 digest=0123456789abcdef0123456789abcdef','role=client owner=1 epoch=1 revision=12 digest=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')}
Reject 'diagnostic overflow' {param($x)$x+[Environment]::NewLine+'PROOF_SKIRMISH_OWNERSHIP_REJECTION_ERROR reason=diagnostic-limit'}
Reject 'zero sequence both rows' {param($x)$x.Replace('sequence=96','sequence=0')}
Reject 'zero unit both rows' {param($x)$x.Replace('unit=20','unit=0')}
Reject 'sequence above envelope range' {param($x)$x.Replace('sequence=96','sequence=2147483648')}
Reject 'zero diagnostic epoch' {param($x)$x.Replace('epoch=1 type=Attack','epoch=0 type=Attack')}
Write-Output "PASS synthetic-only ownership retry cases=$checks; no runtime recovery evidence"
