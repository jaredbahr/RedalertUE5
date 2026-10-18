# Pure diagnostic evidence validation. Dot-sourcing never starts a process or
# queries machine load. The independent preflight uses synthetic records only.
function Read-Rows([string]$Text,[string]$Tag) {
    $schema=switch($Tag){
        'PROOF_BOX_SEED' {'id,owner,kind,hp,x,y,digest,population'}
        'PROOF_BOX_EDGE' {'case,key,pressed,frame,processedFrame,device,user,handled,ready,focus,foreground,injectedPress,injectedRelease,inputSource,pointerSource,physicalInput'}
        'PROOF_BOX_OBSERVE' {'case,name,phase,frame,processedFrame,owner,epoch,matchPhase,width,height,x,y,ids,primary,revision,nativePress,nativeRelease,nativeGroup,nativeType,nativeRight,leftDown,shiftDown,typeDown,armed,dragging,focus,foreground,sequence,queue,digest,capture,typeScope,orderType,physicalInput,osFocusVerified'}
        'PROOF_BOX_RESULT' {'case,name,passed,expected,expectedPrimary,expectedRevisionDelta,nativePressDelta,nativeReleaseDelta,pointerSource,population,physicalInput'}
        'PROOF_BOX_HUD' {'frame,left,top,right,bottom,width,height,screenshot,drawn,physicalInput'}
        'PROOF_BOX_FINISH' {'passed,cases,elapsed,reason,unverified,population'}
        default {throw 'Unknown evidence tag'}
    }
    $keys=$schema.Split(',')
    $rows=@()
    foreach($match in [regex]::Matches($Text,([regex]::Escape($Tag)+' ([^\r\n]+)'))){
        if($match.Groups[1].Value.Length -gt 4096 -or $match.Groups[1].Value -match '[^\x20-\x7E]' -or $match.Groups[1].Value -notmatch '^(?:[A-Za-z][A-Za-z0-9]*=(?:\[[^\]]*\]|\S+))(?: [A-Za-z][A-Za-z0-9]*=(?:\[[^\]]*\]|\S+))*$'){throw 'Malformed evidence row'}
        $row=[ordered]@{}
        foreach($field in [regex]::Matches($match.Groups[1].Value,'([A-Za-z][A-Za-z0-9]*)=(\[[^\]]*\]|\S+)')){
            if($row.Contains($field.Groups[1].Value) -or $field.Groups[1].Value -cnotin $keys){throw 'Duplicate/unknown evidence key'}
            $row[$field.Groups[1].Value]=$field.Groups[2].Value
        }
        if($row.Count -ne $keys.Count){throw 'Missing evidence key'}
        $rows+=,$row
    }
    return $rows
}
function Check-Oracle([string]$Text,[string]$Images) {
    if($Text.Length -gt 16MB){throw 'Oversized diagnostic log'}
    $finish=@(Read-Rows $Text 'PROOF_BOX_FINISH')
    if($finish.Count -ne 1 -or $finish[0].passed -cne '1' -or $finish[0].cases -cne '31'){throw 'Missing complete successful fixture'}
    $observed=@(Read-Rows $Text 'PROOF_BOX_OBSERVE');$outcomes=@(Read-Rows $Text 'PROOF_BOX_RESULT');$seeds=@(Read-Rows $Text 'PROOF_BOX_SEED');$edges=@(Read-Rows $Text 'PROOF_BOX_EDGE')
    if($observed.Count -gt 150 -or $outcomes.Count -ne 32 -or $seeds.Count -ne 19){throw 'Unexpected evidence cardinality'}
    if($edges.Count -ne 126){throw 'Exact126 injected edge records required'}
    for($index=0;$index -lt 19;$index++){
        $s=$seeds[$index];$id=$index+1
        $x=if($id -le 16){800+(($id-1)%4)*70}elseif($id -eq 17){1130}else{1100+($id-18)*70}
        $y=if($id -le 16){350+[math]::Floor(($id-1)/4)*80}elseif($id -eq 17){400}else{600}
        if([int]$s.id -ne $id -or [int]$s.owner -ne $(if($id -eq 18){2}else{1}) -or [int]$s.kind -ne $(if($id -eq 17){2}elseif($id -eq 2){1}else{0}) -or [double]$s.hp -ne $(if($id -eq 19){0}else{100}) -or -not [double]::IsFinite([double]$s.x) -or -not [double]::IsFinite([double]$s.y) -or [math]::Abs([double]$s.x-$x) -gt 0.01 -or [math]::Abs([double]$s.y-$y) -gt 0.01 -or $s.digest -cne $seeds[0].digest -or $s.digest -notmatch '^[0-9a-fA-F]{32}$' -or $s.population -cne 'diagnostic-over-cap'){throw "Fixed diagnostic seed mismatch ID$id"}
    }
    $injectedPress=0;$injectedRelease=0;[uint64]$lastEdgeFrame=0
    foreach($edge in $edges){
        if($edge.key -ceq 'LeftMouseButton'){if($edge.pressed -ceq '1'){$injectedPress++}elseif($edge.pressed -ceq '0'){$injectedRelease++}else{throw 'Invalid edge state'}}
        if([int]$edge.case -lt 0 -or [int]$edge.case -ge 31 -or $edge.pressed -cnotin @('0','1') -or $edge.handled -cnotin @('0','1') -or [int]$edge.device -lt 0 -or [int]$edge.user -lt 0 -or $edge.device -cne $edges[0].device -or $edge.user -cne $edges[0].user -or $edge.ready -cne '1' -or $edge.focus -cne '1' -or $edge.foreground -cne '1' -or $edge.inputSource -cne 'viewport-engine-injection' -or $edge.pointerSource -cne 'explicit-controls-cursor' -or $edge.physicalInput -cne 'false' -or [int]$edge.injectedPress -ne $injectedPress -or [int]$edge.injectedRelease -ne $injectedRelease -or [uint64]$edge.frame -lt $lastEdgeFrame -or [uint64]$edge.processedFrame -eq 0 -or [uint64]$edge.processedFrame -gt [uint64]$edge.frame -or [uint64]$edge.frame-[uint64]$edge.processedFrame -gt 1){throw 'Injected edge/device/current-stack proof mismatch'}
        $lastEdgeFrame=[uint64]$edge.frame
    }
    $names=@('click','release-threshold','sticky-return','box-down-right','box-down-left','box-up-right','box-up-left','shift-latched-cap','late-shift-replaces','empty-clears','shift-empty','hud-start','hud-end-return-held','outside-pointer','real-resize','owner-change','epoch-change','phase-change','duplicate-press-release','group-1','group-2','group-3','group-4','group-5','group-6','group-7','group-8','group-9','type-visible','type-paired','context-move-route')
    $ids=@('[1]','[1]','[1]','[1,2,3,4,5,6,7,8,9,10,11,12]','[1,2,3,4,5,6,7,8,9,10,11,12]','[1,2,3,4,5,6,7,8,9,10,11,12]','[1,2,3,4,5,6,7,8,9,10,11,12]','[1,2,3,4,5,6,7,8,9,10,16,17]','[1,2,3,4,5,6,7,8,9,10,11,12]','[]','[17]','[17]','[17]','[17]','[17]','[]','[]','[17]','[1]','[17]','[17]','[17]','[17]','[17]','[17]','[17]','[17]','[17]','[17]','[17]','[1]')
    $primary=@(1,1,1,1,1,1,1,16,1,-1,17,17,17,17,17,-1,-1,17,1,17,17,17,17,17,17,17,17,17,17,17,1)
    $revisions=@(1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,2,2,0,1,2,2,2,2,2,2,2,2,2,1,2,0)
    [uint64]$lastObservationFrame=0;$lastObservationCase=0
    foreach($row in $observed){if([uint64]$row.frame -lt $lastObservationFrame -or [int]$row.case -lt $lastObservationCase -or [int]$row.case -gt $lastObservationCase+1){throw 'Global observation ordering mismatch'};$lastObservationFrame=[uint64]$row.frame;$lastObservationCase=[int]$row.case}
    for($case=0;$case -lt 31;$case++){
        $rows=@($observed|Where-Object {$_.case -ceq [string]$case})
        $before=@($rows|Where-Object {$_.phase -ceq 'baseline'})
        $after=@($rows|Where-Object {$_.phase -ceq $(if($case -lt 19){'released'}else{'key-regression'})})
        $outcome=@($outcomes|Where-Object {$_.case -ceq [string]$case})
        if($before.Count -ne 1 -or $after.Count -ne 1 -or $outcome.Count -ne $(if($case -eq 18){2}else{1}) -or @($outcome|Where-Object {$_.passed -cne '1'}).Count){throw "Missing/extra/failed case $case"}
        $phases=if($case -in @(28,29)){'baseline,type-first,key-regression'}elseif($case -ge 19){'baseline,key-regression'}elseif($case -in @(0,1)){'baseline,pressed,released'}elseif($case -in @(2,3)){'baseline,pressed,capture-request,capture-complete,sampled,before-release,released'}elseif($case -eq 14){'baseline,pressed,resized,before-release,released'}elseif($case -eq 18){'baseline,pressed,sampled,before-release,released,duplicate-released'}else{'baseline,pressed,sampled,before-release,released'}
        if(($rows.phase -join ',') -cne $phases -or @($rows|Where-Object {$_.name -cne $names[$case]}).Count -or @($outcome|Where-Object {$_.name -cne $names[$case]}).Count){throw "Missing/extra/reordered phase case $case"}
        $caseEdges=@($edges|Where-Object {$_.case -ceq [string]$case})
        $expectedEdges=if($case -eq 7){'LeftShift1,LeftMouseButton1,LeftShift0,LeftMouseButton0'}elseif($case -eq 8){'LeftMouseButton1,LeftShift1,LeftMouseButton0,LeftShift0'}elseif($case -eq 10){'LeftShift1,LeftMouseButton1,LeftMouseButton0,LeftShift0'}elseif($case -eq 18){'LeftMouseButton1,LeftMouseButton1,LeftMouseButton0,LeftMouseButton0'}elseif($case -ge 19 -and $case -le 27){$key=@('One','Two','Three','Four','Five','Six','Seven','Eight','Nine')[$case-19];'LeftControl1,'+$key+'1,'+$key+'0,LeftControl0,LeftMouseButton1,LeftMouseButton0,'+$key+'1,'+$key+'0'}elseif($case -eq 28){'T1,T0'}elseif($case -eq 29){'T1,T0,T1,T0'}elseif($case -eq 30){'RightMouseButton1,RightMouseButton0'}else{'LeftMouseButton1,LeftMouseButton0'}
        if((@($caseEdges|ForEach-Object {$_.key+$_.pressed}) -join ',') -cne $expectedEdges){throw "Exact injected key order mismatch case $case"}
        $b=$before[0];$a=$after[0]
        $expectedBefore=if($case -eq 7){'[16,17]'}elseif($case -eq 30){'[1]'}else{'[17]'}
        if($b.ids -cne $expectedBefore -or $a.ids -cne $ids[$case] -or [int]$a.primary -ne $primary[$case] -or ([uint64]$a.revision - [uint64]$b.revision) -ne [uint64]$revisions[$case] -or $b.digest -cne $seeds[0].digest){throw "Independent IDs/primary/revision oracle mismatch case $case"}
        $lastRelevant=if($case -eq 18){$caseEdges[2]}elseif($case -in @(8,10)){$caseEdges[2]}else{$caseEdges[-1]}
        if([uint64]$a.processedFrame -le [uint64]$lastRelevant.frame -or [uint64]$caseEdges[0].frame -lt [uint64]$b.frame){throw "Callbacks not bound to later processed edges case $case"}
        if([uint64]$a.frame -le [uint64]$b.frame -or [uint64]$a.processedFrame -gt [uint64]$a.frame -or [uint64]$a.frame-[uint64]$a.processedFrame -gt 1 -or $a.focus -cne '1' -or $a.foreground -cne '1' -or $a.armed -cne '0' -or $a.leftDown -cne '0' -or $a.queue -cne '0' -or $a.physicalInput -cne 'false' -or $a.osFocusVerified -cne 'false' -or $a.digest -cne $b.digest){throw "Context/stack/policy mismatch case $case"}
        $press=[int]$a.nativePress-[int]$b.nativePress;$release=[int]$a.nativeRelease-[int]$b.nativeRelease
        $clicks=if($case -ge 28){0}else{1}
        if($case -eq 18){if($press -notin @(1,2) -or $release -notin @(1,2)){throw 'Duplicate edge normalization outside bounds'}}elseif($press -ne $clicks -or $release -ne $clicks){throw "Native edge counter mismatch case $case"}
        if([int]$a.nativeGroup-[int]$b.nativeGroup -ne $(if($case -ge 19 -and $case -le 27){2}else{0}) -or [int]$a.nativeType-[int]$b.nativeType -ne $(if($case -eq 28){1}elseif($case -eq 29){2}else{0}) -or [int]$a.nativeRight-[int]$b.nativeRight -ne $(if($case -eq 30){1}else{0}) -or [int]$a.sequence-[int]$b.sequence -ne $(if($case -eq 30){1}else{0})){throw "Key/command route counter mismatch case $case"}
        if($case -lt 19){
            $pressed=@($rows|Where-Object {$_.phase -ceq 'pressed'})[0]
            $pressEdge=@($caseEdges|Where-Object {$_.key -ceq 'LeftMouseButton' -and $_.pressed -ceq '1'})[0]
            if([uint64]$pressed.processedFrame -le [uint64]$pressEdge.frame -or $pressed.leftDown -cne '1' -or $pressed.ids -cne $b.ids -or $pressed.revision -cne $b.revision -or [int]$pressed.nativePress-[int]$b.nativePress -ne 1){throw "Press-before-selection/current-frame mismatch case $case"}
        }
        foreach($o in $outcome){if($o.expected -cne $ids[$case] -or [int]$o.expectedPrimary -ne $primary[$case] -or [uint64]$o.expectedRevisionDelta -ne [uint64]$revisions[$case] -or $o.pointerSource -cne 'explicit-controls-cursor' -or $o.population -cne 'diagnostic-over-cap' -or $o.physicalInput -cne 'false'){throw "Outcome independent expectation mismatch case $case"}}
        if($case -eq 18){
            $duplicate=@($rows|Where-Object {$_.phase -ceq 'duplicate-released'})[0]
            if([uint64]$duplicate.processedFrame -le [uint64]$caseEdges[-1].frame -or $duplicate.ids -cne $a.ids -or $duplicate.primary -cne $a.primary -or $duplicate.revision -cne $a.revision -or $duplicate.nativePress -cne $a.nativePress -or ([int]$duplicate.nativeRelease-[int]$b.nativeRelease) -notin @(1,2) -or $duplicate.armed -cne '0' -or $duplicate.leftDown -cne '0'){throw 'Duplicate release changed selection or lacks processed-edge evidence'}
        }
    }
    $hud=@(Read-Rows $Text 'PROOF_BOX_HUD')
    if($hud.Count -ne 2 -or @($hud|Where-Object {$_.drawn -cne '1' -or $_.physicalInput -cne 'false'}).Count){throw 'Exact two drawn outline capture rows required'}
    foreach($case in @(2,3)){
        $request=@($observed|Where-Object {$_.case -ceq [string]$case -and $_.phase -ceq 'capture-request'})[0]
        $complete=@($observed|Where-Object {$_.case -ceq [string]$case -and $_.phase -ceq 'capture-complete'})[0]
        $expectedPath=[IO.Path]::GetFullPath((Join-Path $images ('box-held-'+$case+'.png')))
        $draw=@($hud|Where-Object {[string]::Equals([IO.Path]::GetFullPath($_.screenshot),$expectedPath,[StringComparison]::OrdinalIgnoreCase)})
        if($draw.Count -ne 1 -or -not [string]::Equals([IO.Path]::GetFullPath($request.capture),$expectedPath,[StringComparison]::OrdinalIgnoreCase) -or -not [string]::Equals([IO.Path]::GetFullPath($complete.capture),$expectedPath,[StringComparison]::OrdinalIgnoreCase) -or [uint64]$draw[0].frame -lt [uint64]$request.frame -or [uint64]$draw[0].frame -ge [uint64]$complete.frame -or $draw[0].width -cne '1280' -or $draw[0].height -cne '720' -or $request.dragging -cne '1' -or $request.armed -cne '1' -or $request.leftDown -cne '1' -or $complete.dragging -cne '1'){throw "Capture request/HUD/completion binding mismatch case $case"}
        $bounds=if($case -eq 2){@([double]$seeds[0].x,[double]$seeds[0].y,([double]$seeds[0].x+12),[double]$seeds[0].y)}else{@(760,310,1050,630)}
        $fields=@('left','top','right','bottom')
        for($index=0;$index -lt 4;$index++){if(-not [double]::IsFinite([double]$draw[0][$fields[$index]]) -or [math]::Abs([double]$draw[0][$fields[$index]]-$bounds[$index]) -gt 0.00001){throw "Independent held outline bounds mismatch case $case"}}
    }
    Assert-BoxObservationPlan $Text $observed $edges $outcomes $seeds $Images
    return @{passed=$true;fixedCases=31;independentExpectedArrays=$true;exactIntegerRevisions=$true;nativeEdgeCountersVerified=$true;duplicateEngineNormalizationSeparatelyBounded=$true;realPointerRouteVerified=$false;lostReleaseOccurrenceVerified=$false}
}

function Assert-BoxObservationPlan($Text,$Observed,$Edges,$Outcomes,$Seeds,$Images) {
    $known=@('PROOF_BOX_SEED','PROOF_BOX_EDGE','PROOF_BOX_OBSERVE','PROOF_BOX_RESULT','PROOF_BOX_HUD','PROOF_BOX_FINISH')
    $tags=@([regex]::Matches($Text,'PROOF_BOX_[A-Z_]+')|ForEach-Object {$_.Value})
    if($tags.Count -ne 302 -or @($tags|Where-Object {$_ -cnotin $known}).Count -or $Observed.Count -ne 122){throw 'Unconsumed/unknown diagnostic records'}
    $base18=@($Observed|Where-Object {$_.case -ceq '18' -and $_.phase -ceq 'baseline'})[0]
    $held18=@($Observed|Where-Object {$_.case -ceq '18' -and $_.phase -ceq 'before-release'})[0]
    $dup18=@($Observed|Where-Object {$_.case -ceq '18' -and $_.phase -ceq 'duplicate-released'})[0]
    $dropPress=2-([int]$held18.nativePress-[int]$base18.nativePress)
    $dropRelease=2-([int]$dup18.nativeRelease-[int]$base18.nativeRelease)
    if($dropPress -notin @(0,1) -or $dropRelease -notin @(0,1)){throw 'Invalid duplicate normalization'}
    $anchorX=[double]$Seeds[0].x;$anchorY=[double]$Seeds[0].y
    foreach($row in $Observed){
        foreach($field in @('case','frame','processedFrame','epoch','matchPhase','width','height','revision','nativePress','nativeRelease','nativeGroup','nativeType','nativeRight','leftDown','shiftDown','typeDown','armed','dragging','focus','foreground','sequence','queue')){
            $number=[uint64]0
            if($row[$field] -cnotmatch '^(0|[1-9][0-9]*)$' -or -not [uint64]::TryParse($row[$field],[ref]$number)){throw "Noncanonical integer $field"}
        }
        $case=[int]$row.case;$phase=$row.phase
        if($case -lt 0 -or $case -ge 31 -or [uint64]$row.frame -eq 0 -or [uint64]$row.processedFrame -eq 0 -or [uint64]$row.processedFrame -gt [uint64]$row.frame -or [uint64]$row.frame-[uint64]$row.processedFrame -gt 1){throw 'Every observation requires current processed frame'}
        $base=@($Observed|Where-Object {$_.case -ceq [string]$case -and $_.phase -ceq 'baseline'})[0]
        $basePrimary=if($case -eq 7){'16'}elseif($case -eq 30){'1'}else{'17'}
        if($base.primary -cne $basePrimary -or $row.primary -cnotmatch '^(-1|0|[1-9][0-9]*)$'){throw 'Baseline/canonical primary mismatch'}
        $baseline=$phase -ceq 'baseline';$pressed=$phase -ceq 'pressed';$final=$phase -in @('released','duplicate-released','key-regression')
        $changed=$phase -in @('sampled','resized');$pre=$phase -ceq 'before-release'
        $owner=if($case -eq 15 -and $changed){2}else{1}
        $epoch=if($case -eq 16 -and $changed){8}else{7}
        $matchPhase=if($case -eq 17 -and $changed){2}else{1}
        $width=if($case -eq 14 -and $phase -ceq 'resized'){960}else{1280}
        $height=if($case -eq 14 -and $phase -ceq 'resized'){540}else{720}
        $startX=760.0;$startY=310.0;$endX=1050.0;$endY=630.0
        if($case -le 2 -or $case -ge 14){$startX=$anchorX;$startY=$anchorY;$endX=$anchorX+4;$endY=$anchorY}
        if($case -eq 0){$endX=$anchorX+2}
        if($case -in @(4,6)){$startX=1050;$endX=760}
        if($case -in @(5,6)){$startY=630;$endY=310}
        if($case -in @(9,10)){$startX=760;$startY=650;$endX=780;$endY=670}
        if($case -eq 11){$startX=20;$startY=20}
        $x=$endX;$y=$endY
        if($baseline -or $pressed -or $case -in 19..29){$x=$startX;$y=$startY}
        if($case -eq 2 -and -not($baseline -or $pressed)){$x=if($pre -or $final){$anchorX}else{$anchorX+12};$y=$anchorY}
        if($case -eq 12 -and $changed){$x=20;$y=20}
        if($case -eq 13 -and $changed){$x=-1;$y=350}
        if($case -eq 30 -and $final){$x=760;$y=650}
        $shift=if(($pressed -and $case -in @(7,10)) -or (-not($baseline -or $pressed) -and $case -in @(8,10))){1}else{0}
        $left=if($case -lt 19 -and -not($baseline -or $final)){1}else{0}
        $armed=if($case -lt 19 -and -not($baseline -or $final) -and (($pressed -and $case -ne 11) -or (-not $pressed -and $case -notin 11..17))){1}else{0}
        $dragging=if($armed -and -not $pressed){1}else{0}
        $typeDown=if($phase -ceq 'type-first'){1}else{0}
        foreach($pair in @(@('owner',$owner),@('epoch',$epoch),@('matchPhase',$matchPhase),@('width',$width),@('height',$height),@('shiftDown',$shift),@('leftDown',$left),@('typeDown',$typeDown),@('armed',$armed),@('dragging',$dragging),@('focus',1),@('foreground',1),@('queue',0))){if($row[$pair[0]] -cne [string]$pair[1]){throw "Case$case/$phase stimulus/state mismatch $($pair[0])"}}
        foreach($pair in @(@('x',$x),@('y',$y))){$value=[double]0;if(-not [double]::TryParse($row[$pair[0]],[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$value) -or -not [double]::IsFinite($value) -or [math]::Abs($value-$pair[1]) -gt 0.00000001){throw "Case$case/$phase finite pointer mismatch $($pair[0])"}}
        if($row.physicalInput -cne 'false' -or $row.osFocusVerified -cne 'false'){throw 'Observation asserted physical evidence'}
        $digestChanged=$case -in @(16,17) -and $changed
        if($digestChanged -eq ($row.digest -ceq $Seeds[0].digest) -or $row.digest -notmatch '^[0-9a-fA-F]{32}$'){throw 'Snapshot digest/context mismatch'}
        if(-not $final){
            $delta=if($case -in @(15,16) -and $changed){1}elseif($case -in @(15,16) -and $pre){2}elseif($phase -ceq 'type-first'){1}else{0}
            $expectedIds=if($delta -gt 0 -and $case -in @(15,16)){'[]'}else{$base.ids}
            $expectedPrimary=if($expectedIds -ceq '[]'){'-1'}else{$base.primary}
            if($row.ids -cne $expectedIds -or $row.primary -cne $expectedPrimary -or [uint64]$row.revision-[uint64]$base.revision -ne $delta){throw "Intermediate selection/revision mismatch case$case/$phase"}
        }
        $past=@($Edges|Where-Object {[uint64]$_.frame -lt [uint64]$row.processedFrame})
        foreach($pair in @(@('leftDown','LeftMouseButton'),@('shiftDown','LeftShift'),@('typeDown','T'))){
            $keyEvents=@($past|Where-Object {$_.key -ceq $pair[1]})
            $held=if($keyEvents.Count){$keyEvents[-1].pressed}else{'0'}
            if($row[$pair[0]] -cne $held){throw 'Observed held state contradicts processed injected keys'}
        }
        $presses=@($past|Where-Object {$_.key -ceq 'LeftMouseButton' -and $_.pressed -ceq '1'}).Count
        $releases=@($past|Where-Object {$_.key -ceq 'LeftMouseButton' -and $_.pressed -ceq '0'}).Count
        if(@($past|Where-Object {$_.case -ceq '18' -and $_.key -ceq 'LeftMouseButton' -and $_.pressed -ceq '1'}).Count -eq 2){$presses-=$dropPress}
        if(@($past|Where-Object {$_.case -ceq '18' -and $_.key -ceq 'LeftMouseButton' -and $_.pressed -ceq '0'}).Count -eq 2){$releases-=$dropRelease}
        $group=@($past|Where-Object {$_.pressed -ceq '1' -and $_.key -cmatch '^(One|Two|Three|Four|Five|Six|Seven|Eight|Nine)$'}).Count
        $type=@($past|Where-Object {$_.pressed -ceq '1' -and $_.key -ceq 'T'}).Count
        $right=@($past|Where-Object {$_.pressed -ceq '1' -and $_.key -ceq 'RightMouseButton'}).Count
        foreach($pair in @(@('nativePress',$presses),@('nativeRelease',$releases),@('nativeGroup',$group),@('nativeType',$type),@('nativeRight',$right),@('sequence',$right))){if([int]$row[$pair[0]] -ne $pair[1]){throw "Every-observation processed edge/counter mismatch $($pair[0]) case$case/$phase"}}
        $scope=if(($phase -ceq 'type-first') -or ($case -eq 28 -and $final)){'visible'}elseif($case -eq 29 -and $final){'battlefield'}else{'none'}
        $orderType=if($case -eq 30 -and $final){'Move'}else{'none'}
        if($row.typeScope -cne $scope -or $row.orderType -cne $orderType){throw 'Observed type scope/order type mismatch'}
        $captureExpected=if($case -in @(2,3) -and $phase -in @('capture-request','capture-complete','sampled','before-release','released')){[IO.Path]::GetFullPath((Join-Path $Images ('box-held-'+$case+'.png')))}else{'none'}
        if($captureExpected -ceq 'none'){if($row.capture -cne 'none'){throw 'Unexpected capture reference'}}elseif(-not [string]::Equals([IO.Path]::GetFullPath($row.capture),$captureExpected,[StringComparison]::OrdinalIgnoreCase)){throw 'Wrong capture reference on observation'}
    }
    $expectedCases=@(0..18)+@(18)+@(19..30)
    if(($Outcomes.case -join ',') -cne ($expectedCases -join ',')){throw 'Reordered/unconsumed result records'}
    for($index=0;$index -lt $Outcomes.Count;$index++){
        $o=$Outcomes[$index];$c=[int]$o.case;$phase=if($index -eq 19){'duplicate-released'}elseif($c -lt 19){'released'}else{'key-regression'}
        $a=@($Observed|Where-Object {$_.case -ceq [string]$c -and $_.phase -ceq $phase})[0];$b=@($Observed|Where-Object {$_.case -ceq [string]$c -and $_.phase -ceq 'baseline'})[0]
        if([int]$o.nativePressDelta -ne ([int]$a.nativePress-[int]$b.nativePress) -or [int]$o.nativeReleaseDelta -ne ([int]$a.nativeRelease-[int]$b.nativeRelease)){throw 'Outcome counter contradiction'}
    }
}

function Test-BoxInputPng([string]$Path) {
    $file=Get-Item -LiteralPath $Path -ErrorAction Stop
    if($file.Length -lt 45 -or $file.Length -gt 10MB){throw 'PNG size outside bound'}
    Add-Type -AssemblyName System.Drawing.Common
    $stream=[IO.File]::OpenRead($file.FullName);$image=$null;$raster=$null;$graphics=$null;$locked=$null
    try {
        $image=[Drawing.Image]::FromStream($stream,$false,$true)
        if($image.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Png.Guid -or $image.Width -ne 1280 -or $image.Height -ne 720){throw 'Decoded PNG format/dimensions mismatch'}
        $raster=[Drawing.Bitmap]::new(1280,720,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics=[Drawing.Graphics]::FromImage($raster);$graphics.DrawImage($image,[Drawing.Rectangle]::new(0,0,1280,720));$graphics.Dispose();$graphics=$null
        $locked=$raster.LockBits([Drawing.Rectangle]::new(0,0,1280,720),[Drawing.Imaging.ImageLockMode]::ReadOnly,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
        if($locked.Stride -ne 5120){throw 'Unexpected decoded raster stride'}
        $pixels=[byte[]]::new([math]::Abs($locked.Stride)*720);[Runtime.InteropServices.Marshal]::Copy($locked.Scan0,$pixels,0,$pixels.Length)
        return @{decoded=$true;width=1280;height=720;pixelSHA256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($pixels))}
    } finally {if($locked){$raster.UnlockBits($locked)};if($graphics){$graphics.Dispose()};if($raster){$raster.Dispose()};if($image){$image.Dispose()};$stream.Dispose()}
}
