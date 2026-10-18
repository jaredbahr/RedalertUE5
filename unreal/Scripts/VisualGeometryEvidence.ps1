# Offline validation of read-only measurements; does not assume either suspected defect.
function Get-VisualGeometryEvidence {
    param([Parameter(Mandatory)][string]$Log)
    function Need($Pass,[string]$Reason) { if(-not $Pass){throw ('Visual geometry: '+$Reason)} }
    function Number($Value) { return $null-ne$Value -and $Value-isnot[string] -and $Value-isnot[bool] -and $Value-isnot[array] -and [double]::IsFinite([double]$Value) }
    function Vector($Value) { return $Value-is[array] -and $Value.Count-eq3 -and @($Value|Where-Object {-not(Number $_)-or[math]::Abs([double]$_)-gt20000}).Count-eq0 }
    function Keys($Element) {
        if($Element.ValueKind-eq[System.Text.Json.JsonValueKind]::Object){$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase);foreach($p in $Element.EnumerateObject()){Need ($seen.Add($p.Name)) 'duplicate JSON key';Keys $p.Value}}
        elseif($Element.ValueKind-eq[System.Text.Json.JsonValueKind]::Array){foreach($v in $Element.EnumerateArray()){Keys $v}}
    }
    function Component($Value) {
        Need ($Value.asset-is[string] -and $Value.asset-match'^/(Engine|Game)/') 'asset identity'
        foreach($p in @('localMin','localMax','worldMin','worldMax','location','scale','rotationPYR','axisX','axisY','axisZ')){Need (Vector $Value.$p) ('component vector '+$p)}
        for($i=0;$i-lt3;$i++){Need ($Value.localMin[$i]-lt$Value.localMax[$i]-and$Value.worldMin[$i]-lt$Value.worldMax[$i]-and$Value.scale[$i]-gt0) 'ordered bounds and positive scale'}
    }
    $names=@('03-geometry-harvester-contact.png','04-geometry-harvester-motion-a.png','05-geometry-harvester-motion-b.png','06-geometry-rhino-motion-a.png','07-geometry-rhino-motion-b.png')
    $matches=[regex]::Matches($Log,'(?m)^.*PROOF_VISUAL_GEOMETRY (\{[^\r\n]+\})\r?$')
    Need ($matches.Count-eq5) 'exactly five measurement rows'
    $rows=@();$previousRevision=-1;$previousFrame=-1;$previousElapsed=-1
    for($i=0;$i-lt5;$i++){
        $json=$matches[$i].Groups[1].Value;Need ($json.Length-le20000-and$json-notmatch'[^\x00-\x7F]') 'bounded ASCII JSON'
        $document=$null;try{$document=[System.Text.Json.JsonDocument]::Parse($json);Keys $document.RootElement}finally{if($document){$document.Dispose()}}
        $r=$json|ConvertFrom-Json -Depth 10
        Need ($r.capture-ceq$names[$i]-and$r.stage-eq$i) 'capture order'
        foreach($p in @('unit','owner','kind','epoch','revision','frame','elapsed','width','height','orthoWidth','visibleUnitViews')){Need (Number $r.$p) ('numeric '+$p)}
        foreach($p in @('unit','owner','kind','epoch','revision','frame','width','height','visibleUnitViews')){Need ([double]$r.$p-eq[math]::Truncate([double]$r.$p)) ('integer '+$p)}
        Need ($r.unit-gt0-and$r.owner-eq1-and$r.kind-eq$(if($i-lt3){1}else{0})-and$r.epoch-eq1-and$r.width-eq1280-and$r.height-eq720) 'identity/viewport'
        Need ($r.revision-gt$previousRevision-and$r.frame-gt$previousFrame-and$r.elapsed-gt$previousElapsed-and$r.orthoWidth-gt0) 'monotonic sample'
        foreach($p in @('modelModified','physicalInputVerified','hudHidden','otherUnitViewsHidden','rhinoLoaded')){Need ($r.$p-is[bool]) ('boolean '+$p)}
        Need ($r.modelModified-ceq$false-and$r.physicalInputVerified-ceq$false-and$r.hudHidden-ceq$true-and$r.otherUnitViewsHidden-ceq$true-and$r.visibleUnitViews-eq1) 'scope/isolation'
        foreach($p in @('modelPosition','floorActorLocation','floorActorScale','floorActorRotationPYR','cameraLocation','cameraRotationPYR')){Need (Vector $r.$p) ('row vector '+$p)}
        Component $r.component;Component $r.floor
        Need ($r.floor.asset-ceq'/Engine/BasicShapes/Cube.Cube') 'actual floor mesh'
        Need ($r.component.asset-ceq$(if($i-lt3){'/Engine/BasicShapes/Cube.Cube'}else{'/Game/BlockoutGenerated/Meshes/SM_RhinoTank_Blockout.SM_RhinoTank_Blockout'})) 'actual displayed unit mesh'
        foreach($p in @('projectedMin','projectedMax')){Need ($r.$p-is[array]-and$r.$p.Count-eq2-and(Number $r.$p[0])-and(Number $r.$p[1])) 'projected bounds vector'}
        Need ($r.projectedMin[0]-ge16-and$r.projectedMin[1]-ge16-and$r.projectedMax[0]-le$r.width-16-and$r.projectedMax[1]-le$r.height-16-and$r.projectedMax[0]-gt$r.projectedMin[0]-and$r.projectedMax[1]-gt$r.projectedMin[1]) 'whole component bounds inside capture'
        for($axis=0;$axis-lt3;$axis++){$expected=[double]$r.modelPosition[$axis];if($axis-eq2-and-not$r.rhinoLoaded){$expected+=30};Need ([math]::Abs([double]$r.component.location[$axis]-$expected)-lt0.01) 'component/model position binding'}
        $previousRevision=$r.revision;$previousFrame=$r.frame;$previousElapsed=$r.elapsed;$rows+=,$r
    }
    Need ($rows[0].unit-eq$rows[1].unit-and$rows[1].unit-eq$rows[2].unit-and$rows[3].unit-eq$rows[4].unit-and$rows[0].unit-ne$rows[3].unit) 'same-unit observations'
    $written=[regex]::Matches($Log,'PROOF_VISUAL_GEOMETRY_WRITTEN stage=(\d+)')
    Need ($written.Count-eq5) 'five write barriers'
    for($i=0;$i-lt5;$i++){Need ([int]$written[$i].Groups[1].Value-eq$i+1) 'ordered write barrier'}
    $restored=[regex]::Matches($Log,'PROOF_VISUAL_GEOMETRY_RESTORED stage=(\d+) verified=1')
    Need ($restored.Count-eq5) 'five restoration barriers'
    for($i=0;$i-lt5;$i++){Need ([int]$restored[$i].Groups[1].Value-eq$i) 'ordered restoration barrier'}
    $motion=@(foreach($pair in @(@(1,2),@(3,4))){$a=$rows[$pair[0]];$b=$rows[$pair[1]];$delta=@(0..2|ForEach-Object {[double]$b.component.location[$_]-[double]$a.component.location[$_]});$distance=[math]::Sqrt(($delta|ForEach-Object {$_*$_}|Measure-Object -Sum).Sum);Need ($distance-ge100) 'measurable same-unit displacement';[pscustomobject]@{unit=$a.unit;kind=$a.kind;distanceCm=$distance;deltaCm=$delta;headingDegrees=([math]::Atan2($delta[1],$delta[0])*180/[math]::PI);rotationBeforePYR=$a.component.rotationPYR;rotationAfterPYR=$b.component.rotationPYR;semanticMeshFrontVerified=$false}})
    $ground=@(foreach($r in $rows){$horizontal=[math]::Abs($r.floor.rotationPYR[0])-lt0.01-and[math]::Abs($r.floor.rotationPYR[2])-lt0.01;$covered=$true;foreach($i in @(0,1)){$covered=$covered-and$r.floor.worldMin[$i]-le$r.component.worldMin[$i]-and$r.floor.worldMax[$i]-ge$r.component.worldMax[$i]};[pscustomobject]@{unit=$r.unit;capture=$r.capture;componentBottomZ=$r.component.worldMin[2];floorTopZ=$r.floor.worldMax[2];bottomMinusFloorTopCm=([double]$r.component.worldMin[2]-[double]$r.floor.worldMax[2]);floorHorizontal=$horizontal;floorCoversFootprintXY=$covered}})
    [pscustomobject]@{passed=$true;measurements=$rows;grounding=$ground;motion=$motion;physicalInputVerified=$false;modelModified=$false;scope='Actual instrumented runtime transforms/bounds and isolated views; semantic front and visible contact require original pixel review; no balance/art/physical acceptance'}
}
