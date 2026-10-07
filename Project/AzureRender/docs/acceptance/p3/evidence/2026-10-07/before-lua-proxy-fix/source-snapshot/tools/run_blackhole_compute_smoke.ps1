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

$process = Start-Process -FilePath $resolvedExecutable -ArgumentList @("--scene-type", "blackhole", "--smoke-frames", "30") -WindowStyle Minimized -Wait -PassThru -RedirectStandardOutput $standardOutput -RedirectStandardError $standardError

$output = (Get-Content -LiteralPath $standardOutput -Raw) + [Environment]::NewLine + (Get-Content -LiteralPath $standardError -Raw)
if ($process.ExitCode -ne 0) {
    if ($output -match "No GPU|No Vulkan|does not support.*Vulkan") {
        Write-Output "SKIP: no usable Vulkan GPU"
        exit 2
    }
    Write-Error "Blackhole compute smoke exited with $($process.ExitCode). Logs: $OutputDirectory"
    exit 1
}

if ($output -match "Blackhole bloom: fragment fallback path") {
    Write-Output "SKIP: RGBA16F storage images are unavailable"
    exit 2
}
if ($output -notmatch "Blackhole bloom: four-level Vulkan compute path") {
    Write-Error "The renderer did not report the Vulkan compute bloom path. Logs: $OutputDirectory"
    exit 1
}
if ($output -match "validation error|VUID-") {
    Write-Error "Vulkan validation reported an error. Logs: $OutputDirectory"
    exit 1
}

Write-Output "Blackhole four-level compute smoke passed."
