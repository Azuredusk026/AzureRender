param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$Asset,

    [Parameter(Mandatory = $true)]
    [string]$OutputRoot,

    [ValidateSet('all', 'baseline', 'isolation', 'ab')]
    [string]$Mode = 'all',

    [ValidateRange(64, 7680)]
    [int]$Width = 1280,

    [ValidateRange(64, 4320)]
    [int]$Height = 720,

    [ValidateRange(1, 240)]
    [int]$Fps = 60,

    [ValidateRange(2, 600)]
    [int]$LightingSweepFrames = 60,

    [switch]$Resume
)

$ErrorActionPreference = 'Stop'

$resolvedExecutable = [System.IO.Path]::GetFullPath($Executable)
$resolvedAsset = [System.IO.Path]::GetFullPath($Asset)
$resolvedOutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
$shaderRoot = Join-Path (Split-Path -Parent $resolvedExecutable) 'shaders'
$projectRoot = Split-Path -Parent $PSScriptRoot
$rampProfile = Join-Path $projectRoot 'assets_public/toon_ramp_profiles.json'
$rampAtlas = Join-Path $projectRoot 'assets_public/toon_ramp_atlas.ppm'

if (-not (Test-Path -LiteralPath $resolvedExecutable -PathType Leaf)) {
    throw "AzureRender executable not found: $resolvedExecutable"
}
if (-not (Test-Path -LiteralPath $resolvedAsset -PathType Leaf)) {
    throw "Character asset not found: $resolvedAsset"
}
if (-not (Test-Path -LiteralPath $shaderRoot -PathType Container)) {
    throw "Compiled shader directory not found: $shaderRoot"
}
if (-not (Test-Path -LiteralPath $rampProfile -PathType Leaf) -or
    -not (Test-Path -LiteralPath $rampAtlas -PathType Leaf)) {
    throw "CQ-2 toon-ramp assets are missing"
}
if (Test-Path -LiteralPath $resolvedOutputRoot) {
    if (-not (Test-Path -LiteralPath $resolvedOutputRoot -PathType Container)) {
        throw "QA output root is not a directory: $resolvedOutputRoot"
    }
    if (-not $Resume -and (Get-ChildItem -LiteralPath $resolvedOutputRoot -Force | Measure-Object).Count -ne 0) {
        throw "QA output root must be empty: $resolvedOutputRoot"
    }
} else {
    New-Item -ItemType Directory -Path $resolvedOutputRoot | Out-Null
}

$cases = [System.Collections.Generic.List[object]]::new()

function Add-QaCase {
    param(
        [string]$Name,
        [string]$Camera,
        [string]$Light,
        [string]$Isolation = 'beauty',
        [string]$Effect = '',
        [string]$EffectState = '',
        [int]$Frames = 1
    )
    $cases.Add([ordered]@{
        name = $Name
        camera = $Camera
        light = $Light
        isolation = $Isolation
        effect = $Effect
        effectState = $EffectState
        frames = $Frames
    })
}

if ($Mode -in @('all', 'baseline')) {
    $cameras = @(
        'full-body-front',
        'face-front',
        'face-three-quarter',
        'back-detail',
        'lighting-sweep'
    )
    $lights = @(
        'neutral-material',
        'stylized-key',
        'specular-rim',
        'rear-emissive'
    )
    foreach ($camera in $cameras) {
        foreach ($light in $lights) {
            $frames = if ($camera -eq 'lighting-sweep') {
                $LightingSweepFrames
            } else {
                1
            }
            Add-QaCase `
                -Name "baseline_${camera}_${light}" `
                -Camera $camera `
                -Light $light `
                -Frames $frames
        }
    }
}

if ($Mode -in @('all', 'isolation')) {
    $isolationViews = @(
        'beauty',
        'albedo',
        'world-normal',
        'depth',
        'diffuse-band',
        'shadow-visibility',
        'hair-kk',
        'rim',
        'specular',
        'emissive',
        'outline',
        'shadow-map',
        'material-id',
        'style-mask',
        'ambient',
        'direct-diffuse',
        'shadow-tint',
        'face-sdf',
        'overlay',
        'bloom'
    )
    foreach ($view in $isolationViews) {
        $camera = if ($view -eq 'face-sdf') {
            'face-front'
        } elseif ($view -in @('hair-kk', 'rim', 'specular')) {
            'face-three-quarter'
        } elseif ($view -eq 'emissive') {
            'back-detail'
        } else {
            'full-body-front'
        }
        $light = if ($view -in @('hair-kk', 'rim', 'specular')) {
            'specular-rim'
        } elseif ($view -eq 'emissive') {
            'rear-emissive'
        } else {
            'stylized-key'
        }
        Add-QaCase `
            -Name "isolation_${view}" `
            -Camera $camera `
            -Light $light `
            -Isolation $view
    }
}

if ($Mode -in @('all', 'ab')) {
    $effectCases = @(
        @{ effect = 'toon'; camera = 'full-body-front'; light = 'stylized-key' },
        @{ effect = 'shadow'; camera = 'full-body-front'; light = 'stylized-key' },
        @{ effect = 'hair-kk'; camera = 'face-three-quarter'; light = 'specular-rim' },
        @{ effect = 'rim'; camera = 'full-body-front'; light = 'specular-rim' },
        @{ effect = 'specular'; camera = 'face-three-quarter'; light = 'specular-rim' },
        @{ effect = 'emissive'; camera = 'back-detail'; light = 'rear-emissive' },
        @{ effect = 'outline'; camera = 'full-body-front'; light = 'neutral-material' }
        @{ effect = 'face-sdf'; camera = 'face-front'; light = 'stylized-key' },
        @{ effect = 'overlay'; camera = 'face-front'; light = 'neutral-material' },
        @{ effect = 'bloom'; camera = 'face-three-quarter'; light = 'rear-emissive' }
    )
    foreach ($effectCase in $effectCases) {
        foreach ($state in @('enabled', 'disabled', 'isolation')) {
            Add-QaCase `
                -Name "ab_$($effectCase.effect)_${state}" `
                -Camera $effectCase.camera `
                -Light $effectCase.light `
                -Effect $effectCase.effect `
                -EffectState $state
        }
    }
}

$results = [System.Collections.Generic.List[object]]::new()
foreach ($case in $cases) {
    $caseDirectory = Join-Path $resolvedOutputRoot $case.name
    $frame = Join-Path $caseDirectory 'frame_000000.png'
    $manifest = Join-Path $caseDirectory 'capture_manifest.json'
    if ($Resume -and (Test-Path -LiteralPath $caseDirectory)) {
        if ((-not (Test-Path -LiteralPath $frame -PathType Leaf)) -or
            (-not (Test-Path -LiteralPath $manifest -PathType Leaf))) {
            $caseFullPath = [System.IO.Path]::GetFullPath($caseDirectory)
            $outputPrefix = $resolvedOutputRoot.TrimEnd(
                [System.IO.Path]::DirectorySeparatorChar) +
                [System.IO.Path]::DirectorySeparatorChar
            if (-not $caseFullPath.StartsWith(
                $outputPrefix,
                [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Unsafe incomplete QA case path: $caseFullPath"
            }
            Write-Host "[CQ-0] rebuild incomplete $($case.name)"
            Remove-Item -LiteralPath $caseFullPath -Recurse -Force
        } else {
            $manifestData = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
            if ($manifestData.qaHarnessVersion -ne 'CQ-0-v1') {
                throw "Unexpected QA manifest version in case: $($case.name)"
            }
            Write-Host "[CQ-0] resume $($case.name)"
            $results.Add([ordered]@{
                name = $case.name
                directory = $case.name
                camera = $manifestData.qaCamera
                light = $manifestData.qaLight
                effect = $manifestData.qaEffect
                effectState = $manifestData.qaEffectState
                isolation = $manifestData.qaIsolation
                stateHash = $manifestData.qaStateHash
                capturedFrames = $manifestData.capturedFrames
                firstFrameSha256 = (Get-FileHash -LiteralPath $frame -Algorithm SHA256).Hash
            })
            continue
        }
    }
    $arguments = @(
        '--asset', $resolvedAsset,
        '--width', $Width,
        '--height', $Height,
        '--capture-dir', $caseDirectory,
        '--capture-frames', $case.frames,
        '--capture-fps', $Fps,
        '--qa-camera', $case.camera,
        '--qa-light', $case.light,
        '--qa-isolation', $case.isolation
    )
    if ($case.effect) {
        $arguments += @(
            '--qa-effect', $case.effect,
            '--qa-effect-state', $case.effectState
        )
    }

    Write-Host "[CQ-0] $($case.name)"
    & $resolvedExecutable @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "QA case failed with exit code ${LASTEXITCODE}: $($case.name)"
    }

    if (-not (Test-Path -LiteralPath $frame -PathType Leaf)) {
        throw "QA case did not produce its first frame: $($case.name)"
    }
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
        throw "QA case did not produce a manifest: $($case.name)"
    }
    $manifestData = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    if ($manifestData.qaHarnessVersion -ne 'CQ-0-v1') {
        throw "Unexpected QA manifest version in case: $($case.name)"
    }
    $results.Add([ordered]@{
        name = $case.name
        directory = $case.name
        camera = $manifestData.qaCamera
        light = $manifestData.qaLight
        effect = $manifestData.qaEffect
        effectState = $manifestData.qaEffectState
        isolation = $manifestData.qaIsolation
        stateHash = $manifestData.qaStateHash
        capturedFrames = $manifestData.capturedFrames
        firstFrameSha256 = (Get-FileHash -LiteralPath $frame -Algorithm SHA256).Hash
    })
}

$shaderFiles = @(
    Get-ChildItem -LiteralPath $shaderRoot -Filter '*.spv' -File |
        Sort-Object -Property Name |
        ForEach-Object {
            [ordered]@{
                name = $_.Name
                sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
            }
        }
)
if ($shaderFiles.Count -eq 0) {
    throw "No compiled SPIR-V shaders found: $shaderRoot"
}

$index = [ordered]@{
    format = 'AzureRender CQ-0 character QA index v1'
    mode = $Mode
    executable = $resolvedExecutable
    executableSha256 = (Get-FileHash -LiteralPath $resolvedExecutable -Algorithm SHA256).Hash
    shaderDirectory = $shaderRoot
    shaderFiles = $shaderFiles
    toonRampProfile = $rampProfile
    toonRampProfileSha256 = (Get-FileHash -LiteralPath $rampProfile -Algorithm SHA256).Hash
    toonRampAtlas = $rampAtlas
    toonRampAtlasSha256 = (Get-FileHash -LiteralPath $rampAtlas -Algorithm SHA256).Hash
    asset = $resolvedAsset
    assetSha256 = (Get-FileHash -LiteralPath $resolvedAsset -Algorithm SHA256).Hash
    width = $Width
    height = $Height
    fps = $Fps
    caseCount = $results.Count
    cases = $results
}
$indexPath = Join-Path $resolvedOutputRoot 'qa_index.json'
$index | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $indexPath -Encoding UTF8
Write-Host "CQ-0 QA complete: $($results.Count) cases"
Write-Host "Index: $indexPath"
