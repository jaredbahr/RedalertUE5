param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'BoxInputEvidence.ps1')
if(-not $OutputDirectory){$OutputDirectory=Join-Path $PSScriptRoot '../../generated/box-evidence-preflight'}
New-Item -ItemType Directory -Force -Path $OutputDirectory|Out-Null
$images=[IO.Path]::GetFullPath((Join-Path $OutputDirectory 'images'));New-Item -ItemType Directory -Force -Path $images|Out-Null
$watch=[Diagnostics.Stopwatch]::StartNew();$checks=[Collections.Generic.List[object]]::new()
# SYNTHETIC transcripts only. Never invoke the runtime runner or query processes.
function Line($Tag,$Fields){$Tag+' '+(($Fields.GetEnumerator()|ForEach-Object {$_.Key+'='+[string]$_.Value}) -join ' ')}
function Transcript([int]$DuplicateCallbacks=2){
    $events=[Collections.Generic.List[object]]::new()
    $seedDigest=[Convert]::ToHexString([Security.Cryptography.MD5]::HashData([Text.Encoding]::UTF8.GetBytes('synthetic baseline')))
    $changedDigest=[Convert]::ToHexString([Security.Cryptography.MD5]::HashData([Text.Encoding]::UTF8.GetBytes('synthetic changed')))
    for($id=1;$id -le 19;$id++){
        $sx=if($id -le 16){800+(($id-1)%4)*70}elseif($id -eq 17){1130}else{1100+($id-18)*70}
        $sy=if($id -le 16){350+[math]::Floor(($id-1)/4)*80}elseif($id -eq 17){400}else{600}
        $events.Add(@{time=0;priority=$id;tag='PROOF_BOX_SEED';row=[ordered]@{id=$id;owner=$(if($id -eq 18){2}else{1});kind=$(if($id -eq 17){2}elseif($id -eq 2){1}else{0});hp=$(if($id -eq 19){0}else{100});x=$sx;y=$sy;digest=$seedDigest;population='diagnostic-over-cap'}})
    }
    $names='click,release-threshold,sticky-return,box-down-right,box-down-left,box-up-right,box-up-left,shift-latched-cap,late-shift-replaces,empty-clears,shift-empty,hud-start,hud-end-return-held,outside-pointer,real-resize,owner-change,epoch-change,phase-change,duplicate-press-release,group-1,group-2,group-3,group-4,group-5,group-6,group-7,group-8,group-9,type-visible,type-paired,context-move-route'.Split(',')
    $resultIds=@('[1]','[1]','[1]','[1,2,3,4,5,6,7,8,9,10,11,12]','[1,2,3,4,5,6,7,8,9,10,11,12]','[1,2,3,4,5,6,7,8,9,10,11,12]','[1,2,3,4,5,6,7,8,9,10,11,12]','[1,2,3,4,5,6,7,8,9,10,16,17]','[1,2,3,4,5,6,7,8,9,10,11,12]','[]','[17]','[17]','[17]','[17]','[17]','[]','[]','[17]','[1]','[17]','[17]','[17]','[17]','[17]','[17]','[17]','[17]','[17]','[17]','[17]','[1]')
    $deltas=@(1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,2,2,0,1,2,2,2,2,2,2,2,2,2,1,2,0)
    $allEdges=[Collections.Generic.List[object]]::new();$ip=0;$ir=0
    for($c=0;$c -lt 31;$c++){
        $keys=if($c -eq 7){'LeftShift1,LeftMouseButton1,LeftShift0,LeftMouseButton0'}elseif($c -eq 8){'LeftMouseButton1,LeftShift1,LeftMouseButton0,LeftShift0'}elseif($c -eq 10){'LeftShift1,LeftMouseButton1,LeftMouseButton0,LeftShift0'}elseif($c -eq 18){'LeftMouseButton1,LeftMouseButton1,LeftMouseButton0,LeftMouseButton0'}elseif($c -in 19..27){$k=@('One','Two','Three','Four','Five','Six','Seven','Eight','Nine')[$c-19];"LeftControl1,${k}1,${k}0,LeftControl0,LeftMouseButton1,LeftMouseButton0,${k}1,${k}0"}elseif($c -eq 28){'T1,T0'}elseif($c -eq 29){'T1,T0,T1,T0'}elseif($c -eq 30){'RightMouseButton1,RightMouseButton0'}else{'LeftMouseButton1,LeftMouseButton0'}
        $times=if($c -eq 7){@(5,10,25,80)}elseif($c -eq 8){@(10,25,80,95)}elseif($c -eq 10){@(5,10,80,95)}elseif($c -eq 18){@(10,60,80,92)}elseif($c -in 19..27){@(10,20,25,25,30,40,50,60)}elseif($c -eq 28){@(10,11)}elseif($c -eq 29){@(10,11,12,13)}else{@(10,80)}
        # Case10 Shift then LMB are at5/10; Case8 late Shift at25.
        if($c -eq 10){$times=@(5,10,80,95)}
        $index=0
        foreach($key in $keys.Split(',')){
            $name=$key.Substring(0,$key.Length-1);$down=$key.Substring($key.Length-1);if($name -ceq 'LeftMouseButton'){if($down -ceq '1'){$ip++}else{$ir++}}
            $frame=100+$c*100+$times[$index];$e=[ordered]@{case=$c;key=$name;pressed=$down;frame=$frame;processedFrame=$frame;device=0;user=0;handled=1;ready=1;focus=1;foreground=1;injectedPress=$ip;injectedRelease=$ir;inputSource='viewport-engine-injection';pointerSource='explicit-controls-cursor';physicalInput='false'}
            $allEdges.Add($e);$events.Add(@{time=$frame;priority=30+$index;tag='PROOF_BOX_EDGE';row=$e});$index++
        }
    }
    function Counts($Frame){
        $p=@($allEdges|Where-Object {[int]$_.frame -lt $Frame});$press=@($p|Where-Object {$_.key -ceq 'LeftMouseButton' -and $_.pressed -ceq '1'}).Count;$release=@($p|Where-Object {$_.key -ceq 'LeftMouseButton' -and $_.pressed -ceq '0'}).Count
        if($DuplicateCallbacks -eq 1){if(@($p|Where-Object {$_.case -eq 18 -and $_.key -ceq 'LeftMouseButton' -and $_.pressed -ceq '1'}).Count -eq 2){$press--};if(@($p|Where-Object {$_.case -eq 18 -and $_.key -ceq 'LeftMouseButton' -and $_.pressed -ceq '0'}).Count -eq 2){$release--}}
        return @($press,$release,@($p|Where-Object {$_.pressed -ceq '1' -and $_.key -cmatch '^(One|Two|Three|Four|Five|Six|Seven|Eight|Nine)$'}).Count,@($p|Where-Object {$_.pressed -ceq '1' -and $_.key -ceq 'T'}).Count,@($p|Where-Object {$_.pressed -ceq '1' -and $_.key -ceq 'RightMouseButton'}).Count)
    }
    for($c=0;$c -lt 31;$c++){
        $offset=100+$c*100;$baseCounts=Counts $offset
        $phases=if($c -in @(0,1)){'baseline,pressed,released'}elseif($c -in @(2,3)){'baseline,pressed,capture-request,capture-complete,sampled,before-release,released'}elseif($c -eq 14){'baseline,pressed,resized,before-release,released'}elseif($c -eq 18){'baseline,pressed,sampled,before-release,released,duplicate-released'}elseif($c -ge 28 -and $c -le 29){'baseline,type-first,key-regression'}elseif($c -ge 19){'baseline,key-regression'}else{'baseline,pressed,sampled,before-release,released'}
        $bx=760;$by=310;$ex=1050;$ey=630
        if($c -le 2 -or $c -ge 14){$bx=800;$by=350;$ex=804;$ey=350};if($c -eq 0){$ex=802};if($c -in @(4,6)){$bx=1050;$ex=760};if($c -in @(5,6)){$by=630;$ey=310};if($c -in @(9,10)){$bx=760;$by=650;$ex=780;$ey=670};if($c -eq 11){$bx=20;$by=20}
        foreach($phase in $phases.Split(',')){
            $t=switch($phase){baseline{0} pressed{20} 'capture-request'{30} 'capture-complete'{40} sampled{50} resized{50} 'before-release'{70} released{90} 'duplicate-released'{95} 'type-first'{11} 'key-regression'{90}}
            $frame=$offset+$t;$counts=Counts $frame;$isBase=$phase -ceq 'baseline';$isPress=$phase -ceq 'pressed';$isFinal=$phase -in @('released','duplicate-released','key-regression');$mutated=$phase -in @('sampled','resized')
            $x=$ex;$y=$ey;if($isBase -or $isPress -or $c -in 19..29){$x=$bx;$y=$by};if($c -eq 2 -and -not($isBase -or $isPress)){$x=if($phase -in @('before-release','released')){800}else{812};$y=350};if($c -eq 12 -and $mutated){$x=20;$y=20};if($c -eq 13 -and $mutated){$x=-1;$y=350};if($c -eq 30 -and $isFinal){$x=760;$y=650}
            $baseIds=if($c -eq 7){'[16,17]'}elseif($c -eq 30){'[1]'}else{'[17]'};$basePrimary=if($c -eq 7){16}elseif($c -eq 30){1}else{17}
            $ids=$baseIds;$primary=$basePrimary;$revision=1000+$c*10
            if($isFinal){$ids=$resultIds[$c];$primary=if($ids -ceq '[]'){-1}elseif($c -eq 7){16}elseif($c -in @(10,11,12,13,14,17) -or $c -in 19..29){17}else{1};$revision+=$deltas[$c]}
            elseif($c -in @(15,16) -and $phase -in @('sampled','before-release')){$ids='[]';$primary=-1;$revision+= $(if($mutated){1}else{2})}
            elseif($phase -ceq 'type-first'){$revision++}
            $armed=if($c -lt 19 -and -not($isBase -or $isFinal) -and $(if($isPress){$c -ne 11}else{$c -notin 11..17})){1}else{0}
            $scope=if(($phase -ceq 'type-first') -or ($c -eq 28 -and $isFinal)){'visible'}elseif($c -eq 29 -and $isFinal){'battlefield'}else{'none'}
            $capture=if($c -in @(2,3) -and -not($isBase -or $isPress)){Join-Path $images ('box-held-'+$c+'.png')}else{'none'}
            $r=[ordered]@{case=$c;name=$names[$c];phase=$phase;frame=$frame;processedFrame=$frame;owner=$(if($c -eq 15 -and $mutated){2}else{1});epoch=$(if($c -eq 16 -and $mutated){8}else{7});matchPhase=$(if($c -eq 17 -and $mutated){2}else{1});width=$(if($phase -ceq 'resized'){960}else{1280});height=$(if($phase -ceq 'resized'){540}else{720});x=$x;y=$y;ids=$ids;primary=$primary;revision=$revision;nativePress=$counts[0];nativeRelease=$counts[1];nativeGroup=$counts[2];nativeType=$counts[3];nativeRight=$counts[4];leftDown=$(if($c -lt 19 -and -not($isBase -or $isFinal)){1}else{0});shiftDown=$(if(($isPress -and $c -in @(7,10)) -or (-not($isBase -or $isPress) -and $c -in @(8,10))){1}else{0});typeDown=$(if($phase -ceq 'type-first'){1}else{0});armed=$armed;dragging=$(if($armed -and -not $isPress){1}else{0});focus=1;foreground=1;sequence=$counts[4];queue=0;digest=$(if($c -in @(16,17) -and $mutated){$changedDigest}else{$seedDigest});capture=$capture;typeScope=$scope;orderType=$(if($c -eq 30 -and $isFinal){'Move'}else{'none'});physicalInput='false';osFocusVerified='false'}
            $events.Add(@{time=$frame;priority=10;tag='PROOF_BOX_OBSERVE';row=$r})
            if($phase -ceq 'capture-request'){$events.Add(@{time=$frame+1;priority=10;tag='PROOF_BOX_HUD';row=[ordered]@{frame=$frame+1;left=$(if($c -eq 2){800}else{760});top=$(if($c -eq 2){350}else{310});right=$(if($c -eq 2){812}else{1050});bottom=$(if($c -eq 2){350}else{630});width=1280;height=720;screenshot=$capture;drawn=1;physicalInput='false'}})}
            if($isFinal){$events.Add(@{time=$frame;priority=20;tag='PROOF_BOX_RESULT';row=[ordered]@{case=$c;name=$names[$c];passed=1;expected=$ids;expectedPrimary=$primary;expectedRevisionDelta=$deltas[$c];nativePressDelta=($counts[0]-$baseCounts[0]);nativeReleaseDelta=($counts[1]-$baseCounts[1]);pointerSource='explicit-controls-cursor';population='diagnostic-over-cap';physicalInput='false'}})}
        }
    }
    $events.Add(@{time=9999;priority=0;tag='PROOF_BOX_FINISH';row=[ordered]@{passed=1;cases=31;elapsed=110;reason='bounded-diagnostic-cases-complete';unverified='real-pointer,physical-focus,capture,lost-release-occurrence,gameplay';population='diagnostic-over-cap'}})
    return (($events|Sort-Object time,priority|ForEach-Object {Line $_.tag $_.row}) -join "`n")
}
function Check([string]$Name,[scriptblock]$Body,[bool]$Reject=$false){
    if($watch.Elapsed.TotalSeconds -ge 90){throw 'Pure preflight90-second bound exceeded'}
    $errorText=$null;$accepted=$false
    try{& $Body|Out-Null;$accepted=$true}catch{$errorText=$_.Exception.Message}
    $passed=if($Reject){-not $accepted}else{$accepted}
    $checks.Add(@{name=$Name;passed=$passed;rejected= -not $accepted;error=$errorText})
    if(-not $passed){throw ('Preflight failed: '+$Name+' '+$errorText)}
}
$positive=Transcript 2;$normalized=Transcript 1
$positive|Set-Content -LiteralPath (Join-Path $OutputDirectory 'synthetic-positive.log') -Encoding utf8
Check 'positive-complete302records' {Check-Oracle $positive $images}
Check 'positive-normalized-duplicate-edges' {Check-Oracle $normalized $images}
$mutations=@(
    @('owner-stimulus-absent',15,'sampled','owner','1'),@('epoch-stimulus-absent',16,'sampled','epoch','7'),@('phase-stimulus-absent',17,'sampled','matchPhase','1'),
    @('resize-stimulus-absent',14,'resized','width','1280'),@('resize-restoration-absent',14,'before-release','height','540'),
    @('owner-restoration-absent',15,'before-release','owner','2'),@('epoch-restoration-absent',16,'before-release','epoch','8'),@('phase-restoration-absent',17,'before-release','matchPhase','2'),
    @('hud-stimulus-absent',12,'sampled','x','1050'),@('outside-stimulus-absent',13,'sampled','x','1'),@('hud-return-rearmed',12,'before-release','armed','1'),
    @('sticky-return-wrong',2,'before-release','x','812'),@('sticky-latch-lost',2,'before-release','dragging','0'),
    @('shift-latched-stimulus-absent',7,'pressed','shiftDown','0'),@('late-shift-stimulus-absent',8,'sampled','shiftDown','0'),@('early-shift-release-absent',7,'sampled','shiftDown','1'),
    @('nan-pointer',3,'sampled','x','NaN'),@('infinite-pointer',3,'sampled','y','Infinity'),@('wrong-final-width',0,'released','width','960'),
    @('capture-stale-frame',2,'capture-complete','processedFrame','1'),@('capture-counter-wrong',2,'capture-request','nativePress','9'),
    @('duplicate-counter-wrong',18,'duplicate-released','nativeGroup','1'),@('duplicate-context-wrong',18,'duplicate-released','epoch','8'),
    @('first-T-not-visible',29,'type-first','typeScope','none'),@('paired-T-not-battlefield',29,'key-regression','typeScope','visible'),@('Move-wrong-type',30,'key-regression','orderType','Attack'),
    @('held-key-wrong',3,'before-release','leftDown','0'),@('pressed-already-dragged',3,'pressed','dragging','1'),@('baseline-shift-wrong',0,'baseline','shiftDown','1'),
    @('intermediate-id-contradiction',3,'sampled','ids','[1]'),@('intermediate-revision-contradiction',3,'sampled','revision','999'),@('queue-contradiction',3,'capture-complete','queue','1')
)
foreach($m in $mutations){
    $pattern='(?m)^(PROOF_BOX_OBSERVE case='+$m[1]+' name=\S+ phase='+[regex]::Escape($m[2])+' [^\r\n]*)$'
    $bad=[regex]::Replace($positive,$pattern,{param($hit)[regex]::Replace($hit.Value,'(?<!\S)'+[regex]::Escape($m[3])+'=\S+',$m[3]+'='+$m[4])})
    if($bad -ceq $positive){throw ('Mutation did not apply '+$m[0])}
    Check $m[0] {Check-Oracle $bad $images} $true
}
Check 'unknown-record' {Check-Oracle ($positive+"`nPROOF_BOX_EXTRA case=1") $images} $true
Check 'missing-edge' {Check-Oracle ([regex]::Replace($positive,'(?m)^PROOF_BOX_EDGE case=0 [^\r\n]+\n','')) $images} $true
Check 'extra-observation' {Check-Oracle ($positive+"`n"+($positive.Split("`n")|Where-Object {$_ -like 'PROOF_BOX_OBSERVE case=0 *'}|Select-Object -First 1)) $images} $true
Add-Type -AssemblyName System.Drawing.Common
$valid=Join-Path $images 'valid.png';$bitmap=[Drawing.Bitmap]::new(1280,720);try{$bitmap.SetPixel(0,0,[Drawing.Color]::Red);$bitmap.Save($valid,[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
Check 'PNG-real-decode-positive' {Test-BoxInputPng $valid}
$truncated=Join-Path $images 'truncated45.png';$fake=[byte[]]::new(45);[byte[]]$header=0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A;$header.CopyTo($fake,0);[Text.Encoding]::ASCII.GetBytes('IHDR').CopyTo($fake,12);$fake[18]=5;$fake[22]=2;$fake[23]=208;[Text.Encoding]::ASCII.GetBytes('IEND').CopyTo($fake,37);$fake[41]=174;$fake[42]=66;$fake[43]=96;$fake[44]=130;[IO.File]::WriteAllBytes($truncated,$fake)
Check 'PNG45byte-nonimage-rejected' {Test-BoxInputPng $truncated} $true
$wrong=Join-Path $images 'wrong-dimensions.png';$bitmap=[Drawing.Bitmap]::new(1,1);try{$bitmap.Save($wrong,[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
Check 'PNG-wrong-dimensions-rejected' {Test-BoxInputPng $wrong} $true
$broken=Join-Path $images 'truncated-real.png';$pngBytes=[IO.File]::ReadAllBytes($valid);[IO.File]::WriteAllBytes($broken,$pngBytes[0..44]);Check 'PNG-real-truncation-rejected' {Test-BoxInputPng $broken} $true
$result=[ordered]@{kind='PowerShell-only SYNTHETIC evidence parser preflight; no engine acceptance';passed=$true;checks=$checks;count=$checks.Count;elapsedSeconds=$watch.Elapsed.TotalSeconds;helperSHA256=(Get-FileHash (Join-Path $PSScriptRoot 'BoxInputEvidence.ps1')).Hash;preflightSHA256=(Get-FileHash $PSCommandPath).Hash;syntheticLogSHA256=(Get-FileHash (Join-Path $OutputDirectory 'synthetic-positive.log')).Hash;engineStarted=$false;compilerStarted=$false;processOrCpuQueries=$false}
$result|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $OutputDirectory 'result.json') -Encoding utf8
[pscustomobject]$result|Select-Object passed,count,elapsedSeconds,helperSHA256,preflightSHA256|ConvertTo-Json
