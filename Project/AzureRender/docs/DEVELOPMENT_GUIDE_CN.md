# AzureRender 开发指南

## 1. 已验证环境

Windows 基线：

- Windows 10/11
- Vulkan SDK 1.4.350.0，API target 1.3
- CMake 3.20+
- Ninja 1.13+
- GCC/MinGW 13.1，C++17
- NVIDIA RTX 4060 Laptop GPU

Linux CI：Ubuntu 24.04、GCC、Ninja、Vulkan Validation、Mesa lavapipe/Xvfb。

依赖版本由 `vcpkg.json` 的 baseline 固定。不要在功能任务中顺便升级 SDK 或依赖。

## 2. 获取和依赖

```powershell
git clone https://github.com/Azuredusk026/AzureRender.git
cd AzureRender\Project\AzureRender

$env:VULKAN_SDK = "C:\VulkanSDK\1.4.350.0"
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-mingw-dynamic
```

## 3. 配置与构建

Windows 优先使用环境探测脚本；它固定 Ninja、MinGW、vcpkg toolchain 和 `x64-mingw-dynamic` ABI：

```powershell
.\tools\configure_windows.ps1 -Config Debug
.\tools\configure_windows.ps1 -Config Release
```

若 Ninja/MinGW 来自 CLion 且不在 `PATH`，设置 `AZURERENDER_TOOLCHAIN_ROOT` 为包含 `ninja/` 和 `mingw/` 的 CLion 工具目录，或传入同名 `-ToolchainRoot` 参数。脚本不保存本机绝对路径。

如环境变量无法被 preset 识别：

```powershell
cmake -S . -B build/ninja-debug -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_CXX_COMPILER=g++ `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic
cmake --build build/ninja-debug
```

## 4. 测试与回归

单元/契约测试：

```powershell
ctest --test-dir build\ninja-debug --output-on-failure
ctest --test-dir build\ninja-release --output-on-failure
```

公共角色 Validation：

```powershell
.\build\ninja-debug\AzureRender.exe --smoke-frames 120
```

黑洞 Validation：

```powershell
.\build\ninja-debug\AzureRender.exe `
  --scene-type blackhole `
  --smoke-frames 120
```

编辑器 Validation：

```powershell
.\build\ninja-debug\AzureRender.exe `
  --asset .\assets_public\test_model.gltf `
  --create-scene .\build\ninja-debug\public.azscene

.\build\ninja-debug\AzureRender.exe `
  --editor .\build\ninja-debug\public.azscene `
  --smoke-frames 120
```

视觉截图：

```powershell
.\build\ninja-release\AzureRender.exe `
  --scene-type blackhole `
  --width 1280 --height 720 `
  --capture-dir .\captures\blackhole\blackhole_beauty_temporal_v1_1280x720_20260818 `
  --capture-frames 1 --capture-fps 60
```

捕获目录必须为空。需要重复测试时使用新的目录，不覆盖旧证据。

## 5. 发布门禁

```powershell
cmake -DBUILD_DIR="$PWD/build/ninja-debug" `
  -DCONFIG=Debug `
  -P tools/run_release_gate.cmake
```

门禁至少覆盖 shader 编译、目标构建、CTest、安装 manifest 和资源检查。GPU/视觉任务还必须执行实际 Validation 和截图。

## 6. 开发循环

1. 阅读 `DEVELOPMENT_ROADMAP_CN.md`，确认唯一 `Ready/Active` 阶段。
2. 检查 `git status --short --branch`，保留用户已有修改。
3. 先复现基线，再进行局部修改。
4. 添加与风险相称的自动化测试。
5. 执行 Debug、Release、CTest、Validation 和视觉门禁。
6. 更新路线状态、架构事实和受影响专题文档。
7. 执行 `git diff --check`，确认没有无关文件。
8. 使用计划中冻结的标题独立提交。

## 7. 文档工作流

- 当前实现只写入 `ARCHITECTURE_CN.md`。
- 当前/下一阶段只写入 `DEVELOPMENT_ROADMAP_CN.md`。
- 命令变化更新本文件和应用 README。
- 公共接口变化更新 `ARCHITECTURE_CN.md`。
- 资产和画面验收变化更新 `ASSET_AND_VISUAL_QA_CN.md`。
- 不恢复逐提交开发日志；Git 已提供历史。

## 8. 常见问题

### MinGW 与 ImGui ABI

Windows MinGW 使用 `third_party/imgui/` vendored 源码构建，避免链接 vcpkg 的 MSVC 静态库。不要重新引入不匹配的预编译 ImGui ABI。

如果 `g++.exe` 能输出版本、但编译无诊断地以代码 1 退出，直接运行
`cc1plus.exe --version` 并检查是否返回 `0xC0000135`。这表示编译后端找不到
MinGW 运行库；将所用工具链的 `bin` 放入当前会话 `PATH` 后重新构建：

```powershell
$env:PATH = "D:\path\to\mingw\bin;$env:PATH"
```

### Vulkan Validation 不可用

确认 Vulkan SDK 安装、`VK_LAYER_KHRONOS_validation` 可见，并且 Debug 构建启用了 `AZURERENDER_ENABLE_VALIDATION`。

### 运行时找不到 shader/资产

先运行：

```powershell
.\build\ninja-debug\AzureRender.exe --check-resources
```

必要时使用 `--resource-root <directory>`；不要在源码中加入本机绝对路径。
