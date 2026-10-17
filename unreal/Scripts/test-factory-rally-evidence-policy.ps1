# Synthetic parser tests only; never launches UE or claims runtime/physical proof.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RefineryMinerEvidence.ps1')
. (Join-Path $PSScriptRoot 'FactoryRallyEvidence.ps1')
# Reuse the maintained refinery-log generator without executing its test batch.
$policyAst=[System.Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'test-refinery-miner-evidence-policy.ps1'),[ref]$null,[ref]$null)
$generator=$policyAst.Find({param($node)$node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'New-SyntheticRefineryLogs'},$true)
if(-not $generator){throw 'Missing maintained synthetic generator'}
. ([scriptblock]::Create($generator.Extent.Text))
function New-RallyLogs {
    $logs=New-SyntheticRefineryLogs
    foreach($role in @('host','rendered','observer')) {
        $base=($logs[$role] -split '\r?\n'|Where-Object{$_ -like 'PROOF_REFINERY_STATE *'}|Select-Object -Last 1).Substring(21)|ConvertFrom-Json -Depth 16
        foreach($revision in @(10,12,14,16)) {
            $s=$base|ConvertTo-Json -Depth 16 -Compress|ConvertFrom-Json -Depth 16
            $s.revision=$revision;$s.digest=('{0:x32}' -f $revision)
            $s.units+=,[pscustomobject]@{id=9990;owner=1;kind=4;x=0;y=350;z=0;hp=200;cargo=0;rally=1;rallyX=-600;rallyY=-350;rallyZ=0}
            if($revision -ge 12){$s.units+=,[pscustomobject]@{id=9991;owner=1;kind=0;x=(300-25*($revision-12));y=(350-25*($revision-12));z=0;hp=100;cargo=0;rally=0;rallyX=0;rallyY=0;rallyZ=0};($s.players|Where-Object id -eq 1).credits=250}
            $logs[$role]+="`nPROOF_SNAPSHOT role=$($s.role) owner=$($s.viewer) epoch=1 revision=$revision digest=$($s.digest)`nPROOF_REFINERY_STATE "+($s|ConvertTo-Json -Depth 16 -Compress)
        }
    }
    $ids=@(1002,1003,1004,1005,1200,1201,9990)
    foreach($type in @('SetRally','Produce')) {
        $produce=$type -eq 'Produce'
        $e=@{type=$type;owner=1;epoch=1;sequence=$(if($produce){5}else{4});factory=9990;unit=$(if($produce){9991}else{-1});beforeRevision=$(if($produce){10}else{8});revision=$(if($produce){12}else{9});beforeDigest=('1'*32);digest=('2'*32);creditsBefore=300;creditsAfter=$(if($produce){250}else{300});rallyX=-600;rallyY=-350;rallyZ=0;spawnX=$(if($produce){300}else{0});spawnY=$(if($produce){350}else{0});spawnZ=0;beforeIds=$ids;afterIds=$(if($produce){@($ids)+9991}else{@($ids)})}
        if($produce){$e.beforeDigest=('{0:x32}' -f 10);$e.digest=('{0:x32}' -f 12)}
        $logs.host+="`nPROOF_FACTORY_RALLY_EVENT "+($e|ConvertTo-Json -Depth 8 -Compress)
    }
    foreach($phase in @('rallied','spawned','moved')) {
        $first=$phase -eq 'rallied';$last=$phase -eq 'moved'
        $p=@{phase=$phase;owner=1;epoch=1;revision=$(if($first){10}elseif($last){16}else{12});factory=9990;unit=$(if($first){-1}else{9991});setSequence=4;produceSequence=$(if($first){0}else{5});x=$(if($first){0}elseif($last){200}else{300});y=$(if($first){0}elseif($last){250}else{350});z=0;rallyX=-600;rallyY=-350;rallyZ=0;selected=9990;selectionCount=1;nativeRight=0;elapsed=$(if($first){1}elseif($last){3.2}else{2})}
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
function Reject-Rally([string]$Name,[scriptblock]$Mutate) {
    $logs=New-RallyLogs;$before=$logs|ConvertTo-Json -Compress;& $Mutate $logs
    if(($logs|ConvertTo-Json -Compress) -ceq $before){throw "No-op negative mutation: $Name"}
    $rejected=$false;try{Get-FactoryRallyEvidence $logs|Out-Null}catch{$rejected=$true}
    if(-not $rejected){throw "Accepted invalid synthetic rally: $Name"};$script:rallyChecks++
}
$valid=Get-FactoryRallyEvidence (New-RallyLogs)
if(-not $valid.passed -or $valid.commonMovement.Count -ne 2){throw 'Rejected valid synthetic rally'};$rallyChecks++
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
Reject-Rally 'wrong spawned position' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.spawnX=-8000;$e.spawnY=-8000}}}
Reject-Rally 'rally changes between commands' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.rallyX=-601}}}
Reject-Rally 'no common rally before produce' {param($l)foreach($r in @('host','rendered','observer')){$l[$r]=$l[$r] -replace '(?m)^PROOF_REFINERY_STATE[^\n]*"revision":10[^\n]*\n?',''}}
Reject-Rally 'common role rally disagreement' {param($l)Edit-RallyRows $l observer PROOF_REFINERY_STATE {param($s)if($s.revision -ge 10){$s.units[-1].rallyX=-601}}}
Reject-Rally 'not enough later common movement' {param($l)$l.observer=$l.observer -replace '(?m)^PROOF_REFINERY_STATE[^\n]*"revision":16[^\n]*\n?',''}
Reject-Rally 'missing phase' {param($l)$l.rendered=$l.rendered -replace '(?m)^PROOF_FACTORY_RALLY_FIXTURE[^\n]*"phase":"moved"[^\n]*\n?',''}
Reject-Rally 'changed selection' {param($l)Edit-RallyRows $l rendered PROOF_FACTORY_RALLY_FIXTURE {param($p)$p.selected=9991}}
Reject-Rally 'native input changed' {param($l)Edit-RallyRows $l rendered PROOF_FACTORY_RALLY_FIXTURE {param($p)if($p.phase -eq 'moved'){$p.nativeRight=1}}}
Reject-Rally 'fake local movement' {param($l)Edit-RallyRows $l rendered PROOF_FACTORY_RALLY_FIXTURE {param($p)if($p.phase -eq 'moved'){$p.x=201}}}
Reject-Rally 'missing capture' {param($l)$l.rendered=$l.rendered -replace '(?m)^PROOF_FACTORY_RALLY_CAPTURE[^\n]*',''}
Reject-Rally 'malformed empty event' {param($l)$l.host+="`nPROOF_FACTORY_RALLY_EVENT"}
Reject-Rally 'duplicate event key' {param($l)$l.host=$l.host.Replace('"type":"SetRally"','"type":"SetRally","type":"SetRally"')}
Reject-Rally 'published event digest mismatch' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.digest='f'*32}}}
Reject-Rally 'published credit mismatch despite correct cost' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.creditsBefore=1000;$e.creditsAfter=950}}}
Reject-Rally 'published ID mismatch despite exact added pair' {param($l)Edit-RallyRows $l host PROOF_FACTORY_RALLY_EVENT {param($e)if($e.type -eq 'Produce'){$e.beforeIds+=9900;$e.afterIds+=9900}}}
Reject-Rally 'singleton array event root' {param($l)$l.host=[regex]::Replace($l.host,'(?m)^(PROOF_FACTORY_RALLY_EVENT )([^\r\n]*)$',{param($m)$m.Groups[1].Value+'['+$m.Groups[2].Value+']'})}
Reject-Rally 'singleton array fixture root' {param($l)$l.rendered=[regex]::Replace($l.rendered,'(?m)^(PROOF_FACTORY_RALLY_FIXTURE )([^\r\n]*)$',{param($m)$m.Groups[1].Value+'['+$m.Groups[2].Value+']'})}
Reject-Rally 'rallied phase claims future moved revision' {param($l)Edit-RallyRows $l rendered PROOF_FACTORY_RALLY_FIXTURE {param($p)if($p.phase -eq 'rallied'){$p.revision=16}}}
Reject-Rally 'rallied phase occurs after production' {param($l)Edit-RallyRows $l rendered PROOF_FACTORY_RALLY_FIXTURE {param($p)if($p.phase -eq 'rallied'){$p.revision=12}}}
Reject-Rally 'rallied and spawned revisions reversed' {param($l)Edit-RallyRows $l rendered PROOF_FACTORY_RALLY_FIXTURE {param($p)if($p.phase -eq 'rallied'){$p.revision=14}}}
Write-Output "PASS synthetic-only factory rally parser checks=$rallyChecks; no engine/runtime/physical evidence"
