param([string]$OutputPath='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RefineryAutoGatherEvidence.ps1')
$script:TestRun='a'*32;$script:TestSource='b'*40
function Copy-AutoTest($Value) { return ConvertFrom-Json (ConvertTo-Json -InputObject $Value -Depth 24 -Compress) -AsHashtable -Depth 24 }
function New-AutoTestUnit([int]$Id,[int]$Owner,[int]$Kind,[double]$X,[double]$Y,[double]$HP=100) {
    return [ordered]@{id=$Id;owner=$Owner;x=$X;y=$Y;z=0.0;dx=0.0;dy=0.0;dz=0.0;hp=$HP;kind=$Kind;cargo=0;rally=$false;rx=0.0;ry=0.0;rz=0.0}
}
function Set-AutoTestDigests($Value) {
    if($Value -is [Collections.IDictionary]) {
        foreach($key in @($Value.Keys)){if($key -ne 'digest'){Set-AutoTestDigests $Value[$key]}}
        if($Value.Contains('digest')){$Value.digest=Get-AutoSnapshotDigest $Value}
    } elseif($Value -is [array]) {foreach($item in $Value){Set-AutoTestDigests $item}}
}
function Edit-AutoTestSnapshots($Value,[scriptblock]$Edit) {
    if($Value -is [Collections.IDictionary]) {
        if($Value.Contains('digest')){& $Edit $Value;return}
        foreach($key in @($Value.Keys)){Edit-AutoTestSnapshots $Value[$key] $Edit}
    } elseif($Value -is [array]){foreach($item in $Value){Edit-AutoTestSnapshots $item $Edit}}
}
function New-AutoTestFixture {
    $s0=[ordered]@{revision=0;epoch=1;skirmish=$true;phase=1;winner=-1;digest='';players=@(@{id=1;credits=350;team=0;gathered=0},@{id=2;credits=350;team=0;gathered=0});units=@((New-AutoTestUnit 1 1 2 0 0 300),(New-AutoTestUnit 2 1 1 0 -300),(New-AutoTestUnit 3 2 2 0 1000 300),(New-AutoTestUnit 4 2 1 0 700));resources=@(@{id=1;owner=1;x=600.0;y=0.0;z=0.0;remaining=500},@{id=2;owner=2;x=600.0;y=1000.0;z=0.0;remaining=500})}
    $s1=Copy-AutoTest $s0;$s1.revision=3;$s1.players[0].credits-=100;$s1.units+=@((New-AutoTestUnit 5 1 3 0 350 200),(New-AutoTestUnit 6 1 1 300 350))
    $s2=Copy-AutoTest $s1;$s2.revision=6;$s2.players[1].credits-=100;$s2.units+=@((New-AutoTestUnit 7 2 3 0 1350 200),(New-AutoTestUnit 8 2 1 300 1350))
    $s3=Copy-AutoTest $s2;$s3.revision=7
    foreach($id in @(6,8)){$u=Get-AutoUnit $s3 $id;$u.x=400.0;$u.y-=100.0;$u.dx=1/[math]::Sqrt(2);$u.dy=-1/[math]::Sqrt(2)}
    $s4=Copy-AutoTest $s3;$s4.revision=8
    foreach($id in @(6,8)){$u=Get-AutoUnit $s4 $id;$u.x=600.0;$u.y=if($id -eq 6){0.0}else{1000.0};$u.cargo=50};foreach($ore in $s4.resources){$ore.remaining-=50}
    $s5=Copy-AutoTest $s4;$s5.revision=9
    foreach($id in @(6,8)){$u=Get-AutoUnit $s5 $id;$u.x=100.0;$u.y=if($id -eq 6){350.0}else{1350.0};$u.dx=-500/[math]::Sqrt(372500);$u.dy=350/[math]::Sqrt(372500)}
    $s6=Copy-AutoTest $s5;$s6.revision=10
    foreach($id in @(6,8)){$u=Get-AutoUnit $s6 $id;$u.x=0.0;$u.dx=-1.0;$u.dy=0.0;$u.cargo=0};foreach($p in $s6.players){$p.credits+=50;$p.gathered+=50}
    $s7=Copy-AutoTest $s6;$s7.revision=11;$s7.phase=2;$s7.winner=1
    $states=@($s0,$s1,$s2,$s3,$s4,$s5,$s6,$s7);foreach($s in $states){Set-AutoTestDigests $s}
    $hostRows=[Collections.Generic.List[object]]::new()
    $hostRows.Add(@{event='snapshot';state=$s0})
    foreach($owner in @(1,2)){
        $g=@{miner=($owner*2+4);refinery=($owner*2+3);resource=$owner;orderType=5;orderTarget=$owner;returning=$false;resume=-1;ox=0.0;oy=0.0;oz=0.0}
        $hostRows.Add(@{event='command';owner=$owner;type=3;unit=-1;target=-1;sequence=1;payloadEpoch=1;cx=0.0;cy=if($owner -eq 1){350.0}else{1350.0};cz=0.0;accepted=$true;grant=$g;before=$states[$owner-1];after=$states[$owner]})
        $hostRows.Add(@{event='snapshot';state=$states[$owner]})
    }
    $i=3
    foreach($phase in @('travel','load','return','deposit')){
        foreach($owner in @(1,2)){$hostRows.Add(@{event=$phase;owner=$owner;miner=($owner*2+4);refinery=($owner*2+3);resource=$owner;before=$states[$i-1];after=$states[$i]})}
        $hostRows.Add(@{event='snapshot';state=$states[$i]});$i++
    }
    $hostRows.Add(@{event='snapshot';state=$s7})
    # Source allows accepted unrelated starter Gather commands without contaminating the tracked grant.
    # Positive below adds this at a nonterminal point to test the parser distinction.
    $streams=@{host=@($hostRows.ToArray());rendered=@($states|ForEach-Object {@{event='snapshot';state=$_}});observer=@($states|ForEach-Object {@{event='snapshot';state=$_}})}
    return Copy-AutoTest $streams
}
function ConvertTo-AutoTestLogs($Streams,[switch]$Prefix) {
    $logs=@{}
    foreach($role in @('host','rendered','observer')){
        $viewer=@{host=0;rendered=1;observer=2}[$role];$view=if($viewer -eq 0){'host'}else{'client'};$rows=@(Copy-AutoTest $Streams[$role]);$n=0;$snapshots=@($rows|Where-Object event -eq snapshot).Count
        $rows+=,@{event='finish';passed=$true;records=($rows.Count+1);snapshots=$snapshots}
        $lines=@();$sendLogged=$false
        foreach($record in $rows){$r=Copy-AutoTest $record;$r.v=1;$r.run=$script:TestRun;$r.source=$script:TestSource;$r.role=$view;$r.viewer=$viewer;$r.row=++$n;$r.epoch=1
            if($viewer -gt 0 -and -not $sendLogged -and $r.event -eq 'snapshot' -and $r.state.revision -ge (3*$viewer)) {
                $lines+=('PROOF_AUTO_GATHER_SEND owner={0} epoch=1 sequence=1 type=3' -f $viewer)
                $lines+=('PROOF_AUTO_GATHER_RECEIPT owner={0} epoch=1 sequence=1 accepted=1' -f $viewer);$sendLogged=$true
            }
            if($r.event -eq 'snapshot'){$lines+='PROOF_SNAPSHOT role={0} owner={1} epoch=1 revision={2} digest={3}' -f $view,$viewer,$r.state.revision,$r.state.digest}
            $lines+='PROOF_AUTO_GATHER '+(ConvertTo-Json -InputObject $r -Depth 24 -Compress)
        }
        if($Prefix){$lines=@($lines|ForEach-Object {'[2026.10.18-23.00.00:000][  1]LogTemp: Display: '+$_})}
        $logs[$role]=$lines -join "`r`n"
    }
    return $logs
}
$checks=[Collections.Generic.List[object]]::new()
function Accept-AutoTest([string]$Name,$Logs) {
    $r=Get-RefineryAutoGatherEvidence -Logs $Logs -RunId $script:TestRun -SourceTree $script:TestSource
    Assert-AutoGather ($r.passed -and $r.automaticTrips.Count -eq 2) "positive $Name";$checks.Add(@{name=$Name;passed=$true})
}
function Reject-AutoTest([string]$Name,[scriptblock]$Mutation,[switch]$Raw) {
    $data=if($Raw){ConvertTo-AutoTestLogs (New-AutoTestFixture)}else{New-AutoTestFixture}
    $before=ConvertTo-Json -InputObject $data -Depth 24 -Compress
    & $Mutation $data
    Assert-AutoGather ((ConvertTo-Json -InputObject $data -Depth 24 -Compress) -cne $before) "mutation no-op $Name"
    $logs=if($Raw){$data}else{ConvertTo-AutoTestLogs $data};$reason=$null
    try { Get-RefineryAutoGatherEvidence -Logs $logs -RunId $script:TestRun -SourceTree $script:TestSource | Out-Null } catch {$reason=$_.Exception.Message}
    Assert-AutoGather ($null -ne $reason) "negative accepted $Name";$checks.Add(@{name=$Name;passed=$true;rejection=$reason})
}
Accept-AutoTest 'plain full automatic trips' (ConvertTo-AutoTestLogs (New-AutoTestFixture))
Accept-AutoTest 'UE prefixes and CRLF' (ConvertTo-AutoTestLogs (New-AutoTestFixture) -Prefix)
Reject-AutoTest 'source mismatch' {$args[0].host=$args[0].host.Replace(('b'*40),('c'*40))} -Raw
Reject-AutoTest 'run mismatch' {$args[0].observer=$args[0].observer.Replace(('a'*32),('c'*32))} -Raw
Reject-AutoTest 'raw epoch zero' {$args[0].host=$args[0].host.Replace('"epoch":1','"epoch":0')} -Raw
Reject-AutoTest 'truncated stream' {$args[0].host=$args[0].host.Substring(0,$args[0].host.LastIndexOf('PROOF_AUTO_GATHER '))} -Raw
Reject-AutoTest 'truncated row' {$args[0].host+='PROOF_AUTO_GATHER {"v":1'} -Raw
Reject-AutoTest 'runtime error after finish' {$args[0].observer+="`nPROOF_AUTO_GATHER_ERROR reason=late"} -Raw
Reject-AutoTest 'row after finish' {$args[0].host+="`nPROOF_AUTO_GATHER {}"} -Raw
Reject-AutoTest 'duplicate JSON key' {$args[0].host=$args[0].host.Replace('"v":1','"v":1,"v":1')} -Raw
Reject-AutoTest 'missing direction field' {param($d) $d.rendered[3].state.units[0].Remove('dx')|Out-Null}
Reject-AutoTest 'missing skirmish field' {param($d) $d.observer[3].state.Remove('skirmish')|Out-Null}
Reject-AutoTest 'unknown snapshot field' {param($d) $d.rendered[3].state.extra=1}
Reject-AutoTest 'subprecision direction contradiction' {param($d) $u=$d.rendered[3].state.units[5];$u.dx+=1e-14;Set-AutoTestDigests $d}
Reject-AutoTest 'digest forgery' {param($d) $d.observer[2].state.digest='0'*32}
Reject-AutoTest 'wrong initial target' {param($d) @($d.host|Where-Object event -eq command)[0].grant.resource=2}
Reject-AutoTest 'wrong actual order type' {param($d) @($d.host|Where-Object event -eq command)[0].grant.orderType=0}
Reject-AutoTest 'automatic returning order' {param($d) @($d.host|Where-Object event -eq command)[0].grant.returning=$true}
Reject-AutoTest 'wrong order target' {param($d) @($d.host|Where-Object event -eq command)[0].grant.orderTarget=2}
Reject-AutoTest 'starter miner substitution' {param($d) @($d.host|Where-Object event -eq command)[0].grant.miner=2}
Reject-AutoTest 'wrong owner phase' {param($d) @($d.host|Where-Object event -eq travel)[0].owner=2}
Reject-AutoTest 'orphan phase' {param($d) @($d.host|Where-Object event -eq travel)[0].miner=99}
Reject-AutoTest 'missing travel' {param($d) $d.host=@($d.host|Where-Object event -ne travel)}
Reject-AutoTest 'missing return' {param($d) $d.host=@($d.host|Where-Object event -ne return)}
Reject-AutoTest 'missing deposit' {param($d) $d.host=@($d.host|Where-Object event -ne deposit)}
Reject-AutoTest 'duplicate load' {param($d) $index=0;while($d.host[$index].event -ne 'load'){$index++};$duplicate=Copy-AutoTest $d.host[$index];$d.host=@($d.host[0..$index])+@($duplicate)+@($d.host[($index+1)..($d.host.Count-1)])}
Reject-AutoTest 'loss of ore conservation' {param($d) @($d.host|Where-Object event -eq load)[0].after.resources[0].remaining-=1;Set-AutoTestDigests $d}
Reject-AutoTest 'one extra build revision' {param($d) @($d.host|Where-Object event -eq command)[0].after.revision++;Set-AutoTestDigests $d}
Reject-AutoTest 'extra build cost' {param($d) @($d.host|Where-Object event -eq command)[0].after.players[0].credits--;Set-AutoTestDigests $d}
Reject-AutoTest 'existing starter mutation' {param($d) @($d.host|Where-Object event -eq command)[0].after.units[1].cargo=1;Set-AutoTestDigests $d}
Reject-AutoTest 'receipt sequence mismatch' {$args[0].rendered=$args[0].rendered.Replace('sequence=1 accepted=1','sequence=2 accepted=1')} -Raw
Reject-AutoTest 'missing receiving role' {$args[0].Remove('observer')} -Raw
Reject-AutoTest 'accepted Gather contaminates trip' {param($d) $cmd=Copy-AutoTest @($d.host|Where-Object event -eq command)[0];$cmd.type=5;$cmd.unit=6;$cmd.target=1;$cmd.sequence=2;$cmd.grant=$null;$cmd.before=Copy-AutoTest $cmd.after;$d.host=@($d.host[0..2])+@($cmd)+@($d.host[3..($d.host.Count-1)])}
Reject-AutoTest 'accepted Stop overrides trip' {param($d) $cmd=Copy-AutoTest @($d.host|Where-Object event -eq command)[0];$cmd.type=6;$cmd.unit=6;$cmd.sequence=2;$cmd.grant=$null;$cmd.before=Copy-AutoTest $cmd.after;$d.host=@($d.host[0..2])+@($cmd)+@($d.host[3..($d.host.Count-1)])}
Reject-AutoTest 'missing common loaded state' {param($d) $d.observer=@($d.observer|Where-Object {$_.state.revision -notin @(8,9)})}
# Realistic unrelated command after deposit: accepted Gather consumes one revision,
# with the final published state advanced accordingly in all receiving roles.
$unrelated=New-AutoTestFixture;$terminal=$unrelated.host[-1].state;$terminal.revision++
$prior=Copy-AutoTest $unrelated.host[-2].state;$after=Copy-AutoTest $prior;$after.revision++
$cmd=@{event='command';owner=1;type=5;unit=2;target=1;sequence=2;payloadEpoch=1;cx=0.0;cy=0.0;cz=0.0;accepted=$true;grant=$null;before=$prior;after=$after}
$unrelated.host=@($unrelated.host[0..($unrelated.host.Count-2)])+@($cmd,@{event='snapshot';state=$after},@{event='snapshot';state=$terminal})
foreach($role in @('rendered','observer')){$unrelated[$role][-1].state.revision++;$unrelated[$role]=@($unrelated[$role][0..($unrelated[$role].Count-2)])+@(@{event='snapshot';state=(Copy-AutoTest $after)},$unrelated[$role][-1])}
Set-AutoTestDigests $unrelated
Accept-AutoTest 'unrelated starter Gather allowed' (ConvertTo-AutoTestLogs $unrelated)
$during=New-AutoTestFixture
Edit-AutoTestSnapshots $during {param($s) if($s.revision -ge 7){$s.revision++}}
$base=Copy-AutoTest @($during.host|Where-Object {$_.event -eq 'snapshot' -and $_.state.revision -eq 6})[0].state
$changed=Copy-AutoTest $base;$changed.revision=7
$cmd=@{event='command';owner=1;type=5;unit=2;target=1;sequence=2;payloadEpoch=1;cx=0.0;cy=0.0;cz=0.0;accepted=$true;grant=$null;before=$base;after=$changed}
foreach($phase in @($during.host|Where-Object event -eq travel)){$phase.before=Copy-AutoTest $changed}
$index=0;while(-not ($during.host[$index].event -eq 'snapshot' -and $during.host[$index].state.revision -eq 6)){$index++}
$during.host=@($during.host[0..$index])+@($cmd,@{event='snapshot';state=$changed})+@($during.host[($index+1)..($during.host.Count-1)])
Set-AutoTestDigests $during
Accept-AutoTest 'starter Gather before tracked first deposit allowed' (ConvertTo-AutoTestLogs $during)
$contributors=New-AutoTestFixture
Edit-AutoTestSnapshots $contributors {
    param($s)
    if($s.revision -lt 8){return}
    $starter=Get-AutoUnit $s 2;$s.resources[0].remaining-=50
    if($s.revision -lt 10){$starter.cargo=50;$starter.x=if($s.revision -eq 8){600.0}else{100.0};$starter.y=if($s.revision -eq 8){0.0}else{350.0}}
    else{$starter.cargo=0;$starter.x=0.0;$starter.y=350.0;$s.players[0].credits+=50;$s.players[0].gathered+=50}
}
Set-AutoTestDigests $contributors
Accept-AutoTest 'simultaneous own starter contribution reconciled' (ConvertTo-AutoTestLogs $contributors)
Reject-AutoTest 'unattributed deposit credit' {param($d) @($d.host|Where-Object event -eq deposit)[0].after.players[0].credits++;Set-AutoTestDigests $d}
Reject-AutoTest 'row gap' {$args[0].host=$args[0].host.Replace('"row":2,','"row":200,')} -Raw
Reject-AutoTest 'finish record count mismatch' {$args[0].host=[regex]::Replace($args[0].host,'"records":\d+','"records":1')} -Raw
Reject-AutoTest 'finish snapshot count mismatch' {$args[0].host=[regex]::Replace($args[0].host,'"snapshots":\d+','"snapshots":1')} -Raw
Reject-AutoTest 'wrong actual travel direction' {param($d) $r=@($d.host|Where-Object event -eq travel)[0];$u=Get-AutoUnit $r.after 6;$u.dx=-$u.dx;Set-AutoTestDigests $d}
Reject-AutoTest 'accepted Move overrides trip' {param($d) $cmd=Copy-AutoTest @($d.host|Where-Object event -eq command)[0];$cmd.type=0;$cmd.unit=6;$cmd.sequence=2;$cmd.grant=$null;$cmd.before=Copy-AutoTest $cmd.after;$d.host=@($d.host[0..2])+@($cmd)+@($d.host[3..($d.host.Count-1)])}
Reject-AutoTest 'accepted ReturnCargo overrides trip' {param($d) $cmd=Copy-AutoTest @($d.host|Where-Object event -eq command)[0];$cmd.type=9;$cmd.unit=6;$cmd.sequence=2;$cmd.grant=$null;$cmd.before=Copy-AutoTest $cmd.after;$d.host=@($d.host[0..2])+@($cmd)+@($d.host[3..($d.host.Count-1)])}
$result=[ordered]@{passed=$true;checks=$checks.Count;tests=$checks.ToArray();engineLaunched=$false;syntheticOnly=$true;run=$script:TestRun;source=$script:TestSource}
if($OutputPath){$result|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $OutputPath -Encoding utf8}
$result|ConvertTo-Json -Depth 8
