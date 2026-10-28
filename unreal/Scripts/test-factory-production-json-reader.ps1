# Exercise the actual nested reader without running UE or generating policy fixtures.
param([string]$SourcePath=(Join-Path $PSScriptRoot 'FactoryProductionEvidence.ps1'))
$ErrorActionPreference='Stop'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($SourcePath,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Production reader source does not parse'}
$readers=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Row'},$true))
$requires=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Require'},$true))
if($readers.Count -ne 1 -or $requires.Count -ne 1){throw 'Expected the actual production Row and Require definitions'}
. ([scriptblock]::Create($requires[0].Extent.Text))
. ([scriptblock]::Create($readers[0].Extent.Text))
$checks=0
function Accept([string]$Json){
    $null=Row $Json
    $script:checks++
}
function Reject([string]$Json,[string]$Reason=''){
    $message=$null
    try{$null=Row $Json}catch{$message=$_.Exception.Message}
    if($null -eq $message -or ($Reason -and $message -cne ('Factory production evidence: '+$Reason))){throw "Unexpected reader acceptance/error: $Json ($message)"}
    $script:checks++
}
foreach($json in @('{}','{"x":1}','{"x":[]}','{"items":[{"id":1},{"id":2}]}',
    '{"a":{"type":1},"b":{"type":2}}','{"a":{"\u0074ype":1},"b":{"type":2}}',
    '{"type":1,"a":{"type":2}}','{"a:b":1,"a{b}":2,"a\"b":3}',
    '{"owner":1, "other":2}','{"type":1,"a":{"TYPE":2}}')){
    Accept $json
}
foreach($json in @('{"type":1,"type":2}','{"type":1,"\u0074ype":2}',
    '{"\u0074ype":1,"type":2}','{"type":1,"TYPE":2}',
    '{"a":{"type":1,"type":2}}','{"items":[{"id":1,"id":2}]}',
    '{"a":{"x":1},"a":{"x":2}}','{"type":1,"type" :2}')){
    Reject $json 'duplicate key'
}
foreach($json in @('[]','1','true','"text"','null')){Reject $json 'object required'}
Reject '{"x":1' 'unbalanced JSON'
Reject '}}' 'unbalanced JSON'
Reject '{"\q":1}'
Reject '{"x":1} trailing'
try{$null=Row '{"x":1}' 4;throw 'Expected oversized-row rejection'}catch{
    if($_.Exception.Message -cne 'Factory production evidence: oversized row'){throw}
    $checks++
}
# Each invocation has independent object/duplicate state, even with cache hits.
for($i=0;$i -lt 3;$i++){Accept '{"type":1}';Reject '{"type":1,"type":2}' 'duplicate key'}
Write-Output "PASS factory production JSON reader checks=$checks; synthetic reader controls only"
