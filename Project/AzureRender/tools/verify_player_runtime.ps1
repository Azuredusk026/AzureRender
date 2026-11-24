param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$playerPath = (Resolve-Path -LiteralPath $Executable).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$evidencePath = (Resolve-Path -LiteralPath $OutputDirectory).Path
$projectPath = Join-Path $evidencePath 'project'
$movedPath = Join-Path $evidencePath 'project-moved'
$originalPath = $env:PATH
$originalLocation = Get-Location
try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    Set-Location -LiteralPath $evidencePath
    & $playerPath --create-project $projectPath
    if ($LASTEXITCODE -ne 0) { throw 'Player project creation failed' }
    Move-Item -LiteralPath $projectPath -Destination $movedPath
    $projectFile = Join-Path $movedPath 'project.azureproject'
    & $playerPath --project $projectFile --check-project
    if ($LASTEXITCODE -ne 0) { throw 'Player moved project validation failed' }
    foreach ($scene in @('sample', 'character', 'blackhole')) {
        $reportPath = Join-Path $evidencePath "$scene.json"
        $stdoutPath = Join-Path $evidencePath "$scene.stdout.log"
        $stderrPath = Join-Path $evidencePath "$scene.stderr.log"
        $arguments = @('--project', ('"' + $projectFile + '"'), '--scene-type', $scene,
            '--smoke-frames', '30', '--fixed-frame-step', '--gpu-timing',
            '--gpu-timing-output', ('"' + $reportPath + '"'))
        $process = Start-Process -FilePath $playerPath -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
        $output = @((Get-Content -LiteralPath $stdoutPath -Raw), (Get-Content -LiteralPath $stderrPath -Raw))
        $output | Set-Content -LiteralPath (Join-Path $evidencePath "$scene.log") -Encoding utf8
        if ($process.ExitCode -ne 0) { throw "Installed Player $scene failed" }
        if (($output -join "`n") -match 'VUID-|Validation Error') { throw "Player $scene validation error" }
        if (($output -join "`n") -notmatch 'Allocator after unload: buffers=0 images=0') { throw "Player $scene leaked resources" }
        $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
        if ($report.samples -ne 30 -or $report.cpuFrame.completedFrames -ne 30) { throw "Player $scene incomplete timing" }
        if ($scene -eq 'blackhole' -and $report.submission.drawCalls -ne 90) { throw 'Blackhole draw counters incomplete' }
        Write-Output "Installed Player ${scene}: 30 frames passed"
    }
} finally {
    Set-Location -LiteralPath $originalLocation
    $env:PATH = $originalPath
}
Write-Output 'Isolated Player project and runtime check passed'
