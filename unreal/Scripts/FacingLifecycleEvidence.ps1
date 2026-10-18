. (Join-Path $PSScriptRoot 'FacingDisplacementEvidence.ps1')
function Get-FacingStateRows([string]$Log) {
    if($Log.Length-gt24MB){throw 'Facing state log exceeds bound'}
    function Number($V){return $null-ne$V-and$V-isnot[string]-and$V-isnot[bool]-and$V-isnot[array]-and[double]::IsFinite([double]$V)}
    function Keys($E){
        if($E.ValueKind-eq[Text.Json.JsonValueKind]::Object){$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase);foreach($p in $E.EnumerateObject()){if(-not$seen.Add($p.Name)){throw 'Duplicate facing JSON key'};Keys $p.Value}}
        elseif($E.ValueKind-eq[Text.Json.JsonValueKind]::Array){foreach($v in $E.EnumerateArray()){Keys $v}}
    }
    if($Log-match'PROOF_FACING_[A-Z_]+[^\r\n]*(?:ERROR|status=fail)|PROOF_FACING_STATE_ERROR|Fatal error|Assertion failed'){throw 'Facing failure marker present'}
    $matches=[regex]::Matches($Log,'(?m)^.*PROOF_FACING_STATE (\{[^\r\n]*\})\r?$')
    if($matches.Count-gt4096-or$matches.Count-ne[regex]::Matches($Log,'PROOF_FACING_STATE(?=\s|$)').Count){throw 'Facing state rows incomplete or over limit'}
    foreach($m in $matches){
        $doc=[Text.Json.JsonDocument]::Parse($m.Groups[1].Value);try{Keys $doc.RootElement}finally{$doc.Dispose()}
        $r=$m.Groups[1].Value|ConvertFrom-Json -ErrorAction Stop
        foreach($p in @('viewer','epoch','revision','unit','owner','kind')){if(-not(Number $r.$p)-or[double]$r.$p-ne[math]::Truncate([double]$r.$p)-or$r.$p-lt0-or$r.$p-gt[int]::MaxValue){throw 'Facing state integer required'}}
        if(-not(Number $r.hp)-or$r.role-isnot[string]-or$r.role-cnotin@('host','client')-or$r.digest-isnot[string]){throw 'Facing state typed fields required'}
        foreach($p in @('position','lastTravelDirection')){
            $v=$r.$p
            if($v-isnot[array]-or$v.Count-ne3-or@($v|Where-Object {$null-eq$_-or$_-is[string]-or$_-is[bool]-or-not[double]::IsFinite([double]$_)}).Count){throw 'Facing state finite vector required'}
        }
        if($r.epoch-le0-or$r.revision-lt0-or$r.unit-le0-or$r.owner-ne1-or$r.kind-ne1-or$r.hp-le0-or$r.digest-notmatch'^[a-fA-F0-9]{32}$'){throw 'Facing state identity invalid'}
        $n=[math]::Sqrt(($r.lastTravelDirection|ForEach-Object {$_*$_}|Measure-Object -Sum).Sum)
        if([math]::Abs($r.lastTravelDirection[2])-gt0.00001-or($n-gt0.00001-and[math]::Abs($n-1)-gt0.001)){throw 'Facing state must be zero or normalized horizontal direction'}
        $r
    }
}
function Test-FacingVec($A,$B,[double]$Tolerance=0.00001){
    for($i=0;$i-lt3;$i++){if([math]::Abs($A[$i]-$B[$i])-gt$Tolerance){return $false}}
    return $true
}
function Get-FacingLateObserverEvidence {
    param([hashtable]$Logs)
    $rows=@{};$maps=@{}
    foreach($role in @('host','lead','rejoin')){
        $rows[$role]=@(Get-FacingStateRows $Logs[$role]|Where-Object {$_.epoch-eq1})
        if(-not$rows[$role].Count){return $null}
        $maps[$role]=@{}
        foreach($r in $rows[$role]){
            if(($role-eq'host'-and($r.role-cne'host'-or$r.viewer-ne0))-or($role-eq'lead'-and($r.role-cne'client'-or$r.viewer-ne1))-or($role-eq'rejoin'-and($r.role-cne'client'-or$r.viewer-ne2))){throw 'Facing role mismatch'}
            $key=[string]$r.revision
            if($maps[$role].ContainsKey($key)){
                # PostLogin can republish unchanged model state when a deferred
                # seat rebinds. Allow identical content, reject contradictions.
                $prior=$maps[$role][$key]
                if($r.unit-ne$prior.unit-or$r.digest-cne$prior.digest-or$r.hp-ne$prior.hp-or-not(Test-FacingVec $r.position $prior.position)-or-not(Test-FacingVec $r.lastTravelDirection $prior.lastTravelDirection)){throw 'Conflicting facing epoch/revision content'}
                continue
            }
            $maps[$role][$key]=$r
        }
    }
    $pass=[regex]::Match($Logs.lead,'PROOF_FACING_PREFLIGHT status=pass unit=(\d+) epoch=1 stationarySeconds=([\d.]+)')
    if(-not$pass.Success-or[double]$pass.Groups[2].Value-lt1.2){throw 'Facing ordinary movement/stationary preflight missing'}
    $unit=[int]$pass.Groups[1].Value
    $trace=@(Get-FacingDisplacementTrace $Logs.host)
    if($Logs.host-notmatch'PROOF_LIFECYCLE_JOIN owner=2 epoch=1 seeded=0'-or$Logs.host-match'PROOF_LIFECYCLE_RESEED owner=2 epoch=1\b'){throw 'Late observer must use deferred rejoin seat'}
    $moves=[regex]::Matches($Logs.lead,'PROOF_FACING_PREFLIGHT_MOVE step=(0|2) unit=(\d+) sequence=(\d+) x=([-\d.]+) y=([-\d.]+)')
    if($moves.Count-ne2){throw 'Two preflight movement requests required'}
    for($i=0;$i-lt2;$i++){
        if([int]$moves[$i].Groups[1].Value-ne2*$i-or[int]$moves[$i].Groups[2].Value-ne$unit-or$Logs.host-notmatch('PROOF_HOST_RESULT owner=1 sequence='+$moves[$i].Groups[3].Value+' accepted=1 reason=accepted\b')){throw 'Preflight request/host receipt binding missing'}
    }
    $first=$rows.rejoin[0]
    $seq0=[int]$moves[0].Groups[3].Value;$seq1=[int]$moves[1].Groups[3].Value
    if($seq1-le$seq0){throw 'Preflight sequence order'}
    $leg0=@($trace|Where-Object {$_.epoch-eq1-and$_.unit-eq$unit-and$_.sequence-eq$seq0})
    if(-not$leg0.Count){throw 'First actual travel trace missing'}
    $firstLegProof=@(foreach($r in $rows.host){
        if($r.unit-ne$unit-or$r.revision-lt$leg0[0].afterRevision-or$r.revision-gt$leg0[-1].afterRevision){continue}
        $o=Get-FacingDisplacementAt $trace 1 $unit $seq0 $r.revision $r.position
        if(-not(Test-FacingVec $r.lastTravelDirection $o.direction)){throw 'Host first actual travel direction mismatch'}
        if($o.actualTravelCm-ge120){[ordered]@{snapshot=$r;oracle=$o}}
    })
    if(-not$firstLegProof.Count){throw 'Host first actual travel content missing'}
    $stationaryOracle=Get-FacingDisplacementAt $trace 1 $unit $seq1 $first.revision $first.position
    if($first.unit-ne$unit-or$stationaryOracle.actualTravelCm-lt120-or-not(Test-FacingVec $first.lastTravelDirection $stationaryOracle.direction)){throw 'Fresh late observer first content lost stationary direction'}
    $goal=@([double]$moves[1].Groups[4].Value,[double]$moves[1].Groups[5].Value,0.0)
    if(-not(Test-FacingVec $first.position $goal 0.01)-or-not(Test-FacingVec $stationaryOracle.lastAdvance.before $stationaryOracle.lastAdvance.after 0)){throw 'Fresh observer is not stationary at actual arrival'}
    $d0=$firstLegProof[-1].oracle.direction;$d1=$stationaryOracle.direction
    if(($d0[0]*$d1[0]+$d0[1]*$d1[1])-ge0.25){throw 'Distinct actual preflight turn missing'}
    $common=@($maps.host.Keys|Where-Object {$maps.lead.ContainsKey($_)-and$maps.rejoin.ContainsKey($_)}|Sort-Object {[int]$_})
    if($common.Count-lt3){return $null}
    $proof=@(foreach($key in $common){
        $a=$maps.host[$key]
        foreach($role in @('host','lead','rejoin')){
            $b=$maps[$role][$key]
            $o=Get-FacingDisplacementAt $trace 1 $unit $seq1 $b.revision $b.position
            if($b.unit-ne$unit-or$b.digest-cne$a.digest-or-not(Test-FacingVec $b.position $first.position)-or-not(Test-FacingVec $b.lastTravelDirection $o.direction)){throw 'Facing late observer actual content mismatch'}
        }
        [ordered]@{epoch=1;revision=$a.revision;unit=$unit;position=$a.position;lastTravelDirection=$a.lastTravelDirection;digest=$a.digest}
    })
    return [ordered]@{passed=$true;survivorOwner=1;freshObserverOwner=2;firstObserverRow=$first;commonActualContent=$proof;firstLegActualDisplacement=$firstLegProof[-1];stationaryActualDisplacement=$stationaryOracle;stationarySeconds=[double]$pass.Groups[2].Value;clientHistoryRequired=$false;owner2ReseededBeforeRematch=$false}
}
function Get-FacingResetEvidence {
    param([hashtable]$Logs)
    $proof=@(foreach($epoch in @(2,3)){
        $first=@{}
        foreach($role in @('host','lead','rejoin')){
            $r=@(Get-FacingStateRows $Logs[$role]|Where-Object {$_.epoch-eq$epoch})|Select-Object -First 1
            if(-not$r-or-not(Test-FacingVec $r.lastTravelDirection @(0,0,0))){throw 'Fresh rematch content retained old direction'}
            if(($role-eq'host'-and($r.role-cne'host'-or$r.viewer-ne0))-or($role-eq'lead'-and($r.role-cne'client'-or$r.viewer-ne1))-or($role-eq'rejoin'-and($r.role-cne'client'-or$r.viewer-ne2))){throw 'Facing reset role/viewer mismatch'}
            $first[$role]=$r
        }
        if($first.host.unit-ne$first.lead.unit-or$first.host.unit-ne$first.rejoin.unit-or-not(Test-FacingVec $first.host.position $first.lead.position)-or-not(Test-FacingVec $first.host.position $first.rejoin.position)){throw 'Rematch fresh unit content disagrees'}
        [ordered]@{epoch=$epoch;unit=$first.host.unit;position=$first.host.position;lastTravelDirection=$first.host.lastTravelDirection;allThreeRolesFreshZero=$true}
    })
    return [ordered]@{passed=$true;rematches=$proof;identityIncludesEpoch=$true}
}
