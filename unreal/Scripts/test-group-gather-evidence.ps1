$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RefineryMinerEvidence.ps1')
. (Join-Path $PSScriptRoot 'GroupGatherEvidence.ps1')
function New-Fixture {
    $phases=@(
        @{phase='selected';revision=10;elapsed=1.0;queue=0;nextNow=3;lastSequence=2},
        @{phase='queued';revision=10;elapsed=1.0;queue=2;nextNow=3;lastSequence=2},
        @{phase='accepted';revision=12;elapsed=1.1;queue=0;nextNow=5;lastSequence=4},
        @{phase='complete';revision=14;elapsed=3.0;queue=0;nextNow=5;lastSequence=4})
    $lines=@()
    foreach($p in $phases){foreach($pair in @{owner=1;epoch=1;a=2;b=4;resource=1;beforeSequence=2;nextBefore=3;count=2}.GetEnumerator()){$p[$pair.Key]=$pair.Value};$lines+='PROOF_GROUP_GATHER_PHASE '+($p|ConvertTo-Json -Compress)}
    foreach($i in 0..1){$lines+='PROOF_GROUP_GATHER_COMMAND '+(@{owner=1;epoch=1;sequence=3+$i;unit=2+2*$i;resource=1;accepted=1;revision=11+$i}|ConvertTo-Json -Compress);$lines+="PROOF_SKIRMISH_REQUEST owner=1 sequence=$([int](3+$i)) type=Gather unit=$([int](2+2*$i)) target=1"}
    $lines+='PROOF_REFINERY_EVENT '+(@{event='grant';owner=1;epoch=1;beforeIds=@(1,2);miner=4}|ConvertTo-Json -Compress)
    foreach($miner in 2,4){$lines+='PROOF_GROUP_GATHER_DEPOSIT '+(@{owner=1;epoch=1;beforeRevision=13;revision=14;miner=$miner;depot=3;before=50;after=0;sum=100;creditsBefore=50;creditsAfter=150;gatheredBefore=0;gatheredAfter=100;x=350;y=0;z=0;depotX=350;depotY=0;depotZ=0;conservedBefore=1000;conservedAfter=1000;contributors=@(@{miner=2;depot=3;before=50;after=0},@{miner=4;depot=3;before=50;after=0})}|ConvertTo-Json -Depth 16 -Compress)}
    $lines+='PROOF_GROUP_GATHER_CAPTURE name=03-group-gather.png selectedCount=2 written=1 physicalInputVerified=0'
    $maps=@{host=@{};rendered=@{}}
    foreach($revision in 10,12,13,14){
        $loaded=$revision -eq 13;$deposited=$revision -eq 14
        $state=[pscustomobject]@{epoch=1;revision=$revision;phase=1;jobs=@();units=@(
            [pscustomobject]@{id=2;owner=1;kind=1;hp=100;cargo=$(if($loaded){50}else{0});x=$(if($loaded){550}elseif($deposited){350}else{100});y=0;z=0},
            [pscustomobject]@{id=4;owner=1;kind=1;hp=100;cargo=$(if($loaded){50}else{0});x=$(if($loaded){550}elseif($deposited){350}else{650});y=0;z=0},
            [pscustomobject]@{id=3;owner=1;kind=3;hp=200;cargo=0;x=350;y=0;z=0});
            resources=@([pscustomobject]@{id=1;remaining=$(if($loaded -or $deposited){400}else{500})},[pscustomobject]@{id=2;remaining=500});players=@([pscustomobject]@{id=1;credits=$(if($deposited){150}else{50});gathered=$(if($deposited){100}else{0})})}
        foreach($unit in $state.units){foreach($key in @('rally','rallyX','rallyY','rallyZ')){$unit|Add-Member -NotePropertyName $key -NotePropertyValue 0}}
        $maps.host["1:$revision"]=$state;$maps.rendered["1:$revision"]=$state
    }
    $lines=@($lines|ForEach-Object{
        if($_ -like 'PROOF_GROUP_GATHER_DEPOSIT *'){
            $row=$_.Substring('PROOF_GROUP_GATHER_DEPOSIT '.Length)|ConvertFrom-Json
            $row|Add-Member -NotePropertyName productionPayment -NotePropertyValue 0
            $row|Add-Member -NotePropertyName productionRefund -NotePropertyValue 0
            $row|Add-Member -NotePropertyName productionBefore -NotePropertyValue $maps.host['1:13']
            $row|Add-Member -NotePropertyName productionAfter -NotePropertyValue $maps.host['1:14']
            'PROOF_GROUP_GATHER_DEPOSIT '+($row|ConvertTo-Json -Depth 16 -Compress)
        }else{$_}
    })
    return [pscustomobject]@{Log=($lines -join "`r`n");Context=@{maps=$maps;common=@('1:10','1:12','1:13','1:14')};Refinery=@{passed=$true;grants=@(@{owner=1;epoch=1;miner=4;resource=1;gatherSequence=4})}}
}
$checks=0
function Check([string]$Name,[scriptblock]$Mutate,[bool]$Expected){
    $fixture=New-Fixture
    $before=$fixture|ConvertTo-Json -Depth 16 -Compress
    & $Mutate $fixture
    if(-not $Expected -and ($fixture|ConvertTo-Json -Depth 16 -Compress) -ceq $before){throw "$Name mutation was a no-op"}
    $passed=$true
    try {$null=Get-GroupGatherEvidence -Log $fixture.Log -ValidatedStateContext $fixture.Context -RefineryEvidence $fixture.Refinery} catch {$passed=$false;$reason=$_.Exception.Message}
    if($passed -ne $Expected){throw "$Name expected=$Expected actual=$passed $reason"}
    $script:checks++
}
Check 'valid CRLF' {} $true
Check 'valid LF' {param($f)$f.Log=$f.Log.Replace("`r`n","`n")} $true
Check 'missing phase' {param($f)$f.Log=$f.Log -replace '(?m)^PROOF_GROUP_GATHER_PHASE[^\r\n]*"phase":"queued"[^\r\n]*\r?\n',''} $false
Check 'duplicate phase' {param($f)$f.Log+="`r`n"+($f.Log.Split("`n")[0])} $false
Check 'wrong phase order' {param($f)$f.Log=$f.Log.Replace('"phase":"queued"','"phase":"wrong"')} $false
Check 'invalid elapsed' {param($f)$f.Log=$f.Log -replace '"elapsed":1\.0','"elapsed":"NaN"'} $false
Check 'overflow elapsed' {param($f)$f.Log=$f.Log.Replace('"elapsed":3.0','"elapsed":221')} $false
Check 'duplicate JSON key' {param($f)$f.Log=$f.Log.Replace('"phase":"selected"','"phase":"selected","phase":"selected"')} $false
Check 'string sequence' {param($f)$f.Log=$f.Log.Replace('"sequence":3','"sequence":"3"')} $false
Check 'boolean accepted' {param($f)$f.Log=$f.Log.Replace('"accepted":1','"accepted":true')} $false
Check 'wrong queued size' {param($f)$f.Log=$f.Log.Replace('"queue":2','"queue":1')} $false
Check 'early allocation' {param($f)$f.Log=$f.Log.Replace('"nextNow":3','"nextNow":4')} $false
Check 'wrong command epoch' {param($f)$f.Log=[regex]::new('(PROOF_GROUP_GATHER_COMMAND[^\r\n]*"epoch":)1').Replace($f.Log,'${1}2',1)} $false
Check 'wrong command target' {param($f)$f.Log=$f.Log.Replace('"resource":1','"resource":2')} $false
Check 'missing shared Request' {param($f)$f.Log=$f.Log.Replace('PROOF_SKIRMISH_REQUEST owner=1 sequence=3','OTHER owner=1 sequence=3')} $false
Check 'duplicate shared Request' {param($f)$f.Log+="`r`nPROOF_SKIRMISH_REQUEST owner=1 sequence=3 type=Gather unit=2 target=1"} $false
Check 'rejected command' {param($f)$f.Log=$f.Log.Replace('"accepted":1','"accepted":0')} $false
Check 'extra authoritative command' {param($f)$f.Log+="`r`nPROOF_GROUP_GATHER_COMMAND {}"} $false
Check 'missing command' {param($f)$f.Log=$f.Log -replace '(?m)^PROOF_GROUP_GATHER_COMMAND[^\r\n]*"sequence":3[^\r\n]*\r?\n',''} $false
Check 'missing capture' {param($f)$f.Log=$f.Log.Replace('PROOF_GROUP_GATHER_CAPTURE','OTHER_CAPTURE')} $false
Check 'duplicate capture' {param($f)$f.Log+="`r`nPROOF_GROUP_GATHER_CAPTURE name=03-group-gather.png selectedCount=2 written=1 physicalInputVerified=0"} $false
Check 'missing phase state' {param($f)$f.Context.maps.rendered.Remove('1:10')} $false
Check 'foreign miner' {param($f)$f.Context.maps.host['1:13'].units[0].owner=2} $false
Check 'dead miner' {param($f)$f.Context.maps.host['1:13'].units[0].hp=0} $false
Check 'one miner never loads' {param($f)$f.Context.maps.host['1:13'].units[0].cargo=0} $false
Check 'one miner never moves' {param($f)foreach($s in $f.Context.maps.host.Values){$s.units[0].x=100}} $false
Check 'no delivery increase' {param($f)$f.Context.maps.host['1:14'].players[0].gathered=0} $false
Check 'rendered skipped exact deposit frame' {
    param($f)
    $later=$f.Context.maps.host['1:14'];$later.revision=15;$later.units[0].x=365
    foreach($role in @('host','rendered')){$f.Context.maps[$role].Remove('1:14');$f.Context.maps[$role]['1:15']=$later}
    $f.Context.common=@('1:10','1:12','1:13','1:15')
    $f.Log=[regex]::Replace($f.Log,'PROOF_GROUP_GATHER_PHASE([^\r\n]*)',{
        param($m)$row=$m.Groups[1].Value|ConvertFrom-Json
        if($row.phase -eq 'complete'){$row.revision=15}
        'PROOF_GROUP_GATHER_PHASE '+($row|ConvertTo-Json -Compress)
    })
} $true
Check 'foreign refinery' {param($f)$f.Context.maps.host['1:14'].units[2].owner=2} $false
Check 'wrong grant' {param($f)$f.Refinery.grants[0].miner=2} $false
Check 'starter absent before grant' {param($f)$f.Log=$f.Log.Replace('"beforeIds":[1,2]','"beforeIds":[1]')} $false
Check 'missing validated context' {param($f)$f.Refinery.passed=$false} $false
Check 'runtime failure' {param($f)$f.Log+="`r`nPROOF_SKIRMISH_COMPLETE status=fail"} $false
Check 'negative first elapsed' {param($f)$f.Log=$f.Log.Replace('"elapsed":1.0','"elapsed":-0.5')} $false
Check 'extra malformed capture' {param($f)$f.Log+="`r`nPROOF_GROUP_GATHER_CAPTURE malformed"} $false
Check 'capture trailing fields' {param($f)$f.Log=$f.Log.Replace('physicalInputVerified=0','physicalInputVerified=0 extra=1')} $false
Check 'contradictory same sequence Request' {param($f)$f.Log+="`r`nPROOF_SKIRMISH_REQUEST owner=1 sequence=3 type=Move unit=999 target=1"} $false
Check 'preloaded starter' {param($f)$f.Context.maps.host['1:10'].units[0].cargo=50} $false
function Change-Complete($Fixture,[int]$Next,[int]$Last) {
    $Fixture.Log=[regex]::Replace($Fixture.Log,'PROOF_GROUP_GATHER_PHASE([^\r\n]*)',{
        param($m)
        $row=$m.Groups[1].Value|ConvertFrom-Json
        if($row.phase -eq 'complete'){$row.nextNow=$Next;$row.lastSequence=$Last}
        return 'PROOF_GROUP_GATHER_PHASE '+($row|ConvertTo-Json -Compress)
    })
}
Check 'normal production and defense after acceptance' {param($f)Change-Complete $f 12 11} $true
Check 'complete allocator rollback' {param($f)Change-Complete $f 4 3} $false
Check 'complete inconsistent last request' {param($f)Change-Complete $f 12 10} $false
function Change-Deposit($Fixture,[scriptblock]$Mutation) {
    $Fixture.Log=[regex]::Replace($Fixture.Log,'PROOF_GROUP_GATHER_DEPOSIT([^\r\n]*)',{
        param($m)
        $row=$m.Groups[1].Value|ConvertFrom-Json
        & $Mutation $row
        return 'PROOF_GROUP_GATHER_DEPOSIT '+($row|ConvertTo-Json -Depth 16 -Compress)
    })
}
Check 'missing deposit boundary' {param($f)$f.Log=$f.Log.Replace('PROOF_GROUP_GATHER_DEPOSIT','OTHER_DEPOSIT')} $false
Check 'deposit boundary too far' {param($f)Change-Deposit $f {param($d)$d.x=365}} $false
Check 'deposit not conserved' {param($f)Change-Deposit $f {param($d)$d.conservedAfter=1001}} $false
Check 'wrong deposit credit attribution' {param($f)Change-Deposit $f {param($d)$d.creditsAfter=151}} $false
Check 'wrong simultaneous contribution sum' {param($f)Change-Deposit $f {param($d)$d.contributors[0].before=49}} $false
Check 'duplicate deposit source' {param($f)Change-Deposit $f {param($d)$d.miner=2}} $false
Check 'deposit before Gather' {param($f)Change-Deposit $f {param($d)$d.revision=11}} $false
Check 'wrong depot coordinates' {param($f)Change-Deposit $f {param($d)$d.x=351;$d.depotX=351}} $false
Check 'phantom simultaneous contributor' {param($f)Change-Deposit $f {param($d)$d.contributors[1].miner=999}} $false
Check 'phantom contributor depot' {param($f)Change-Deposit $f {param($d)$d.contributors[1].depot=999}} $false

Check 'missing production context' {param($f)Change-Deposit $f {param($d)$d.productionBefore=$null}} $false
Check 'fake production payment' {param($f)Change-Deposit $f {param($d)$d.productionPayment=1}} $false
Check 'fake production refund' {param($f)Change-Deposit $f {param($d)$d.productionRefund=1}} $false
Check 'wrong context owner' {param($f)Change-Deposit $f {param($d)$d.productionAfter.players[0].id=2}} $false


Check 'realistic large contextual refinery deposit does not hide grant' {
    param($f)
    $row=([regex]::Match($f.Log,'PROOF_GROUP_GATHER_DEPOSIT([^\r\n]*)').Groups[1].Value|ConvertFrom-Json -Depth 16)
    # Retained dead Combat records are ordinary model history, not padding or a
    # live-cap override. Their complete before/after state makes this >4 KiB.
    foreach($id in 20..39){
        $record=[pscustomobject]@{id=$id;owner=1;kind=0;hp=0;cargo=0;x=1000+$id;y=1000;z=0;rally=0;rallyX=0;rallyY=0;rallyZ=0}
        $row.productionBefore.units+=,$record;$row.productionAfter.units+=,$record
    }
    $refinery=[ordered]@{event='deposit';epoch=1;owner=1;refinery=3;depot=3;miner=4;beforeRevision=13;revision=14;cargoBefore=50;cargoAfter=0;depositSum=100;creditsBefore=50;creditsAfter=150;gatheredBefore=0;gatheredAfter=100;contributors=$row.contributors;productionPayment=0;productionRefund=0;productionBefore=$row.productionBefore;productionAfter=$row.productionAfter}
    $json=$refinery|ConvertTo-Json -Depth 16 -Compress
    if($json.Length -le 4096 -or $json.Length -gt 131072){throw 'Large valid contextual row fixture size drifted'}
    Assert-ProductionAdvance $refinery.productionBefore $refinery.productionAfter 1 0 0
    $f.Log+="`nPROOF_REFINERY_EVENT "+$json
} $true

Check 'same-digest after context contradicts published state' {param($f)Change-Deposit $f {param($d)$d.productionAfter.units[0].x=350.5}} $false
Check 'same-digest before context contradicts published state' {param($f)Change-Deposit $f {param($d)$d.productionBefore.units[0].x=550.5}} $false
Check 'unpublished simultaneous deposit contexts disagree' {
    param($f)
    $later=$f.Context.maps.host['1:14'];$later.revision=15
    foreach($role in @('host','rendered')){$f.Context.maps[$role].Remove('1:14');$f.Context.maps[$role]['1:15']=$later}
    $f.Context.common=@('1:10','1:12','1:13','1:15')
    $f.Log=[regex]::Replace($f.Log,'PROOF_GROUP_GATHER_PHASE([^\r\n]*)',{
        param($m)$row=$m.Groups[1].Value|ConvertFrom-Json
        if($row.phase -eq 'complete'){$row.revision=15}
        'PROOF_GROUP_GATHER_PHASE '+($row|ConvertTo-Json -Compress)
    })
    Change-Deposit $f {param($d)if($d.miner -eq 2){$d.productionAfter.units[0].x=350.5}}
} $false
Write-Output "$checks parser checks passed; synthetic policy fixtures only, no engine execution."
