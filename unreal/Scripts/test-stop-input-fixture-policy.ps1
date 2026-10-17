# Lightweight fixture guards; no engine, OS input, process mutation or build.
$ErrorActionPreference='Stop'
$checks=0
$runner=Join-Path $PSScriptRoot 'test-controls-proof.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($runner,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Controls runner parse failed'};$checks++
foreach($file in @('StopInputBindingEvidence.ps1','test-stop-input-binding-policy.ps1')){
 $null=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $file),[ref]$tokens,[ref]$errors)
 if($errors.Count){throw ('Policy parse failed '+$file)};$checks++
}
foreach($argsMap in @(@{StopInputBinding=$true},@{StopControls=$true;StopInputBinding=$true;TypeSelection=$true})){
 $rejected=$false
 try { & $runner @argsMap | Out-Null } catch {$rejected=$_.Exception.Message -match 'StopInputBinding requires|StopControls and TypeSelection cannot'}
 if(-not $rejected){throw 'Invalid mode did not fail at the preflight flag guard'};$checks++
}
foreach($entry in @(@('TimeoutSeconds','90'),@('WarmupTimeoutSeconds','300'))){
 $p=$ast.ParamBlock.Parameters | Where-Object {$_.Name.VariablePath.UserPath -eq $entry[0]}
 if(-not $p -or $p.DefaultValue.Extent.Text -ne $entry[1]){throw ('Changed deadline '+$entry[0])};$checks++
}
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$source=[IO.File]::ReadAllText((Join-Path $root 'unreal/Source/RedalertProof/Presentation/ProofStopInputBinding.cpp'))
if($source -match '\b(?:StopSelected|OnManualStop|SetMouseLocation|SetCursorPos|SendInput|SetForegroundWindow|SetInputMode|PushInputComponent)\s*\('){throw 'Injection fixture contains a shared-handler/focus/input shortcut'};$checks++
if($source -notmatch 'Viewport->InputKey\(FInputKeyEventArgs\(' -or $source -notmatch 'EKeys::S, bPressed \? IE_Pressed : IE_Released, FPlatformTime::Cycles64\(\)'){throw 'Missing timestamped viewport S-edge route'};$checks++
if($source -notmatch 'GetStopInputProcessedFrame\(InputComponent.Get\(\)\)' -or $source -notmatch 'Binding.Chord.Key == EKeys::S.*Binding.KeyEvent == IE_Pressed.*Binding.KeyDelegate.IsBound\(\)'){throw 'Missing normal-stack/bound S readiness'};$checks++
if($source -notmatch 'StopInputEdges == \(bPressed \? 0 : 1\)' -or $source -notmatch 'StopInputStage == 1' -or $source -notmatch 'StopInputStage == 2'){throw 'Missing single press/release stage guards'};$checks++
$pawn=[IO.File]::ReadAllText((Join-Path $root 'unreal/Source/RedalertProof/Presentation/ProofRTSPawn.cpp'))
if($pawn -notmatch 'ControlsElapsed > 55.f' -or $pawn -notmatch 'if \(bStopInputBinding && !TickStopInputBinding\(\)\) return;'){throw 'Fixture deadline or input completion gate missing'};$checks++
$controller=[IO.File]::ReadAllText((Join-Path $root 'unreal/Source/RedalertProof/Runtime/ProofPlayerController.cpp'))
if($controller -notmatch 'Super::BuildInputStack\(InputStack\);' -or $controller -notmatch 'Super::ProcessPlayerInput\(DeltaTime, bGamePaused\);'){throw 'Normal input processing bypassed'};$checks++
# Launcher names are repository-defined; inspect only the rendered-only flag gate.
if($ast.Extent.Text -notmatch 'if\(\$StopInputBinding -and \$Role -eq ''rendered''\) \{ \$launchArguments\+=''-ProofStopInputBinding'' \}'){throw 'Native flag not restricted to rendered role'};$checks++
Write-Output "PASS Stop input fixture source/guard checks=$checks; engine build/runtime remain unverified"
