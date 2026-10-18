# Synthetic production-parser regressions only; no engine or visual acceptance.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HUDCriticalTextEvidence.ps1')
$checks=0
function Accept([string]$Log,[string[]]$Names=@('one.png')) {
    $result=@(Get-HUDCriticalTextEvidence $Log $Names)
    if($result.Count -ne $Names.Count*2) { throw 'Accepted record count mismatch' }
    $script:checks++
}
function Reject([string]$Log,[string[]]$Names=@('one.png')) {
    $failed=$false
    try { Get-HUDCriticalTextEvidence $Log $Names | Out-Null } catch { $failed=$true }
    if(-not $failed) { throw 'Invalid critical evidence accepted' }
    $script:checks++
}
$economy='PROOF_HUD_CRITICAL_CAPTURE name=one.png group=economy variant=0 complete=1 width=100 available=100 height=19 maxheight=19'
$selection='PROOF_HUD_CRITICAL_CAPTURE name=one.png group=selection variant=1 complete=1 width=90 available=100 height=18 maxheight=19'
$valid=$economy+"`n"+$selection
Accept $valid
Accept ($valid.Replace("`n","`r`n"))
Accept ($valid+"`n"+$valid.Replace('one.png','two.png')) @('one.png','two.png')
Accept ($valid.Replace('width=100','width=99.99999999999999'))
Accept ($valid.Replace('width=100','width=1e2'))
foreach($field in @('width','available','height','maxheight')) {
    foreach($bad in @('NaN','Infinity','-Infinity','0','-1','oops')) {
        Reject ([regex]::Replace($valid,('\b'+$field+'=\S+'),($field+'='+$bad)))
    }
}
Reject ($valid.Replace('width=100','width=100.00000000000001'))
Reject ([regex]::Replace($valid,'\bheight=19\b','height=20'))
Reject ($valid.Replace('complete=1','complete=0'))
Reject ($valid.Replace('variant=0','variant=-1'))
Reject ($valid.Replace('variant=0','variant=2'))
Reject ($valid.Replace('variant=0','variant=99999999999999999'))
Reject $economy
Reject ($valid+"`n"+$selection)
Reject ($valid.Replace('group=selection','group=economy'))
Reject ($valid.Replace('group=selection','group=other'))
Reject ($valid.Replace('name=one.png','name=wrong.png'))
Reject ($valid+"`nPROOF_HUD_CRITICAL_CAPTURE malformed")
Reject ($valid.Replace('maxheight=19','maxheight=19 unexpected=1'))
Reject $valid @('one.png','one.png')
Reject $valid @()
Write-Output ('PASS critical text parser checks='+$checks+'; synthetic policy only, no engine/visual proof')