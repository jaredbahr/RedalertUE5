function Get-CameraEdgeCaptureEvidence([string]$Log) {
    $fields=@('beforeX','beforeY','afterX','afterY','expectedX','expectedY','errorPixels','deltaX','deltaY','deltaZ')
    $pattern='PROOF_CAMERA_EDGE_CAPTURE unit=(?<unit>[1-9]\d*)'
    foreach($field in $fields) { $pattern+=' '+$field+'=(?<'+$field+'>-?\d+(?:\.\d+)?)' }
    $pattern+=' views=(?<views>[1-9]\d*) digestBefore=(?<before>[A-Fa-f0-9]{32}) digestAfter=(?<after>[A-Fa-f0-9]{32}) rotationUnchanged=1 zoomUnchanged=1 viewsUnchanged=1 input=synthetic focus=synthetic physicalInput=unverified'
    $markers=[regex]::Matches($Log,$pattern)
    if($markers.Count -ne 1 -or [regex]::Matches($Log,'PROOF_CAMERA_EDGE_CAPTURE').Count -ne 1) { throw 'Need exactly one valid camera edge projection observation.' }
    $complete='PROOF_CAMERA_EDGE_COMPLETE status=pass captures=2 input=synthetic focus=synthetic physicalInput=unverified'
    if([regex]::Matches($Log,[regex]::Escape($complete)).Count -ne 1 -or [regex]::Matches($Log,'PROOF_CAMERA_EDGE_COMPLETE').Count -ne 1) { throw 'Need exactly one successful camera edge completion.' }
    $marker=$markers[0]
    $record=[ordered]@{unit=[int]$marker.Groups['unit'].Value;views=[int]$marker.Groups['views'].Value;input='synthetic';focus='synthetic';physicalInputVerified=$false}
    foreach($field in $fields) {
        $number=[double]::Parse($marker.Groups[$field].Value,[Globalization.CultureInfo]::InvariantCulture)
        if(-not [double]::IsFinite($number)) { throw 'Nonfinite camera edge observation.' }
        $record[$field]=$number
    }
    $record.digestBefore=$marker.Groups['before'].Value; $record.digestAfter=$marker.Groups['after'].Value
    $error=[math]::Sqrt([math]::Pow($record.afterX-$record.expectedX,2)+[math]::Pow($record.afterY-$record.expectedY,2))
    if($record.digestBefore -ne $record.digestAfter -or $record.afterX -ge $record.beforeX-10 -or [math]::Abs($record.afterY-$record.beforeY) -gt 2 -or
        $error -gt 2 -or $record.errorPixels -lt 0 -or [math]::Abs($error-$record.errorPixels) -gt .0001 -or
        [math]::Abs($record.deltaX+600) -gt .001 -or [math]::Abs($record.deltaY) -gt .001 -or [math]::Abs($record.deltaZ) -gt .001) {
        throw 'Camera edge projection, rig delta or frozen snapshot observation failed.'
    }
    [pscustomobject]$record
}
