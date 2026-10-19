param([string]$RunnerPath=(Join-Path $PSScriptRoot 'test-input-bindings.ps1'))
$ErrorActionPreference='Stop'
# Extract only the pure policy functions; never execute runner launch/setup code.
$tokensFound=$null; $errorsFound=$null
$ast=[System.Management.Automation.Language.Parser]::ParseFile($RunnerPath,[ref]$tokensFound,[ref]$errorsFound)
if($errorsFound.Count) { throw 'Runner does not parse.' }
$wanted=@('Get-HigherGroupRequiredChecks','Get-HigherGroupSlotChecks','Assert-HigherGroupEvidence')
$functions=@($ast.FindAll({param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -in $wanted},$true))
if($functions.Count -ne 3) { throw 'Expected exactly three pure higher-group policy functions.' }
foreach($functionNode in $functions) { . ([scriptblock]::Create($functionNode.Extent.Text)) }
$checks=@(Get-HigherGroupRequiredChecks)
if($checks.Count -ne 11 -or @($checks | Select-Object -Unique).Count -ne 11) { throw 'Expected eleven distinct higher-group acceptance checks.' }
$valid=[pscustomobject]@{
    fixtureMode='higherGroups'; status='pass'; inputSource='engineInjection'; pointerSource='projectedSynthetic'
    physicalInputVerified=$false; osWindowFocusVerified=$false; positiveRealFocusPanVerified=$false
    positivePanFocusSource='notExercised'; simulatedLostFocus=$false; observedRealFocusSuppression=$false
    checks=($checks+@(Get-HigherGroupSlotChecks)); injectedEdges=240
    nativeCallbacks=[pscustomobject]@{left=54;groups=54;right=0;produce=0;build=0;wheelUp=0;wheelDown=0;rematch=0}
    finalInputState=[pscustomobject]@{controlDown=$false;leftControlDown=$false;rightControlDown=$false;digitsDown=@();higherDigitsDownMask=0;higherDigitsBoundMask=63}
}
$validLog=($valid.checks | ForEach-Object {'PROOF_INPUT_BINDING_CHECK stage=0 status=pass evidence='+$_+' inputSource=engineInjection'}) -join "`n"
$caseCount=0
function Test-PolicyCase([string]$Name,[scriptblock]$Mutate,[bool]$ShouldPass,[string]$Log=$validLog) {
    $caseEvidence=$valid|ConvertTo-Json -Depth 8|ConvertFrom-Json
    if($Mutate) { & $Mutate $caseEvidence }
    $accepted=$true
    try { Assert-HigherGroupEvidence $caseEvidence $Log } catch { $accepted=$false }
    if($accepted -ne $ShouldPass) { throw "Unexpected acceptance for policy case: $Name" }
    $script:caseCount++
}
Test-PolicyCase 'complete valid engine-injection evidence' {} $true
Test-PolicyCase 'reject full mode' {param($e) $e.fixtureMode='full'} $false
Test-PolicyCase 'reject physical-input claim' {param($e) $e.physicalInputVerified=$true} $false
Test-PolicyCase 'reject missing OS-focus classification' {param($e) $e.PSObject.Properties.Remove('osWindowFocusVerified')} $false
Test-PolicyCase 'reject virtual pan claim' {param($e) $e.positivePanFocusSource='virtualFixture'} $false
Test-PolicyCase 'reject simulated focus-loss claim' {param($e) $e.simulatedLostFocus=$true} $false
Test-PolicyCase 'reject missing overwrite-round check' {param($e) $e.checks=@($e.checks|Where-Object {$_ -ne 'higher_groups_round_5_complete'})} $false
Test-PolicyCase 'reject missing specific slot recall' {param($e) $e.checks=@($e.checks|Where-Object {$_ -ne 'higher_groups_round_3_slot_8_exact_recall'})} $false
Test-PolicyCase 'reject missing specific overwritten slot' {param($e) $e.checks=@($e.checks|Where-Object {$_ -ne 'higher_groups_round_6_slot_9_saved'})} $false
Test-PolicyCase 'reject success JSON without corresponding check log' {} $false ($validLog -replace 'evidence=higher_groups_round_6_complete','evidence=unrelated')
Test-PolicyCase 'reject wrong edge count' {param($e) $e.injectedEdges=239} $false
Test-PolicyCase 'reject wrong group callback count' {param($e) $e.nativeCallbacks.groups=53} $false
Test-PolicyCase 'reject extra gameplay callback' {param($e) $e.nativeCallbacks.produce=1} $false
Test-PolicyCase 'reject missing zero callback' {param($e) $e.nativeCallbacks.PSObject.Properties.Remove('rematch')} $false
Test-PolicyCase 'reject held right control' {param($e) $e.finalInputState.rightControlDown=$true} $false
Test-PolicyCase 'reject held digit' {param($e) $e.finalInputState.digitsDown=@('Nine')} $false
Test-PolicyCase 'reject held digit mask' {param($e) $e.finalInputState.higherDigitsDownMask=32} $false
Test-PolicyCase 'reject incomplete digit bindings' {param($e) $e.finalInputState.higherDigitsBoundMask=31} $false
[pscustomobject]@{passed=$true;caseCount=$caseCount;scope='Pure runner evidence acceptance; no engine launch or input dispatch tested'}|ConvertTo-Json -Compress
