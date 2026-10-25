param([string]$OutputDirectory,[string]$CandidateSHA256,[switch]$OfflineWindowConfirmed,[switch]$Execute)
$ErrorActionPreference='Stop'
if(-not $Execute){@{preparedOnly=$true;liveSamplerInvoked=$false;requires='Reviewed exact candidate and a coordinated bounded offline-only test window'}|ConvertTo-Json;return}
if(-not $OfflineWindowConfirmed -or $CandidateSHA256 -cnotmatch '^[a-f0-9]{64}$'){throw 'Exact reviewed candidate and offline window required.'}
$candidate=Join-Path $PSScriptRoot 'check-admission-strict-cpu.ps1'
$baseline=Join-Path $PSScriptRoot 'fixtures\original-admission.ps1.txt'
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
if((Hash $baseline) -cne 'f891ebaf39f09a738578ce0da9d94fa47352b27e84d8ce0fb56d447e49c5c353' -or (Hash $candidate) -cne $CandidateSHA256){throw 'Fixture source hash mismatch.'}
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(-not $output.StartsWith('D:\UserTemp\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $output)){throw 'Fresh local D: fixture output required.'}
$parent=Get-Item -LiteralPath ([IO.Path]::GetDirectoryName($output))
if(($parent.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw 'Plain output parent required.'}
$me=[Diagnostics.Process]::GetCurrentProcess();$me.ProcessorAffinity=[IntPtr]68;$me.PriorityClass='Normal'
$clock=[Diagnostics.Stopwatch]::StartNew()
function Check-Bound{if($clock.Elapsed.TotalSeconds -ge 20){throw 'Offline fixture functional bound exceeded.'}}
function Cpu-Region([string]$Text){
    $tokens=$null;$parseErrors=$null
    $ast=[Management.Automation.Language.Parser]::ParseInput($Text,[ref]$tokens,[ref]$parseErrors)
    if($parseErrors.Count){throw 'Source parse errors.'}
    $loops=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.ForStatementAst] -and $node.Condition.Extent.Text.Trim() -ceq '$i -lt 3'},$true))
    if($loops.Count -ne 1 -or $loops[0].Body.Statements.Count -ne 6){throw 'Original three-sample loop shape required.'}
    $body=$loops[0].Body.Statements
    if($body[1] -isnot [Management.Automation.Language.AssignmentStatementAst] -or $body[1].Left.Extent.Text -cne '$values' -or $body[2] -isnot [Management.Automation.Language.ForEachStatementAst] -or $body[2].Variable.VariablePath.UserPath -cne 'entry' -or $body[2].Condition.Extent.Text -cne '$perf' -or $body[3] -isnot [Management.Automation.Language.IfStatementAst]){throw 'Exact CPU conversion boundary required.'}
    $start=$body[2].Extent.StartOffset;$end=$body[2].Extent.EndOffset
    $region=$body[1].Extent.Text+[Environment]::NewLine+$body[2].Extent.Text+[Environment]::NewLine+$body[3].Extent.Text+[Environment]::NewLine+'$values'
    $regionTokens=$null;$regionErrors=$null
    $regionAst=[Management.Automation.Language.Parser]::ParseInput($region,[ref]$regionTokens,[ref]$regionErrors)
    if($regionErrors.Count -or @($regionAst.FindAll({param($node) $node -is [Management.Automation.Language.CommandAst]},$true)).Count){throw 'CPU fixture region may contain no commands.'}
    $allowedVariables=@('values','entry','perf','rawValue','isNumeric','cpuValue','null','true','false')
    foreach($variable in @($regionAst.FindAll({param($node) $node -is [Management.Automation.Language.VariableExpressionAst]},$true))){
        if($variable.VariablePath.UserPath -cnotin $allowedVariables){throw 'Unexpected fixture-region variable.'}
    }
    foreach($call in @($regionAst.FindAll({param($node) $node -is [Management.Automation.Language.InvokeMemberExpressionAst]},$true))){
        $member=$call.Member.Value
        if($member -ceq 'ContainsKey' -and $call.Expression.Extent.Text -ceq '$values'){continue}
        if($member -cin @('IsNaN','IsInfinity') -and $call.Expression -is [Management.Automation.Language.TypeExpressionAst] -and $call.Expression.TypeName.FullName -cin @('double','System.Double')){continue}
        throw 'Unexpected fixture-region method call.'
    }
    [pscustomobject]@{Prefix=$Text.Substring(0,$start);Suffix=$Text.Substring($end);Code=[scriptblock]::Create($region)}
}
function Preserved($Original,$Changed){
    if($Original.Prefix -cne $Changed.Prefix -or $Original.Suffix -cne $Changed.Suffix){throw 'Source outside CPU conversion changed.'}
}
function Entries([string]$Name,[object]$Value,[switch]$Missing){
    $rows=@()
    foreach($key in @('_Total','2','6')){
        if($key -ceq $Name){
            if($Missing){$rows+=[pscustomobject]@{Name=$key}}
            else{$rows+=[pscustomobject]@{Name=$key;PercentProcessorTime=$Value}}
        }else{$rows+=[pscustomobject]@{Name=$key;PercentProcessorTime=[uint64]12}}
    }
    ,$rows
}
function Observe($Code,[object[]]$Rows){
    try{
        $returned=@(& {param($Fragment,$SyntheticRows) Set-StrictMode -Off;$perf=$SyntheticRows;& $Fragment} $Code $Rows)
        if($returned.Count -ne 1 -or $returned[0] -isnot [Collections.IDictionary]){throw 'One CPU dictionary required.'}
        $values=$returned[0];$over=$false
        foreach($key in @('_Total','2','6')){if($values[$key] -ge 80){$over=$true}}
        [pscustomobject]@{accepted=$true;values=$values;cpuEligible=(-not $over);error=$null}
    }catch{[pscustomobject]@{accepted=$false;values=$null;cpuEligible=$false;error=$_.Exception.Message}}
}
[IO.Directory]::CreateDirectory($output)|Out-Null
$results=[Collections.Generic.List[object]]::new();$structural=[Collections.Generic.List[object]]::new();$fatal=$null;$passed=$false
try{
    $originalText=[IO.File]::ReadAllText($baseline,[Text.UTF8Encoding]::new($false,$true))
    $candidateText=[IO.File]::ReadAllText($candidate,[Text.UTF8Encoding]::new($false,$true))
    $originalRegion=Cpu-Region $originalText;$candidateRegion=Cpu-Region $candidateText
    Preserved $originalRegion $candidateRegion
    $cases=[Collections.Generic.List[object]]::new()
    $typed=@([byte]25,[sbyte]25,[int16]25,[uint16]25,[int32]25,[uint32]25,[int64]25,[uint64]25,[single]25,[double]25,[decimal]25)
    foreach($value in $typed){$cases.Add(@{name=('valid-'+$value.GetType().Name);rows=(Entries '2' $value);valid=$true;eligible=$true;baselineWeakness=$false})}
    foreach($value in @([double]0,[double]79.9999,[double]80,[double]100)){$cases.Add(@{name=('boundary-'+$value.ToString([Globalization.CultureInfo]::InvariantCulture));rows=(Entries '2' $value);valid=$true;eligible=($value -lt 80);baselineWeakness=$false})}
    $near80=[decimal]::Parse('79.99999999999999999999999999',[Globalization.CultureInfo]::InvariantCulture)
    $cases.Add(@{name='decimal-below80-rounds-to80';rows=(Entries '2' $near80);valid=$true;eligible=$false;baselineWeakness=$false})
    $bad=@(
        @{name='null';value=$null},@{name='numeric-string';value='0'},@{name='fraction-string';value='79.5'},
        @{name='empty-string';value=''},@{name='false';value=$false},@{name='true';value=$true},
        @{name='nan';value=[double]::NaN},@{name='positive-infinity';value=[double]::PositiveInfinity},
        @{name='negative-infinity';value=[double]::NegativeInfinity},@{name='negative';value=[double]-1},
        @{name='above100';value=[double]100.0001},@{name='array';value=@(0)},
        @{name='object';value=[pscustomobject]@{Value=0}},@{name='char';value=[char]'0'},
        @{name='enum';value=[DayOfWeek]::Tuesday},
        @{name='decimal-above100-rounds-to100';value=[decimal]::Parse('100.00000000000000000000000001',[Globalization.CultureInfo]::InvariantCulture)}
    )
    foreach($key in @('_Total','2','6')){
        $cases.Add(@{name=('missing-value-'+$key);rows=(Entries $key $null -Missing);valid=$false;eligible=$false;baselineWeakness=$true})
        foreach($item in $bad){$cases.Add(@{name=($item.name+'-'+$key);rows=(Entries $key $item.value);valid=$false;eligible=$false;baselineWeakness=($item.name -ceq 'null')})}
        $complete=Entries '2' ([uint64]12)
        $cases.Add(@{name=('missing-instance-'+$key);rows=@($complete|Where-Object Name -CNE $key);valid=$false;eligible=$false;baselineWeakness=$false})
        $cases.Add(@{name=('duplicate-instance-'+$key);rows=@($complete)+@([pscustomobject]@{Name=$key;PercentProcessorTime=[uint64]0});valid=$false;eligible=$false;baselineWeakness=$false})
    }
    $unknown=Entries '2' ([uint64]12)
    $cases.Add(@{name='unknown-instance';rows=@($unknown)+@([pscustomobject]@{Name='7';PercentProcessorTime=[uint64]0});valid=$false;eligible=$false;baselineWeakness=$false})
    $caseVariant=Entries '2' ([uint64]12);$caseVariant[0].Name='_total'
    $cases.Add(@{name='provider-name-case';rows=$caseVariant;valid=$true;eligible=$true;baselineWeakness=$false})
    foreach($case in $cases){
        Check-Bound
        $before=Observe $originalRegion.Code $case.rows;$after=Observe $candidateRegion.Code $case.rows
        $ok=($after.accepted -eq $case.valid)
        if($case.valid){
            $ok=$ok -and $before.accepted -and $after.cpuEligible -eq $case.eligible
            foreach($key in @('_Total','2','6')){$ok=$ok -and $before.values[$key] -ceq $after.values[$key]}
        }
        if($case.baselineWeakness){$ok=$ok -and $before.accepted -and $before.cpuEligible -and -not $after.accepted}
        $results.Add([ordered]@{name=$case.name;passed=[bool]$ok;expectedValid=$case.valid;expectedCpuEligible=$case.eligible;baselineWeaknessRequired=$case.baselineWeakness;before=$before;after=$after})
    }
    foreach($mutation in @(
        @{name='threshold';old='-ge 80';new='-ge 81'},@{name='sample-count';old='$i -lt 3';new='$i -lt 4'},
        @{name='sleep';old='Start-Sleep -Seconds 1';new='Start-Sleep -Seconds 2'},@{name='mask';old="mask='0x44'";new="mask='0x88'"}
    )){
        Check-Bound
        if(-not $candidateText.Contains($mutation.old)){throw 'Structural negative-control target missing.'}
        $rejected=$false
        try{Preserved $originalRegion (Cpu-Region ($candidateText.Replace($mutation.old,$mutation.new)))}catch{$rejected=$true}
        $structural.Add([ordered]@{name=$mutation.name;rejected=$rejected})
    }
    $passed=@($results|Where-Object {-not $_.passed}).Count -eq 0 -and @($structural|Where-Object {-not $_.rejected}).Count -eq 0
    Check-Bound
}catch{$fatal=$_.Exception.Message;$passed=$false}
$report=[ordered]@{scope='offline actual CPU conversion regions only';baselineSha256=Hash $baseline;candidateSha256=Hash $candidate;testsExecuted=$true;liveSamplerInvoked=$false;cimQueries=0;gitInvocations=0;sleepCalls=0;runtimeAcceptance=$false;sourcePreservationPassed=($null -eq $fatal);elapsedSeconds=$clock.Elapsed.TotalSeconds;functionalBoundSeconds=20;externalOwnerAndCleanupStillRequired=$true;fixtureCaseCount=$results.Count;structuralControlCount=$structural.Count;cases=$results.ToArray();structuralControls=$structural.ToArray();fatalError=$fatal;passed=$passed}
$bytes=[Text.UTF8Encoding]::new($false).GetBytes(($report|ConvertTo-Json -Depth 12))
$stream=[IO.FileStream]::new((Join-Path $output 'FIXTURE_RESULT.json'),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
@{passed=$passed;fixtureCases=$results.Count;structuralControls=$structural.Count;fatalError=$fatal;report=(Join-Path $output 'FIXTURE_RESULT.json');liveSamplerInvoked=$false}|ConvertTo-Json -Compress
if(-not $passed){exit 1}
