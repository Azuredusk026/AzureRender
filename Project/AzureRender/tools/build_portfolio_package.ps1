param(
    [string]$FfmpegExecutable = "ffmpeg"
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot ".."))
$captureRoot = Join-Path $projectRoot "captures"
$portfolioRoot = Join-Path $projectRoot "portfolio"
$imageRoot = Join-Path $portfolioRoot "images"

$ffmpeg = Get-Command $FfmpegExecutable -ErrorAction Stop
$fontPath = "C\:/Windows/Fonts/consola.ttf"

$beautyVideo = Join-Path $captureRoot `
    "Afterglow_S28_Portfolio_1080p60_20s.mp4"
$technicalVideo = Join-Path $captureRoot `
    "Afterglow_S33_TechnicalTitles_1080p60_20s.mp4"
$performanceReport = Join-Path $captureRoot `
    "s29_gpu_timing_release_1080p.json"
$technicalFrames = Join-Path $captureRoot `
    "s33_technical_titles_1080p60_20s"

$requiredFiles = @(
    $beautyVideo,
    $technicalVideo,
    $performanceReport,
    (Join-Path $technicalFrames "capture_manifest.json"),
    (Join-Path $technicalFrames "frame_000030.png"),
    (Join-Path $technicalFrames "frame_000120.png"),
    (Join-Path $technicalFrames "frame_000270.png"),
    (Join-Path $technicalFrames "frame_000510.png"),
    (Join-Path $technicalFrames "frame_000750.png"),
    (Join-Path $technicalFrames "frame_000990.png")
)

foreach ($path in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required portfolio source is missing: $path"
    }
}

New-Item -ItemType Directory -Path $imageRoot -Force | Out-Null

$coverPath = Join-Path $imageRoot "azurerender_cover_1920x1080.png"
$coverFilter = @(
    "scale=1920:1080",
    "drawbox=x=0:y=0:w=760:h=1080:color=0x0B1724@0.82:t=fill",
    "drawbox=x=84:y=252:w=8:h=340:color=0x6DEBFF@0.95:t=fill",
    "drawtext=fontfile='$fontPath':text='AZURERENDER':fontcolor=white:fontsize=72:x=120:y=270",
    "drawtext=fontfile='$fontPath':text='STYLIZED VULKAN RENDERER':fontcolor=0xA8EFFF:fontsize=31:x=122:y=372",
    "drawtext=fontfile='$fontPath':text='REAL-TIME CHARACTER RENDERING':fontcolor=0xCFDCE6:fontsize=23:x=122:y=438",
    "drawtext=fontfile='$fontPath':text='WU CHENFENG  |  FYP 2026':fontcolor=0x8CA5B7:fontsize=20:x=122:y=532"
) -join ","

& $ffmpeg.Source -hide_banner -loglevel error -y `
    -i (Join-Path $technicalFrames "frame_000120.png") `
    -vf $coverFilter -frames:v 1 $coverPath
if ($LASTEXITCODE -ne 0) {
    throw "FFmpeg failed to generate the portfolio cover."
}

$contactSheetPath = Join-Path $imageRoot `
    "technical_contact_sheet_1920x1080.png"
$labelFilters = @(
    "[0:v]scale=640:360,drawbox=x=0:y=0:w=640:h=44:color=0x0B1724@0.78:t=fill,drawtext=fontfile='$fontPath':text='01  BEAUTY':fontcolor=white:fontsize=24:x=18:y=9[a]",
    "[1:v]scale=640:360,drawbox=x=0:y=0:w=640:h=44:color=0x0B1724@0.78:t=fill,drawtext=fontfile='$fontPath':text='02  WORLD NORMAL':fontcolor=white:fontsize=24:x=18:y=9[b]",
    "[2:v]scale=640:360,drawbox=x=0:y=0:w=640:h=44:color=0x0B1724@0.78:t=fill,drawtext=fontfile='$fontPath':text='03  INTERNAL OUTLINE':fontcolor=white:fontsize=24:x=18:y=9[c]",
    "[3:v]scale=640:360,drawbox=x=0:y=0:w=640:h=44:color=0x0B1724@0.78:t=fill,drawtext=fontfile='$fontPath':text='04  SHADOW MAP':fontcolor=white:fontsize=24:x=18:y=9[d]",
    "[4:v]scale=640:360,drawbox=x=0:y=0:w=640:h=44:color=0x0B1724@0.78:t=fill,drawtext=fontfile='$fontPath':text='05  BEAUTY + GPU HUD':fontcolor=white:fontsize=24:x=18:y=9[e]",
    "color=c=0x102131:s=640x360:d=1,drawtext=fontfile='$fontPath':text='VULKAN PIPELINE':fontcolor=0x6DEBFF:fontsize=28:x=(w-text_w)/2:y=135,drawtext=fontfile='$fontPath':text='SHADOW  |  MAIN  |  OUTLINE':fontcolor=0x8CA5B7:fontsize=17:x=(w-text_w)/2:y=190[f]",
    "[a][b][c][d][e][f]xstack=inputs=6:layout=0_0|640_0|1280_0|0_360|640_360|1280_360[grid]",
    "[grid]pad=1920:1080:0:180:color=0x102131,drawtext=fontfile='$fontPath':text='AZURERENDER  /  TECHNICAL BREAKDOWN':fontcolor=white:fontsize=48:x=84:y=62,drawtext=fontfile='$fontPath':text='DETERMINISTIC 1080P60 CAPTURE  /  FIVE RENDERER VIEWS':fontcolor=0x6DEBFF:fontsize=21:x=86:y=128,drawtext=fontfile='$fontPath':text='STYLIZED LIGHTING  |  GPU SKINNING  |  SHADOW MAP  |  DEPTH-NORMAL OUTLINES  |  GPU TIMESTAMPS':fontcolor=0x8CA5B7:fontsize=19:x=(w-text_w)/2:y=948[out]"
) -join ";"

& $ffmpeg.Source -hide_banner -loglevel error -y `
    -i (Join-Path $technicalFrames "frame_000030.png") `
    -i (Join-Path $technicalFrames "frame_000270.png") `
    -i (Join-Path $technicalFrames "frame_000510.png") `
    -i (Join-Path $technicalFrames "frame_000750.png") `
    -i (Join-Path $technicalFrames "frame_000990.png") `
    -filter_complex $labelFilters -map "[out]" -frames:v 1 $contactSheetPath
if ($LASTEXITCODE -ne 0) {
    throw "FFmpeg failed to generate the technical contact sheet."
}

$timing = Get-Content $performanceReport -Raw | ConvertFrom-Json
$technicalManifest = Get-Content `
    (Join-Path $technicalFrames "capture_manifest.json") -Raw |
    ConvertFrom-Json

function Get-ArtifactInfo {
    param(
        [string]$Role,
        [string]$AbsolutePath
    )

    $item = Get-Item -LiteralPath $AbsolutePath
    $rootPrefix = $projectRoot.TrimEnd("\") + "\"
    if (-not $item.FullName.StartsWith(
            $rootPrefix,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Portfolio artifact is outside the project root: $AbsolutePath"
    }
    $relativePath = $item.FullName.Substring($rootPrefix.Length).
        Replace("\", "/")
    [ordered]@{
        role = $Role
        path = $relativePath
        bytes = $item.Length
        sha256 = (Get-FileHash $item.FullName -Algorithm SHA256).Hash
    }
}

$manifest = [ordered]@{
    format = "AzureRender portfolio package v1"
    milestone = "S34"
    generatedUtc = (Get-Date).ToUniversalTime().ToString("o")
    project = "AzureRender - Stylized Vulkan Character Renderer"
    author = "Wu Chenfeng"
    rendererHighlights = @(
        "Vulkan stylized forward rendering",
        "GPU skinning and deterministic animation",
        "2048x2048 alpha-aware shadow map with 3x3 PCF",
        "Depth-normal screen-space internal outlines",
        "Renderer-native HUD and GPU timestamp queries",
        "Deterministic 1080p60 PNG and H.264 capture"
    )
    primaryArtifacts = @(
        (Get-ArtifactInfo "clean-beauty-video" $beautyVideo),
        (Get-ArtifactInfo "technical-breakdown-video" $technicalVideo),
        (Get-ArtifactInfo "portfolio-cover" $coverPath),
        (Get-ArtifactInfo "technical-contact-sheet" $contactSheetPath),
        (Get-ArtifactInfo "release-gpu-timing-report" $performanceReport)
    )
    technicalVideo = [ordered]@{
        width = $technicalManifest.width
        height = $technicalManifest.height
        fps = $technicalManifest.fps
        frames = $technicalManifest.capturedFrames
        durationSeconds = $technicalManifest.durationSeconds
        chapters = $technicalManifest.technicalChapters
        fadeFrames = $technicalManifest.technicalFadeFrames
        titleFrames = $technicalManifest.technicalTitleFrames
    }
    gpuTiming1080p = [ordered]@{
        gpu = $timing.gpu
        samples = $timing.samples
        shadowAverageMs = $timing.shadowAverageMs
        mainSceneAverageMs = $timing.mainSceneAverageMs
        internalOutlineAverageMs = $timing.internalOutlineAverageMs
        totalAverageMs = $timing.totalAverageMs
        scope = "GPU render passes only; excludes CPU, presentation, and PNG encoding"
    }
    assetNotice = "Character assets are referenced for a non-commercial technical portfolio demonstration and are not included in the public source package."
}

$manifestPath = Join-Path $portfolioRoot "portfolio_manifest.json"
$manifest | ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath $manifestPath -Encoding utf8

Write-Output "Portfolio package generated:"
Get-Item $coverPath, $contactSheetPath, $manifestPath |
    Select-Object FullName, Length, LastWriteTime
