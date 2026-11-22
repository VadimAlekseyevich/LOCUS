# Run after a Windows Release build. Leave the source tree clean even
# when an incremental regression check fails.
param([string]$BuildDir = "build/windows")

$ErrorActionPreference = "Stop"
$source = (Resolve-Path -LiteralPath "assets/ui/rml/editor.rcss").Path
$release = Join-Path $BuildDir "Release"
$destination = Join-Path $release "assets/ui/rml/editor.rcss"
$probeSource = Join-Path "assets/ui/rml" "ci-resource-probe.rcss"
$probeDestination = Join-Path $release "assets/ui/rml/ci-resource-probe.rcss"
$executable = (Resolve-Path -LiteralPath (Join-Path $release "locus.exe")).Path
$executableTimestamp = (Get-Item -LiteralPath $executable).LastWriteTimeUtc
$originalBytes = [System.IO.File]::ReadAllBytes($source)

function Invoke-IncrementalBuild {
    & cmake --build $BuildDir --config Release --parallel 2
    if ($LASTEXITCODE -ne 0) {
        throw "Incremental Windows build failed ($LASTEXITCODE)"
    }
}

try {
    [System.IO.File]::AppendAllText($source, "`n/* CI UI asset sync probe */`n")
    Invoke-IncrementalBuild
    if ((Get-FileHash -LiteralPath $source).Hash -ne
        (Get-FileHash -LiteralPath $destination).Hash) {
        throw "Editing editor.rcss did not update the staged file"
    }

    [System.IO.File]::WriteAllText($probeSource, "/* new runtime asset */`n")
    Invoke-IncrementalBuild
    if (-not (Test-Path -LiteralPath $probeDestination -PathType Leaf) -or
        (Get-FileHash -LiteralPath $probeSource).Hash -ne
        (Get-FileHash -LiteralPath $probeDestination).Hash) {
        throw "Added runtime asset did not reach the executable directory"
    }

    Remove-Item -LiteralPath $probeSource
    Invoke-IncrementalBuild
    if (Test-Path -LiteralPath $probeDestination) {
        throw "Deleted runtime asset remained in the executable directory"
    }
}
finally {
    [System.IO.File]::WriteAllBytes($source, $originalBytes)
    if (Test-Path -LiteralPath $probeSource) {
        Remove-Item -LiteralPath $probeSource
    }
    Invoke-IncrementalBuild
}

if ((Get-Item -LiteralPath $executable).LastWriteTimeUtc -ne
    $executableTimestamp) {
    throw "UI-only edits unnecessarily relinked locus.exe"
}
Write-Output "UI_ASSET_INCREMENTAL_SYNC_PASS"
