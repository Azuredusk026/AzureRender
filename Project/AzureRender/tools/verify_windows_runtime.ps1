param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = "Stop"
$executablePath = (Resolve-Path -LiteralPath $Executable).Path
$originalPath = $env:PATH
try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    & $executablePath --version
    if ($LASTEXITCODE -ne 0) {
        throw "Installed executable failed --version with exit code $LASTEXITCODE"
    }
    & $executablePath --check-resources
    if ($LASTEXITCODE -ne 0) {
        throw "Installed executable failed --check-resources with exit code $LASTEXITCODE"
    }
} finally {
    $env:PATH = $originalPath
}

Write-Output "Isolated Windows runtime check passed: $executablePath"
