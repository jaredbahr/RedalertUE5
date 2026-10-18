# Pure synthetic unit tests of the actual PowerShell policy, not engine evidence.
# No process launch, installation, file output, screenshots or native pass claim.
# Run with an already installed PowerShell 7: pwsh -NoProfile -File <this script>.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ReturnCargoEvidence.ps1')
function New-SyntheticCase {
    $case=@{phases=[Collections.Generic.List[object]]::new();commands=[Collections.Generic.List[object]]::new();travels=[Collections.Generic.List[object]]::new();deposits=[Collections.Generic.List[object]]::new();resumes=[Collections.Generic.List[object]]::new();extra='';transform=$null}
    $case.context=@{maps=@{host=@{};rendered=@{}};common=@();roles=@('host','rendered')}
    # Deliberately a companion-policy unit seam. Get-RefineryMinerEvidence is
    # tested separately and is mandatory in the real launcher. These synthetic
    # maps are never passed off as captured/digest-verified engine states.
    $case.refinery=@{passed=$true;grants=@(@{owner=1;epoch=1;miner=2;refinery=3;resource=1;gatherSequence=7})}
    function Add-Phase($Name,$Rev,$Before,$Next,$Now,$Last,$Queue,$Count,$A,$B,$Gathered,$AX,$BX) {
        $p=[pscustomobject][ordered]@{phase=$Name;owner=1;epoch=1;revision=$Rev;a=1;b=2;refinery=3;resource=1;beforeSequence=$Before;nextBefore=$Next;nextNow=$Now;lastSequence=$Last;queue=$Queue;count=$Count;cargoA=$A;cargoB=$B;credits=100+$Gathered;gathered=$Gathered;elapsed=$case.phases.Count;physicalInputVerified=0;nativeCallbackInvoked=0;sharedHandlers=1;networkTransportVerified=0}
        $case.phases.Add($p)
        $s=[pscustomobject]@{epoch=1;revision=$Rev;digest=('0'*32);phase=1;winner=-1;units=@(
            [pscustomobject]@{id=1;owner=1;kind=1;hp=100;cargo=$A;x=$AX;y=0;z=0},
            [pscustomobject]@{id=2;owner=1;kind=1;hp=100;cargo=$B;x=$BX;y=0;z=0},
            [pscustomobject]@{id=3;owner=1;kind=3;hp=100;cargo=0;x=0;y=0;z=0},
            [pscustomobject]@{id=4;owner=1;kind=2;hp=100;cargo=0;x=-500;y=0;z=0});players=@([pscustomobject]@{id=1;credits=100+$Gathered;gathered=$Gathered});resources=@([pscustomobject]@{id=1;owner=1;remaining=1000-$Gathered-$A-$B;x=1000;y=0;z=0})}
        $key="1:$Rev"; $case.context.maps.host[$key]=$s; $case.context.maps.rendered[$key]=$s
        if($case.context.common -notcontains $key){$case.context.common+=,$key}
    }
    Add-Phase selected 10 2 3 3 2 0 1 0 0 0 2000 300
    Add-Phase gather-a 10 2 3 4 3 0 1 0 0 0 2000 300
    Add-Phase single-return 20 3 4 5 4 0 1 50 0 0 1000 300
    Add-Phase single-accepted 22 3 4 5 4 0 1 50 0 0 1000 300
    Add-Phase single-deposited 31 3 4 5 4 0 1 0 0 50 0 300
    Add-Phase single-resumed 41 3 4 5 4 0 1 50 0 50 1000 300
    Add-Phase loaded-only-queued 41 4 5 5 4 1 2 50 0 50 1000 300
    Add-Phase loaded-only-accepted 44 4 5 6 5 0 2 50 0 50 1000 300
    Add-Phase loaded-only-deposited 51 4 5 6 5 0 1 0 0 100 0 300
    Add-Phase loaded-only-resumed 61 4 5 6 5 0 1 50 0 100 1000 300
    Add-Phase hold-a 61 5 6 6 5 1 1 50 0 100 1000 300
    Add-Phase held-a 64 5 6 7 6 0 1 50 0 100 1000 300
    Add-Phase gather-b 64 6 7 8 7 0 1 50 0 100 1000 300
    Add-Phase group-queued 70 7 8 8 7 2 2 50 50 100 1000 1000
    Add-Phase group-accepted 75 7 8 10 9 0 2 50 50 100 1000 1000
    Add-Phase group-deposited 81 7 8 10 9 0 1 0 0 200 0 0
    Add-Phase group-resumed 91 7 8 10 9 0 1 0 50 200 0 1000
    Add-Phase restore-a 91 9 10 11 10 0 1 0 50 200 0 1000
    Add-Phase complete 94 9 10 11 10 0 2 0 50 200 0 1000
    # Untyped negative command arguments are strings in PowerShell argument mode.
    # Bind numeric fields before JSON serialization; keep the policy strict.
    function Add-Command([int]$Sequence,[int]$Type,[int]$Unit,[int]$Target,[int]$Before,[int]$Rev,[int]$Cargo,[int]$Gathered,[double]$X,[int]$Saved) {
        $case.commands.Add([pscustomobject][ordered]@{owner=1;epoch=1;sequence=$Sequence;type=$Type;unit=$Unit;target=$Target;accepted=1;revisionBefore=$Before;revision=$Rev;cargoBefore=$Cargo;cargoAfter=$Cargo;creditsBefore=100+$Gathered;creditsAfter=100+$Gathered;gatheredBefore=$Gathered;gatheredAfter=$Gathered;xBefore=$X;yBefore=0;zBefore=0;x=$X;y=0;z=0;resumeResource=$Saved;conservedBefore=1000;conservedAfter=1000})
    }
    Add-Command 1 3 4 -1 1 2 0 0 -500 -1
    Add-Command 2 4 4 -1 2 3 0 0 -500 -1
    Add-Command 3 5 1 1 11 12 0 0 2000 1
    Add-Command 4 9 1 3 20 21 50 0 1000 1
    Add-Command 5 9 1 3 42 43 50 50 1000 1
    Add-Command 6 6 1 -1 62 63 50 100 1000 -1
    Add-Command 7 5 2 1 65 66 0 100 300 1
    Add-Command 8 9 1 3 71 72 50 100 1000 -1
    Add-Command 9 9 2 3 73 74 50 100 1000 1
    Add-Command 10 5 1 1 92 93 0 200 0 1
    foreach($s in @(@(4,1,23),@(5,1,45),@(8,1,76),@(9,2,77))) {
        $case.travels.Add([pscustomobject][ordered]@{owner=1;epoch=1;revision=$s[2];sequence=$s[0];miner=$s[1];depot=3;cargoBefore=50;cargoAfter=50;xBefore=1000;yBefore=0;zBefore=0;x=990;y=0;z=0;depotX=0;depotY=0;depotZ=0})
    }
    function Contributor($Miner,$Sequence) { return [pscustomobject][ordered]@{miner=$Miner;before=50;after=0;depot=3;sequence=$Sequence} }
    function Add-Deposit([int]$Sequence,[int]$Miner,[int]$Rev,[int]$Gathered,[int]$Saved,$Contributors) {
        $sum=50*$Contributors.Count
        $case.deposits.Add([pscustomobject][ordered]@{owner=1;epoch=1;revision=$Rev;sequence=$Sequence;resumeResource=$Saved;miner=$Miner;depot=3;before=50;after=0;sum=$sum;creditsBefore=100+$Gathered;creditsAfter=100+$Gathered+$sum;gatheredBefore=$Gathered;gatheredAfter=$Gathered+$sum;x=0;y=0;z=0;depotX=0;depotY=0;depotZ=0;conservedBefore=1000;conservedAfter=1000;contributors=@($Contributors)})
    }
    Add-Deposit 4 1 30 0 1 @((Contributor 1 4))
    Add-Deposit 5 1 50 50 1 @((Contributor 1 5))
    Add-Deposit 8 1 80 100 -1 @((Contributor 1 8),(Contributor 2 9))
    Add-Deposit 9 2 80 100 1 @((Contributor 1 8),(Contributor 2 9))
    foreach($s in @(@(4,1,40,950,900),@(5,1,60,900,850),@(9,2,90,800,750))) {
        $case.resumes.Add([pscustomobject][ordered]@{owner=1;epoch=1;revision=$s[2];sequence=$s[0];miner=$s[1];resource=1;cargoBefore=0;cargoAfter=50;remainingBefore=$s[3];remainingAfter=$s[4];x=1000;y=0;z=0;resourceX=1000;resourceY=0;resourceZ=0})
    }
    return $case
}
function Get-SyntheticLog($Case) {
    $lines=[Collections.Generic.List[string]]::new()
    foreach($pair in @(@('phases','PHASE'),@('commands','COMMAND'),@('travels','TRAVEL'),@('deposits','DEPOSIT'),@('resumes','RESUME'))) {
        foreach($row in $Case[$pair[0]]){$lines.Add('PROOF_RETURN_CARGO_'+$pair[1]+' '+($row | ConvertTo-Json -Compress -Depth 8))}
    }
    $types=@('Move','Attack','Produce','BuildRefinery','BuildFactory','Gather','Stop','SetRally','AttackMove','ReturnCargo')
    foreach($c in $Case.commands) {
        $lines.Add('PROOF_SKIRMISH_REQUEST owner=1 sequence='+$c.sequence+' type='+$types[$c.type]+' unit='+$c.unit+' target='+$c.target)
        $lines.Add('PROOF_RESULT owner=1 sequence='+$c.sequence+' accepted='+$c.accepted+' reason='+$(if($c.accepted -eq 1){'accepted'}else{'cargo'}))
    }
    $lines.Add('PROOF_REFINERY_EVENT {"event":"grant","owner":1,"epoch":1,"beforeIds":[1,4],"afterIds":[1,2,3,4]}')
    $lines.Add('PROOF_RETURN_CARGO_CAPTURE name=03-return-cargo.png selectedCount=2 written=1 sharedHandlers=1 nativeCallbackInvoked=0 physicalInputVerified=0 networkTransportVerified=0')
    $lines.Add('PROOF_SKIRMISH_COMPLETE role=client owner=1 status=pass scenario=Victory winnerTeam=0 built=1 gathered=1 produced=1 botBuilt=1 botProduced=1 botAttacked=1 reason=complete_loop physicalInput=unverified')
    $log=($lines -join "`n")+"`n"+$Case.extra
    if($Case.transform){$log=& $Case.transform $log}
    return $log
}
$cases=[Collections.Generic.List[object]]::new()
function Case([string]$Name,[bool]$Pass,[scriptblock]$Mutate) { $cases.Add(@{name=$Name;pass=$Pass;mutate=$Mutate}) }
Case 'complete two-miner sequence with simultaneous group credit counted once' $true { param($c) }
Case 'separate group deposit boundaries in arrival order' $true {
    param($c)
    $a=$c.deposits[2]; $b=$c.deposits[3]
    $a.sum=50; $a.creditsBefore=250; $a.gatheredBefore=150; $a.contributors=@($a.contributors[0])
    $b.revision=79; $b.sum=50; $b.creditsAfter=250; $b.gatheredAfter=150; $b.contributors=@($b.contributors[1])
    $c.deposits[2]=$b; $c.deposits[3]=$a
}
Case 'unrelated factory command creates an allowed sequence gap' $true {
    param($c)
    $other=$c.commands[3] | ConvertTo-Json | ConvertFrom-Json
    $other.type=7; $other.unit=5; $other.target=-1; $other.revisionBefore=15; $other.revision=16
    $other.cargoBefore=0; $other.cargoAfter=0; $other.xBefore=0; $other.x=0; $other.resumeResource=-1
    foreach($command in $c.commands){if($command.sequence -ge 4){$command.sequence++}}
    $c.commands.Insert(3,$other)
    for($i=2;$i -lt $c.phases.Count;$i++) { foreach($field in @('beforeSequence','nextBefore','nextNow','lastSequence')){$c.phases[$i].$field++} }
    foreach($list in @('travels','deposits','resumes')){foreach($row in $c[$list]){$row.sequence++}}
    foreach($d in $c.deposits){foreach($contributor in $d.contributors){$contributor.sequence++}}
    foreach($state in $c.context.maps.host.Values){$state.units+=,[pscustomobject]@{id=5;owner=1;kind=4;hp=100;cargo=0;x=0;y=500;z=0}}
}
Case 'ordinary Produce spends between phase publications' $true {
    param($c)
    $other=$c.commands[3] | ConvertTo-Json | ConvertFrom-Json
    $other.type=2; $other.unit=5; $other.target=-1; $other.revisionBefore=16; $other.revision=18
    $other.cargoBefore=0; $other.cargoAfter=0; $other.xBefore=0; $other.x=0; $other.resumeResource=-1
    $other.creditsAfter=50
    foreach($command in $c.commands){if($command.sequence -ge 4){$command.sequence++; $command.creditsBefore-=50; $command.creditsAfter-=50}}
    $c.commands.Insert(3,$other)
    for($i=2;$i -lt $c.phases.Count;$i++) { foreach($field in @('beforeSequence','nextBefore','nextNow','lastSequence')){$c.phases[$i].$field++}; $c.phases[$i].credits-=50 }
    foreach($list in @('travels','deposits','resumes')){foreach($row in $c[$list]){$row.sequence++}}
    foreach($d in $c.deposits){$d.creditsBefore-=50; $d.creditsAfter-=50; foreach($contributor in $d.contributors){$contributor.sequence++}}
    foreach($state in $c.context.maps.host.Values){
        $state.units+=,[pscustomobject]@{id=5;owner=1;kind=4;hp=100;cargo=0;x=0;y=500;z=0}
        if($state.revision -ge 18){$state.players[0].credits-=50}
    }
}
Case 'missing prerequisite' $false { param($c) $c.refinery.passed=$false }
Case 'phase missing' $false { param($c) $c.phases.RemoveAt(4) }
Case 'phase reordered' $false { param($c) $c.phases[3].phase='single-deposited' }
Case 'phase identity drift' $false { param($c) $c.phases[3].epoch=2 }
Case 'no common authority/source checkpoint' $false { param($c) $c.context.common=@($c.context.common | Where-Object {$_ -ne '1:22'}) }
Case 'rendered checkpoint missing' $false { param($c) $c.context.maps.rendered.Remove('1:22') }
Case 'phase cargo contradicts snapshot' $false { param($c) $c.phases[3].cargoA=49 }
Case 'phase ownership wrong' $false { param($c) $c.context.maps.host['1:22'].units[0].owner=100 }
Case 'numeric string rejected' $false { param($c) $c.phases[3].revision='22' }
Case 'boolean integer rejected' $false { param($c) $c.phases[3].sharedHandlers=$true }
Case 'nonfinite time rejected' $false { param($c) $c.phases[18].elapsed=[double]::NaN }
Case 'unchanged 220-second fixture limit' $false { param($c) $c.phases[18].elapsed=220.000001 }
Case 'physical input overclaim' $false { param($c) $c.phases[0].physicalInputVerified=1 }
Case 'native callback overclaim' $false { param($c) $c.phases[0].nativeCallbackInvoked=1 }
Case 'network transport overclaim' $false { param($c) $c.phases[0].networkTransportVerified=1 }
Case 'not shared handler' $false { param($c) $c.phases[0].sharedHandlers=0 }
Case 'extra event field' $false { param($c) $c.phases[0] | Add-Member -NotePropertyName extra -NotePropertyValue 1 }
Case 'duplicate JSON key' $false { param($c) $c.transform={param($l) $l.Replace('"phase":"selected"','"phase":"selected","phase":"selected"')} }
Case 'case-colliding JSON key' $false { param($c) $c.transform={param($l) $l.Replace('"phase":"selected"','"phase":"selected","Phase":"selected"')} }
Case 'unknown event kind' $false { param($c) $c.extra='PROOF_RETURN_CARGO_UNKNOWN {}' }
Case 'runtime error' $false { param($c) $c.extra='PROOF_RETURN_CARGO_ERROR deposit_attribution' }
Case 'command absent' $false { param($c) $c.commands.RemoveAt(3) }
Case 'command duplicate' $false { param($c) $c.commands.Add($c.commands[3]) }
Case 'unexplained allocator sequence gap' $false { param($c) $c.commands[0].sequence=0 }
Case 'observation batch drift' $false { param($c) $c.phases[4].nextBefore=3 }
Case 'stopped loaded miner moved while B gathered' $false { param($c) $c.context.maps.host['1:70'].units[0].x=990 }
Case 'command wrong target' $false { param($c) $c.commands[3].target=4 }
Case 'command wrong owner' $false { param($c) $c.commands[3].owner=100 }
Case 'command wrong epoch' $false { param($c) $c.commands[3].epoch=2 }
Case 'command rejected' $false { param($c) $c.commands[3].accepted=0 }
Case 'command gives immediate credit' $false { param($c) $c.commands[3].creditsAfter++ }
Case 'command equal-delta credit offset rejected' $false { param($c) $c.commands[3].creditsBefore++; $c.commands[3].creditsAfter++ }
Case 'command equal-delta gathered offset rejected' $false { param($c) $c.commands[3].gatheredBefore++; $c.commands[3].gatheredAfter++ }
Case 'deposit equal-delta credit offset rejected' $false { param($c) $c.deposits[0].creditsBefore++; $c.deposits[0].creditsAfter++ }
Case 'intermediate common credit checkpoint cannot drift' $false { param($c) $c.context.maps.host['1:22'].players[0].credits++; $c.phases[3].credits++ }
Case 'non-spending command cannot hide a credit decrease' $false { param($c) $c.commands[3].creditsAfter-- }
Case 'command teleports miner' $false { param($c) $c.commands[3].x=0 }
Case 'command empties cargo' $false { param($c) $c.commands[3].cargoAfter=0 }
Case 'wrong saved Gather provenance' $false { param($c) $c.commands[3].resumeResource=-1 }
Case 'missing receipt' $false { param($c) $c.transform={param($l) $l.Replace('PROOF_RESULT owner=1 sequence=4 accepted=1 reason=accepted','')} }
Case 'duplicate receipt' $false { param($c) $c.extra='PROOF_RESULT owner=1 sequence=4 accepted=1 reason=accepted' }
Case 'missing shared Request' $false { param($c) $c.transform={param($l) $l.Replace('PROOF_SKIRMISH_REQUEST owner=1 sequence=4 type=ReturnCargo unit=1 target=3','')} }
Case 'contradictory shared Request' $false { param($c) $c.transform={param($l) $l.Replace('sequence=4 type=ReturnCargo unit=1 target=3','sequence=4 type=ReturnCargo unit=2 target=3')} }
Case 'empty group member not skipped' $false { param($c) $c.phases[6].queue=2 }
Case 'queue stamped too early' $false { param($c) $c.phases[6].nextNow=6 }
Case 'group accepted wrong count' $false { param($c) $c.phases[14].nextNow=11 }
Case 'accepted selection wrong' $false { param($c) $c.phases[14].count=1 }
Case 'missing positive travel' $false { param($c) $c.travels.RemoveAt(0) }
Case 'stationary travel' $false { param($c) $c.travels[0].x=1000 }
Case 'travel away from refinery' $false { param($c) $c.travels[0].x=1010 }
Case 'travel unloads before arrival' $false { param($c) $c.travels[0].cargoAfter=0 }
Case 'travel at depot is not positive route' $false { param($c) $c.travels[0].x=0 }
Case 'duplicate travel' $false { param($c) $c.travels[1].sequence=4 }
Case 'missing authoritative deposit' $false { param($c) $c.deposits.RemoveAt(0) }
Case 'duplicate credit row' $false { param($c) $c.deposits.Add($c.deposits[0]) }
Case 'deposit outside exact tolerance' $false { param($c) $c.deposits[0].x=1.000001 }
Case 'deposit exact tolerance retained' $true { param($c) $c.deposits[0].x=1.0 }
Case 'nonfinite deposit coordinate' $false { param($c) $c.deposits[0].x=[double]::PositiveInfinity }
Case 'invented depot position' $false { param($c) $c.deposits[0].depotX=100; $c.deposits[0].x=100 }
Case 'wrong deposit sequence' $false { param($c) $c.deposits[0].sequence=5 }
Case 'deposit double credits' $false { param($c) $c.deposits[0].creditsAfter+=50 }
Case 'duplicate contributor' $false { param($c) $c.deposits[0].contributors+=,$c.deposits[0].contributors[0] }
Case 'contributor wrong sequence' $false { param($c) $c.deposits[0].contributors[0].sequence=8 }
Case 'conservation loss' $false { param($c) $c.deposits[0].conservedAfter-- }
Case 'contradictory simultaneous boundary' $false { param($c) $c.deposits[3].creditsBefore++; $c.deposits[3].creditsAfter++ }
Case 'reused aggregate credit boundary' $false { param($c) $c.deposits[1].gatheredBefore=0; $c.deposits[1].gatheredAfter=50 }
Case 'missing saved Gather reload' $false { param($c) $c.resumes.RemoveAt(0) }
Case 'wrong saved Gather resource' $false { param($c) $c.resumes[0].resource=2 }
Case 'reload before deposit' $false { param($c) $c.resumes[0].revision=29 }
Case 'reload lacks ore consumption' $false { param($c) $c.resumes[0].remainingAfter=950 }
Case 'reload equal-delta ore offset rejected' $false { param($c) $c.resumes[0].remainingBefore+=10000; $c.resumes[0].remainingAfter+=10000 }
Case 'reload offset within initial ore still rejected by nearer checkpoints' $false { param($c) $c.resumes[0].remainingBefore+=10; $c.resumes[0].remainingAfter+=10 }
Case 'reload below following published ore rejected' $false { param($c) $c.resumes[0].remainingBefore-=10; $c.resumes[0].remainingAfter-=10 }
Case 'reported reload cargo must match first resumed phase' $false { param($c) $c.resumes[0].cargoAfter=49; $c.resumes[0].remainingBefore=949 }
Case 'reload away from resource' $false { param($c) $c.resumes[0].x=998.999999 }
Case 'stopped miner automatically resumes' $false { param($c) $c.context.maps.host['1:91'].units[0].cargo=50 }
Case 'stopped miner moves before explicit restore' $false { param($c) $c.context.maps.host['1:91'].units[0].x=10 }
Case 'capture overclaims physical input' $false { param($c) $c.transform={param($l) $l.Replace('nativeCallbackInvoked=0 physicalInputVerified=0','nativeCallbackInvoked=0 physicalInputVerified=1')} }
Case 'missing capture' $false { param($c) $c.transform={param($l) $l -replace 'PROOF_RETURN_CARGO_CAPTURE[^\r\n]*',''} }
Case 'ordinary Victory missing' $false { param($c) $c.transform={param($l) $l -replace 'PROOF_SKIRMISH_COMPLETE[^\r\n]*',''} }
Case 'ordinary completion reason mismatch' $false { param($c) $c.transform={param($l) $l.Replace('reason=complete_loop','reason=synthetic')} }
Case 'wrong winner' $false { param($c) $c.transform={param($l) $l.Replace('scenario=Victory winnerTeam=0','scenario=Victory winnerTeam=1')} }
$passed=0
foreach($test in $cases) {
    $case=New-SyntheticCase; & $test.mutate $case | Out-Null
    $accepted=$false; $reason=''
    try { $result=Get-ReturnCargoEvidence -Log (Get-SyntheticLog $case) -ValidatedStateContext $case.context -RefineryEvidence $case.refinery; $accepted=$result.passed }
    catch { $reason=$_.Exception.Message }
    if($accepted -ne $test.pass){throw "Synthetic policy case '$($test.name)' expected pass=$($test.pass), got pass=$accepted. $reason"}
    $passed++; Write-Output ('PASS synthetic policy: '+$test.name)
}
Write-Output ("Synthetic PowerShell policy cases passed: $passed. No engine, native callback, physical input, network or screenshot verification was performed.")
