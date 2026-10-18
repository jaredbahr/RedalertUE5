$ErrorActionPreference='Stop'
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
    $lines+='PROOF_GROUP_GATHER_CAPTURE name=03-group-gather.png selectedCount=2 written=1 physicalInputVerified=0'
    $maps=@{host=@{};rendered=@{}}
    foreach($revision in 10,12,13,14){
        $loaded=$revision -eq 13;$deposited=$revision -eq 14
        $state=[pscustomobject]@{epoch=1;revision=$revision;units=@(
            [pscustomobject]@{id=2;owner=1;kind=1;hp=100;cargo=$(if($loaded){50}else{0});x=$(if($loaded){550}elseif($deposited){350}else{100});y=0;z=0},
            [pscustomobject]@{id=4;owner=1;kind=1;hp=100;cargo=$(if($loaded){50}else{0});x=$(if($loaded){550}elseif($deposited){350}else{650});y=0;z=0},
            [pscustomobject]@{id=3;owner=1;kind=3;hp=200;cargo=0;x=350;y=0;z=0});
            resources=@([pscustomobject]@{id=1;remaining=400});players=@([pscustomobject]@{id=1;gathered=$(if($deposited){100}else{0})})}
        $maps.host["1:$revision"]=$state;$maps.rendered["1:$revision"]=$state
    }
    return [pscustomobject]@{Log=($lines -join "`r`n");Context=@{maps=$maps;common=@('1:10','1:12','1:13','1:14')};Refinery=@{passed=$true;grants=@(@{owner=1;epoch=1;miner=4;resource=1;gatherSequence=4})}}
}
$checks=0
function Check([string]$Name,[scriptblock]$Mutate,[bool]$Expected){
    $fixture=New-Fixture
    & $Mutate $fixture
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
Check 'string sequence' {param($f)$f.Log=$f.Log.Replace('"sequence":3,','"sequence":"3",')} $false
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
Check 'wrong deposit location' {param($f)$f.Context.maps.host['1:14'].units[0].x=800} $false
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
Write-Output "$checks parser checks passed; synthetic policy fixtures only, no engine execution."
