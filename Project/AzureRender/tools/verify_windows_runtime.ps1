param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = "Stop"
$executablePath = (Resolve-Path -LiteralPath $Executable).Path
$originalPath = $env:PATH
try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    foreach ($argument in @('--version', '--check-resources')) {
        $process = Start-Process -FilePath $executablePath -ArgumentList $argument -NoNewWindow -Wait -PassThru
        if ($process.ExitCode -ne 0) {
            throw "Installed executable failed $argument with exit code $($process.ExitCode)"
        }
    }
} finally {
    $env:PATH = $originalPath
}

Write-Output "Isolated Windows runtime check passed: $executablePath"
