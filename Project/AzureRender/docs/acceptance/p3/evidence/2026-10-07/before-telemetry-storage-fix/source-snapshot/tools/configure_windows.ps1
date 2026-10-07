param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$ToolchainRoot = $env:AZURERENDER_TOOLCHAIN_ROOT
)

$ErrorActionPreference = "Stop"
$sourceRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ToolchainRoot)) {
    $ninjaCommand = Get-Command ninja.exe -ErrorAction SilentlyContinue
    $compilerCommand = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($null -eq $ninjaCommand -or $null -eq $compilerCommand) {
        throw "Ninja and g++ must be on PATH, or set AZURERENDER_TOOLCHAIN_ROOT."
    }
    $ninja = $ninjaCommand.Source
    $compiler = $compilerCommand.Source
} else {
    $ninja = Join-Path $ToolchainRoot "ninja\win\x64\ninja.exe"
    $compiler = Join-Path $ToolchainRoot "mingw\bin\g++.exe"
}
$mingwBin = Split-Path -Parent $compiler

if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    throw "Set VCPKG_ROOT or pass -VcpkgRoot."
}
$vcpkgToolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"

foreach ($required in @($ninja, $compiler, $vcpkgToolchain)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required build tool was not found: $required"
    }
}

$preset = "ninja-$($Config.ToLowerInvariant())"
$env:PATH = "$mingwBin;$env:PATH"
cmake --preset $preset `
    -DCMAKE_MAKE_PROGRAM:FILEPATH="$ninja" `
    -DCMAKE_CXX_COMPILER:FILEPATH="$compiler" `
    -DCMAKE_TOOLCHAIN_FILE:FILEPATH="$vcpkgToolchain" `
    -DVCPKG_TARGET_TRIPLET:STRING=x64-mingw-dynamic
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build --preset $preset
exit $LASTEXITCODE
