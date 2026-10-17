# Pure evidence parser. Engine injection is never evidence of hardware or OS focus.
function Get-StopInputBindingEvidence {
    param([Parameter(Mandatory)][string]$Log,[Parameter(Mandatory)]$StopEvidence)
    function Require($Condition,[string]$Reason){if(-not $Condition){throw "Stop input evidence: $Reason"}}
    Require ($Log.Length -le 16MB -and $Log -notmatch 'PROOF_STOP_INPUT_ERROR') 'oversized log or runtime error'
    Require ($StopEvidence.passed -is [bool] -and $StopEvidence.passed -and $StopEvidence.expectedNativeStopDelta -eq 1 -and ($StopEvidence.nativeStopCounts -join ',') -ceq '0,1,1') 'validated native Stop evidence required'
    Require ($StopEvidence.owner -eq 1 -and $StopEvidence.unit -gt 0 -and $StopEvidence.epoch -eq 0 -and ($StopEvidence.sequences -join ',') -ceq '5,6,7' -and $StopEvidence.stationarySimulationSeconds -gt 0 -and $StopEvidence.stationaryCommonRevisions.Count -gt 1) 'invalid Stop evidence identity/window'
    $fields=@('phase','owner','unit','epoch','frame','processedFrame','device','user','edges','nativeStop','selected','selectionCount','sequence','keyDown','handled','ready','key','inputSource','pointerSource','physicalInputVerified','osWindowFocusVerified')
    $rows=@()
    foreach($match in [regex]::Matches($Log,'PROOF_STOP_INPUT([^\r\n]*)')){
        Require ($rows.Count -lt 4) 'extra input row'
        $text=$match.Groups[1].Value.Trim()
        Require ($text.Length -gt 0 -and $text.Length -le 4096 -and $text -notmatch '[^\x00-\x7F]') 'empty, oversized or non-ASCII row'
        $document=$null
        try{
            $document=[System.Text.Json.JsonDocument]::Parse($text)
            $root=$document.RootElement
            Require ($root.ValueKind -eq [System.Text.Json.JsonValueKind]::Object) 'JSON object required'
            $keys=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
            foreach($property in $root.EnumerateObject()){
                Require ($keys.Add($property.Name) -and $property.Name -cin $fields) 'duplicate or unknown JSON key'
            }
            Require ($keys.Count -eq $fields.Count) 'missing JSON key'
            foreach($field in @('frame','processedFrame')){
                $number=[uint64]0
                Require ($root.GetProperty($field).ValueKind -eq [System.Text.Json.JsonValueKind]::Number -and $root.GetProperty($field).TryGetUInt64([ref]$number) -and $number -gt 0) "invalid uint64 $field"
            }
            foreach($field in @('owner','unit','epoch','device','user','edges','nativeStop','selected','selectionCount','sequence','keyDown','handled','ready')){
                $number=[int]0
                Require ($root.GetProperty($field).ValueKind -eq [System.Text.Json.JsonValueKind]::Number -and $root.GetProperty($field).TryGetInt32([ref]$number)) "invalid integer $field"
            }
            foreach($field in @('physicalInputVerified','osWindowFocusVerified')){
                Require ($root.GetProperty($field).ValueKind -eq [System.Text.Json.JsonValueKind]::False) 'false physical/focus claim required'
            }
            foreach($field in @('phase','key','inputSource','pointerSource')){
                Require ($root.GetProperty($field).ValueKind -eq [System.Text.Json.JsonValueKind]::String) "invalid string $field"
            }
            $row=$text|ConvertFrom-Json -Depth 5 -ErrorAction Stop
            $row.frame=$root.GetProperty('frame').GetUInt64();$row.processedFrame=$root.GetProperty('processedFrame').GetUInt64()
        }finally{if($document){$document.Dispose()}}
        Require ($row.owner -eq 1 -and $row.unit -gt 0 -and $row.epoch -eq 0 -and $row.device -ge 0 -and $row.user -ge 0 -and $row.selected -eq $row.unit -and $row.selectionCount -eq 1) 'invalid participant/selection'
        Require ($row.processedFrame -le $row.frame -and ($row.frame-$row.processedFrame) -le [uint64]1 -and $row.keyDown -in @(0,1) -and $row.handled -in @(0,1) -and $row.ready -eq 1) 'invalid or stale frame/key/readiness'
        Require ($row.key -ceq 'S' -and $row.inputSource -ceq 'engineInjection' -and $row.pointerSource -ceq 'projectedSynthetic') 'incorrect synthetic labels'
        $rows+=$row
    }
    Require ($rows.Count -eq 4 -and ($rows.phase -join ',') -ceq 'press,callback,release,released') 'missing/reordered input phases'
    $press,$callback,$release,$released=$rows
    for($index=1;$index -lt $rows.Count;$index++){
        Require ($rows[$index].processedFrame -ge $rows[$index-1].processedFrame) 'processed input frame regressed'
    }
    Require (($rows.edges -join ',') -ceq '1,1,2,2' -and ($rows.nativeStop -join ',') -ceq '0,1,1,1' -and ($rows.sequence -join ',') -ceq '5,6,6,6') 'edge/callback/sequence mismatch'
    foreach($row in $rows){
        Require ($row.owner -eq $press.owner -and $row.unit -eq $press.unit -and $row.epoch -eq $press.epoch -and $row.device -eq $press.device -and $row.user -eq $press.user) 'input identity changed'
        Require ($row.owner -eq $StopEvidence.owner -and $row.unit -eq $StopEvidence.unit -and $row.epoch -eq $StopEvidence.epoch) 'input/Stop identity mismatch'
    }
    Require ($callback.frame -gt $press.frame -and $callback.processedFrame -ge $press.frame -and $callback.keyDown -eq 1) 'callback must follow processed press'
    Require ($release.frame -eq $callback.frame -and $release.processedFrame -eq $callback.processedFrame -and $released.frame -gt $release.frame -and $released.processedFrame -gt $release.frame -and $released.keyDown -eq 0) 'release must finish a later input frame'
    return [ordered]@{passed=$true;inputSource='engineInjection';pointerSource='projectedSynthetic';physicalInputVerified=$false;osWindowFocusVerified=$false;key='S';owner=$press.owner;unit=$press.unit;epoch=$press.epoch;device=$press.device;user=$press.user;injectedEdges=2;nativeStopCallbacks=1;pressFrame=$press.frame;callbackFrame=$callback.frame;releaseFrame=$release.frame;releasedFrame=$released.frame;releasedProcessedFrame=$released.processedFrame;sequences=@(5,6,7);stationarySimulationSeconds=$StopEvidence.stationarySimulationSeconds}
}
