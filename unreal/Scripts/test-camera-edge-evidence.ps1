$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CameraEdgeEvidence.ps1')
$line='PROOF_CAMERA_EDGE_CAPTURE unit=4 beforeX=384.000000 beforeY=260.000000 afterX=230.400000 afterY=260.000000 expectedX=230.400000 expectedY=260.000000 errorPixels=0.000000 deltaX=-600.000000 deltaY=0.000000 deltaZ=0.000000 views=12 digestBefore=0123456789abcdef0123456789abcdef digestAfter=0123456789abcdef0123456789abcdef rotationUnchanged=1 zoomUnchanged=1 viewsUnchanged=1 input=synthetic focus=synthetic physicalInput=unverified'
$complete='PROOF_CAMERA_EDGE_COMPLETE status=pass captures=2 input=synthetic focus=synthetic physicalInput=unverified'
$valid=$line+[Environment]::NewLine+$complete
$null=Get-CameraEdgeCaptureEvidence $valid
$cases=@(
    $valid.Replace('afterX=230.400000','afterX=380.000000'),
    $valid.Replace('digestAfter=0123456789abcdef0123456789abcdef','digestAfter=1123456789abcdef0123456789abcdef'),
    $valid.Replace('deltaX=-600.000000','deltaX=-60.000000'),
    $valid.Replace('deltaY=0.000000','deltaY=2.000000'),
    $valid.Replace('afterY=260.000000','afterY=270.000000'),
    $valid.Replace('viewsUnchanged=1','viewsUnchanged=0'),
    $valid.Replace('input=synthetic','input=physical'),
    $valid.Replace('focus=synthetic','focus=native'),
    $valid.Replace('views=12','views=0'),
    $valid.Replace('afterX=230.400000','afterX=NaN'),
    ($valid+[Environment]::NewLine+$line),
    ($valid+[Environment]::NewLine+$complete),
    $line,
    $valid.Replace('errorPixels=0.000000','errorPixels=1.000000'),
    $valid.Replace('digestAfter=0123456789abcdef0123456789abcdef','digestAfter=bad'),
    $valid.Replace('digestAfter=0123456789abcdef0123456789abcdef','digestAfter=0123456789abcdef0123456789abcdeg'),
    $valid.Replace(' digestAfter=0123456789abcdef0123456789abcdef',''),
    $valid.Replace('deltaX=-600.000000','deltaX=-600.010000')
)
foreach($case in $cases) {
    $rejected=$false
    try { $null=Get-CameraEdgeCaptureEvidence $case } catch { $rejected=$true }
    if(-not $rejected) { throw 'Adversarial camera evidence was accepted.' }
}
Write-Output ('Camera evidence parser: one positive plus '+$cases.Count+' negative cases passed; synthetic input only.')
