# Entire process is contained before resume by the prepared native watchdog.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$SpecBase64,[Parameter(Mandatory)][string]$SpecSHA256)
$ErrorActionPreference='Stop'
Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class CommandCardOwnConsole {
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
}
'@
# Keep the PowerShell host's console mode usable through its final native Python
# call. This finalizer remains inside the same owned job and absolute deadline.
try {
    $specBytes=[Convert]::FromBase64String($SpecBase64)
    if($specBytes.Length -gt 8192 -or [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($specBytes)).ToLowerInvariant() -cne $SpecSHA256){throw 'Exact bounded dispatch spec required.'}
    $spec=[Text.UTF8Encoding]::new($false,$true).GetString($specBytes)|ConvertFrom-Json
    $identity=Get-Content -LiteralPath (Join-Path $spec.output ($spec.mode+'.identity.json')) -Raw|ConvertFrom-Json
    if($identity.pid -ne $PID -or $identity.jobAssignedBeforeResume -ne $true -or $identity.affinityMask -cne '0x44' -or $identity.priority -cne 'Normal' -or $identity.boundSeconds -ne $spec.seconds -or $identity.frequency -ne [Diagnostics.Stopwatch]::Frequency){throw 'Whole controller was not bound before resume.'}
    $spec.stageStartQpc=$identity.deadlineStartQpc
    $deadline=[long]$spec.stageStartQpc+[long]$spec.seconds*[Diagnostics.Stopwatch]::Frequency
    function Check-Deadline {if([Diagnostics.Stopwatch]::GetTimestamp() -ge $deadline){throw 'Whole-controller deadline includes setup and final reconciliation.'}}
    function Save-New([string]$Path,$Value){
        Check-Deadline
        $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($Value|ConvertTo-Json -Depth 10))
        if($bytes.Length -gt 262144){throw 'Metadata cap exceeded.'}
        $file=[IO.FileStream]::new($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
        try{$file.Write($bytes,0,$bytes.Length);$file.Flush($true)}finally{$file.Dispose()}
        Check-Deadline
    }
    Check-Deadline
    $SpecPath=Join-Path $spec.output 'SPEC.json'
    Save-New $SpecPath $spec
    $SpecSHA256=(Get-FileHash -LiteralPath $SpecPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if($PSVersionTable.PSVersion.Major -lt 7 -or $spec.ownedLanesDrained -ne $true -or $spec.coordinatedWindow -ne $true){throw 'Current supported host and coordinator confirmations required.'}
    foreach($path in @($spec.bindings,$spec.review,$spec.prerequisites)){if(-not $path -or -not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -gt 1MB){throw 'Actual bounded prerequisite files required.'}}
    if($spec.bindingsSha256 -cnotmatch '^[a-f0-9]{64}$' -or (Get-FileHash -LiteralPath $spec.bindings -Algorithm SHA256).Hash.ToLowerInvariant() -cne $spec.bindingsSha256){throw 'Exact reviewed binding required.'}
    if($spec.prerequisitesSha256 -cnotmatch '^[a-f0-9]{64}$' -or (Get-FileHash -LiteralPath $spec.prerequisites -Algorithm SHA256).Hash.ToLowerInvariant() -cne $spec.prerequisitesSha256){throw 'Exact actual prerequisite bundle required.'}
    $binding=Get-Content -LiteralPath $spec.bindings -Raw|ConvertFrom-Json
    if($binding.sourceHead -cne $spec.head -or $binding.cppTree -cne $spec.cppTree -or [IO.Path]::GetFullPath($PSScriptRoot) -cne [IO.Path]::GetFullPath((Join-Path $binding.sourceRoot 'unreal/Scripts/CommandCardValidation'))){throw 'Controller source/root binding mismatch.'}
    if([Environment]::ProcessPath -ine $binding.executables.pwsh.path){throw 'Controller executable binding mismatch.'}
    $python=$binding.executables.python.path
    $policy=Join-Path $PSScriptRoot 'policy.py'
    $policyPin=@($binding.files|Where-Object relative -CEQ 'unreal/Scripts/CommandCardValidation/policy.py')
    if((Get-FileHash -LiteralPath $python -Algorithm SHA256).Hash.ToLowerInvariant() -cne $binding.executables.python.sha256 -or $policyPin.Count -ne 1 -or (Get-FileHash -LiteralPath $policy -Algorithm SHA256).Hash.ToLowerInvariant() -cne $policyPin[0].sha256){throw 'Policy/checker source pin mismatch.'}
    & $python -B $policy bindings $spec.bindings --sha256 $spec.bindingsSha256 --review $spec.review --head $spec.head
    if($LASTEXITCODE -ne 0){throw 'Exact source/executable/content/review binding rejected.'}
    if($binding.preparedWatchdog.path -cne $spec.preparedWatchdog -or $binding.preparedWatchdog.sha256 -cne $spec.preparedWatchdogSha256){throw 'Dispatcher watchdog receipt differs from the reviewed binding.'}
    & $python -B $policy prerequisites $spec.prerequisites --sha256 $spec.prerequisitesSha256 --head $spec.head --frequency ([Diagnostics.Stopwatch]::Frequency)
    if($LASTEXITCODE -ne 0){throw 'Qualification/reservation/fresh dispatch admission rejected.'}
    $prerequisites=Get-Content -LiteralPath $spec.prerequisites -Raw|ConvertFrom-Json
    $lease=Get-Content -LiteralPath $prerequisites.lease.path -Raw|ConvertFrom-Json
    if($lease.expiresUtc -cne $spec.capacityExpiresUtc -or $lease.cancelPath -cne $spec.cancelPath -or (Test-Path -LiteralPath $spec.cancelPath)){throw 'Native watchdog and controller must share the same uncancelled lease.'}
    $mutex=[Threading.Mutex]::new($false,'Local\RedalertUE5-PR71-validation')
    $owned=$false
    try{
        $owned=$mutex.WaitOne(0)
        if(-not $owned){throw 'Another RA2 validation owns the slot; do not wait or compete.'}
        Check-Deadline
        # Same process, not a nested supervisor/job or a second cleanup budget.
        & (Join-Path $PSScriptRoot 'stage-worker.ps1') -SpecPath $SpecPath -SpecSHA256 $SpecSHA256
        Check-Deadline
        $proofPath=Join-Path $spec.output 'PAYLOAD_RESULT.json'
        $proof=Get-Content -LiteralPath $proofPath -Raw|ConvertFrom-Json
        if($proof.automatedStageValidated -ne $true -or $proof.head -cne $spec.head -or $proof.bindingsSha256 -cne $spec.bindingsSha256 -or $proof.cppTree -cne $spec.cppTree -or $null -ne $proof.error){throw 'Complete exact payload proof required.'}
        $proofSha=(Get-FileHash -LiteralPath $proofPath -Algorithm SHA256).Hash.ToLowerInvariant()
        $innerPath=Join-Path $spec.output 'INNER_RESULT.json'
        $inner=[ordered]@{passed=$true;error=$null;head=$spec.head;bindingsSha256=$spec.bindingsSha256;cppTree=$spec.cppTree;proofSha256=$proofSha;finishedUtc=[DateTime]::UtcNow.ToString('o');acceptanceEvidence=$false}
        Save-New $innerPath $inner
        # Source/executable reconciliation remains inside the owned whole controller.
        & $python -B $policy bindings $spec.bindings --sha256 $spec.bindingsSha256 --review $spec.review --head $spec.head
        if($LASTEXITCODE -ne 0){throw 'Final exact binding reconciliation failed.'}
        if((Get-FileHash -LiteralPath $proofPath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $proofSha){throw 'Payload proof changed during final reconciliation.'}
        $complete=[ordered]@{passed=$true;error=$null;head=$spec.head;bindingsSha256=$spec.bindingsSha256;cppTree=$spec.cppTree;proofSha256=$proofSha;innerOutcomeSha256=(Get-FileHash -LiteralPath $innerPath -Algorithm SHA256).Hash.ToLowerInvariant();completedQpc=[Diagnostics.Stopwatch]::GetTimestamp();acceptanceEvidence=$false}
        Save-New (Join-Path $spec.output 'CONTROLLER_COMPLETE.json') $complete
    }finally{
        if($owned){$mutex.ReleaseMutex()}
        $mutex.Dispose()
    }
    Check-Deadline
    # Native watchdog must still observe normal root exit and zero descendants.
}finally{
    if(-not [CommandCardOwnConsole]::FreeConsole()){throw ('Own-console release failed: '+[Runtime.InteropServices.Marshal]::GetLastWin32Error())}
}
