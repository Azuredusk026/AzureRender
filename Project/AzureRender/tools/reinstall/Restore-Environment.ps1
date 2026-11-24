param(
    [switch]$RestoreSkills,
    [switch]$PreparePython,
    [switch]$Configure,
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$VulkanSdk = $env:VULKAN_SDK
)
$ErrorActionPreference = 'Stop'
$engineRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if ([IO.Path]::GetPathRoot($engineRoot) -ne 'D:\') { throw '请将工程恢复到 D 盘后执行。' }
$bundle = Join-Path $engineRoot 'build/maintenance/reinstall-2026-10-29/environment'
if (!(Test-Path -LiteralPath $bundle)) { throw '缺少本地环境包，请恢复完整压缩包。' }
$cache = Join-Path $engineRoot 'build/reinstall-environment'
New-Item -ItemType Directory -Force $cache | Out-Null
$env:TEMP = $cache
$env:TMP = $cache
$env:PIP_CACHE_DIR = Join-Path $cache 'pip-cache'
$env:UV_CACHE_DIR = Join-Path $cache 'uv-cache'
function Invoke-Checked([string]$Command, [string[]]$Arguments) {
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command 执行失败：$LASTEXITCODE" }
}
$manifest = Get-Content -LiteralPath (Join-Path $bundle 'skills-manifest.json') -Raw | ConvertFrom-Json
foreach ($entry in $manifest) {
    $path = Join-Path $bundle $entry.path
    if (!(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.sha256) { throw "技能文件校验失败：$path" }
}
if ($RestoreSkills) {
    $skillRoot = Join-Path $env:USERPROFILE '.agents/skills'
    New-Item -ItemType Directory -Force $skillRoot | Out-Null
    foreach ($skill in Get-ChildItem -LiteralPath (Join-Path $bundle 'skills') -Directory) {
        $target = Join-Path $skillRoot $skill.Name
        if (!(Test-Path -LiteralPath $target)) { Copy-Item -LiteralPath $skill.FullName -Destination $target -Recurse }
    }
}
$checks = @('git','python','cmake','ninja','cl','glslc','node','gh') | ForEach-Object {
    $found = Get-Command $_ -ErrorAction SilentlyContinue
    [pscustomobject]@{tool=$_; available=[bool]$found; path=if($found){$found.Source}else{$null}}
}
$checks | Format-Table -AutoSize
$checks | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $cache 'tool-check.json') -Encoding UTF8
if ($PreparePython) {
    $venv = Join-Path $cache 'venv'
    if (!(Test-Path -LiteralPath (Join-Path $venv 'Scripts/python.exe'))) { Invoke-Checked 'python' @('-m','venv',$venv) }
    $py = Join-Path $venv 'Scripts/python.exe'
    foreach ($req in @((Join-Path $bundle 'requirements.txt'), (Join-Path $engineRoot 'tools/requirements-code-intelligence.txt'), (Join-Path $engineRoot 'tools/requirements-character.txt'), (Join-Path $engineRoot 'tools/requirements-character-qa.txt'))) {
        Invoke-Checked $py @('-m','pip','install','-r',$req)
    }
}
if ($Configure) {
    if (!(Get-Command cl -ErrorAction SilentlyContinue)) { throw '请从 Visual Studio x64 开发者 PowerShell 执行配置。' }
    if (!$VcpkgRoot -or !(Test-Path -LiteralPath (Join-Path $VcpkgRoot 'scripts/buildsystems/vcpkg.cmake'))) { throw '请通过 -VcpkgRoot 指定 D 盘 vcpkg。' }
    if (!$VulkanSdk -or !(Test-Path -LiteralPath (Join-Path $VulkanSdk 'Include/vulkan/vulkan.h'))) { throw '请通过 -VulkanSdk 指定 Vulkan SDK。' }
    $env:VCPKG_ROOT = $VcpkgRoot
    $env:VULKAN_SDK = $VulkanSdk
    $env:PATH = (Join-Path $VulkanSdk 'Bin') + ';' + $env:PATH
    $buildDir = Join-Path $engineRoot ('build/reinstall-msvc-' + $Configuration.ToLowerInvariant())
    Invoke-Checked 'cmake' @('-S',$engineRoot,'-B',$buildDir,'-G','Ninja',"-DCMAKE_BUILD_TYPE=$Configuration","-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake",'-DVCPKG_TARGET_TRIPLET=x64-windows')
    Write-Output "配置完成。构建命令：cmake --build $buildDir -j 2"
}
Write-Output '检查已记录。未执行的安装、配置和构建仍需按交接文档完成。'
