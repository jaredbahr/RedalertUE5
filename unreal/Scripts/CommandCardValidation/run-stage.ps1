[CmdletBinding()]
param(
 [ValidateSet('Build','NativeHUD','Render1280','Render960','Input1280','Input960')][string]$Case='Build',
 [string]$Bindings,[string]$ReviewedBindingsSHA256,[string]$IndependentReview,[string]$Prerequisites,[string]$PrerequisitesSHA256,
 [switch]$Execute,[switch]$OwnedLanesDrained,[switch]$CoordinatedWindow
)
$ErrorActionPreference='Stop'
if(-not $Execute){[ordered]@{preparedOnly=$true;acceptanceEvidence=$false;engineInvoked=$false;case=$Case;requires=@('independent exact binding review','fresh qualified resource receipts','owned lanes drained','coordinated window')}|ConvertTo-Json;return}
$stageStartQpc=[Diagnostics.Stopwatch]::GetTimestamp()
$mode=if($Case -eq 'Build'){'build'}elseif($Case -eq 'NativeHUD'){'native'}elseif($Case.StartsWith('Render')){'render'}else{'interactive'}
$seconds=if($mode -eq 'build'){900}elseif($mode -eq 'native'){120}else{220}
$viewport=if($Case.EndsWith('960')){'960x540'}else{'1280x720'}
if(-not $OwnedLanesDrained -or -not $CoordinatedWindow){throw 'Explicit current coordinator drain/window confirmation required.'}
if($PSVersionTable.PSVersion.Major -lt 7){throw 'Use the supported PowerShell 7 host route.'}
foreach($path in @($Bindings,$IndependentReview,$Prerequisites)){if(-not $path -or -not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -gt 1MB){throw 'Bounded actual prerequisite files required.'}}
if($ReviewedBindingsSHA256 -cnotmatch '^[a-f0-9]{64}$' -or (Get-FileHash -LiteralPath $Bindings -Algorithm SHA256).Hash.ToLowerInvariant() -cne $ReviewedBindingsSHA256){throw 'Exact reviewed binding required.'}
if($PrerequisitesSHA256 -cnotmatch '^[a-f0-9]{64}$' -or (Get-FileHash -LiteralPath $Prerequisites -Algorithm SHA256).Hash.ToLowerInvariant() -cne $PrerequisitesSHA256){throw 'Exact actual prerequisite bundle required.'}
$prerequisiteBytes=[IO.File]::ReadAllBytes($Prerequisites)
$binding=Get-Content -LiteralPath $Bindings -Raw|ConvertFrom-Json
if([IO.Path]::GetFullPath($PSScriptRoot) -cne [IO.Path]::GetFullPath((Join-Path $binding.sourceRoot 'unreal/Scripts/CommandCardValidation'))){throw 'Execute only the bound source copy, not a sealed archive or another checkout.'}
$python=$binding.executables.python.path
# The controller's own source is a member of the externally reviewed hash set.
if((Get-FileHash -LiteralPath $python -Algorithm SHA256).Hash.ToLowerInvariant() -cne $binding.executables.python.sha256){throw 'Python executable pin mismatch.'}
$policy=Join-Path $PSScriptRoot 'policy.py'
$policyPin=@($binding.files|Where-Object relative -CEQ 'unreal/Scripts/CommandCardValidation/policy.py')
if($policyPin.Count -ne 1 -or (Get-FileHash -LiteralPath $policy -Algorithm SHA256).Hash.ToLowerInvariant() -cne $policyPin[0].sha256){throw 'Policy source pin mismatch.'}
& $python -B $policy bindings $Bindings --sha256 $ReviewedBindingsSHA256 --review $IndependentReview --head $binding.sourceHead
if($LASTEXITCODE -ne 0){throw 'Binding/review validation rejected.'}
& $python -B $policy prerequisites $Prerequisites --sha256 $PrerequisitesSHA256 --head $binding.sourceHead --frequency ([Diagnostics.Stopwatch]::Frequency)
if($LASTEXITCODE -ne 0){throw 'Qualification/reservation/fresh dispatch admission rejected.'}
$p=[Text.Encoding]::UTF8.GetString($prerequisiteBytes)|ConvertFrom-Json
$leaseBytes=[IO.File]::ReadAllBytes($p.lease.path)
if([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($leaseBytes)).ToLowerInvariant() -cne $p.lease.sha256){throw 'Lease changed after prerequisite checking.'}
$lease=[Text.Encoding]::UTF8.GetString($leaseBytes)|ConvertFrom-Json
if(-not $lease.cancelPath -or (Test-Path -LiteralPath $lease.cancelPath)){throw 'Lease cancellation path missing or cancelled.'}
$guard=Join-Path $PSScriptRoot 'CommandCardStageSupervisor.cs'
if('RA2CommandCardStageSupervisor' -as [type]){throw 'Use a fresh dedicated PowerShell 7 process for each case.'}
$guardBytes=[IO.File]::ReadAllBytes($guard)
$guardPin=@($binding.files|Where-Object relative -CEQ 'unreal/Scripts/CommandCardValidation/CommandCardStageSupervisor.cs')
if($guardPin.Count -ne 1 -or [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($guardBytes)).ToLowerInvariant() -cne $guardPin[0].sha256){throw 'Checked supervisor bytes changed.'}
Add-Type -TypeDefinition ([Text.Encoding]::UTF8.GetString($guardBytes))
$mutex=[Threading.Mutex]::new($false,'Local\RedalertUE5-PR71-validation')
$owned=$false
try{
    $owned=$mutex.WaitOne(0)
    if(-not $owned){throw 'Another RA2 validation owns the project slot; do not wait or compete.'}
    # Recheck freshness after compilation/mutex acquisition; the case clock is already running.
    & $python -B $policy prerequisites $Prerequisites --sha256 $PrerequisitesSHA256 --head $binding.sourceHead --frequency ([Diagnostics.Stopwatch]::Frequency)
    if($LASTEXITCODE -ne 0){throw 'Dispatch admission became stale.'}
    $caseRoot=Join-Path $binding.sourceRoot ('generated/pr72-'+$Case.ToLowerInvariant()+'-'+[guid]::NewGuid().ToString('N'))
    if(Test-Path -LiteralPath $caseRoot){throw 'Refusing reused case output directory.'}
    $temp=Join-Path $caseRoot 'temp'
    [IO.Directory]::CreateDirectory($temp)|Out-Null
    $spec=[ordered]@{case=$Case;mode=$mode;seconds=$seconds;viewport=$viewport;stageStartQpc=$stageStartQpc;output=$caseRoot;bindings=$Bindings;bindingsSha256=$ReviewedBindingsSHA256;review=$IndependentReview;prerequisites=$Prerequisites}
    $specPath=Join-Path $caseRoot 'SPEC.json'
    [IO.File]::WriteAllText($specPath,($spec|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
    $specSHA=(Get-FileHash -LiteralPath $specPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $arguments=@('-NoProfile','-File',(Join-Path $PSScriptRoot 'stage-worker.ps1'),'-SpecPath',$specPath,'-SpecSHA256',$specSHA)
    $outcome=[RA2CommandCardStageSupervisor]::Run($binding.executables.pwsh.path,[string[]]$arguments,$binding.sourceRoot,$seconds,$caseRoot,$mode,$temp,$lease.expiresUtc,$stageStartQpc,$lease.cancelPath)
    $assessment=Join-Path $caseRoot 'RETURNED_OUTCOME.json'
    [IO.File]::WriteAllText($assessment,($outcome|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    & $python -B $policy outcome $assessment --head $binding.sourceHead --stage $mode --frequency ([Diagnostics.Stopwatch]::Frequency)
    if($LASTEXITCODE -ne 0){throw 'Native owned-job outcome rejected.'}
    $payload=Get-Content -LiteralPath (Join-Path $caseRoot 'PAYLOAD_RESULT.json') -Raw|ConvertFrom-Json
    if($payload.automatedStageValidated -ne $true -or $payload.head -cne $binding.sourceHead -or $payload.error){throw 'Missing complete payload reconciliation.'}
    $summary=[ordered]@{automatedStageValidated=$true;acceptanceEvidence=$false;physicalInputVerified=$false;case=$Case;sourceHead=$binding.sourceHead;runDirectory=$caseRoot;independentHumanChecklistRequired=($mode -eq 'interactive')}|ConvertTo-Json
    if([Diagnostics.Stopwatch]::GetTimestamp() -ge $stageStartQpc+[long]$seconds*[Diagnostics.Stopwatch]::Frequency){throw 'Case setup/parsing/receipt completion exceeded its enclosing deadline.'}
    Write-Output $summary
}finally{
    if($owned){$mutex.ReleaseMutex()}
    $mutex.Dispose()
}
