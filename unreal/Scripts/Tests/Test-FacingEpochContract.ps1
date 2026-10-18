$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\FacingDisplacementEvidence.ps1"
$runtime=Join-Path $PSScriptRoot '../../Source/RedalertProof/Runtime'
$producer=Get-Content (Join-Path $runtime 'ProofFacingEvidence.cpp') -Raw
$caller=Get-Content (Join-Path $runtime 'ProofGameMode.cpp') -Raw

# Lightweight source-binding regression, not execution of the C++ hooks.
# Fresh engine validation must still verify the actual emitted records.
function Test-ProducerBinding([string]$Source,[string]$Caller) {
    $command=[regex]::Match($Source,'(?s)void ObserveProofFacingCommand\(int32 TrustedMatchEpoch,.*?(?=// Read-only snapshots)')
    $advance=[regex]::Match($Source,'(?s)void ObserveProofFacingAdvance\(int32 TrustedMatchEpoch,.*?(?=void LogProofFacingSnapshot)')
    if(-not$command.Success-or-not$advance.Success){throw 'Trusted epoch hook signatures missing'}
    foreach($hook in @($command.Value,$advance.Value)){
        if($hook-match'(?:Before|After)\.MatchEpoch'-or$hook-notmatch'TrustedMatchEpoch <= 0'){throw 'Hook trusts raw model epoch or lacks positive epoch guard'}
    }
    if($command.Value-notmatch'ObservedMoves\.Add\(Unit->UnitId, \{TrustedMatchEpoch, Command\.Sequence\}\)'-or$command.Value-notmatch'TrustedMatchEpoch, Unit->UnitId, Command\.Sequence, After\.Revision'){throw 'Accepted Move epoch identity not stamped from trusted owner'}
    if($advance.Value-notmatch'Entry\.Value\.Epoch != TrustedMatchEpoch'-or$advance.Value-notmatch'SetNumberField\(TEXT\("epoch"\), TrustedMatchEpoch\)'){throw 'Advance epoch identity not stamped from trusted owner'}
    if($Caller-notmatch'ObserveProofFacingCommand\(MatchEpoch, TrustedOwnerId, Command, Result, Model\.GetSnapshot\(\)\)'-or$Caller-notmatch'ObserveProofFacingAdvance\(MatchEpoch, EvidenceBeforeAdvance, After\)'){throw 'GameMode epoch not passed to both hooks'}
    if($Caller.IndexOf('FProofCommandResult Result = Model.Submit(TrustedOwnerId, Command);')-ge$Caller.IndexOf('ObserveProofFacingCommand(MatchEpoch,')){throw 'Observation precedes authoritative Submit'}
    if($command.Value-notmatch'!Result\.bAccepted'-or$command.Value-notmatch'Owner != 1'){throw 'Trusted accepted owner binding missing'}
    foreach($binding in @('"unit"\), Entry.Key','"sequence"\), Entry.Value.Sequence','"beforeRevision"\), Before.Revision','"afterRevision"\), After.Revision')){if($advance.Value-notmatch$binding){throw 'Advance identity/revision field binding missing'}}
    if($advance.Value-notmatch'FacingVector\(Row, TEXT\("before"\), A->Position\)'-or$advance.Value-notmatch'FacingVector\(Row, TEXT\("after"\), B->Position\)'){throw 'Actual position fields missing'}
}
function Fixture([int]$CommandEpoch=1,[int]$TraceEpoch=1,[int]$Unit=7) {
    $row=[ordered]@{epoch=$TraceEpoch;unit=$Unit;sequence=10;beforeRevision=1;afterRevision=2;before=@(0,0,0);after=@(3,4,0)}
    return ('PROOF_FACING_MOVE_ACCEPTED epoch={0} unit=7 sequence=10 revision=1' -f $CommandEpoch)+"`nPROOF_FACING_DISPLACEMENT "+($row|ConvertTo-Json -Compress)
}
$results=[Collections.Generic.List[object]]::new()
function Pass([string]$Name){$results.Add([ordered]@{case=$Name;passed=$true})}
function Reject([string]$Name,[scriptblock]$Action){
    $caught=$false;$reason=$null
    try{&$Action|Out-Null}catch{$caught=$true;$reason=$_.Exception.Message}
    if(-not$caught){throw ('Regression missed '+$Name)}
    $results.Add([ordered]@{case=$Name;passed=$true;rejection=$reason})
}
Test-ProducerBinding $producer $caller;Pass 'both actual source hooks bind trusted owning GameMode epoch'
Reject 'old raw epoch source in accepted command hook' {Test-ProducerBinding ($producer.Replace('TrustedMatchEpoch, Unit->UnitId, Command.Sequence, After.Revision','After.MatchEpoch, Unit->UnitId, Command.Sequence, After.Revision')) $caller}
Reject 'old raw epoch source in Advance hook' {Test-ProducerBinding ($producer.Replace('SetNumberField(TEXT("epoch"), TrustedMatchEpoch)','SetNumberField(TEXT("epoch"), After.MatchEpoch)')) $caller}
Reject 'wrong caller epoch source' {Test-ProducerBinding $producer ($caller.Replace('ObserveProofFacingAdvance(MatchEpoch,','ObserveProofFacingAdvance(After.MatchEpoch,'))}
$trace=@(Get-FacingDisplacementTrace (Fixture));$o=Get-FacingDisplacementAt $trace 1 7 10 2 @(3,4,0)
if([math]::Abs($o.direction[0]-.6)-gt1e-12-or[math]::Abs($o.direction[1]-.8)-gt1e-12){throw 'Trusted epoch1 actual displacement mismatch'}
Pass 'trusted published epoch1 accepted by parser with independent 3-4-5 direction'
Reject 'raw model epoch0 in both record types' {Get-FacingDisplacementTrace (Fixture 0 0)}
Reject 'raw model epoch0 in command acceptance only' {Get-FacingDisplacementTrace (Fixture 0 1)}
Reject 'raw model epoch0 in Advance only' {Get-FacingDisplacementTrace (Fixture 1 0)}
Reject 'mismatched positive epoch' {Get-FacingDisplacementTrace (Fixture 1 2)}
Reject 'mismatched unit identity' {Get-FacingDisplacementTrace (Fixture 1 1 8)}
Reject 'missing command acceptance' {Get-FacingDisplacementTrace ((Fixture)-replace'(?m)^PROOF_FACING_MOVE_ACCEPTED[^\r\n]*\r?\n','')}
$rematch=(Fixture)+"`n"+((Fixture 2 2)-replace'"after":\[3,4,0\]','"after":[0,-5,0]')
$trace=@(Get-FacingDisplacementTrace $rematch);$o=Get-FacingDisplacementAt $trace 2 7 10 2 @(0,-5,0)
if($o.direction[0]-ne0-or$o.direction[1]-ne-1){throw 'Epoch2 reused unit/sequence mixed prior epoch content'}
Pass 'rematch epoch2 isolates reused unit sequence and revision'
$results|ConvertTo-Json -Depth 4
