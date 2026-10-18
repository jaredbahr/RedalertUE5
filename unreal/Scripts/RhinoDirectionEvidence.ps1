. (Join-Path $PSScriptRoot 'FacingDisplacementEvidence.ps1')
function Get-RhinoDirectionEvidence {
    param([string]$Log)
    function Need($Pass,[string]$Reason){if(-not $Pass){throw ('Rhino direction: '+$Reason)}}
    function Num($V){return $null-ne$V-and$V-isnot[string]-and$V-isnot[bool]-and$V-isnot[array]-and[double]::IsFinite([double]$V)}
    function Keys($E){
        if($E.ValueKind-eq[Text.Json.JsonValueKind]::Object){$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase);foreach($p in $E.EnumerateObject()){Need ($seen.Add($p.Name)) 'duplicate JSON key';Keys $p.Value}}
        elseif($E.ValueKind-eq[Text.Json.JsonValueKind]::Array){foreach($v in $E.EnumerateArray()){Keys $v}}
    }
    function Vec($V){return $V-is[array]-and$V.Count-eq3-and@($V|Where-Object {$null-eq$_-or$_-is[string]-or$_-is[bool]-or-not[double]::IsFinite([double]$_)}).Count-eq0}
    function Dist($A,$B){return [math]::Sqrt([math]::Pow(($A[0]-$B[0]),2)+[math]::Pow(($A[1]-$B[1]),2)+[math]::Pow(($A[2]-$B[2]),2))}
    Need ($Log.Length-le24MB) 'bounded log'
    Need ($Log-notmatch'PROOF_(?:RHINO_DIRECTION|VISUAL_GEOMETRY)_ERROR|PROOF_[A-Z_]+[^\r\n]*status=fail|Fatal error|Assertion failed') 'no failure marker'
    $m=[regex]::Matches($Log,'(?m)^.*PROOF_RHINO_DIRECTION (\{[^\r\n]*\})\r?$')
    Need ($m.Count-eq4-and[regex]::Matches($Log,'PROOF_RHINO_DIRECTION(?=\s|$)').Count-eq4) 'four complete observations'
    $rows=@($m|ForEach-Object {$json=$_.Groups[1].Value;$doc=[Text.Json.JsonDocument]::Parse($json);try{Keys $doc.RootElement}finally{$doc.Dispose()};$json|ConvertFrom-Json -ErrorAction Stop})
    $trace=@(Get-FacingDisplacementTrace $Log)
    $oracles=@()
    $names=@('08-rhino-direction-first.png','09-rhino-direction-turn.png','10-rhino-stationary-a.png','11-rhino-stationary-b.png')
    $stages=@(1,3,4,5)
    $restored=[regex]::Matches($Log,'(?m)^.*PROOF_RHINO_DIRECTION_RESTORED stage=(\d+) verified=1\r?$')
    $written=[regex]::Matches($Log,'(?m)^.*PROOF_RHINO_DIRECTION_WRITTEN stage=(\d+)\r?$')
    Need ($restored.Count-eq4-and$written.Count-eq4-and[regex]::Matches($Log,'PROOF_RHINO_DIRECTION_RESTORED').Count-eq4-and[regex]::Matches($Log,'PROOF_RHINO_DIRECTION_WRITTEN').Count-eq4) 'four complete restore/write barriers'
    for($i=0;$i-lt4;$i++){
        $r=$rows[$i]
        foreach($p in @('stage','unit','owner','kind','epoch','revision','frame','width','height','visibleUnitViews','moveSequence')){Need ((Num $r.$p)-and[double]$r.$p-eq[math]::Truncate([double]$r.$p)-and[double]$r.$p-ge0-and[double]$r.$p-le[int]::MaxValue) ('integer '+$p)}
        foreach($p in @('elapsed','orthoWidth')){Need ((Num $r.$p)-and$r.$p-ge0) ('numeric '+$p)}
        foreach($p in @('rhinoLoaded','moveReceiptAccepted','otherUnitViewsHidden','hudHidden','modelModified','physicalInputVerified')){Need ($r.$p-is[bool]) ('boolean '+$p)}
        Need ($r.unit-gt0-and$r.epoch-gt0-and$r.revision-gt0-and$r.capture-is[string]-and$r.snapshotDigest-is[string]-and$r.snapshotDigest-match'^[a-fA-F0-9]{32}$') 'positive identities and snapshot digest'
        Need ($r.capture-ceq$names[$i]-and$r.stage-eq$stages[$i]-and$r.kind-eq0-and$r.owner-eq1-and$r.unit-eq$rows[0].unit-and$r.epoch-eq$rows[0].epoch) 'same combat identity and ordered captures'
        foreach($v in @('modelPosition','moveOrigin','moveGoal','authoritativeLastTravelDirection')){Need (Vec $r.$v) ('finite '+$v)}
        foreach($v in @('location','rotationPYR','axisX','axisY','axisZ','scale','localMin','localMax','worldMin','worldMax')){Need (Vec $r.component.$v) ('finite component '+$v)}
        foreach($v in @('localMin','localMax','worldMin','worldMax','location','scale','rotationPYR','axisX','axisY','axisZ')){Need (Vec $r.floor.$v) ('finite floor '+$v)}
        Need ($r.component.asset-is[string]-and$r.floor.asset-is[string]-and$r.floor.asset-ceq'/Engine/BasicShapes/Cube.Cube') 'typed asset identities'
        Need ($r.component.asset-ceq'/Game/BlockoutGenerated/Meshes/SM_RhinoTank_Blockout.SM_RhinoTank_Blockout'-and$r.rhinoLoaded-and$r.moveReceiptAccepted-and$r.moveSequence-gt0) 'loaded Rhino and accepted Move'
        Need ($r.width-eq1280-and$r.height-eq720-and$r.visibleUnitViews-eq1-and$r.otherUnitViewsHidden-and$r.hudHidden-and-not$r.modelModified-and-not$r.physicalInputVerified) 'capture scope'
        Need ((Dist $r.component.location $r.modelPosition)-lt0.01-and[math]::Abs($r.component.worldMin[2]-$r.floor.worldMax[2])-lt0.01) 'placement and grounding'
        $oracle=Get-FacingDisplacementAt $trace $r.epoch $r.unit $r.moveSequence $r.revision $r.modelPosition
        $oracles+=,$oracle
        $expectedDirection=$oracle.direction
        Need ($oracle.actualTravelCm-ge100) 'actual post-acceptance travel'
        Need ((Dist $r.authoritativeLastTravelDirection $expectedDirection)-lt0.001) 'actual authoritative last travel content'
        Need ((Dist $r.component.axisY $expectedDirection)-lt0.001) 'current Rhino local +Y maps to authoritative travel direction'
        $minimum=@([double]::PositiveInfinity,[double]::PositiveInfinity,[double]::PositiveInfinity)
        $maximum=@([double]::NegativeInfinity,[double]::NegativeInfinity,[double]::NegativeInfinity)
        for($corner=0;$corner-lt8;$corner++){
            $local=@(0..2|ForEach-Object {if($corner-band(1-shl$_)){$r.component.localMax[$_]}else{$r.component.localMin[$_]}})
            for($axis=0;$axis-lt3;$axis++){
                $world=$r.component.location[$axis]+$r.component.axisX[$axis]*$local[0]*$r.component.scale[0]+$r.component.axisY[$axis]*$local[1]*$r.component.scale[1]+$r.component.axisZ[$axis]*$local[2]*$r.component.scale[2]
                $minimum[$axis]=[math]::Min($minimum[$axis],$world);$maximum[$axis]=[math]::Max($maximum[$axis],$world)
            }
        }
        Need ((Dist $minimum $r.component.worldMin)-lt0.01-and(Dist $maximum $r.component.worldMax)-lt0.01) 'independent transformed corners match actual bounds'
        Need ([int]$restored[$i].Groups[1].Value-eq$stages[$i]-and[int]$written[$i].Groups[1].Value-eq$stages[$i]+1-and$m[$i].Index-lt$restored[$i].Index-and$restored[$i].Index-lt$written[$i].Index) 'ordered restoration/write'
        if($i-gt0){Need ($r.revision-gt$rows[$i-1].revision-and$r.frame-gt$rows[$i-1].frame-and$r.elapsed-gt$rows[$i-1].elapsed-and$written[$i-1].Index-lt$m[$i].Index) 'progressing snapshots and barriers'}
    }
    $moves=[regex]::Matches($Log,'(?m)^.*PROOF_RHINO_DIRECTION_MOVE stage=(0|2) unit=(\d+) sequence=(\d+) destinationX=(-?\d+(?:\.\d+)?) destinationY=(-?\d+(?:\.\d+)?)\r?$')
    Need ($moves.Count-eq2-and[regex]::Matches($Log,'PROOF_RHINO_DIRECTION_MOVE').Count-eq2-and$moves[0].Index-lt$m[0].Index-and$moves[1].Index-gt$written[0].Index-and$moves[1].Index-lt$m[1].Index) 'two complete ordinary moves in order'
    for($i=0;$i-lt2;$i++){
        $r=$rows[$i];$seq=[int]$moves[$i].Groups[3].Value
        Need ($seq-eq$r.moveSequence-and[int]$moves[$i].Groups[2].Value-eq$r.unit-and[int]$moves[$i].Groups[1].Value-eq2*$i) 'command identity'
        Need ([regex]::IsMatch($Log,('PROOF_SKIRMISH_REQUEST owner=1 sequence='+$seq+' type=Move unit='+$r.unit+' target=-1\b'))) 'ordinary Move request'
        Need ($oracles[$i].actualTravelCm-ge100) 'observed actual travel'
        Need ([math]::Abs($r.moveGoal[0]-[double]$moves[$i].Groups[4].Value)-lt0.01-and[math]::Abs($r.moveGoal[1]-[double]$moves[$i].Groups[5].Value)-lt0.01) 'bound destination'
    }
    $dx0=$oracles[0].direction[0];$dy0=$oracles[0].direction[1]
    $dx1=$oracles[1].direction[0];$dy1=$oracles[1].direction[1]
    $h0=[math]::Atan2($dy0,$dx0)*180/[math]::PI;$h1=[math]::Atan2($dy1,$dx1)*180/[math]::PI
    Need (($dx0*$dx1+$dy0*$dy1)-lt0.25-and$rows[1].moveSequence-gt$rows[0].moveSequence) 'distinct actual turn headings'
    Need ($rows[2].moveSequence-eq$rows[1].moveSequence-and$rows[3].moveSequence-eq$rows[1].moveSequence-and(Dist $rows[2].modelPosition $rows[2].moveGoal)-lt0.01-and(Dist $rows[3].modelPosition $rows[2].modelPosition)-lt0.01-and$rows[3].elapsed-$rows[2].elapsed-ge1.2) 'arrival and stationary interval'
    return [ordered]@{passed=$true;measurements=$rows;independentDisplacementOracles=$oracles;firstTravelHeadingDegrees=$h0;secondTravelHeadingDegrees=$h1;stationaryIntervalSeconds=($rows[3].elapsed-$rows[2].elapsed);stationaryDisplacementCm=(Dist $rows[3].modelPosition $rows[2].modelPosition);observedRotationPYR=@($rows|ForEach-Object {,@($_.component.rotationPYR)});acceptedCurrentMeshFront='+Y';physicalInputVerified=$false;scope='Two ordinary shared-handler Move requests; expected direction from actual authoritative before/after Advance positions; local +Y and transformed bounds through arrival; diagnostic only'}
}
