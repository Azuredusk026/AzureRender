param()

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$manifestPath = Join-Path $projectRoot "portfolio/portfolio_manifest.json"
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json

if ($manifest.format -ne "AzureRender portfolio evidence v2") {
    throw "Unsupported portfolio manifest format: $($manifest.format)"
}
if (-not $manifest.publicAssetsOnly) {
    throw "Portfolio manifest must be public-assets-only."
}

foreach ($artifact in $manifest.artifacts) {
    $relativePath = [string]$artifact.path
    if ($relativePath -match '(^|/)(assets_private|captures)(/|$)') {
        throw "Forbidden portfolio path: $relativePath"
    }
    $absolutePath = [System.IO.Path]::GetFullPath(
        (Join-Path $projectRoot $relativePath))
    if (-not $absolutePath.StartsWith(
            $projectRoot + [System.IO.Path]::DirectorySeparatorChar,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Portfolio path escapes project root: $relativePath"
    }
    $file = Get-Item -LiteralPath $absolutePath
    if ($file.Length -ne [long]$artifact.bytes) {
        throw "Size mismatch: $relativePath"
    }
    $actualHash = (Get-FileHash -LiteralPath $absolutePath -Algorithm SHA256).Hash
    if ($actualHash -ne [string]$artifact.sha256) {
        throw "SHA-256 mismatch: $relativePath"
    }
}

foreach ($evidencePath in $manifest.evidence) {
    $absolutePath = [System.IO.Path]::GetFullPath(
        (Join-Path $projectRoot ([string]$evidencePath)))
    if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
        throw "Missing evidence file: $evidencePath"
    }
    Get-Content -LiteralPath $absolutePath -Raw | ConvertFrom-Json | Out-Null
}

Write-Output (
    "Portfolio verified: {0} public artifacts, {1} evidence files" -f `
        $manifest.artifacts.Count, $manifest.evidence.Count)
