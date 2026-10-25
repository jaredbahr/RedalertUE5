Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:ProbeRoot = 'D:/UserTemp/PR72-Admission-CPU-Attribution-20261025'
$script:ProbeSource = "$script:ProbeRoot/source"
$script:ProbeHost = 'C:/Program Files/PowerShell/7/pwsh.exe'
function Get-ProbeHash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Read-ProbeJson([string]$Path,[int]$MaxBytes=262144) {
    if($MaxBytes -le 0){throw 'Invalid JSON byte cap'}
    $before=Get-Item -LiteralPath $Path
    if($before.Length -le 0 -or $before.Length -gt $MaxBytes){throw 'JSON empty or exceeds byte cap'}
    $length=$before.Length; $writeTime=$before.LastWriteTimeUtc
    $stream=[IO.FileStream]::new($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {
        if($stream.Length -ne $length){throw 'JSON changed before read'}
        $bytes=[byte[]]::new([int]$length); $offset=0
        while($offset -lt $length){$n=$stream.Read($bytes,$offset,[int]($length-$offset));if($n -le 0){throw 'Truncated JSON'};$offset+=$n}
        if($stream.ReadByte() -ne -1 -or $stream.Length -ne $length){throw 'JSON has changed or trailing bytes'}
        $after=Get-Item -LiteralPath $Path
        if($after.Length -ne $length -or $after.LastWriteTimeUtc -ne $writeTime){throw 'JSON changed during read'}
    } finally {$stream.Dispose()}
    $text=[Text.UTF8Encoding]::new($false,$true).GetString($bytes)
    $options=[Text.Json.JsonDocumentOptions]::new(); $options.MaxDepth=16
    $doc=[Text.Json.JsonDocument]::Parse($text,$options)
    function Test-Keys($element){
        if($element.ValueKind -eq [Text.Json.JsonValueKind]::Object){
            $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
            foreach($p in $element.EnumerateObject()){if(!$seen.Add($p.Name)){throw 'Duplicate JSON property'}; Test-Keys $p.Value}
        } elseif($element.ValueKind -eq [Text.Json.JsonValueKind]::Array){foreach($v in $element.EnumerateArray()){Test-Keys $v}}
    }
    try { Test-Keys $doc.RootElement; return ConvertFrom-Json -InputObject $text -AsHashtable -Depth 16 -DateKind String } finally {$doc.Dispose()}
}
function Assert-ProbePin([string]$Path,[string]$Sha256){if($Sha256 -cnotmatch '^[0-9a-f]{64}$' -or (Get-ProbeHash $Path) -cne $Sha256){throw "Hash mismatch: $Path"}}
function Assert-ProbeBool($Value,[bool]$Expected,[string]$Name){if($Value -isnot [bool] -or $Value -ne $Expected){throw "Invalid boolean: $Name"}}
function Assert-ProbeFields($Object,[string[]]$Names){
    if($null -eq $Object){throw 'Missing object'}
    foreach($name in $Names){$present=if($Object -is [Collections.IDictionary]){$Object.Contains($name)}else{$null -ne $Object.PSObject.Properties[$name]}; if(!$present){throw "Missing field: $name"}}
}
function Assert-ProbeUtc($Value){
    if($Value -isnot [string]){throw 'UTC must be a string'}
    $parsed=[DateTimeOffset]::MinValue
    if($Value -cnotmatch '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{7}(Z|\+00:00)$' -or ![DateTimeOffset]::TryParse($Value,[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::None,[ref]$parsed) -or $parsed.Offset -ne [TimeSpan]::Zero){throw 'Invalid UTC timestamp'}
}
function Assert-ProbeNumber($Value,[string]$Name,[switch]$Integer,[decimal]$Min=0,[decimal]$Max=[decimal]::MaxValue){
    $numeric=$Value -is [byte] -or $Value -is [sbyte] -or $Value -is [int16] -or $Value -is [uint16] -or $Value -is [int32] -or $Value -is [uint32] -or $Value -is [int64] -or $Value -is [uint64] -or $Value -is [single] -or $Value -is [double] -or $Value -is [decimal]
    if(!$numeric -or ![double]::IsFinite([double]$Value)){throw "Invalid number: $Name"}
    if($Integer -and $Value -isnot [byte] -and $Value -isnot [sbyte] -and $Value -isnot [int16] -and $Value -isnot [uint16] -and $Value -isnot [int32] -and $Value -isnot [uint32] -and $Value -isnot [int64] -and $Value -isnot [uint64]){throw "Noninteger counter: $Name"}
    if([decimal]$Value -lt $Min -or [decimal]$Value -gt $Max){throw "Number outside bounds: $Name"}
}
function Assert-ProbeReview($Review,[string]$Scope){
    Assert-ProbeFields $Review @('scope','approved','reviewType','reviewer')
    Assert-ProbeBool $Review.approved $true 'approved'
    if($Review.scope -isnot [string] -or $Review.scope -cne $Scope -or $Review.reviewer -isnot [string]){throw 'Wrong review scope or reviewer type'}
    if($Review.approved -cne $true -or $Review.reviewType -cne 'independent' -or [string]::IsNullOrWhiteSpace($Review.reviewer)){throw 'Independent approval required'}
}
function ConvertTo-ProbeCanonical($Value){
    if($null -eq $Value){return 'null'}
    if($Value -is [Collections.IDictionary]){
        $parts=foreach($key in @($Value.Keys | Sort-Object -CaseSensitive)){
            ($key | ConvertTo-Json -Compress)+':'+(ConvertTo-ProbeCanonical $Value[$key])
        }
        return '{'+($parts -join ',')+'}'
    }
    if($Value -is [Collections.IEnumerable] -and $Value -isnot [string]){
        $parts=foreach($item in $Value){ConvertTo-ProbeCanonical $item}
        return '['+($parts -join ',')+']'
    }
    return ConvertTo-Json -InputObject $Value -Compress -Depth 16
}
function Assert-ProbeSource([string]$ReviewSHA256,[switch]$OwnedPayload,[switch]$DiagnosticPreparation){
    $bindingPath="$script:ProbeRoot/PROBE_SOURCE_BINDINGS.json"; $reviewPath="$script:ProbeRoot/SOURCE_REVIEW.json"
    Assert-ProbePin $reviewPath $ReviewSHA256
    $b=Read-ProbeJson $bindingPath; $r=Read-ProbeJson $reviewPath
    Assert-ProbeReview $r 'PR72-admission-cpu-source-review'
    if($DiagnosticPreparation){
        Assert-ProbeFields $r @('diagnosticPreparationOnly','freshHostInspectionRequired')
        Assert-ProbeBool $r.diagnosticPreparationOnly $true 'diagnosticPreparationOnly'
        Assert-ProbeBool $r.freshHostInspectionRequired $true 'freshHostInspectionRequired'
    }
    Assert-ProbeBool $r.sourceReviewApproved $true 'sourceReviewApproved'; Assert-ProbeBool $r.runtimeAcceptance $false 'runtimeAcceptance'; Assert-ProbeBool $r.sourceOnly $true 'sourceOnly'; Assert-ProbeBool $b.runtimeApproved $false 'runtimeApproved'
    if($r.sourceReviewApproved -cne $true -or $r.runtimeAcceptance -cne $false -or $r.sourceOnly -cne $true -or $r.sourceBindingsSha256 -cne (Get-ProbeHash $bindingPath)){throw 'Invalid source approval'}
    if($b.scope -cne 'PR72-admission-cpu-source' -or $b.runtimeApproved -cne $false -or $b.parentGameSourceHead -cne '02e08abd10ddc062a0b81c6eb18eb9d939232f3d' -or $b.gameCppTree -cne 'c71f8618bed849de1e11add1d15a6b0dc54aa6b1'){throw 'Wrong source scope'}
    if($b.gameSourceRoot -cne 'D:/UserTemp/PR72-Engine-Preparation-02e08abd-20261024/candidate'){throw 'Wrong game source root'}
    if($r.diagnosticSourceHead -cne $b.diagnosticSourceHead -or $r.parentGameSourceHead -cne $b.parentGameSourceHead){throw 'Review source mismatch'}
    # Native Git checks run only in the already-owned payload. The controller
    # prefix performs local file/hash/metadata checks without launching children.
    if($OwnedPayload){
        $head=(& git -C $script:ProbeSource rev-parse HEAD); if($LASTEXITCODE -ne 0 -or $head -cne $b.diagnosticSourceHead){throw 'Source HEAD mismatch'}
        $dirty=(& git -C $script:ProbeSource status --porcelain --untracked-files=all); if($LASTEXITCODE -ne 0 -or $dirty){throw 'Source must be clean'}
    }
    $names=@('AdmissionCpuSupervisor.cs','Probe.Common.ps1','InvokeProbe.ps1','ObserveAdmission.ps1','CompileObserver.ps1','README.md')
    if(@($b.files).Count -ne 6){throw 'Exactly six source bindings required'}
    foreach($name in $names){$f=@($b.files | Where-Object relative -CEQ "tools/PR72AdmissionCpu/$name"); if($f.Count -ne 1){throw 'Source leaf missing or duplicated'}; Assert-ProbeNumber $f[0].bytes 'source bytes' -Integer; $p="$script:ProbeSource/$($f[0].relative)"; Assert-ProbePin $p $f[0].sha256; if((Get-Item -LiteralPath $p).Length -ne $f[0].bytes){throw 'Source byte mismatch'}}
    $pins=@(
        @('originalOwner','D:/RedalertUE5-Evidence/PR71-Original-Acceptance-22575d1-20261021/engine-preparation/AcceptanceStageSupervisor.cs','0ffb82e568a1ae7bce90e751621a2f994fa92a6e7cd13442f4d65aaada1092c6'),
        @('sampler','D:/RedalertUE5-Evidence/PR71-AttackMove-Timing-22575d1-20261020/check-admission.ps1','f891ebaf39f09a738578ce0da9d94fa47352b27e84d8ce0fb56d447e49c5c353'),
        @('host',$script:ProbeHost,'fa2c8bb1e35cdd62ad15cd06a1d8f2c5961e9725a9bfa1e7cfd2c322f49e9619'),
        @('gameBinding','D:/UserTemp/PR72-Engine-Preparation-02e08abd-20261024/review-package/BINDINGS.json','6f63fbda96a1e38f60cc5fcb9b511bd1100d28ce96537e1d69b53cb30bb68f3d'),
        @('gameReview','D:/RedalertUE5-Evidence/PR72-Engine-Preparation-02e08abd-20261024-ENGINE-BINDING-REVIEW.json','e16d50d05be9be6df9c3f432f4ccaf8b429608d50a3e6063cc18a66968485cdb'))
    foreach($pin in $pins){if($b[$pin[0]].path -cne $pin[1] -or $b[$pin[0]].sha256 -cne $pin[2]){throw 'Immutable pin changed'}; Assert-ProbePin $pin[1] $pin[2]}
    if([Environment]::ProcessPath.Replace('\','/') -ine $script:ProbeHost){throw 'Pinned pwsh required'}
    if($b.observer.source -cne 'AdmissionCpuSupervisor.cs' -or $b.observer.className -cne 'RA2AdmissionCpuSupervisor'){throw 'Wrong observer'}
    Assert-ProbePin "$PSScriptRoot/AdmissionCpuSupervisor.cs" $b.observer.sha256
    foreach($key in @('nativeSeconds','cleanupSeconds','affinityMask','maxFrames','minPeriodMs','maxObservationBytes','markerCount')){Assert-ProbeNumber $b.bounds[$key] $key -Integer}
    if($b.bounds.nativeSeconds -ne 20 -or $b.bounds.cleanupSeconds -ne 5 -or $b.bounds.affinityMask -ne 68 -or $b.bounds.priority -cne 'Normal' -or $b.bounds.maxFrames -ne 81 -or $b.bounds.minPeriodMs -ne 250 -or $b.bounds.maxObservationBytes -ne 196608 -or $b.bounds.markerCount -ne 4){throw 'Bounds changed'}
    $frozenPath='D:/RedalertUE5-Evidence/PR71-Original-Acceptance-22575d1-20261021/retained-admission/ACCEPTANCE_BINDINGS.json'
    Assert-ProbePin $frozenPath 'b12e88cf741181a1b6ebb93e0210eca6983d0366adb902194764369b95f249f2'
    $frozen=Read-ProbeJson $frozenPath
    if(@($frozen.files).Count -ne 35){throw 'Frozen pin count changed'}
    foreach($f in $frozen.files){Assert-ProbePin $f.path $f.sha256; if((Get-Item -LiteralPath $f.path).Length -ne $f.bytes){throw 'Frozen size changed'}}
    $p=$b.prepareOwner
    if($p.receiptPath -cne 'D:/UserTemp/PR72-Engine-Preparation-02e08abd-20261024/capability/PREPARED_WATCHDOG.json' -or $p.receiptSha256 -cne '627eb9815d436cfcbcc3933dd6ee24c5833bb9ade9ad11db9c9b3565f0581bf4' -or $p.assemblyPath -cne 'D:/UserTemp/PR72-Engine-Preparation-02e08abd-20261024/capability/CommandCardStageSupervisor.dll' -or $p.assemblySha256 -cne '121c9d2f8a1f8b858f4cd8995666a42e8a9ed3a72e10153f5187ebcdda19dad7' -or $p.moduleVersionId -cne 'e4902f3a-5e9a-4d56-84bd-7b34afa0a02e' -or $p.className -cne 'RA2CommandCardStageSupervisor' -or $p.sourceSha256 -cne 'c92ef422b3519b122a405bb5edbc95184c53ce42da768115cf22d64ccdd466ee'){throw 'Preparation owner pin mismatch'}
    Assert-ProbePin $p.receiptPath $p.receiptSha256; Assert-ProbePin $p.assemblyPath $p.assemblySha256
    return $b
}
function Assert-ProbeNative($o,[switch]$Historical){
    Assert-ProbeFields $o @('passed','resumed','jobAssigned','exitCode','timedOut','reservationLost','terminationRequested','cleanupSucceeded','outputComplete','activeOwnedProcessesAfterCleanup','error','cleanupError','boundSeconds','pid','creationFileTime','frequency','deadlineStartQpc','observedExitQpc','elapsedSeconds','startedUtc','finishedUtc','capacityExpiresUtc','stdoutBytes','stderrBytes')
    foreach($key in @('passed','resumed','jobAssigned','cleanupSucceeded','outputComplete')){Assert-ProbeBool $o.$key $true $key}
    foreach($key in @('timedOut','reservationLost','terminationRequested')){Assert-ProbeBool $o.$key $false $key}
    foreach($key in @('exitCode','activeOwnedProcessesAfterCleanup','boundSeconds','pid','creationFileTime','frequency','deadlineStartQpc','observedExitQpc','stdoutBytes','stderrBytes')){Assert-ProbeNumber $o.$key $key -Integer}
    Assert-ProbeNumber $o.elapsedSeconds 'elapsedSeconds' -Max 20
    if($null -ne $o.error -or $null -ne $o.cleanupError -or $o.stdoutBytes -gt 1048576 -or $o.stderrBytes -gt 1048576 -or $o.frequency -ne [Diagnostics.Stopwatch]::Frequency -or $o.observedExitQpc -lt $o.deadlineStartQpc){throw 'Native output or interval invalid'}
    Assert-ProbeUtc $o.startedUtc; Assert-ProbeUtc $o.finishedUtc
    if(!$Historical){Assert-ProbeBool $o.leaseCancelled $false 'leaseCancelled'; Assert-ProbeNumber $o.cleanupElapsedSeconds 'cleanupElapsedSeconds' -Max 5; Assert-ProbeNumber $o.completedQpc 'completedQpc' -Integer -Min $o.observedExitQpc}
    if($o.passed -cne $true -or $o.resumed -cne $true -or $o.jobAssigned -cne $true -or $o.exitCode -ne 0 -or $o.timedOut -cne $false -or $o.reservationLost -cne $false -or $o.terminationRequested -cne $false -or $o.cleanupSucceeded -cne $true -or $o.outputComplete -cne $true -or $o.activeOwnedProcessesAfterCleanup -ne 0 -or $o.error -or $o.cleanupError -or $o.boundSeconds -ne 20 -or $o.pid -le 0 -or $o.creationFileTime -le 0){throw 'Native ownership result failed'}
    if(!$Historical -and ($o.leaseCancelled -cne $false -or $o.cleanupElapsedSeconds -gt 5)){throw 'Native lease or cleanup failed'}
}
function Assert-ProbeClocks($Outcome,[long]$Origin){
    $now=[Diagnostics.Stopwatch]::GetTimestamp(); $f=[double][Diagnostics.Stopwatch]::Frequency
    $total=($now-$Origin)/$f; $post=($now-$Outcome.observedExitQpc)/$f
    if(![double]::IsFinite($total) -or ![double]::IsFinite($post) -or $total -ge 20 -or $post -gt 5 -or $post -lt 0){throw 'Through-return or post-exit budget failed'}
}
function Assert-ProbeAdmission($a,[switch]$RequireEligible){
    Assert-ProbeFields $a @('head','tree','sourceTree','clean','eligible','samples','utc','cpuThresholdPercent','mask','priority','conflictsBefore','conflictsAfter','gamesBefore','gamesAfter','freeMemoryKiB','freeDBytes')
    Assert-ProbeBool $a.clean $true 'clean'; if($a.eligible -isnot [bool]){throw 'Eligible must be boolean'}
    Assert-ProbeUtc $a.utc
    if($a.samples -isnot [array]){throw 'Samples must be an array'}
    foreach($key in @('cpuThresholdPercent','conflictsBefore','conflictsAfter','gamesBefore','gamesAfter','freeMemoryKiB','freeDBytes')){Assert-ProbeNumber $a[$key] $key -Integer}
    if($a.head -cne '22575d19c0f50805aeacf1d0b1d1b5f2d861d65d' -or $a.tree -cne '479733a5c279815169df5c51fd0df6fbdd47d7e9' -or $a.sourceTree -cne '673bb524e9d63f1e783580a218f55e69da28b856' -or $a.clean -cne $true -or $a.cpuThresholdPercent -ne 80 -or $a.mask -cne '0x44' -or $a.priority -cne 'Normal' -or @($a.samples).Count -ne 3){throw 'Original admission identity/row contract mismatch'}
    $cpuGood=$true
    foreach($row in $a.samples){Assert-ProbeFields $row @('utc','aggregate','cpu2','cpu6'); Assert-ProbeUtc $row.utc; foreach($key in @('aggregate','cpu2','cpu6')){Assert-ProbeNumber $row[$key] $key -Max 100; $v=[double]$row[$key]; if($v -ge 80){$cpuGood=$false}}}
    $expected=$cpuGood -and $a.conflictsBefore -eq 0 -and $a.conflictsAfter -eq 0 -and $a.gamesBefore -eq 0 -and $a.gamesAfter -eq 0 -and $a.freeMemoryKiB -ge 8388608 -and $a.freeDBytes -ge 21474836480
    if($a.eligible -isnot [bool] -or $a.eligible -ne $expected -or ($RequireEligible -and !$expected)){throw 'Original admission eligibility mismatch'}
}
function Assert-ProbeMarkers([string]$Directory,$Outcome){
    $status=Read-ProbeJson "$Directory/marker-status.json"
    Assert-ProbeFields $status @('complete','error','count'); Assert-ProbeBool $status.complete $true 'marker complete'; Assert-ProbeNumber $status.count 'marker count' -Integer
    if($null -ne $status.error){throw 'Marker error present'}
    if($status.complete -cne $true -or $status.error -or $status.count -ne 4){throw 'Phase marker status incomplete'}
    $names=@('before_preflight','after_preflight','before_sampler','after_sampler'); $previous=$Outcome.deadlineStartQpc; $previousCpu=[decimal]0
    for($i=0;$i -lt 4;$i++){
        $m=Read-ProbeJson "$Directory/phase-$i.json"
        Assert-ProbeFields $m @('phase','index','pid','creationFileTime','utc','qpcStart','qpcEnd','frequency','rootTotalCpu100ns')
        foreach($key in @('index','pid','creationFileTime','qpcStart','qpcEnd','frequency','rootTotalCpu100ns')){Assert-ProbeNumber $m[$key] $key -Integer}
        Assert-ProbeUtc $m.utc
        $finalRoot=[decimal]$Outcome.observation.finalRootUser100ns+[decimal]$Outcome.observation.finalRootKernel100ns
        if([decimal]$m.rootTotalCpu100ns -lt $previousCpu -or [decimal]$m.rootTotalCpu100ns -gt $finalRoot){throw 'Marker root CPU not monotonic or exceeds final retained root counter'}
        if($m.phase -cne $names[$i] -or $m.index -ne $i -or $m.pid -ne $Outcome.pid -or $m.creationFileTime -ne $Outcome.creationFileTime -or $m.frequency -ne $Outcome.frequency -or $m.qpcStart -lt $previous -or $m.qpcEnd -lt $m.qpcStart -or $m.qpcEnd -gt $Outcome.observedExitQpc -or $m.rootTotalCpu100ns -lt 0){throw 'Invalid marker identity or interval'}
        $previous=$m.qpcEnd
        $previousCpu=[decimal]$m.rootTotalCpu100ns
    }
}
function Assert-ProbeObservation($Outcome){
    $o=$Outcome.observation
    Assert-ProbeFields $o @('schema','complete','error','observerPid','observerCreationFileTime','frequency','frames','discardedFrames','finalJobUser100ns','finalJobKernel100ns','finalJobQpcStart','finalJobQpcEnd','finalJobTotalProcesses','finalRootUser100ns','finalRootKernel100ns','finalRootQpcStart','finalRootQpcEnd','acceptanceEvidence')
    Assert-ProbeBool $o.complete $true 'observation complete'; Assert-ProbeBool $o.acceptanceEvidence $false 'observation acceptanceEvidence'
    if($o.schema -cne 'pr72-admission-job-cpu-v1' -or $null -ne $o.error){throw 'Wrong observation schema or error'}
    foreach($key in @('observerPid','observerCreationFileTime','frequency','discardedFrames','finalJobQpcStart','finalJobQpcEnd','finalRootQpcStart','finalRootQpcEnd')){Assert-ProbeNumber $o.$key $key -Integer}
    $self=[Diagnostics.Process]::GetCurrentProcess()
    try{$selfCreation=$self.StartTime.ToUniversalTime().ToFileTimeUtc()}finally{$self.Dispose()}
    if($o.observerPid -ne [Environment]::ProcessId -or $o.observerCreationFileTime -ne $selfCreation -or $o.frequency -ne $Outcome.frequency -or $o.frequency -ne [Diagnostics.Stopwatch]::Frequency -or $o.discardedFrames -ne 0){throw 'Observation observer identity/frequency/discard mismatch'}
    if($o.frames -isnot [Collections.IList] -or $o.frames.Count -lt 1 -or $o.frames.Count -gt 81){throw 'Observation frame count invalid'}
    $counters=@('jobUser100ns','jobKernel100ns','rootUser100ns','rootKernel100ns','observerUser100ns','observerKernel100ns','totalProcesses')
    $previous=$null
    foreach($frame in $o.frames){
        Assert-ProbeFields $frame @('qpcStart','qpcEnd','utc','jobUser100ns','jobKernel100ns','activeProcesses','totalProcesses','rootUser100ns','rootKernel100ns','observerUser100ns','observerKernel100ns')
        foreach($key in @('qpcStart','qpcEnd')){Assert-ProbeNumber $frame.$key $key -Integer -Min $Outcome.deadlineStartQpc -Max $Outcome.observedExitQpc}
        foreach($key in @('jobUser100ns','jobKernel100ns')){Assert-ProbeNumber $frame.$key $key -Integer -Max ([long]::MaxValue)}
        foreach($key in @('rootUser100ns','rootKernel100ns','observerUser100ns','observerKernel100ns')){Assert-ProbeNumber $frame.$key $key -Integer -Max ([ulong]::MaxValue)}
        foreach($key in @('activeProcesses','totalProcesses')){Assert-ProbeNumber $frame.$key $key -Integer -Max ([uint32]::MaxValue)}
        Assert-ProbeUtc $frame.utc
        if($frame.qpcEnd -lt $frame.qpcStart -or $frame.activeProcesses -gt $frame.totalProcesses){throw 'Invalid frame bracket or process counts'}
        if($null -ne $previous){
            if(([decimal]$frame.qpcStart-[decimal]$previous.qpcEnd)*4 -lt [decimal]$o.frequency){throw 'Observation period shorter than 250ms end-to-start'}
            foreach($key in $counters){if([decimal]$frame.$key -lt [decimal]$previous.$key){throw "Observation counter regressed: $key"}}
        }
        $previous=$frame
    }
    foreach($key in @('finalJobUser100ns','finalJobKernel100ns')){Assert-ProbeNumber $o.$key $key -Integer -Max ([long]::MaxValue)}
    foreach($key in @('finalRootUser100ns','finalRootKernel100ns')){Assert-ProbeNumber $o.$key $key -Integer -Max ([ulong]::MaxValue)}
    Assert-ProbeNumber $o.finalJobTotalProcesses 'finalJobTotalProcesses' -Integer -Min 1 -Max ([uint32]::MaxValue)
    if($o.finalJobQpcStart -lt $Outcome.observedExitQpc -or $o.finalJobQpcStart -lt $previous.qpcEnd -or $o.finalJobQpcEnd -lt $o.finalJobQpcStart -or $o.finalRootQpcStart -lt $o.finalJobQpcEnd -or $o.finalRootQpcEnd -lt $o.finalRootQpcStart -or $o.finalRootQpcEnd -gt $Outcome.completedQpc -or ([decimal]$o.finalRootQpcEnd-[decimal]$Outcome.deadlineStartQpc) -ge ([decimal]$o.frequency*20)){throw 'Terminal CPU brackets outside native interval'}
    foreach($pair in @(@('finalJobUser100ns','jobUser100ns'),@('finalJobKernel100ns','jobKernel100ns'),@('finalRootUser100ns','rootUser100ns'),@('finalRootKernel100ns','rootKernel100ns'),@('finalJobTotalProcesses','totalProcesses'))){if([decimal]$o.($pair[0]) -lt [decimal]$previous.($pair[1])){throw 'Terminal counter regressed'}}
    if([Text.Encoding]::UTF8.GetByteCount(($o | ConvertTo-Json -Depth 16 -Compress)) -gt 196608){throw 'Observation exceeds serialized byte cap'}
}
function Assert-ProbeArtifactInterval([string]$Path,$Outcome){
    $item=Get-Item -LiteralPath $Path
    $start=[DateTime]::Parse($Outcome.startedUtc,[Globalization.CultureInfo]::InvariantCulture).ToUniversalTime()
    $end=[DateTime]::Parse($Outcome.finishedUtc,[Globalization.CultureInfo]::InvariantCulture).ToUniversalTime()
    foreach($time in @($item.CreationTimeUtc,$item.LastWriteTimeUtc)){if($time -lt $start -or $time -gt $end){throw 'Prepared artifact birth/write outside actual native interval'}}
}
function Assert-ProbeReceipt($Receipt,$Bindings,[string]$ReviewHash,[string]$BindingsHash){
    Assert-ProbeFields $Receipt @('scope','sourceBindingsSha256','sourceReviewSha256','observerSourceSha256','assemblyPath','assemblySha256','moduleVersionId','className','pwshPath','acceptanceEvidence','runtimeAcceptance')
    Assert-ProbeBool $Receipt.acceptanceEvidence $false 'receipt acceptanceEvidence'; Assert-ProbeBool $Receipt.runtimeAcceptance $false 'receipt runtimeAcceptance'
    if($Receipt.scope -cne 'PR72-admission-cpu-capability' -or $Receipt.sourceBindingsSha256 -cne $BindingsHash -or $Receipt.sourceReviewSha256 -cne $ReviewHash -or $Receipt.observerSourceSha256 -cne $Bindings.observer.sha256 -or $Receipt.className -cne 'RA2AdmissionCpuSupervisor' -or $Receipt.assemblyPath -cne "$script:ProbeRoot/capability/AdmissionCpuSupervisor.dll" -or $Receipt.pwshPath -cne $script:ProbeHost){throw 'Compiled receipt source or identity mismatch'}
    $mvid=[guid]::Empty
    if($Receipt.moduleVersionId -isnot [string] -or ![guid]::TryParseExact($Receipt.moduleVersionId,'D',[ref]$mvid) -or $mvid -eq [guid]::Empty){throw 'Invalid compiled module identity'}
    Assert-ProbePin $Receipt.assemblyPath $Receipt.assemblySha256
}
