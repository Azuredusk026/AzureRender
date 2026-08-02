param(
    [Parameter(Mandatory = $true)]
    [string]$CaptureDirectory,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [string]$FfmpegExecutable = "ffmpeg"
)

$capturePath = [System.IO.Path]::GetFullPath($CaptureDirectory)
$manifestPath = Join-Path $capturePath "capture_manifest.json"
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Capture manifest was not found: $manifestPath"
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw |
    ConvertFrom-Json
if ($manifest.format -ne "Afterglow PNG sequence v1") {
    throw "Unsupported capture format: $($manifest.format)"
}
if ($manifest.capturedFrames -le 0 -or $manifest.fps -le 0) {
    throw "Capture manifest contains invalid frame or FPS values"
}

$firstFrame = Join-Path $capturePath "frame_000000.png"
if (-not (Test-Path -LiteralPath $firstFrame -PathType Leaf)) {
    throw "The first captured frame was not found: $firstFrame"
}

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
if (Test-Path -LiteralPath $resolvedOutput) {
    throw "Output already exists; refusing to overwrite: $resolvedOutput"
}
$outputParent = Split-Path -Parent $resolvedOutput
if ($outputParent -and -not (Test-Path -LiteralPath $outputParent)) {
    New-Item -ItemType Directory -Path $outputParent | Out-Null
}

$ffmpegCommand = Get-Command $FfmpegExecutable -ErrorAction Stop
$framePattern = Join-Path $capturePath "frame_%06d.png"
& $ffmpegCommand.Source `
    -hide_banner `
    -loglevel info `
    -framerate ([string]$manifest.fps) `
    -start_number 0 `
    -i $framePattern `
    -frames:v ([string]$manifest.capturedFrames) `
    -an `
    -c:v libx264 `
    -preset slow `
    -crf 15 `
    -pix_fmt yuv420p `
    -color_primaries bt709 `
    -color_trc bt709 `
    -colorspace bt709 `
    -color_range tv `
    -x264-params "colorprim=bt709:transfer=bt709:colormatrix=bt709" `
    -movflags +faststart `
    -n `
    $resolvedOutput

if ($LASTEXITCODE -ne 0) {
    throw "FFmpeg failed with exit code $LASTEXITCODE"
}

Get-Item -LiteralPath $resolvedOutput |
    Select-Object FullName, Length, LastWriteTime
