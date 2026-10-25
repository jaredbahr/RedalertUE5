param(
    [ValidateSet('Prepare','Observe')][string]$Stage='Observe',
    [string]$SourceReviewSHA256,[string]$CapabilityReviewSHA256,
    [string]$PreparationAdmission,[string]$PreparationAdmissionSHA256,
    [switch]$Execute,[switch]$OwnedLanesDrained,[switch]$CoordinatedWindow
)
# Capture the origin before source/hash/load work. Synchronous prefix stalls fail on return.
$origin=[Diagnostics.Stopwatch]::GetTimestamp()
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
. "$PSScriptRoot/Probe.Common.ps1"
if(!$Execute){
    [ordered]@{schema='pr72-admission-cpu-plan-v1';stage=$Stage;executionRequested=$false;nativeSeconds=20;cleanupSeconds=5;sourceReviewRequired=$true;compiledReviewRequired=($Stage -eq 'Observe');acceptanceEvidence=$false;gameAcceptance=$false;workAuthorized=$false} | ConvertTo-Json -Compress
    return
}
if(!$OwnedLanesDrained -or !$CoordinatedWindow){throw 'Explicit drained lanes and newly coordinated window required'}
$b=Assert-ProbeSource $SourceReviewSHA256
$callerProcess=[Diagnostics.Process]::GetCurrentProcess()
try {
    $callerProcess.ProcessorAffinity=[IntPtr]68
    $callerProcess.PriorityClass=[Diagnostics.ProcessPriorityClass]::Normal
    if($callerProcess.ProcessorAffinity.ToInt64() -ne 68 -or $callerProcess.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::Normal){throw 'Controller resource policy mismatch'}
} finally {$callerProcess.Dispose()}
$bindingsHash=Get-ProbeHash "$script:ProbeRoot/PROBE_SOURCE_BINDINGS.json"
$caseName=if($Stage -eq 'Prepare'){'preparation'}else{'observation'}
$case="$script:ProbeRoot/$caseName"; $temp="$case/temp"
if(Test-Path -LiteralPath $case){throw 'Case already exists; no retry or overwrite'}
$expires=[DateTime]::UtcNow.AddSeconds(30).ToString('o')
if($Stage -eq 'Prepare'){
    Assert-ProbePin $PreparationAdmission $PreparationAdmissionSHA256
    $reference=Read-ProbeJson $PreparationAdmission
    foreach($key in @('admission','admissionOutcome','returnedStage','callerCompletion')){Assert-ProbePin $reference[$key].path $reference[$key].sha256}
    $admission=Read-ProbeJson $reference.admission.path
    $prior=Read-ProbeJson $reference.admissionOutcome.path
    $caller=Read-ProbeJson $reference.callerCompletion.path
    Assert-ProbeAdmission $admission -RequireEligible
    Assert-ProbeNative $prior -Historical
    $stageGate=Read-ProbeJson $reference.returnedStage.path
    Assert-ProbeFields $stageGate @('scope','stage','sourceHead','cppTree','reviewSha256','inputsSha256','passed','failure','boundSeconds','aggregateCleanupSeconds','originalOriginThroughReturnSeconds','postExitThroughReturnSeconds','outcome')
    Assert-ProbeFields $caller @('actualNativeExitCode','checksPassed','terminalOutputReconciled','failure','stage','reviewSha256','returnedStageSha256','originalOriginThroughReturnSeconds','postExitThroughReturnSeconds')
    Assert-ProbeBool $stageGate.passed $true 'returned stage passed'; Assert-ProbeBool $caller.checksPassed $true 'caller checksPassed'; Assert-ProbeBool $caller.terminalOutputReconciled $true 'terminalOutputReconciled'
    foreach($key in @('boundSeconds','aggregateCleanupSeconds')){Assert-ProbeNumber $stageGate[$key] $key -Integer}
    Assert-ProbeNumber $caller.actualNativeExitCode 'actualNativeExitCode' -Integer -Max 0
    if($null -ne $stageGate.failure -or $null -ne $caller.failure){throw 'Original stage or caller failure present'}
    $oldReview='6fcdbb3dc3164b76863431557d73af0501ff83817f8edc52d07af7b1858b1bdb'
    if($stageGate.scope -cne 'PR72-engine-preparation-only' -or $stageGate.stage -cne 'Admission' -or $stageGate.sourceHead -cne $b.parentGameSourceHead -or $stageGate.cppTree -cne $b.gameCppTree -or $stageGate.reviewSha256 -cne $oldReview -or $stageGate.inputsSha256 -cne 'f69e816f8149baaacc122dd72afa943d52e4f05d91df65e9991ea547c7230a57' -or $stageGate.passed -cne $true -or $stageGate.failure -or $stageGate.boundSeconds -ne 20 -or $stageGate.aggregateCleanupSeconds -ne 5 -or (ConvertTo-ProbeCanonical $stageGate.outcome) -cne (ConvertTo-ProbeCanonical $prior)){throw 'Fresh original returned stage mismatch'}
    if($caller.actualNativeExitCode -ne 0 -or $caller.checksPassed -cne $true -or $caller.terminalOutputReconciled -cne $true -or $caller.failure -or $caller.stage -cne 'Admission' -or $caller.reviewSha256 -cne $oldReview -or $caller.returnedStageSha256 -cne $reference.returnedStage.sha256){throw 'Original caller reconciliation missing'}
    foreach($record in @($stageGate,$caller)){foreach($field in @('originalOriginThroughReturnSeconds','postExitThroughReturnSeconds')){Assert-ProbeNumber $record[$field] $field; $value=[double]$record[$field]; if(($field -eq 'originalOriginThroughReturnSeconds' -and $value -ge 20) -or ($field -eq 'postExitThroughReturnSeconds' -and $value -gt 5)){throw 'Original final timing failed'}}}
    $age=([DateTime]::UtcNow-[DateTime]::Parse($admission.utc).ToUniversalTime()).TotalSeconds
    $capacity=[DateTime]::Parse($prior.capacityExpiresUtc).ToUniversalTime()
    if($age -lt 0 -or $age -gt 60 -or $capacity -lt [DateTime]::UtcNow.AddSeconds(30)){throw 'Admission freshness or remaining capacity failed'}
    $expires=$prior.capacityExpiresUtc
    if(Test-Path -LiteralPath "$script:ProbeRoot/capability"){throw 'Capability output must be absent'}
    # The new source approval covers this exact compile-child invocation. The
    # existing DLL is identity-pinned; historical engine permission is not reused.
    $assembly=[Reflection.Assembly]::LoadFrom($b.prepareOwner.assemblyPath)
    if($assembly.Location.Replace('\','/') -cne $b.prepareOwner.assemblyPath -or $assembly.ManifestModule.ModuleVersionId.ToString() -cne $b.prepareOwner.moduleVersionId){throw 'Preparation owner loaded identity mismatch'}
    $owner=$assembly.GetType($b.prepareOwner.className,$true)
    $argv=@('-NoProfile','-File',"$PSScriptRoot/CompileObserver.ps1",'-SourceReviewSHA256',$SourceReviewSHA256,'-OutputDirectory',"$script:ProbeRoot/capability")
}else{
    $capPath="$script:ProbeRoot/CAPABILITY_REVIEW.json"
    Assert-ProbePin $capPath $CapabilityReviewSHA256
    $cap=Read-ProbeJson $capPath; Assert-ProbeReview $cap 'PR72-admission-cpu-capability-review'
    if($cap.sourceBindingsSha256 -cne $bindingsHash -or $cap.sourceReviewSha256 -cne $SourceReviewSHA256){throw 'Compiled review source mismatch'}
    $receiptPath="$script:ProbeRoot/capability/PREPARED_OBSERVER.json"
    Assert-ProbePin $receiptPath $cap.preparedObserverSha256
    $receipt=Read-ProbeJson $receiptPath
    Assert-ProbeReceipt $receipt $b $SourceReviewSHA256 $bindingsHash
    if($receipt.sourceBindingsSha256 -cne $bindingsHash -or $receipt.sourceReviewSha256 -cne $SourceReviewSHA256 -or $receipt.observerSourceSha256 -cne $b.observer.sha256 -or $receipt.assemblyPath -cne "$script:ProbeRoot/capability/AdmissionCpuSupervisor.dll" -or $receipt.assemblySha256 -cne $cap.assemblySha256 -or $receipt.moduleVersionId -cne $cap.moduleVersionId -or $receipt.pwshPath -cne $script:ProbeHost -or $receipt.acceptanceEvidence -cne $false){throw 'Prepared receipt mismatch'}
    Assert-ProbePin "$script:ProbeRoot/preparation/returned-outcome.json" $cap.preparationOutcomeSha256
    Assert-ProbePin "$script:ProbeRoot/preparation/CALLER_COMPLETION.json" $cap.preparationCallerCompletionSha256
    $prepared=Read-ProbeJson "$script:ProbeRoot/preparation/returned-outcome.json"
    $preparedCaller=Read-ProbeJson "$script:ProbeRoot/preparation/CALLER_COMPLETION.json"
    Assert-ProbeNative $prepared
    Assert-ProbeFields $preparedCaller @('callerExitCode','checksPassed','terminalReconciled','returnedOutcomeSha256','sourceBindingsSha256','throughReturnSeconds','postExitSeconds','externalFinalMeasurement')
    foreach($key in @('checksPassed','terminalReconciled','externalFinalMeasurement')){Assert-ProbeBool $preparedCaller[$key] $true $key}
    Assert-ProbeNumber $preparedCaller.callerExitCode 'callerExitCode' -Integer -Max 0
    if($preparedCaller.callerExitCode -ne 0 -or $preparedCaller.checksPassed -cne $true -or $preparedCaller.terminalReconciled -cne $true -or $preparedCaller.returnedOutcomeSha256 -cne $cap.preparationOutcomeSha256 -or $preparedCaller.sourceBindingsSha256 -cne $bindingsHash -or $preparedCaller.throughReturnSeconds -ge 20 -or $preparedCaller.postExitSeconds -gt 5 -or $preparedCaller.externalFinalMeasurement -cne $true){throw 'Preparation outer reconciliation missing'}
    foreach($field in @('throughReturnSeconds','postExitSeconds')){Assert-ProbeNumber $preparedCaller[$field] $field}
    foreach($path in @($receiptPath,$receipt.assemblyPath)){Assert-ProbeArtifactInterval $path $prepared}
    Assert-ProbePin $receipt.assemblyPath $receipt.assemblySha256
    $assembly=[Reflection.Assembly]::LoadFrom($receipt.assemblyPath)
    if($assembly.Location.Replace('\','/') -cne $receipt.assemblyPath -or $assembly.ManifestModule.ModuleVersionId.ToString() -cne $receipt.moduleVersionId){throw 'Loaded assembly identity mismatch'}
    $owner=$assembly.GetType('RA2AdmissionCpuSupervisor',$true)
    $argv=@('-NoProfile','-File',"$PSScriptRoot/ObserveAdmission.ps1",'-SourceReviewSHA256',$SourceReviewSHA256,'-OutputDirectory',$case)
}
[IO.Directory]::CreateDirectory($temp) | Out-Null
$outcome=$owner::Run($script:ProbeHost,[string[]]$argv,$script:ProbeSource,20,$case,'admission',$temp,$expires,$origin,"$case/CANCELLED")
$owner::ExportReturnedOutcome("$case/returned-outcome.json",$outcome)
Assert-ProbeNative $outcome
Assert-ProbeClocks $outcome $origin
$eligible=$false
if($Stage -eq 'Observe'){
    Assert-ProbeObservation $outcome
    Assert-ProbeMarkers $case $outcome
    $admission=Read-ProbeJson "$case/ADMISSION.json"; Assert-ProbeAdmission $admission
    $eligible=$admission.eligible
}else{
    $receipt=Read-ProbeJson "$script:ProbeRoot/capability/PREPARED_OBSERVER.json"
    Assert-ProbeReceipt $receipt $b $SourceReviewSHA256 $bindingsHash
    foreach($path in @("$script:ProbeRoot/capability/PREPARED_OBSERVER.json","$script:ProbeRoot/capability/AdmissionCpuSupervisor.dll")){Assert-ProbeArtifactInterval $path $outcome}
    $terminal=Read-ProbeJson "$case/admission.stdout.log" 1048576
    if((ConvertTo-ProbeCanonical $terminal) -cne (ConvertTo-ProbeCanonical $receipt)){throw 'Captured preparation terminal receipt differs from durable receipt'}
    Assert-ProbePin $receipt.assemblyPath $receipt.assemblySha256
}
$gate=[ordered]@{schema='pr72-admission-cpu-controller-gate-v1';stage=$Stage;nonterminal=$true;controllerChecksPassed=$true;diagnosticComplete=($Stage -eq 'Observe');resourceEligible=$eligible;returnedOutcomeSha256=(Get-ProbeHash "$case/returned-outcome.json");sourceBindingsSha256=$bindingsHash;requiresActualOuterExitAndTerminalReconciliation=$true;requiresExternalFinal20And5Measurement=$true;acceptanceEvidence=$false;gameAcceptance=$false;workAuthorized=$false}
$owner::DurableNew("$case/controller-gate.json",($gate | ConvertTo-Json -Compress))
Assert-ProbeClocks $outcome $origin
$gate | ConvertTo-Json -Compress
# CLI zero is only this nonterminal controller gate. An external caller must measure
# final through-return <20s/post-exit <=5s, actual exit 0 and terminal/raw equality.
