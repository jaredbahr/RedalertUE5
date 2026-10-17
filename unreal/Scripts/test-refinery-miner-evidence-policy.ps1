# Synthetic policy validation only. No UE process, model, transport, or physical input.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RefineryMinerEvidence.ps1')
function New-SyntheticRefineryLogs([int[]]$Owners=@(1,2,100)) {
    $events=@();$states=@()
    foreach($o in $owners){
        $r=1000+$o*2;$m=$r+1
        $events+=@{event='grant';epoch=1;owner=$o;sequence=1;refinery=$r;miner=$m;beforeRevision=1;revision=2;beforeDigest=('0'*31+'1');digest=('0'*31+'2');added=2;refineriesAdded=1;minersAdded=1;creditsBefore=350;creditsAfter=250;cost=100;x=300;y=0;z=0;rx=0;ry=0;rz=0;beforeIds=@(1,2);afterIds=@(1,2,$r,$m)}
        $events+=@{event='gather';epoch=1;owner=$o;sequence=2;refinery=$r;miner=$m;resource=$o;revision=3}
        $events+=@{event='load';epoch=1;owner=$o;refinery=$r;miner=$m;resource=$o;revision=4;cargo=50;oreBefore=500;oreAfter=450}
        $events+=@{event='deposit';epoch=1;owner=$o;refinery=$r;depot=$r;miner=$m;revision=6;cargoBefore=50;cargoAfter=0;depositSum=50;creditsBefore=250;creditsAfter=300;gatheredBefore=0;gatheredAfter=50;contributors=@(@{miner=$m;before=50;after=0;depot=$r})}
    }
    foreach($rev in @(3,5,7)){
        $s=[ordered]@{epoch=1;revision=$rev;digest=('0'*31+$rev);phase=1;winner=-1;units=@();players=@();resources=@()}
        foreach($o in $owners){$r=1000+$o*2;$m=$r+1
            $s.units+=@{id=$r;owner=$o;kind=3;x=0;y=0;z=0;hp=200;cargo=0}
            $s.units+=@{id=$m;owner=$o;kind=1;x=300;y=0;z=0;hp=100;cargo=$(if($rev -eq 5){50}else{0})}
            $s.players+=@{id=$o;team=$(if($o -eq 100){1}else{0});credits=$(if($rev -eq 7){300}else{250});gathered=$(if($rev -eq 7){50}else{0})}
            $s.resources+=@{id=$o;owner=$o;x=600;y=0;z=0;remaining=$(if($rev -ge 5){450}else{500})}
        };$states+=,$s
    }
    $logs=@{}
    foreach($role in @('host','rendered','observer')){
        $view=if($role -eq 'host'){'host'}else{'client'};$v=if($role -eq 'host'){0}elseif($role -eq 'observer'){2}else{1};$lines=@()
        if($role -eq 'host'){foreach($e in $events){$lines+='PROOF_REFINERY_EVENT '+($e|ConvertTo-Json -Compress -Depth 12)}}
        foreach($s in $states){$row=[ordered]@{role=$view;viewer=$v};foreach($k in $s.Keys){$row[$k]=$s[$k]};$lines+="PROOF_SNAPSHOT role=$view owner=$v epoch=1 revision=$($s.revision) digest=$($s.digest)";$lines+='PROOF_REFINERY_STATE '+($row|ConvertTo-Json -Compress -Depth 12)}
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
Write-Output "PASS synthetic-only refinery/miner parser checks=$checks; no engine/runtime/physical evidence"
