# Thin dispatcher around an already prepared/loaded native watchdog. All controller
# hashing, Python, setup, payload and reconciliation run in the fresh owned root.
[CmdletBinding()]
param(
 [ValidateSet('Build','NativeHUD','Render1280','Render960','Input1280','Input960')][string]$Case='Build',
 [string]$Bindings,[string]$ReviewedBindingsSHA256,[string]$IndependentReview,[string]$Prerequisites,[string]$PrerequisitesSHA256,
 [string]$SourceHead,[string]$CppTree,[psobject]$WatchdogCapability,
 [string]$CapacityExpiresUtc,[string]$CancellationPath,
 [switch]$Execute,[switch]$OwnedLanesDrained,[switch]$CoordinatedWindow
)
$ErrorActionPreference='Stop'
if(-not $Execute){[ordered]@{preparedOnly=$true;acceptanceEvidence=$false;engineInvoked=$false;case=$Case;requires=@('independent exact binding review','prepared native whole-controller watchdog','fresh qualified resource receipts','owned lanes drained','coordinated window')}|ConvertTo-Json;return}
$origin=[Diagnostics.Stopwatch]::GetTimestamp()
if(-not $OwnedLanesDrained -or -not $CoordinatedWindow){throw 'Explicit current coordinator drain/window confirmation required.'}
if($PSVersionTable.PSVersion.Major -lt 7 -or -not ('RA2CommandCardStageSupervisor' -as [type])){throw 'Prepare/load the exact reviewed watchdog before functional dispatch; compilation is not controller setup.'}
if($SourceHead -cnotmatch '^[a-f0-9]{40}$' -or $CppTree -cnotmatch '^[a-f0-9]{40}$' -or $ReviewedBindingsSHA256 -cnotmatch '^[a-f0-9]{64}$' -or $WatchdogCapability.receiptSha256 -cnotmatch '^[a-f0-9]{64}$'){throw 'Exact reviewed dispatch identities required.'}
# Capability data is already in memory. No filesystem/file-hash/Python/Add-Type setup
# occurs outside the fresh controller after this shared origin.
$prepared=$WatchdogCapability
$loaded=[RA2CommandCardStageSupervisor].Assembly
if($prepared.head -cne $SourceHead -or $loaded.Location -ine $prepared.assemblyPath -or $loaded.ManifestModule.ModuleVersionId.ToString() -cne $prepared.moduleVersionId -or [Environment]::ProcessPath -ine $prepared.pwshPath){throw 'Loaded native watchdog is not the prepared exact candidate.'}
$mode=if($Case -eq 'Build'){'build'}elseif($Case -eq 'NativeHUD'){'native'}elseif($Case.StartsWith('Render')){'render'}else{'interactive'}
$seconds=if($mode -eq 'build'){900}elseif($mode -eq 'native'){120}else{220}
$viewport=if($Case.EndsWith('960')){'960x540'}else{'1280x720'}
$sourceRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$caseRoot=Join-Path $sourceRoot ('generated/pr72-'+$Case.ToLowerInvariant()+'-'+[guid]::NewGuid().ToString('N'))
$temp=Join-Path $caseRoot 'temp'
$spec=[ordered]@{case=$Case;mode=$mode;seconds=$seconds;viewport=$viewport;stageStartQpc=0;output=$caseRoot;head=$SourceHead;cppTree=$CppTree;bindings=$Bindings;bindingsSha256=$ReviewedBindingsSHA256;review=$IndependentReview;prerequisites=$Prerequisites;prerequisitesSha256=$PrerequisitesSHA256;capacityExpiresUtc=$CapacityExpiresUtc;cancelPath=$CancellationPath;preparedWatchdog=$prepared.receiptPath;preparedWatchdogSha256=$prepared.receiptSha256;ownedLanesDrained=$true;coordinatedWindow=$true}
$specBytes=[Text.UTF8Encoding]::new($false).GetBytes(($spec|ConvertTo-Json))
if($specBytes.Length -gt 8192){throw 'Dispatch metadata cap exceeded.'}
$specSha=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($specBytes)).ToLowerInvariant()
$arguments=@('-NoProfile','-File',(Join-Path $PSScriptRoot 'stage-controller.ps1'),'-SpecBase64',[Convert]::ToBase64String($specBytes),'-SpecSHA256',$specSha)
# One origin, one owned job, one cleanup budget. No setup/reconciliation subprocess
# runs in this dispatcher after the origin; the entire fresh controller is guarded.
$outcome=[RA2CommandCardStageSupervisor]::Run($prepared.pwshPath,[string[]]$arguments,$sourceRoot,$seconds,$caseRoot,$mode,$temp,$CapacityExpiresUtc,$origin,$CancellationPath,$SourceHead,$ReviewedBindingsSHA256,$CppTree)
$terminal=Join-Path $caseRoot 'RETURNED_OUTCOME.json'
[RA2CommandCardStageSupervisor]::ExportReturnedOutcome($terminal,$outcome)
if(-not $outcome.passed -or -not $outcome.wholeController){throw 'Terminal whole-controller outcome failed; payload/inner files cannot be reused as success.'}
Write-Output $terminal
