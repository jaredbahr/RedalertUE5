# Synthetic parser tests only; never launches UE or claims runtime/physical proof.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RefineryMinerEvidence.ps1')
. (Join-Path $PSScriptRoot 'FactoryRallyEvidence.ps1')
# Reuse the maintained refinery-log generator without executing its test batch.
$policyAst=[System.Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'test-refinery-miner-evidence-policy.ps1'),[ref]$null,[ref]$null)
$generator=$policyAst.Find({param($node)$node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'New-SyntheticRefineryLogs'},$true)
if(-not $generator){throw 'Missing maintained synthetic generator'}
. ([scriptblock]::Create($generator.Extent.Text))
function New-RallyLogs([switch]$CashPause,[switch]$CompletionDeposit) {
    $logs=New-SyntheticRefineryLogs
    function Copy($O){return ($O|ConvertTo-Json -Depth 16 -Compress|ConvertFrom-Json -Depth 16)}
    function Digest([int]$R){return ('{0:x32}' -f $R)}
    $base=($logs.host -split '\r?\n'|Where-Object{$_ -like 'PROOF_REFINERY_STATE *'}|Select-Object -Last 1).Substring(21)|ConvertFrom-Json -Depth 16
    $base.units+=,[pscustomobject]@{id=9990;owner=1;kind=4;x=0;y=350;z=0;hp=200;cargo=0;rally=1;rallyX=-600;rallyY=-350;rallyZ=0}
    $current=Copy @{phase=1;jobs=@();units=$base.units;players=@($base.players|ForEach-Object{@{id=$_.id;credits=$_.credits;gathered=$_.gathered}})}
    $initialCredits=if($CashPause){1}else{300};($current.players|Where-Object id -eq 1).credits=$initialCredits
    if($CashPause -or $CompletionDeposit){$miner=@($current.units|Where-Object id -eq 1003)[0];$miner.cargo=50;if($CashPause){$miner.x=30}else{$miner.x=750}}
    $states=@();$advances=@();$events=@();$currentRevision=10
    function State([int]$Revision,$Context){$s=Copy $base;$s.revision=$Revision;$s.digest=Digest $Revision;$s.units=Copy $Context.units;$s.jobs=@($Context.jobs|ForEach-Object{Copy $_});foreach($p in $s.players){$q=@($Context.players|Where-Object id -eq $p.id)[0];$p.credits=$q.credits;$p.gathered=$q.gathered};return $s}
    $setBefore=Copy $current;$unrallied=@($setBefore.units|Where-Object id -eq 9990)[0];$unrallied.rally=0;$unrallied.rallyX=0;$unrallied.rallyY=0
    $states+=,(State 8 $setBefore);$states+=,(State 9 $current);$states+=,(State 10 $current)
    $ids=@($current.units.id)
    $events+=,@{type='SetRally';owner=1;epoch=1;sequence=4;factory=9990;unit=-1;beforeRevision=8;revision=9;beforeDigest=(Digest 8);digest=(Digest 9);creditsBefore=$initialCredits;creditsAfter=$initialCredits;rallyX=-600;rallyY=-350;rallyZ=0;spawnX=0;spawnY=0;spawnZ=0;beforeIds=$ids;afterIds=$ids;productionBefore=$setBefore;productionAfter=(Copy $current);productionPayment=0;productionRefund=0}
    $start=@{type='Produce';owner=1;epoch=1;sequence=5;factory=9990;unit=-1;beforeRevision=10;revision=11;beforeDigest=(Digest 10);digest=(Digest 11);creditsBefore=$initialCredits;creditsAfter=$initialCredits;rallyX=-600;rallyY=-350;rallyZ=0;spawnX=0;spawnY=0;spawnZ=0;beforeIds=$ids;afterIds=$ids;productionBefore=(Copy $current);productionPayment=0;productionRefund=0}
    $current.jobs=@([pscustomobject]@{owner=1;sequence=5;factory=9990;work=0;paid=0;held=0});$start.productionAfter=Copy $current;$events+=,$start;$states+=,(State 11 $current);$currentRevision=11
    $stepCount=if($CashPause){51}else{50}
    for($step=1;$step -le $stepCount;$step++){
        $before=Copy $current;$beforeRevision=$currentRevision;$payment=if($CashPause -and $step -eq 2){0}else{1}
        ($current.players|Where-Object id -eq 1).credits-=$payment
        $paid=if($CashPause -and $step -ge 2){$step-1}else{$step}
        if($step -lt $stepCount){$current.jobs[0].work=$paid*100000;$current.jobs[0].paid=$paid;$currentRevision++}
        else{$current.jobs=@();$current.units+=,[pscustomobject]@{id=9991;owner=1;kind=0;x=300;y=350;z=0;hp=100;cargo=0;rally=0;rallyX=0;rallyY=0;rallyZ=0};$currentRevision+=2}
        if($CompletionDeposit){($current.units|Where-Object id -eq 1003).x=750-15*$step}
        if($CashPause -and $step -eq 1){($current.units|Where-Object id -eq 1003).x=15}
        if(($CashPause -and $step -eq 2) -or ($CompletionDeposit -and $step -eq $stepCount)){
            $miner=@($current.units|Where-Object id -eq 1003)[0];$miner.x=0;$miner.cargo=0
            $payer=@($current.players|Where-Object id -eq 1)[0];$payer.credits+=50;$payer.gathered+=50
        }
        $advance=@{index=$step;owner=1;epoch=1;sequence=5;factory=9990;beforeRevision=$beforeRevision;revision=$currentRevision;beforeDigest=(Digest $beforeRevision);digest=(Digest $currentRevision);productionBefore=$before;productionAfter=(Copy $current);productionPayment=$payment;productionRefund=0};$advances+=,$advance
        if($step -in @(10,25,$stepCount)){$states+=,(State $currentRevision $current)}
    }
    $events+=,@{type='Complete';advanceCount=$stepCount;owner=1;epoch=1;sequence=5;factory=9990;unit=9991;beforeRevision=$advance.beforeRevision;revision=$currentRevision;beforeDigest=$advance.beforeDigest;digest=$advance.digest;creditsBefore=(@($advance.productionBefore.players|Where-Object id -eq 1)[0].credits);creditsAfter=(@($current.players|Where-Object id -eq 1)[0].credits);rallyX=-600;rallyY=-350;rallyZ=0;spawnX=300;spawnY=350;spawnZ=0;beforeIds=$ids;afterIds=@($ids)+9991;productionBefore=$advance.productionBefore;productionAfter=$advance.productionAfter;productionPayment=1;productionRefund=0}
    $current.units[-1].x=255;$current.units[-1].y=315;$states+=,(State ($currentRevision+4) $current)
    $current.units[-1].x=220;$current.units[-1].y=270;$states+=,(State ($currentRevision+8) $current)
    foreach($role in @('host','rendered','observer')){
        $view=if($role -eq 'host'){'host'}else{'client'};$viewer=if($role -eq 'host'){0}elseif($role -eq 'rendered'){1}else{2}
        foreach($source in $states){$s=Copy $source;$s.role=$view;$s.viewer=$viewer;$logs[$role]+="`nPROOF_SNAPSHOT role=$view owner=$viewer epoch=1 revision=$($s.revision) digest=$($s.digest)`nPROOF_REFINERY_STATE "+($s|ConvertTo-Json -Depth 16 -Compress)}
    }
    foreach($e in $events){
        if($e.type -eq 'Complete'){foreach($a in $advances){$logs.host+="`nPROOF_FACTORY_RALLY_ADVANCE "+($a|ConvertTo-Json -Depth 16 -Compress)}}
        $logs.host+="`nPROOF_FACTORY_RALLY_EVENT "+($e|ConvertTo-Json -Depth 16 -Compress)
    }
    foreach($phase in @('rallied','spawned','moved')) {
        $first=$phase -eq 'rallied';$last=$phase -eq 'moved'
        $p=@{phase=$phase;owner=1;epoch=1;revision=$(if($first){10}elseif($last){$currentRevision+8}else{$currentRevision});factory=9990;unit=$(if($first){-1}else{9991});setSequence=4;produceSequence=$(if($first){0}else{5});x=$(if($first){0}elseif($last){220}else{300});y=$(if($first){0}elseif($last){270}else{350});z=0;rallyX=-600;rallyY=-350;rallyZ=0;selected=9990;selectionCount=1;nativeRight=0;elapsed=$(if($first){1}elseif($last){7.2}else{6})}
        $logs.rendered+="`nPROOF_FACTORY_RALLY_FIXTURE "+($p|ConvertTo-Json -Compress)
    }
    $logs.rendered+="`nPROOF_FACTORY_RALLY_CAPTURE name=03-factory-rally.png selected=9990 written=1 physicalInputVerified=0"
    return $logs
}
function Edit-RallyRows($Logs,[string]$Role,[string]$Marker,[scriptblock]$Edit) {
    $Logs[$Role]=($Logs[$Role] -split '\r?\n'|ForEach-Object{
        if($_.StartsWith($Marker+' ')){$o=$_.Substring($Marker.Length+1)|ConvertFrom-Json -Depth 16;& $Edit $o;$Marker+' '+($o|ConvertTo-Json -Depth 16 -Compress)}else{$_}
    }) -join "`n"
}
$rallyChecks=0
function Reject-Rally([string]$Name,[scriptblock]$Mutate,[switch]$CashPause,[switch]$CompletionDeposit) {
    $logs=New-RallyLogs -CashPause:$CashPause -CompletionDeposit:$CompletionDeposit;$before=$logs|ConvertTo-Json -Compress;& $Mutate $logs
    if(($logs|ConvertTo-Json -Compress) -ceq $before){throw "No-op negative mutation: $Name"}
    $rejected=$false;try{Get-FactoryRallyEvidence $logs|Out-Null}catch{$rejected=$true}
    if(-not $rejected){throw "Accepted invalid synthetic rally: $Name"};$script:rallyChecks++
}
$valid=Get-FactoryRallyEvidence (New-RallyLogs)
if(-not $valid.passed -or $valid.commonMovement.Count -ne 2 -or $valid.timedProduction.advanceCount -ne 50){throw 'Rejected valid synthetic rally'};$rallyChecks++
$paused=Get-FactoryRallyEvidence (New-RallyLogs -CashPause)
if(-not $paused.passed -or $paused.timedProduction.advanceCount -ne 51 -or $paused.timedProduction.paid -ne 50){throw 'Rejected cash-pause/deposit/resume synthetic rally'};$rallyChecks++
$deposit=Get-FactoryRallyEvidence (New-RallyLogs -CompletionDeposit)
if(-not $deposit.passed -or $deposit.timedProduction.advanceCount -ne 50 -or $deposit.timedProduction.paid -ne 50){throw 'Rejected simultaneous completion/deposit synthetic rally'};$rallyChecks++
Reject-Rally 'missing observer' {param($l)$l.Remove('observer')}
Reject-Rally 'missing event' {param($l)$l.host=$l.host -replace '(?m)^PROOF_FACTORY_RALLY_EVENT[^\n]*"SetRally"[^\n]*\n?',''}
Reject-Rally 'duplicate event' {param($l)$l.host+="`n"+($l.host -split '\n'|Where-Object{$_ -like 'PROOF_FACTORY_RALLY_EVENT *'}|Select-Object -First 1)}
Reject-Rally 'wrong owner' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)$e.owner=2}}
Reject-Rally 'cross epoch' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.epoch=2}}}
Reject-Rally 'wrong cost' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.creditsAfter=251}}}
Reject-Rally 'sequence string' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)$e.sequence=[string]$e.sequence}}
Reject-Rally 'factory boolean' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)$e.factory=$true}}
Reject-Rally 'extra unit grant' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.afterIds+=9992}}}
Reject-Rally 'existing produced ID' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.beforeIds+=9991}}}
Reject-Rally 'duplicate IDs' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)$e.beforeIds+=9990}}
Reject-Rally 'wrong spawned position' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Complete'){$e.spawnX=-8000;$e.spawnY=-8000}}}
Reject-Rally 'rally changes between commands' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.rallyX=-601}}}
Reject-Rally 'no common rally before produce' {param($l)foreach($r in @('host','rendered','observer')){$l[$r]=$l[$r] -replace '(?m)^PROOF_REFINERY_STATE[^\n]*"revision":(9|10)[,}][^\n]*\n?',''}}
Reject-Rally 'common role rally disagreement' {param($l)Edit-RallyRows $l observer PROOF_REFINERY_STATE {param($s)if($s.revision -ge 10){$s.units[-1].rallyX=-601}}}
Reject-Rally 'not enough later common movement' {param($l)$l.observer=$l.observer -replace '(?m)^PROOF_REFINERY_STATE[^\n]*"revision":70[^\n]*\n?',''}
Reject-Rally 'missing phase' {param($l)$l.rendered=$l.rendered -replace '(?m)^PROOF_FACTORY_RALLY_FIXTURE[^\n]*"phase":"moved"[^\n]*\n?',''}
Reject-Rally 'changed selection' {param($l)Edit-RallyRows $l rendered PROOF_FACTORY_RALLY_FIXTURE {param($p)$p.selected=9991}}
Reject-Rally 'native input changed' {param($l)Edit-RallyRows $l rendered PROOF_FACTORY_RALLY_FIXTURE {param($p)if($p.phase -eq 'moved'){$p.nativeRight=1}}}
Reject-Rally 'fake local movement' {param($l)Edit-RallyRows $l rendered PROOF_FACTORY_RALLY_FIXTURE {param($p)if($p.phase -eq 'moved'){$p.x=201}}}
Reject-Rally 'missing capture' {param($l)$l.rendered=$l.rendered -replace '(?m)^PROOF_FACTORY_RALLY_CAPTURE[^\n]*',''}
Reject-Rally 'malformed empty event' {param($l)$l.host+="`nPROOF_FACTORY_RALLY_EVENT"}
Reject-Rally 'duplicate event key' {param($l)$l.host=$l.host.Replace('"type":"SetRally"','"type":"SetRally","type":"SetRally"')}
Reject-Rally 'published event digest mismatch' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.digest='f'*32}}}
Reject-Rally 'published credit mismatch despite correct cost' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.creditsBefore=1000;$e.creditsAfter=1000}}}
Reject-Rally 'published ID mismatch despite exact added pair' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.beforeIds+=9900;$e.afterIds+=9900}}}
Reject-Rally 'singleton array event root' {param($l)$l.host=[regex]::Replace($l.host,'(?m)^(PROOF_FACTORY_RALLY_EVENT )([^\r\n]*)$',{param($m)$m.Groups[1].Value+'['+$m.Groups[2].Value+']'})}
Reject-Rally 'singleton array fixture root' {param($l)$l.rendered=[regex]::Replace($l.rendered,'(?m)^(PROOF_FACTORY_RALLY_FIXTURE )([^\r\n]*)$',{param($m)$m.Groups[1].Value+'['+$m.Groups[2].Value+']'})}
Reject-Rally 'rallied phase claims future moved revision' {param($l)Edit-RallyRows $l rendered PROOF_FACTORY_RALLY_FIXTURE {param($p)if($p.phase -eq 'rallied'){$p.revision=70}}}
Reject-Rally 'rallied phase occurs after production' {param($l)Edit-RallyRows $l rendered PROOF_FACTORY_RALLY_FIXTURE {param($p)if($p.phase -eq 'rallied'){$p.revision=62}}}
Reject-Rally 'rallied and spawned revisions reversed' {param($l)Edit-RallyRows $l rendered PROOF_FACTORY_RALLY_FIXTURE {param($p)if($p.phase -eq 'rallied'){$p.revision=66}}}

Reject-Rally 'completion sequence spliced' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Complete'){$e.sequence=6}}}
Reject-Rally 'completion factory spliced' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Complete'){$e.factory=9992}}}
Reject-Rally 'completion refunds spent credits' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Complete'){$e.productionRefund=50}}}
Reject-Rally 'completion payment numeric string' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Complete'){$e.productionPayment='1'}}}
Reject-Rally 'start immediately spawns' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.unit=9991}}}
Reject-Rally 'start has paid work' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.productionAfter.jobs[0].paid=1;$e.productionAfter.jobs[0].work=100000}}}
Reject-Rally 'start is held' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.productionAfter.jobs[0].held=1}}}
Reject-Rally 'Advance pays twice' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_ADVANCE {param($a)if($a.revision -eq 12){$a.productionPayment=2}}}
Reject-Rally 'Advance refunds while progressing' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_ADVANCE {param($a)if($a.revision -eq 12){$a.productionRefund=1}}}
Reject-Rally 'Advance changes hold' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_ADVANCE {param($a)if($a.revision -eq 12){$a.productionAfter.jobs[0].held=1}}}
Reject-Rally 'Advance unpaid work' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_ADVANCE {param($a)if($a.revision -eq 12){$a.productionAfter.jobs[0].work=200000}}}
Reject-Rally 'Advance changed job sequence' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_ADVANCE {param($a)if($a.revision -eq 12){$a.productionAfter.jobs[0].sequence=6}}}
Reject-Rally 'Advance cross epoch' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_ADVANCE {param($a)if($a.revision -eq 12){$a.epoch=2}}}
Reject-Rally 'Advance missing jobs' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_ADVANCE {param($a)if($a.revision -eq 12){$a.productionAfter.PSObject.Properties.Remove('jobs')}}}
Reject-Rally 'missing paid Advance' {param($l)$l.host=$l.host -replace '(?m)^PROOF_FACTORY_RALLY_ADVANCE[^\n]*"revision":12[,}][^\n]*\n?',''}
Reject-Rally 'duplicate Advance' {param($l)$l.host+="`n"+($l.host -split '\n'|Where-Object{$_ -like 'PROOF_FACTORY_RALLY_ADVANCE *'}|Select-Object -First 1)}
Reject-Rally 'Advance outside host' {param($l)$l.observer+='PROOF_FACTORY_RALLY_ADVANCE {}'}
Reject-Rally 'completion outside host' {param($l)$l.rendered+='PROOF_FACTORY_RALLY_EVENT {}'}
Reject-Rally 'missing common job progress' {param($l)$l.observer=$l.observer -replace '(?m)^PROOF_REFINERY_STATE[^\n]*"revision":(21|36)[,}][^\n]*\n?',''}
Reject-Rally 'observer job differs at same digest' {param($l)Edit-RallyRows $l observer PROOF_REFINERY_STATE {param($s)if($s.revision -eq 21){$s.jobs[0].paid=11;$s.jobs[0].work=1100000}}}
Reject-Rally 'completion actual spawn changed' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Complete'){$e.productionAfter.units[-1].x=301}}}
Reject-Rally 'extra malformed capture' {param($l)$l.rendered+="`nPROOF_FACTORY_RALLY_CAPTURE malformed"}
Reject-Rally 'trailing capture junk' {param($l)$l.rendered=$l.rendered.Replace('physicalInputVerified=0','physicalInputVerified=0 junk')}
Reject-Rally 'cash pause charges from same-step deposit' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_ADVANCE {param($a)if($a.revision -eq 13){$a.productionPayment=1;$a.productionAfter.jobs[0].paid=2;$a.productionAfter.jobs[0].work=200000;($a.productionAfter.players|Where-Object id -eq 1).credits--}}} -CashPause
Reject-Rally 'cash pause doubles resume work' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_ADVANCE {param($a)if($a.revision -eq 14){$a.productionPayment=2;$a.productionAfter.jobs[0].paid=3;$a.productionAfter.jobs[0].work=300000;($a.productionAfter.players|Where-Object id -eq 1).credits--}}} -CashPause
Reject-Rally 'completion deposit is hidden in payment' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_ADVANCE {param($a)if($a.revision -eq 62){$a.productionPayment=0}}} -CompletionDeposit
Reject-Rally 'completion deposit missing gathered credit' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_ADVANCE {param($a)if($a.revision -eq 62){($a.productionAfter.players|Where-Object id -eq 1).gathered-=50}}} -CompletionDeposit
Reject-Rally 'oversized unchanged phase' {param($l)Edit-RallyRows $l rendered PROOF_FACTORY_RALLY_FIXTURE {param($p)$p|Add-Member -NotePropertyName padding -NotePropertyValue ('x'*4096)}}
Reject-Rally 'negative phase time' {param($l)Edit-RallyRows $l rendered PROOF_FACTORY_RALLY_FIXTURE {param($p)if($p.phase -eq 'rallied'){$p.elapsed=-0.5}}}

Reject-Rally 'missing cash-paused no-work Advance' {param($l)$l.host=$l.host -replace '(?m)^PROOF_FACTORY_RALLY_ADVANCE[^\n]*"index":2[,}][^\n]*\n?',''} -CashPause
Reject-Rally 'missing no-work Advance with lowered completion count' {param($l)
    $l.host=$l.host -replace '(?m)^PROOF_FACTORY_RALLY_ADVANCE[^\n]*"index":2[,}][^\n]*\n?',''
    Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Complete'){$e.advanceCount--}}
} -CashPause
Reject-Rally 'Advance index duplicated without duplicate row' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_ADVANCE {param($a)if($a.index -eq 2){$a.index=1}}}
Reject-Rally 'Advance index string' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_ADVANCE {param($a)if($a.index -eq 1){$a.index='1'}}}
Reject-Rally 'Advance index missing' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_ADVANCE {param($a)if($a.index -eq 1){$a.PSObject.Properties.Remove('index')}}}
Reject-Rally 'completion false Advance count' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Complete'){$e.advanceCount++}}}
Reject-Rally 'completion missing Advance count' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Complete'){$e.PSObject.Properties.Remove('advanceCount')}}}
Reject-Rally 'completion Advance count string' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Complete'){$e.advanceCount='50'}}}
Reject-Rally 'raw Advance rows reordered' {param($l)
    $lines=@($l.host -split '\r?\n');$positions=@(0..($lines.Count-1)|Where-Object{$lines[$_].StartsWith('PROOF_FACTORY_RALLY_ADVANCE ')})
    $saved=$lines[$positions[0]];$lines[$positions[0]]=$lines[$positions[1]];$lines[$positions[1]]=$saved;$l.host=$lines -join "`n"
}
Reject-Rally 'last Advance occurs after Complete' {param($l)
    $lines=@($l.host -split '\r?\n');$position=@(0..($lines.Count-1)|Where-Object{$lines[$_].StartsWith('PROOF_FACTORY_RALLY_ADVANCE ') -and $lines[$_] -match '"index":50[,}]'})[0]
    $saved=$lines[$position];$l.host=(@(for($i=0;$i -lt $lines.Count;$i++){if($i -ne $position){$lines[$i]}})+$saved) -join "`n"
}
Reject-Rally 'first Advance occurs before Produce' {param($l)
    $lines=@($l.host -split '\r?\n');$position=@(0..($lines.Count-1)|Where-Object{$lines[$_].StartsWith('PROOF_FACTORY_RALLY_ADVANCE ')})[0]
    $saved=$lines[$position];$l.host=(@($saved)+@(for($i=0;$i -lt $lines.Count;$i++){if($i -ne $position){$lines[$i]}})) -join "`n"
}

Reject-Rally 'SetRally missing before context' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'SetRally'){$e.PSObject.Properties.Remove('productionBefore')}}}
Reject-Rally 'SetRally missing after context' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'SetRally'){$e.PSObject.Properties.Remove('productionAfter')}}}
Reject-Rally 'SetRally falsely claims production debit' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'SetRally'){$e.productionPayment=1}}}
Reject-Rally 'SetRally falsely claims production refund' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'SetRally'){$e.productionRefund=1}}}
Reject-Rally 'SetRally missing zero payment' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'SetRally'){$e.PSObject.Properties.Remove('productionPayment')}}}
Reject-Rally 'SetRally string zero refund' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'SetRally'){$e.productionRefund='0'}}}
Reject-Rally 'SetRally context credits disagree' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'SetRally'){($e.productionAfter.players|Where-Object id -eq 1).credits++}}}
Reject-Rally 'SetRally context IDs disagree' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'SetRally'){$e.productionAfter.units[0].id=9900}}}
Reject-Rally 'SetRally self-consistent unrelated miner damage' {param($l)
    Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'SetRally'){($e.productionAfter.units|Where-Object id -eq 1003).hp=99}}
    foreach($role in @('host','rendered','observer')){Edit-RallyRows $l $role PROOF_REFINERY_STATE {param($s)if($s.revision -eq 9){($s.units|Where-Object id -eq 1003).hp=99}}}
}
Reject-Rally 'SetRally self-consistent other player grant' {param($l)
    Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'SetRally'){($e.productionAfter.players|Where-Object id -eq 2).credits++}}
    foreach($role in @('host','rendered','observer')){Edit-RallyRows $l $role PROOF_REFINERY_STATE {param($s)if($s.revision -eq 9){($s.players|Where-Object id -eq 2).credits++}}}
}
Reject-Rally 'SetRally self-consistent unexpected job' {param($l)
    Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'SetRally'){$e.productionAfter.jobs=@([pscustomobject]@{owner=1;sequence=99;factory=9990;work=0;paid=0;held=0})}}
    foreach($role in @('host','rendered','observer')){Edit-RallyRows $l $role PROOF_REFINERY_STATE {param($s)if($s.revision -eq 9){$s.jobs=@([pscustomobject]@{owner=1;sequence=99;factory=9990;work=0;paid=0;held=0})}}}
}
Write-Output "PASS synthetic-only factory rally parser checks=$rallyChecks; no engine/runtime/physical evidence"
