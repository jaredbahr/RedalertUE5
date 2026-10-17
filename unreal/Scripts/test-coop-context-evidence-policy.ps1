# Synthetic policy tests only. Context stands in for already validated shared states.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CoopContextEvidence.ps1')
$script:coopChecks=0
function New-CoopCase {
    $maps=@{host=@{};rendered=@{};observer=@{}};$common=@()
    foreach($rev in 1..14){
        $combatX=if($rev -le 3){0}elseif($rev -le 7){[math]::Min(200,($rev-3)*100)}else{200+($rev-7)*100}
        $harvesterX=if($rev -le 7){0}else{($rev-7)*100}
        $s=[pscustomobject]@{epoch=1;revision=$rev;phase=1;units=@([pscustomobject]@{id=10;owner=1;kind=0;hp=100;x=$combatX;y=0;z=0;cargo=0},[pscustomobject]@{id=11;owner=1;kind=1;hp=100;x=$harvesterX;y=0;z=0;cargo=0},[pscustomobject]@{id=12;owner=1;kind=4;hp=200;x=-100;y=0;z=0},[pscustomobject]@{id=20;owner=2;kind=2;hp=300;x=1000;y=0;z=0},[pscustomobject]@{id=30;owner=100;kind=2;hp=300;x=3000;y=0;z=0});players=@([pscustomobject]@{id=1;team=0},[pscustomobject]@{id=2;team=0},[pscustomobject]@{id=100;team=1});resources=@([pscustomobject]@{id=40;remaining=100;x=500;y=500;z=0})}
        $key="1:$rev";$common+=$key;foreach($role in @('host','rendered','observer')){$maps[$role][$key]=$s}
    }
    $phases=@();$specs=@(@('factory_hostile_rejected',1,3,3,12,-1,30,3000,0),@('factory_allied_rally',2,3,4,12,-1,20,1000,0),@('single_ally_issued',3,5,6,10,-1,20,1000,0),@('single_ally_moved',6,5,6,10,-1,20,1000,0),@('group_ally_issued',7,6,6,10,11,20,1000,0),@('group_ally_moved',10,6,8,10,11,20,1000,0),@('single_enemy_issued',11,8,9,10,-1,30,3000,0),@('group_enemy_issued',12,9,9,10,11,30,3000,0),@('resource_issued',13,10,11,11,-1,40,500,500),@('context_complete',14,10,11,11,-1,40,500,500))
    foreach($s in $specs){$phases+=[ordered]@{phase=$s[0];epoch=1;revision=$s[1];beforeSequence=$s[2];sequence=$s[3];unit=$s[4];secondary=$s[5];target=$s[6];x=$s[7];y=$s[8];z=0;nativeRight=0;elapsed=[double]$s[1]}}
    $commands=@();foreach($s in @(@(4,7,12,-1,1000,0,2),@(5,2,12,-1,0,0,3),@(6,0,10,-1,1000,0,4),@(7,0,10,-1,1100,0,8),@(8,0,11,-1,900,0,8),@(9,1,10,30,0,0,11),@(10,1,10,30,0,0,12),@(11,5,11,40,0,0,14))){$commands+=[ordered]@{owner=1;epoch=1;sequence=$s[0];type=$s[1];unit=$s[2];target=$s[3];x=$s[4];y=$s[5];z=0;accepted=1;revision=$s[6]}}
    $phases[1].revision=1;$phases[1].elapsed=1
    # Issued phases precede their asynchronous accepted host command revision.
    $phases[6].revision=10;$phases[7].revision=11
    $startup=@()
    foreach($spec in @(@(1,3,-1,-1),@(2,4,-1,-1),@(3,5,11,40))){$startup+=[ordered]@{owner=1;epoch=1;sequence=$spec[0];type=$spec[1];unit=$spec[2];target=$spec[3];x=0;y=0;z=0;accepted=1;revision=1}}

    return @{phases=$phases;commands=$commands;startup=$startup;context=@{maps=$maps;common=$common};rally=@{passed=$true;epoch=1;factory=12;unit=10;setSequence=4;produceSequence=5;phaseRows=@(@{rallyX=1000;rallyY=0;rallyZ=0})};logs=@{host='';rendered='';observer=''}}
}
function Evaluate-Coop($Case){
    $logs=@{host=$Case.logs.host;rendered=$Case.logs.rendered;observer=$Case.logs.observer}
    foreach($row in @($Case.startup)+@($Case.commands)){$logs.host+="`nPROOF_COOP_CONTEXT_COMMAND "+($row|ConvertTo-Json -Compress)}
    foreach($row in $Case.phases){$logs.rendered+="`nPROOF_COOP_CONTEXT_PHASE "+($row|ConvertTo-Json -Compress)}
    return Get-CoopContextEvidence -Logs $logs -ValidatedStateContext $Case.context -RallyEvidence $Case.rally
}
function Reject-Coop([string]$Name,[scriptblock]$Edit){$case=New-CoopCase;&$Edit $case;$failed=$false;try{Evaluate-Coop $case|Out-Null}catch{$failed=$true};if(-not $failed){throw "Invalid coop accepted: $Name"};$script:coopChecks++}
$good=Evaluate-Coop (New-CoopCase);if(-not $good.passed -or $good.singleCommonMovement.Count -lt 2 -or $good.groupCommonMovement.Count -lt 2 -or $good.physicalInputVerified){throw 'Good synthetic rejected'};$script:coopChecks++
Reject-Coop 'missing phase' {param($c)$c.phases=@($c.phases|Select-Object -First 9)}
Reject-Coop 'phase reorder' {param($c)$c.phases[2].phase='single_ally_moved'}
Reject-Coop 'duplicate phase' {param($c)$c.phases+=,$c.phases[0]}
Reject-Coop 'string revision' {param($c)$c.phases[0].revision='1'}
Reject-Coop 'bool coordinate' {param($c)$c.phases[2].x=$true}
Reject-Coop 'fractional sequence' {param($c)$c.phases[2].sequence=6.5}
Reject-Coop 'native counter changed' {param($c)$c.phases[2].nativeRight=1}
Reject-Coop 'bad phase identity' {param($c)$c.phases[2].unit=11}
Reject-Coop 'bad selection' {param($c)$c.phases[4].secondary=10}
Reject-Coop 'wrong phase team' {param($c)$c.context.maps.rendered['1:3'].players[1].team=1}
Reject-Coop 'unknown team' {param($c)$c.context.maps.rendered['1:3'].players[1].team=-1}
Reject-Coop 'dead target' {param($c)$c.context.maps.rendered['1:3'].units[3].hp=0}
Reject-Coop 'missing exact rendered state' {param($c)$c.context.maps.rendered.Remove('1:3')}
Reject-Coop 'finished match' {param($c)$c.context.maps.rendered['1:3'].phase=2}
Reject-Coop 'wrong command route' {param($c)$c.commands[2].type=1;$c.commands[2].target=20}
Reject-Coop 'group omitted harvester' {param($c)$c.commands=@($c.commands|Where-Object sequence -ne 8)}
Reject-Coop 'group wrong offset' {param($c)$c.commands[3].x=1000}
Reject-Coop 'group enemy harvester attacks' {param($c)$c.commands[6].unit=11}
Reject-Coop 'host rejected command' {param($c)$c.commands[2].accepted=0}
Reject-Coop 'duplicate accepted sequence' {param($c)$c.commands+=,$c.commands[7]}
Reject-Coop 'sequence gap' {param($c)$c.commands[4].sequence=88}
Reject-Coop 'cross epoch' {param($c)$c.commands[0].epoch=2}
Reject-Coop 'factory negative allocates' {param($c)$c.phases[0].sequence=4}
Reject-Coop 'factory action not synchronous' {param($c)$c.phases[1].revision=2}
Reject-Coop 'command predates action' {param($c)$c.commands[2].revision=3}
Reject-Coop 'click destination unrelated to target' {param($c)$c.phases[0].x=1000}
Reject-Coop 'resource click unrelated to target' {param($c)$c.phases[8].y=0;$c.phases[9].y=0}
Reject-Coop 'coherent ally click and command miss HQ' {param($c)$c.phases[2].x=1500;$c.phases[3].x=1500;$c.commands[2].x=1500}
Reject-Coop 'coherent allied rally misses HQ' {param($c)$c.phases[1].x=1500;$c.commands[0].x=1500;$c.rally.phaseRows[0].rallyX=1500}
Reject-Coop 'hostile Attack nonzero destination' {param($c)$c.commands[5].x=3000}
Reject-Coop 'Gather nonzero destination' {param($c)$c.commands[7].x=500}
Reject-Coop 'single Move noncanonical target' {param($c)$c.commands[2].target=20}
Reject-Coop 'group Move noncanonical target' {param($c)$c.commands[3].target=20}
Reject-Coop 'movement ownership changes' {param($c)$c.context.maps.host['1:9'].units[1].owner=2}
Reject-Coop 'rally mismatch' {param($c)$c.rally.setSequence=8}
Reject-Coop 'rally destination mismatch' {param($c)$c.rally.phaseRows[0].rallyX=500}
Reject-Coop 'unvalidated rally' {param($c)$c.rally.passed=$false}
Reject-Coop 'move destination changes' {param($c)$c.phases[3].x=500}
Reject-Coop 'short movement window' {param($c)$c.phases[3].elapsed=3.5}
Reject-Coop 'short completion hold' {param($c)$c.phases[9].elapsed=13.1}
Reject-Coop 'resource changed' {param($c)$c.phases[9].target=41}
Reject-Coop 'missing Gather receipt' {param($c)$c.commands=@($c.commands|Where-Object type -ne 5)}
Reject-Coop 'late Gather receipt' {param($c)$c.commands[7].revision=15}
Reject-Coop 'late single Move receipt' {param($c)$c.commands[2].revision=7}
Reject-Coop 'late group Move receipt' {param($c)$c.commands[4].revision=11}
Reject-Coop 'nonbuild negative actor' {param($c)$c.commands[2].unit=-1}
Reject-Coop 'build actor below sentinel' {param($c)$c.startup[0].unit=-2}
Reject-Coop 'build actor zero' {param($c)$c.startup[0].unit=0}
Reject-Coop 'build actor string' {param($c)$c.startup[0].unit='-1'}
Reject-Coop 'Attack beyond completion' {param($c)$c.commands[6].revision=15;$c.commands[7].revision=15}
Reject-Coop 'no common movement' {param($c)$c.context.common=@('1:1','1:2','1:3')}
Reject-Coop 'no actual group movement' {param($c)foreach($r in 8..10){$c.context.maps.host["1:$r"].units[1].x=0}}
Reject-Coop 'movement away from destination' {param($c)foreach($r in 4..6){$c.context.maps.host["1:$r"].units[0].x=-500}}
Reject-Coop 'unknown key' {param($c)$c.commands[0].extra=1}
Reject-Coop 'malformed tag' {param($c)$c.logs.rendered='PROOF_COOP_CONTEXT_PHASE {'}
Reject-Coop 'empty tag' {param($c)$c.logs.host='PROOF_COOP_CONTEXT_COMMAND'}
Reject-Coop 'duplicate JSON key' {param($c)$c.logs.host='PROOF_COOP_CONTEXT_COMMAND {"owner":1,"owner":1}'}
Reject-Coop 'runtime error' {param($c)$c.logs.observer='PROOF_COOP_CONTEXT_ERROR test'}
Reject-Coop 'wrong-role command' {param($c)$c.logs.observer='PROOF_COOP_CONTEXT_COMMAND {}'}
Reject-Coop 'command cap' {param($c)$c.commands=@($c.commands[0])*257}
# Accepted receipts may be newer than the rendered snapshot at the next action.
$stalePublication=New-CoopCase
$stalePublication.commands[5].revision=12
$stalePublication.commands[6].revision=13
if(-not (Evaluate-Coop $stalePublication).passed){throw 'Valid receipt-before-snapshot publication order rejected'};$script:coopChecks++
Reject-Coop 'post-rally sequence gap' {param($c)$c.rally.produceSequence=4}
Write-Output "PASS synthetic-only Coop context evidence checks=$script:coopChecks; no engine/runtime/physical evidence"
