# Synthetic policy validation only. No UE process, model, transport, or physical input.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RefineryMinerEvidence.ps1')
function New-SyntheticRefineryLogs([int[]]$Owners=@(1,2,100),[switch]$ExactDeposit) {
    $events=@();$states=@()
    foreach($o in $owners){
        $r=1000+$o*2;$m=$r+1
        $events+=@{event='grant';epoch=1;owner=$o;sequence=1;refinery=$r;miner=$m;beforeRevision=1;revision=2;beforeDigest=('0'*31+'1');digest=('0'*31+'2');added=2;refineriesAdded=1;minersAdded=1;creditsBefore=350;creditsAfter=250;cost=100;x=300;y=0;z=0;rx=0;ry=0;rz=0;beforeIds=@(1,2);afterIds=@(1,2,$r,$m)}
        $events+=@{event='gather';epoch=1;owner=$o;sequence=2;refinery=$r;miner=$m;resource=$o;revision=3}
        $events+=@{event='load';epoch=1;owner=$o;refinery=$r;miner=$m;resource=$o;revision=4;cargo=50;oreBefore=500;oreAfter=450}
        $events+=@{event='deposit';epoch=1;owner=$o;refinery=$r;depot=$r;miner=$m;revision=6;cargoBefore=50;cargoAfter=0;depositSum=50;creditsBefore=250;creditsAfter=300;gatheredBefore=0;gatheredAfter=50;contributors=@(@{miner=$m;before=50;after=0;depot=$r})}
    }
    foreach($rev in @(3,5,7)){
        $s=[ordered]@{epoch=1;revision=$rev;digest=('0'*31+$rev);phase=1;winner=-1;units=@();players=@();resources=@();jobs=@()}
        foreach($o in $owners){$r=1000+$o*2;$m=$r+1
            $s.units+=@{id=$r;owner=$o;kind=3;x=0;y=0;z=0;hp=200;cargo=0}
            $s.units+=@{id=$m;owner=$o;kind=1;x=300;y=0;z=0;hp=100;cargo=$(if($rev -eq 5){50}else{0})}
            $s.players+=@{id=$o;team=$(if($o -eq 100){1}else{0});credits=$(if($rev -eq 7){300}else{250});gathered=$(if($rev -eq 7){50}else{0})}
            $s.resources+=@{id=$o;owner=$o;x=600;y=0;z=0;remaining=$(if($rev -ge 5){450}else{500})}
        };$states+=,$s
    }
    foreach($state in $states){foreach($unit in $state.units){$unit.rally=0;$unit.rallyX=0;$unit.rallyY=0;$unit.rallyZ=0}}
    $loadedState=@($states|Where-Object revision -eq 5)[0]
    $depositBefore=[ordered]@{phase=1;jobs=@();units=$loadedState.units;players=$loadedState.players}|ConvertTo-Json -Depth 16 -Compress|ConvertFrom-Json -Depth 16
    $depositAfter=$depositBefore|ConvertTo-Json -Depth 16 -Compress|ConvertFrom-Json -Depth 16
    foreach($unit in $depositAfter.units){if($unit.kind -eq 1){$unit.cargo=0;$unit.x=0}}
    foreach($player in $depositAfter.players){$player.credits=300;$player.gathered=50}
    foreach($event in $events|Where-Object event -eq deposit){
        $event.beforeRevision=5;$event.productionPayment=0;$event.productionRefund=0;$event.productionBefore=$depositBefore;$event.productionAfter=$depositAfter
    }
    if($ExactDeposit){
        $exact=$loadedState|ConvertTo-Json -Depth 16 -Compress|ConvertFrom-Json -Depth 16
        $exact.revision=6;$exact.digest=('0'*31+'6');$exact.units=$depositAfter.units;$exact.players=$depositAfter.players
        $states+=,$exact;$states=@($states|Sort-Object revision)
    }
    $logs=@{}
    foreach($role in @('host','rendered','observer')){
        $view=if($role -eq 'host'){'host'}else{'client'};$v=if($role -eq 'host'){0}elseif($role -eq 'observer'){2}else{1};$lines=@()
        if($role -eq 'host'){foreach($e in $events){$lines+='PROOF_REFINERY_EVENT '+($e|ConvertTo-Json -Compress -Depth 12)}}
        foreach($s in $states){$row=[ordered]@{role=$view;viewer=$v};if($s -is [Collections.IDictionary]){foreach($k in $s.Keys){$row[$k]=$s[$k]}}else{foreach($property in $s.PSObject.Properties){$row[$property.Name]=$property.Value}};$lines+="PROOF_SNAPSHOT role=$view owner=$v epoch=1 revision=$($s.revision) digest=$($s.digest)";$lines+='PROOF_REFINERY_STATE '+($row|ConvertTo-Json -Compress -Depth 12)}
        $logs[$role]=$lines -join "`n"
    };return $logs
}
function Edit-Event($Logs,[string]$Kind,[scriptblock]$Edit) {
    $Logs.host=(@($Logs.host -split "`n" | ForEach-Object {
        if($_ -like 'PROOF_REFINERY_EVENT *') { $e=$_.Substring(21)|ConvertFrom-Json; if($e.event -eq $Kind){ &$Edit $e }; 'PROOF_REFINERY_EVENT '+($e|ConvertTo-Json -Compress -Depth 12) } else { $_ }
    }) -join "`n")
}
$checks=0
function Reject([string]$Name,[scriptblock]$Mutate){$logs=New-SyntheticRefineryLogs;$before=$logs|ConvertTo-Json -Compress;&$Mutate $logs;if(($logs|ConvertTo-Json -Compress) -ceq $before){throw "Synthetic mutation was a no-op: $Name"};$failed=$false;try{Get-RefineryMinerEvidence $logs Coop|Out-Null}catch{$failed=$true};if(-not $failed){throw "Synthetic negative accepted: $Name"};$script:checks++}
$valid=Get-RefineryMinerEvidence (New-SyntheticRefineryLogs) Coop
if(-not $valid.passed -or $valid.grants.Count -ne 3 -or $valid.commonEpochRevisions.Count -ne 3){throw 'Valid synthetic rejected'};$checks++
foreach($proof in $valid.grants){if($proof.gatherSequence -ne 2 -or $proof.resource -ne $proof.owner -or $proof.checkpoints.loaded -cne '1:5'){throw 'Valid final common-load proof lost Gather identity'};$checks++}
Reject 'missing Gather' {param($x)$x.host=$x.host -replace '(?m)^PROOF_REFINERY_EVENT [^\n]*"event":"gather"[^\n]*\n',''}
Reject 'wrong cost' {param($x)$x.host=$x.host.Replace('"cost":100','"cost":99')}
Reject 'duplicate grant ID' {param($x)$x.host=$x.host.Replace('"miner":1005','"miner":1003')}
Reject 'wrong Gather miner' {param($x) Edit-Event $x gather {param($e) $e.miner=999} }
Reject 'wrong credit attribution' {param($x)$x.host=$x.host.Replace('"creditsAfter":300','"creditsAfter":301')}
Reject 'starter-only contribution' {param($x) Edit-Event $x deposit {param($e) $e.contributors[0].miner=999} }
Reject 'missing load' {param($x)$x.host=$x.host -replace '(?m)^PROOF_REFINERY_EVENT [^\n]*"event":"load"[^\n]*\n',''}
Reject 'missing deposit' {param($x)$x.host=$x.host -replace '(?m)^PROOF_REFINERY_EVENT [^\n]*"event":"deposit"[^\n]*\n',''}
Reject 'common row wrong cargo same digest' {param($x)$x.observer=$x.observer.Replace('"cargo":50','"cargo":49')}
Reject 'ordinary digest mismatch' {param($x)$x.observer=$x.observer.Replace('revision=5 digest=00000000000000000000000000000005','revision=5 digest=00000000000000000000000000000009')}
Reject 'cross epoch splice' {param($x)$x.host=$x.host.Replace('"epoch":1','"epoch":2')}
Reject 'loaded checkpoint unavailable' {param($x)$x.observer=$x.observer -replace '(?m)^PROOF_REFINERY_STATE [^\n]*"revision":5[^\n]*(\n|$)',''}
Reject 'runtime failure' {param($x)$x.rendered+="`nPROOF_REFINERY_ERROR reason=ore-conservation"}
Reject 'duplicate JSON key' {param($x)$x.host=$x.host.Replace('"added":2','"added":2,"added":2')}
Reject 'fractional ID' {param($x)$x.observer=$x.observer.Replace('"id":1003','"id":1003.5')}
Reject 'numeric string ID' {param($x)$x.observer=$x.observer.Replace('"id":1003','"id":"1003"')}
Reject 'malformed JSON' {param($x)$x.host+="`nPROOF_REFINERY_EVENT {"}
Reject 'duplicate semantic row' {param($x)$x.host+="`n"+($x.host -split "`n" |Where-Object {$_ -like 'PROOF_REFINERY_STATE *'}|Select-Object -First 1)}
Reject 'wrong owner' {param($x)$x.host=$x.host.Replace('"owner":100','"owner":101')}
Reject 'grant pair existed before' {param($x) Edit-Event $x grant {param($e) $e.beforeIds=@($e.refinery,$e.miner)} }
Reject 'numeric string cost' {param($x) Edit-Event $x grant {param($e) $e.cost='100'} }
Reject 'bool count' {param($x) Edit-Event $x grant {param($e) $e.added=$true} }
Reject 'numeric string cargoAfter' {param($x) Edit-Event $x deposit {param($e) $e.cargoAfter='0'} }
Reject 'wrong resource' {param($x) Edit-Event $x gather {param($e) $e.resource=999} }
Reject 'wrong tracked depot' {param($x) Edit-Event $x deposit {param($e) $e.depot=999} }
Reject 'nonhex digest' {param($x)$x.observer=$x.observer.Replace(('0'*31+'5'),('z'*32))}
# Standalone joins the two distinct actual roles in one log, without guessing viewer.
$logs=New-SyntheticRefineryLogs;$both=$logs.host+"`n"+$logs.rendered;$logs.host=$both;$logs.rendered=$both
Get-RefineryMinerEvidence $logs Coop|Out-Null;$checks++
foreach($scenario in @('Victory','Defeat')) {
    $logs=New-SyntheticRefineryLogs @(1,100);$both=$logs.host+"`n"+$logs.rendered
    $summary=Get-RefineryMinerEvidence @{host=$both;rendered=$both} $scenario
    if(-not $summary.passed -or $summary.grants.Count -ne 2){throw 'Valid standalone synthetic rejected'};$checks++
}
Reject 'load cross-epoch phase splice' {param($x) Edit-Event $x load {param($e) $e.epoch=2} }
Reject 'missing before ID set' {param($x) Edit-Event $x grant {param($e) $e.beforeIds=$null} }
Reject 'bool spawn coordinate' {param($x) Edit-Event $x grant {param($e) $e.x=$true} }
Reject 'contributors object instead of array' {param($x) Edit-Event $x deposit {param($e) $e.contributors=$e.contributors[0]} }
Reject 'empty malformed event suffix' {param($x)$x.host+="`nPROOF_REFINERY_EVENT"}
function Edit-RefineryStates($Logs,[scriptblock]$Edit){
    foreach($role in @('host','rendered','observer')){
        $Logs[$role]=($Logs[$role] -split '\r?\n'|ForEach-Object{
            if($_ -like 'PROOF_REFINERY_STATE *'){$row=$_.Substring(21)|ConvertFrom-Json -Depth 20;& $Edit $row;'PROOF_REFINERY_STATE '+($row|ConvertTo-Json -Depth 20 -Compress)}else{$_}
        }) -join [Environment]::NewLine
    }
}
function Add-RallyFactory($Logs){
    Edit-RefineryStates $Logs {param($s)$s.units+=[pscustomobject]@{id=2000;owner=1;kind=4;x=500;y=500;z=0;hp=200;cargo=0;rally=1;rallyX=123.0000001;rallyY=-456.25;rallyZ=0}}
    # The same unchanged factory is present at each exact deposit context.
    Edit-Event $Logs deposit {param($e)
        foreach($side in @('productionBefore','productionAfter')){
            $e.$side.units+=[pscustomobject]@{id=2000;owner=1;kind=4;x=500;y=500;z=0;hp=200;cargo=0;rally=1;rallyX=123.0000001;rallyY=-456.25;rallyZ=0}
        }
    }
}
$logs=New-SyntheticRefineryLogs;Add-RallyFactory $logs
if(-not (Get-RefineryMinerEvidence $logs Coop).passed){throw 'Valid nonzero factory rally semantic state rejected'};$checks++
foreach($field in @('rally','rallyX','rallyY','rallyZ')){
    $missingField=$field
    Reject "missing required unit $field" {param($x)Edit-RefineryStates $x {param($s)$s.units[0].PSObject.Properties.Remove($missingField)}}
}
Reject 'bool rally flag' {param($x)Edit-RefineryStates $x {param($s)$s.units[0].rally=$false}}
Reject 'string rally flag' {param($x)Edit-RefineryStates $x {param($s)$s.units[0].rally='0'}}
Reject 'array rally flag' {param($x)Edit-RefineryStates $x {param($s)$s.units[0].rally=@(0)}}
Reject 'fractional rally flag' {param($x)Edit-RefineryStates $x {param($s)$s.units[0].rally=.5}}
Reject 'out of range rally flag' {param($x)Edit-RefineryStates $x {param($s)$s.units[0].rally=2}}
Reject 'string rally coordinate' {param($x)Edit-RefineryStates $x {param($s)$s.units[0].rallyX='0'}}
Reject 'bool rally coordinate' {param($x)Edit-RefineryStates $x {param($s)$s.units[0].rallyY=$false}}
Reject 'array rally coordinate' {param($x)Edit-RefineryStates $x {param($s)$s.units[0].rallyZ=@(0)}}
Reject 'nonfinite rally coordinate' {param($x)Edit-RefineryStates $x {param($s)$s.units[0].rallyX='NaN'}}
Reject 'out of world rally coordinate' {param($x)Edit-RefineryStates $x {param($s)$s.units[0].rallyX=10001}}
Reject 'disabled nonzero rally tuple' {param($x)Edit-RefineryStates $x {param($s)$s.units[0].rallyX=1}}
Reject 'rally must be ground point' {param($x)Add-RallyFactory $x;Edit-RefineryStates $x {param($s)$s.units[-1].rallyZ=1}}
Reject 'nonfactory cannot carry rally' {param($x)Edit-RefineryStates $x {param($s)$s.units[0].rally=1}}
Reject 'dead factory cannot carry rally' {param($x)Add-RallyFactory $x;Edit-RefineryStates $x {param($s)$s.units[-1].hp=0}}
Reject 'same digest different rally across roles' {param($x)Add-RallyFactory $x;$x.observer=$x.observer.Replace('123.0000001','123.0000002')}
# Canonical comparison uses explicit fields and stable IDs, not JSON key/insertion order.
$logs=New-SyntheticRefineryLogs;Add-RallyFactory $logs
$logs.observer=($logs.observer -split '\r?\n'|ForEach-Object{
    if($_ -like 'PROOF_REFINERY_STATE *'){$row=$_.Substring(21)|ConvertFrom-Json -Depth 20;$row.units=@($row.units|Sort-Object id -Descending);'PROOF_REFINERY_STATE '+($row|ConvertTo-Json -Depth 20 -Compress)}else{$_}
}) -join [Environment]::NewLine
if(-not (Get-RefineryMinerEvidence $logs Coop).passed){throw 'Canonical same-state reordered records rejected'};$checks++
$logs=New-SyntheticRefineryLogs;$context=@{}
$normal=Get-RefineryMinerEvidence -Logs $logs -Scenario Coop
$withContext=Get-RefineryMinerEvidence -Logs $logs -Scenario Coop -ValidatedStateContext $context
if(($normal|ConvertTo-Json -Depth 20 -Compress) -cne ($withContext|ConvertTo-Json -Depth 20 -Compress) -or $context.maps.Count -ne 3 -or $context.common.Count -ne 3 -or $context.roles.Count -ne 3 -or $context.maps.host['1:5'].units[0].rally -ne 0){throw 'Validated context handoff changed result or omitted strict state'};$checks++
$invalid=New-SyntheticRefineryLogs;$invalid.host+="`nPROOF_REFINERY_ERROR reason=synthetic";$context=@{sentinel=1};$rejected=$false
try{Get-RefineryMinerEvidence -Logs $invalid -Scenario Coop -ValidatedStateContext $context|Out-Null}catch{$rejected=$true}
if(-not $rejected -or $context.Count -ne 1 -or $context.sentinel -ne 1){throw 'Failed validation exposed a partial context'};$checks++
Reject 'singleton-array snapshot root' {param($x)$x.host=($x.host -split '\r?\n'|ForEach-Object{if($_ -like 'PROOF_REFINERY_STATE *'){'PROOF_REFINERY_STATE ['+$_.Substring(21)+']'}else{$_}}) -join [Environment]::NewLine}
Reject 'singleton-array event root' {param($x)$x.host=($x.host -split '\r?\n'|ForEach-Object{if($_ -like 'PROOF_REFINERY_EVENT *'){'PROOF_REFINERY_EVENT ['+$_.Substring(21)+']'}else{$_}}) -join [Environment]::NewLine}

# Independent paid-work/deposit oracle controls. Synthetic only, never engine evidence.
function New-ProductionBoundary {
    $before=[ordered]@{phase=1;jobs=@([ordered]@{owner=1;sequence=7;factory=2;work=0;paid=0;held=0});units=@(
        [ordered]@{id=2;owner=1;kind=4;hp=200;cargo=0;x=0;y=0;z=0;rally=0;rallyX=0;rallyY=0;rallyZ=0});players=@([ordered]@{id=1;credits=100;gathered=0})}
    $after=$before|ConvertTo-Json -Depth 16 -Compress|ConvertFrom-Json -Depth 16
    $after.jobs[0].work=100000;$after.jobs[0].paid=1;$after.players[0].credits=99
    return [pscustomobject]@{before=$before;after=$after;payment=1;refund=0}
}
function Reject-ProductionBoundary([string]$Name,[scriptblock]$Mutate){
    $f=New-ProductionBoundary;$old=$f|ConvertTo-Json -Depth 20 -Compress;&$Mutate $f
    if(($f|ConvertTo-Json -Depth 20 -Compress) -ceq $old){throw "No-op production negative: $Name"}
    $failed=$false;try{Assert-ProductionAdvance $f.before $f.after 1 $f.payment $f.refund}catch{$failed=$true}
    if(-not $failed){throw "Accepted production negative: $Name"};$script:checks++
}
$f=New-ProductionBoundary;Assert-ProductionAdvance $f.before $f.after 1 $f.payment $f.refund;$checks++
Reject-ProductionBoundary 'paid without work' {param($f)$f.after.jobs[0].work=0}
Reject-ProductionBoundary 'unpaid progress' {param($f)$f.after.jobs[0].paid=0}
Reject-ProductionBoundary 'held progress' {param($f)$f.before.jobs[0].held=1;$f.after.jobs[0].held=1}
Reject-ProductionBoundary 'hold changed during advance' {param($f)$f.after.jobs[0].held=1}
Reject-ProductionBoundary 'wrong active sequence' {param($f)$f.after.jobs[0].sequence=8}
Reject-ProductionBoundary 'wrong factory identity' {param($f)$f.after.jobs[0].factory=3}
Reject-ProductionBoundary 'credit-derived fake payment' {param($f)$f.payment=2;$f.after.players[0].credits=98}
Reject-ProductionBoundary 'live factory cancel masquerades as refund' {param($f)$f.after.jobs=@();$f.refund=1;$f.payment=0;$f.after.players[0].credits=101}
Reject-ProductionBoundary 'instant completed spawn' {param($f)$f.after.jobs=@();$f.after.units+= [pscustomobject]@{id=3;owner=1;kind=0;hp=100;cargo=0;x=300;y=0;z=0;rally=0;rallyX=0;rallyY=0;rallyZ=0}}
$f=New-ProductionBoundary;$f.before.jobs[0].work=4900000;$f.before.jobs[0].paid=49;$f.after.jobs=@();$f.after.units+=[pscustomobject]@{id=3;owner=1;kind=0;hp=100;cargo=0;x=300;y=0;z=0;rally=0;rallyX=0;rallyY=0;rallyZ=0}
Assert-ProductionAdvance $f.before $f.after 1 1 0;$checks++
$f.after.units[-1].x=-300;$failed=$false;try{Assert-ProductionAdvance $f.before $f.after 1 1 0}catch{$failed=$true};if(-not $failed){throw 'Wrong deterministic spawn accepted'};$checks++
$f=New-ProductionBoundary;$f.before.jobs[0].work=100000;$f.before.jobs[0].paid=1;$f.after.jobs=@();$f.after.units[0].hp=0;$f.after.players[0].credits=101
Assert-ProductionAdvance $f.before $f.after 1 0 1;$checks++
$f.after.units[0].hp=200;$failed=$false;try{Assert-ProductionAdvance $f.before $f.after 1 0 1}catch{$failed=$true};if(-not $failed){throw 'Refund without matching lost factory accepted'};$checks++
Reject 'missing snapshot jobs' {param($x)Edit-RefineryStates $x {param($s)$s.PSObject.Properties.Remove('jobs')}}
Reject 'deposit job context missing' {param($x)Edit-Event $x deposit {param($e)$e.productionBefore=$null}}
Reject 'deposit false payment' {param($x)Edit-Event $x deposit {param($e)$e.productionPayment=1}}
Reject 'deposit false refund' {param($x)Edit-Event $x deposit {param($e)$e.productionRefund=1}}

# Round-trip coordinates distinguish changes that six-decimal JSON would hide.
$f=New-ProductionBoundary;$f.before.jobs[0].work=4900000;$f.before.jobs[0].paid=49;$f.after.jobs=@();$f.after.units+=[pscustomobject]@{id=3;owner=1;kind=0;hp=100;cargo=0;x=300.00000001;y=0;z=0;rally=0;rallyX=0;rallyY=0;rallyZ=0}
$failed=$false;try{Assert-ProductionAdvance $f.before $f.after 1 1 0}catch{$failed=$true};if(-not $failed){throw 'Sub-microcentimeter spawn corruption accepted'};$checks++
Reject 'deposit just outside one-centimeter boundary' {param($x)Edit-Event $x deposit {param($e)$e.productionAfter.units[1].x=1.00000001}}

$exact=New-SyntheticRefineryLogs -ExactDeposit
$exactProof=Get-RefineryMinerEvidence $exact Coop
if(-not $exactProof.passed -or $exactProof.commonEpochRevisions.Count -ne 4 -or @($exactProof.grants|Where-Object{$_.checkpoints.deposited -ne '1:6'}).Count -gt 0){throw 'Exact deposit checkpoint positive failed'};$checks++
Edit-Event $exact deposit {param($e)$e.productionAfter.units[1].x=.5}
$failed=$false;try{Get-RefineryMinerEvidence $exact Coop|Out-Null}catch{$failed=$true};if(-not $failed){throw 'Same-digest exact deposit context contradiction accepted'};$checks++
Reject 'same-digest before deposit context contradiction' {param($x)Edit-Event $x deposit {param($e)$e.productionBefore.units[1].x+=.5}}
Reject 'unpublished simultaneous deposit contexts disagree' {param($x)Edit-Event $x deposit {param($e)if($e.owner -eq 1){$e.productionAfter.units[3].x=.5}}}
Write-Output "PASS synthetic-only refinery/miner and production accounting parser checks=$checks; no engine/runtime/physical evidence"
