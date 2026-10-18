param(
    [Parameter(Mandatory)][string]$ExpectedHead,
    [Parameter(Mandatory)][string]$ExpectedSource,
    [Parameter(Mandatory)][string]$ExpectedDll,
    [string]$EngineRoot='D:\Epic Games\UE_5.7'
)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$project=Join-Path $repo 'unreal/RedalertProof.uproject'
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
$dll=Join-Path $repo 'unreal/Binaries/Win64/UnrealEditor-RedalertProof.dll'
if([IO.Path]::GetPathRoot($repo) -ne 'D:\'){throw 'D: isolation required'}
if((& git -C $repo rev-parse HEAD).Trim() -cne $ExpectedHead -or (& git -C $repo rev-parse 'HEAD:unreal/Source').Trim() -cne $ExpectedSource -or [bool](& git -C $repo status --porcelain)){throw 'Frozen clean source required'}
if((Get-FileHash -LiteralPath $dll).Hash -cne $ExpectedDll){throw 'Frozen DLL required'}
$run=Join-Path $repo ('generated/box-input-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
$cache=Join-Path $run 'cache';$temp=Join-Path $run 'temp';$images=Join-Path $run 'images'
New-Item -ItemType Directory -Force -Path $run,$cache,$temp,$images|Out-Null
$log=Join-Path $run 'fixture.log'
$mask=[int64]68;$invoker=[Diagnostics.Process]::GetCurrentProcess();$priorAffinity=$invoker.ProcessorAffinity
if(($priorAffinity.ToInt64() -band $mask) -ne $mask){throw 'CPUs2/6 unavailable'}
function Get-Competing {
    @(Get-CimInstance Win32_Process -ErrorAction Stop|Where-Object {$_.Name -match '^(UnrealEditor(?:-Cmd)?|UnrealBuildTool|UnrealHeaderTool|ShaderCompileWorker|cl|link|clang(?:\+\+)?|lld-link|MSBuild|BuildGraph|AutomationTool|UbaAgent|UbaHost|UbaSessionServer)\.exe$' -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|UnrealHeaderTool|AutomationTool')}|Select-Object ProcessId,Name,CreationDate)
}
$counterPaths=@('\Processor(_Total)\% Processor Time','\Processor(2)\% Processor Time','\Processor(6)\% Processor Time')
$samples=@(Get-Counter $counterPaths -SampleInterval 1 -MaxSamples 3 -ErrorAction Stop|ForEach-Object {@{utc=$_.Timestamp.ToUniversalTime().ToString('o');counters=@($_.CounterSamples|ForEach-Object {@{path=$_.Path;value=$_.CookedValue;status=$_.Status}})}})
$samples|ConvertTo-Json -Depth 7|Set-Content (Join-Path $run 'admission-samples.json') -Encoding utf8
if($samples.Count -ne 3){throw 'Three fresh samples required'}
foreach($sample in $samples){foreach($path in $counterPaths){$matches=@($sample.counters|Where-Object {$_.path.EndsWith($path,[StringComparison]::OrdinalIgnoreCase)});if($matches.Count -ne 1 -or $matches[0].status -ne 0 -or -not [double]::IsFinite($matches[0].value) -or $matches[0].value -lt 0 -or $matches[0].value -gt 80){throw 'CPU admission blocked'}}}
if(@(Get-Competing).Count){throw 'Competing engine/build work blocks allocation'}
$result=[ordered]@{passed=$false;head=$ExpectedHead;sourceTree=$ExpectedSource;dllSHA256=$ExpectedDll;startedUtc=[DateTime]::UtcNow.ToString('o');finishedUtc=$null;runDirectory=$run;error=$null;scenarioDeadlineSeconds=220;totalDeadlineSeconds=240;fps=30;affinityMask=$mask;priority='Normal';physicalInputVerified=$false;realPointerRouteVerified=$false;osFocusCaptureVerified=$false;lostReleaseOccurrenceVerified=$false;population='diagnostic-over-cap';gameplayProof=$false;pollingIsNotContinuousConfinement=$true;policyRepairs=@();cleanupErrors=@();screenshots=@();logSHA256=$null;oracle=$null;owned=@();ownedExits=@();ownedExitConfirmed=$false;remainingCompetingWork=@();storageChecks=0;peakCacheBytes=[int64]0;peakTempBytes=[int64]0}
$prior=@{};$owned=@{};$process=$null;$watch=[Diagnostics.Stopwatch]::StartNew()
function Read-Rows([string]$Text,[string]$Tag) {
    $schema=switch($Tag){
        'PROOF_BOX_SEED' {'id,owner,kind,hp,x,y,digest,population'}
        'PROOF_BOX_EDGE' {'case,key,pressed,frame,processedFrame,device,user,handled,ready,focus,foreground,injectedPress,injectedRelease,inputSource,pointerSource,physicalInput'}
        'PROOF_BOX_OBSERVE' {'case,name,phase,frame,processedFrame,owner,epoch,matchPhase,width,height,x,y,ids,primary,revision,nativePress,nativeRelease,nativeGroup,nativeType,nativeRight,leftDown,shiftDown,armed,dragging,focus,foreground,sequence,queue,digest,capture,physicalInput,osFocusVerified'}
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
function Check-Oracle([string]$Text) {
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
        $phases=if($case -ge 19){'baseline,key-regression'}elseif($case -in @(0,1)){'baseline,pressed,released'}elseif($case -in @(2,3)){'baseline,pressed,capture-request,capture-complete,sampled,released'}elseif($case -eq 14){'baseline,pressed,resized,released'}elseif($case -eq 18){'baseline,pressed,sampled,released,duplicate-released'}else{'baseline,pressed,sampled,released'}
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
    return @{passed=$true;fixedCases=31;independentExpectedArrays=$true;exactIntegerRevisions=$true;nativeEdgeCountersVerified=$true;duplicateEngineNormalizationSeparatelyBounded=$true;realPointerRouteVerified=$false;lostReleaseOccurrenceVerified=$false}
}
try {
    foreach($name in @('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')){$prior[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$cache,'Process');[Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process')
    [Environment]::SetEnvironmentVariable('TEMP',$temp,'Process');[Environment]::SetEnvironmentVariable('TMP',$temp,'Process')
    $invoker.ProcessorAffinity=[IntPtr]$mask
    if(@(Get-Competing).Count){throw 'Competing work appeared before launch'}
    $arguments=@(('"'+$project+'"'),'/Game/CommandProofGenerated/Maps/CommandProof?game=/Script/RedalertProof.ProofBoxInputGameMode','-game','-windowed','-ResX=1280','-ResY=720','-unattended','-nosound','-nosplash','-NoSourceControl','-NoP4','-CoreLimit=2','-ddc=BlockoutDDC','-ProofBoxInputFixture',('-ProofBoxInputOutput="'+$images+'"'),'-ini:GameUserSettings:[/Script/Engine.GameUserSettings]:FrameRateLimit=30','-ini:Engine:[DevOptions.Shaders]:WorkerProcessPriority=0,[ConsoleVariables]:t.MaxFPS=30,[ShaderCompiler]:r.Shaders.Optimize=1',[string]('-abslog="'+$log+'"'))
    $process=Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory (Join-Path $repo 'unreal') -WindowStyle Hidden -PassThru
    $owned[$process.Id]=@{id=$process.Id;start=$process.StartTime;path=$process.Path;depth=0;process=$process}
    while($true){
        $all=@(Get-CimInstance Win32_Process -ErrorAction Stop)
        for($depth=0;$depth -lt 8;$depth++){foreach($child in $all){
            if($owned.ContainsKey([int]$child.ProcessId) -or -not $owned.ContainsKey([int]$child.ParentProcessId)){continue}
            $parent=$owned[[int]$child.ParentProcessId];$liveParent=Get-Process -Id $parent.id -ErrorAction SilentlyContinue
            if(-not $liveParent -or $liveParent.HasExited -or $liveParent.StartTime -ne $parent.start -or $liveParent.Path -cne $parent.path -or $child.CreationDate -lt $parent.start){continue}
            $live=Get-Process -Id $child.ProcessId -ErrorAction SilentlyContinue
            if(-not $live -or $live.HasExited -or -not $live.Path -or [math]::Abs(($live.StartTime-$child.CreationDate).TotalSeconds) -gt 1){continue}
            $owned[[int]$child.ProcessId]=@{id=[int]$child.ProcessId;start=$live.StartTime;path=$live.Path;depth=([int]$parent.depth+1);parent=$parent.id;process=$live}
        }}
        foreach($record in @($owned.Values)){
            $live=Get-Process -Id $record.id -ErrorAction SilentlyContinue;if(-not $live -or $live.HasExited){continue}
            if($live.StartTime -ne $record.start -or $live.Path -cne $record.path){throw 'Owned process identity changed'}
            if($live.ProcessorAffinity.ToInt64() -ne $mask -or [string]$live.PriorityClass -ne 'Normal'){
                $result.policyRepairs+=@{name=$live.ProcessName;beforeMask=$live.ProcessorAffinity.ToInt64();beforePriority=[string]$live.PriorityClass}
                $live.ProcessorAffinity=[IntPtr]$mask;$live.PriorityClass='Normal'
            }
        }
        foreach($pair in @(@{path=$cache;limit=2GB;field='peakCacheBytes'},@{path=$temp;limit=1GB;field='peakTempBytes'})){
            $bytes=[int64]0;foreach($file in @(Get-ChildItem -LiteralPath $pair.path -File -Force -Recurse -ErrorAction Stop)){$bytes+=$file.Length}
            $result[$pair.field]=[math]::Max([int64]$result[$pair.field],$bytes);if($bytes -gt $pair.limit){throw 'Storage guard exceeded'}
        }
        $result.storageChecks++;$process.Refresh()
        if($process.HasExited){if($watch.Elapsed.TotalSeconds -ge 240){throw 'Completion first observed outside240-second bound'};break}
        if($watch.Elapsed.TotalSeconds -ge 240){throw 'Original240-second total deadline exceeded'}
        Start-Sleep -Milliseconds 500
    }
    if($process.ExitCode -ne 0){throw ('Fixture exit '+$process.ExitCode)}
    $text=Get-Content -LiteralPath $log -Raw;$result.oracle=Check-Oracle $text
    foreach($case in @(2,3)){
        $png=Join-Path $images ('box-held-'+$case+'.png');$file=Get-Item -LiteralPath $png
        if($file.Length -lt 45 -or $file.Length -gt 10MB -or $file.LastWriteTimeUtc -lt [DateTime]::Parse($result.startedUtc).ToUniversalTime()){throw 'Missing/stale/oversized outline image'}
        $bytes=[IO.File]::ReadAllBytes($png)
        $width=([uint32]$bytes[16]*16777216)+([uint32]$bytes[17]*65536)+([uint32]$bytes[18]*256)+[uint32]$bytes[19]
        $height=([uint32]$bytes[20]*16777216)+([uint32]$bytes[21]*65536)+([uint32]$bytes[22]*256)+[uint32]$bytes[23]
        if([Convert]::ToHexString($bytes[0..7]) -cne '89504E470D0A1A0A' -or [Convert]::ToHexString($bytes[12..15]) -cne '49484452' -or [Convert]::ToHexString($bytes[($bytes.Length-8)..($bytes.Length-1)]) -cne '49454E44AE426082' -or $width -ne 1280 -or $height -ne 720){throw 'Invalid PNG format/dimensions/completion'}
        $result.screenshots+=@{case=$case;path=$png;bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $png).Hash}
    }
    $result.passed=$true
} catch {$result.error=$_.Exception.Message}
finally {
    foreach($record in @($owned.Values|Sort-Object depth -Descending)){
        try{
            $pinned=$record.process;$pinned.Refresh()
            if(-not $pinned.HasExited){
                $live=Get-Process -Id $record.id -ErrorAction Stop
                if($live.StartTime -ne $record.start -or $live.Path -cne $record.path){throw 'Cleanup identity changed'}
                $pinned.Kill()
            }
            if(-not $pinned.WaitForExit(5000)){throw 'Original owned process exit not signaled'}
            $result.ownedExits+=@{id=$record.id;start=$record.start;path=$record.path;exitUtc=$pinned.ExitTime.ToUniversalTime().ToString('o');exitCode=$pinned.ExitCode;originalProcessExitSignaled=$true}
        }catch{$result.cleanupErrors+=@($_.Exception.Message);$result.passed=$false}
    }
    $result.owned=@(foreach($record in $owned.Values){@{id=$record.id;start=$record.start;path=$record.path;depth=$record.depth;parent=$record.parent}})
    $result.ownedExitConfirmed=($result.owned.Count -gt 0 -and $result.owned.Count -eq $result.ownedExits.Count -and $result.cleanupErrors.Count -eq 0)
    try{$result.remainingCompetingWork=@(Get-Competing)}catch{$result.cleanupErrors+=@($_.Exception.Message);$result.passed=$false}
    $invoker.ProcessorAffinity=$priorAffinity
    foreach($name in $prior.Keys){[Environment]::SetEnvironmentVariable($name,$prior[$name],'Process')}
    if(Test-Path -LiteralPath $log){$result.logSHA256=(Get-FileHash -LiteralPath $log).Hash}
    $result.finishedUtc=[DateTime]::UtcNow.ToString('o');$result.elapsedSeconds=$watch.Elapsed.TotalSeconds
    if($result.elapsedSeconds -ge 240){$result.passed=$false;if(-not $result.error){$result.error='Acceptance/cleanup exceeded original240-second total bound'}}
    $result|ConvertTo-Json -Depth 12|Set-Content (Join-Path $run 'result.json') -Encoding utf8
}
if(-not $result.passed){throw ($result.error+' See '+$run+'\result.json')}
Write-Output (Join-Path $run 'result.json')
