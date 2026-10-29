function Get-ProofInteractiveViewport {
    param([ValidateSet('1280x720','960x540')][string]$Viewport = '1280x720')
    $name = $Viewport.ToLowerInvariant()
    $parts = $name.Split('x')
    [pscustomobject]@{
        Name = $name
        Width = [int]$parts[0]
        Height = [int]$parts[1]
        ResX = '-ResX=' + $parts[0]
        ResY = '-ResY=' + $parts[1]
    }
}
