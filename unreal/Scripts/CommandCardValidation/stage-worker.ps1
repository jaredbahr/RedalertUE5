# Internal owned-job payload. Invoke through run-stage.ps1, never as a launcher.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$SpecPath,[Parameter(Mandatory)][string]$SpecSHA256)
$ErrorActionPreference='Stop'
if((Get-FileHash -LiteralPath $SpecPath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $SpecSHA256){throw 'Worker spec changed.'}
$spec=Get-Content -LiteralPath $SpecPath -Raw|ConvertFrom-Json
$identity=Get-Content -LiteralPath (Join-Path $spec.output ($spec.mode+'.identity.json')) -Raw|ConvertFrom-Json
if($identity.pid -ne $PID -or $identity.jobAssignedBeforeResume -ne $true -or $identity.affinityMask -cne '0x44'){throw 'Worker was not launched by its exact owned-job guard.'}
$spec|Add-Member -NotePropertyName stageStartQpc -NotePropertyValue $identity.deadlineStartQpc -Force
# The whole controller retains its console through every native invocation and
# releases only its own attachment in its finalizer. This worker shares that host.
$binding=Get-Content -LiteralPath $spec.bindings -Raw|ConvertFrom-Json
$python=$binding.executables.python.path
$policy=Join-Path $PSScriptRoot 'policy.py'
& $python -B $policy bindings $spec.bindings --sha256 $spec.bindingsSha256 --review $spec.review --head $binding.sourceHead
if($LASTEXITCODE -ne 0){throw 'Worker source binding rejected.'}
$root=$binding.sourceRoot
$git=$binding.executables.git.path
$monitor=& $git -C $root config --get core.fsmonitor
if($LASTEXITCODE -gt 1 -or ($monitor -and $monitor -notin @('false','no','0'))){throw 'Unsupported fsmonitor configuration.'}
if((& $git --no-optional-locks -C $root rev-parse HEAD).Trim() -cne $binding.sourceHead -or @(& $git --no-optional-locks -C $root status --porcelain).Count){throw 'Exact clean candidate source required.'}
if((& $git --no-optional-locks -C $root rev-parse HEAD:unreal/Source/RedalertProof).Trim() -cne $binding.cppTree){throw 'C++ source tree mismatch.'}
$deadline=[long]$spec.stageStartQpc+[long]$spec.seconds*[Diagnostics.Stopwatch]::Frequency
function Check-Deadline {if([Diagnostics.Stopwatch]::GetTimestamp() -ge $deadline){throw 'Case deadline includes setup, parsing and receipt completion.'}}
function Save-New([string]$Path,$Value){
    $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($Value|ConvertTo-Json -Depth 10))
    if($bytes.Length -gt 262144){throw 'Metadata cap exceeded.'}
    $f=[IO.FileStream]::new($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try{$f.Write($bytes,0,$bytes.Length);$f.Flush($true)}finally{$f.Dispose()}
}
Check-Deadline
$dll=Join-Path $root 'unreal/Binaries/Win64/UnrealEditor-RedalertProof.dll'
$modules=Join-Path $root 'unreal/Binaries/Win64/UnrealEditor.modules'
$started=[DateTime]::UtcNow
$proof=[ordered]@{case=$spec.case;mode=$spec.mode;head=$binding.sourceHead;bindingsSha256=$spec.bindingsSha256;cppTree=$binding.cppTree;sourceRoot=$root;startedUtc=$started.ToString('o');finishedUtc=$null;automatedStageValidated=$false;acceptanceEvidence=$false;physicalInputVerified=$false;rawReceipt=$null;rawReceiptSha256=$null;dllSha256=$null;modulesSha256=$null;error=$null}
try{
    if($spec.mode -eq 'build'){
        if(Test-Path -LiteralPath (Join-Path $root 'unreal/Binaries')){throw 'Fresh build requires absent candidate binaries; never reuse the demo DLL.'}
        & (Join-Path $root 'unreal/Scripts/build-native-proof.ps1') -EngineRoot $binding.engineRoot
        if($LASTEXITCODE -ne 0){throw 'Build payload failed.'}
        Check-Deadline
        foreach($path in @($dll,$modules)){
            $file=Get-Item -LiteralPath $path
            if($file.Length -le 0 -or $file.CreationTimeUtc -lt $started -or $file.LastWriteTimeUtc -lt $started -or $file.LastWriteTimeUtc -gt [DateTime]::UtcNow){throw 'Build artifact is missing or not fresh in the build interval.'}
        }
        $module=Get-Content -LiteralPath $modules -Raw|ConvertFrom-Json
        if(-not ($module.BuildId -is [string]) -or -not $module.BuildId.Trim() -or $module.Modules.RedalertProof -cne 'UnrealEditor-RedalertProof.dll'){throw 'Actual module map/BuildId required.'}
        $proof.dllSha256=(Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant()
        $proof.modulesSha256=(Get-FileHash -LiteralPath $modules -Algorithm SHA256).Hash.ToLowerInvariant()
        $proof.buildId=$module.BuildId
    }else{
        & $python -B $policy build-reference $spec.prerequisites --head $binding.sourceHead --root $root --frequency ([Diagnostics.Stopwatch]::Frequency) --bindings-sha256 $spec.bindingsSha256 --cpp-tree $binding.cppTree
        if($LASTEXITCODE -ne 0){throw 'Actual candidate build provenance rejected.'}
        foreach($asset in $binding.assets){
            $path=Join-Path $root ('unreal/Content/'+$asset.relative)
            if((Get-Item -LiteralPath $path).Length -ne $asset.bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $asset.sha256){throw 'Reused content hash mismatch.'}
        }
        $proof.dllSha256=(Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant()
        $proof.modulesSha256=(Get-FileHash -LiteralPath $modules -Algorithm SHA256).Hash.ToLowerInvariant()
        switch($spec.mode){
            'native' {$output=@(& (Join-Path $root 'unreal/Scripts/test-hud-layout-model.ps1') -EngineRoot $binding.engineRoot -TimeoutSeconds 90)}
            'render' {$output=@(& (Join-Path $root 'unreal/Scripts/test-small-match.ps1') -EngineRoot $binding.engineRoot -Scenario Victory -TimeoutSeconds 220 -CleanPresentation -ReadabilityCaptures -CameraEdgeCaptures -Viewport $spec.viewport)}
            'interactive' {$output=@(& (Join-Path $root 'unreal/Scripts/start-small-match.ps1') -EngineRoot $binding.engineRoot -StorageRoot $binding.storageRoot -SessionSeconds 180 -CameraPan -CameraEdgePan -Viewport $spec.viewport)}
            default {throw 'Unrecognized fixed payload mode.'}
        }
        Check-Deadline
        $rawPath=[string]$output[-1]
        $allowed=[IO.Path]::GetFullPath((Join-Path $root 'generated'))+[IO.Path]::DirectorySeparatorChar
        if(-not [IO.Path]::GetFullPath($rawPath).StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Payload receipt escaped the candidate output root.'}
        & $python -B $policy $spec.mode $rawPath --head $binding.sourceHead --viewport $spec.viewport --frequency ([Diagnostics.Stopwatch]::Frequency)
        if($LASTEXITCODE -ne 0){throw 'Inner receipt rejected independently of wrapper status.'}
        & $python -B $policy artifacts $rawPath --head $binding.sourceHead --viewport $spec.viewport --started $proof.startedUtc
        if($LASTEXITCODE -ne 0){throw 'Missing, stale, incomplete or mismatched logs/captures.'}
        if((Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant() -cne $proof.dllSha256 -or (Get-FileHash -LiteralPath $modules -Algorithm SHA256).Hash.ToLowerInvariant() -cne $proof.modulesSha256){throw 'Built inputs changed during the case.'}
        $proof.rawReceipt=$rawPath
        $proof.rawReceiptSha256=(Get-FileHash -LiteralPath $rawPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    & $python -B $policy bindings $spec.bindings --sha256 $spec.bindingsSha256 --review $spec.review --head $binding.sourceHead
    if($LASTEXITCODE -ne 0){throw 'Source/executable pins changed during the case.'}
    Check-Deadline
    $proof.automatedStageValidated=$true
}catch{$proof.error=$_.Exception.Message}
$proof.finishedUtc=[DateTime]::UtcNow.ToString('o')
Save-New (Join-Path $spec.output 'PAYLOAD_RESULT.json') $proof
Check-Deadline
if($proof.error -or -not $proof.automatedStageValidated){throw 'The bounded payload did not validate.'}
# Returning zero is necessary; only the outer native job outcome can confirm no survivors.
