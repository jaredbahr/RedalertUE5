param(
    [ValidateSet('Prepare','Observe')][string]$Stage='Observe',
    [string]$SourceReviewSHA256,[string]$CapabilityReviewSHA256,
    [switch]$Execute,[switch]$OwnedLanesDrained,[switch]$CoordinatedWindow,
    [switch]$DiagnosticPreparationAuthorized,[switch]$CurrentHostInspectionConfirmed
)
# Capture the origin before source/hash/load work. Synchronous prefix stalls fail on return.
$origin=[Diagnostics.Stopwatch]::GetTimestamp()
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
. "$PSScriptRoot/Probe.Common.ps1"
if(!$Execute){
    [ordered]@{schema='pr72-admission-cpu-plan-v1';stage=$Stage;executionRequested=$false;preparationMode='diagnostic-only';requiresFreshHostInspection=$true;nativeSeconds=20;cleanupSeconds=5;sourceReviewRequired=$true;compiledReviewRequired=($Stage -eq 'Observe');acceptanceEvidence=$false;gameAcceptance=$false;workAuthorized=$false} | ConvertTo-Json -Compress
    return
}
if(!$OwnedLanesDrained -or !$CoordinatedWindow){throw 'Explicit drained lanes and newly coordinated window required'}
if($Stage -eq 'Prepare' -and (!$DiagnosticPreparationAuthorized -or !$CurrentHostInspectionConfirmed)){throw 'Diagnostic-only preparation requires explicit authorization and fresh host inspection confirmation'}
$b=Assert-ProbeSource $SourceReviewSHA256 -DiagnosticPreparation:($Stage -eq 'Prepare')
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
$eligible=$null
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
$gate=[ordered]@{schema='pr72-admission-cpu-controller-gate-v1';stage=$Stage;preparationMode='diagnostic-only';requiresFreshHostInspection=$true;nonterminal=$true;controllerChecksPassed=$true;diagnosticComplete=($Stage -eq 'Observe');resourceEligible=$eligible;returnedOutcomeSha256=(Get-ProbeHash "$case/returned-outcome.json");sourceBindingsSha256=$bindingsHash;requiresActualOuterExitAndTerminalReconciliation=$true;requiresExternalFinal20And5Measurement=$true;acceptanceEvidence=$false;gameAcceptance=$false;workAuthorized=$false}
$owner::DurableNew("$case/controller-gate.json",($gate | ConvertTo-Json -Compress))
Assert-ProbeClocks $outcome $origin
$gate | ConvertTo-Json -Compress
# CLI zero is only this nonterminal controller gate. An external caller must measure
# final through-return <20s/post-exit <=5s, actual exit 0 and terminal/raw equality.
