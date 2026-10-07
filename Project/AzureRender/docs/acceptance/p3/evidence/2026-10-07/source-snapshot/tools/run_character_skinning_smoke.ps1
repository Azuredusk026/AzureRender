param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"
$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$standardOutput = Join-Path $OutputDirectory "stdout.log"
$standardError = Join-Path $OutputDirectory "stderr.log"
$process = Start-Process -FilePath $resolvedExecutable -ArgumentList @("--smoke-frames", "30", "--qa-morph-weights", "0.75", "0") -WindowStyle Minimized -Wait -PassThru -RedirectStandardOutput $standardOutput -RedirectStandardError $standardError
$output = (Get-Content -LiteralPath $standardOutput -Raw) + [Environment]::NewLine + (Get-Content -LiteralPath $standardError -Raw)

if ($process.ExitCode -ne 0) {
    if ($output -match "No GPU|No Vulkan|does not support.*Vulkan") {
        Write-Output "SKIP: no usable Vulkan GPU"
        exit 2
    }
    Write-Error "Character skinning smoke exited with $($process.ExitCode). Logs: $OutputDirectory"
    exit 1
}
if ($output -match "Character skinning and morph: vertex-shader fallback path") {
    Write-Output "SKIP: compute queues are unavailable"
    exit 2
}
if ($output -notmatch "Character skinning and morph: Vulkan compute path") {
    Write-Error "The renderer did not report the Compute skinning path. Logs: $OutputDirectory"
    exit 1
}
if (($output -notmatch "[1-9][0-9]* Morph targets") -or ($output -notmatch "Morph weights: 0.750000, 0.000000")) {
    Write-Error "The smoke scene did not exercise a nonzero Morph target. Logs: $OutputDirectory"
    exit 1
}
if ($output -notmatch "Skinning: enabled" -or $output -notmatch "Animations: 1") {
    Write-Error "The smoke scene did not exercise animated skin joints. Logs: $OutputDirectory"
    exit 1
}
if ($output -match "validation error|VUID-") {
    Write-Error "Vulkan validation reported an error. Logs: $OutputDirectory"
    exit 1
}
Write-Output "Character Compute skinning smoke passed."
