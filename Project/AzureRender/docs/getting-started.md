# 构建与使用

本页给出从源码构建、运行两个场景、创建编辑器场景、捕获图像和排查运行时问题的完整路径。命令默认从 AzureRender 仓库根目录执行。

## 环境要求

| 组件 | 要求 |
| --- | --- |
| 操作系统 | Windows 10/11。Linux CI 使用 Ubuntu 24.04 |
| Vulkan | Vulkan SDK，目标 API 1.3，需包含 `glslc` |
| 构建 | CMake 3.20+、Ninja |
| 编译器 | MinGW/GCC 13+ 或项目 CI 支持的 MSVC/GCC |
| 依赖 | vcpkg manifest mode |

Windows 主验收使用 MSVC 14.44、Vulkan SDK 1.4.350 和 Ninja。设置 `VULKAN_SDK` 与 `VCPKG_ROOT` 后，从工程目录执行命令。

## 配置与构建

```powershell
.\tools\msvc_env.bat cmake --preset msvc-debug
.\tools\msvc_env.bat cmake --build --preset msvc-debug
.\tools\msvc_env.bat cmake --preset msvc-release
.\tools\msvc_env.bat cmake --build --preset msvc-release
```

MSVC 预设使用 `x64-windows` 依赖。MinGW 环境可使用 `ninja-debug`、`ninja-release` 预设及 `x64-mingw-dynamic` 依赖。

源码与产物哈希、公共回归和本机 GPU 验收见[构建复现说明](runtime/build-reproducibility.md)。

构建过程调用 `glslc`，把 GLSL 编译到构建目录的 `shaders/`。Shader 是主目标的显式依赖，编译失败会终止构建。

## 运行公共角色场景

无参数启动默认选择 Character 和公共测试模型：

```powershell
.\build\ninja-msvc-debug\AzureRender.exe
```

公共 Smoke：

```powershell
.\build\ninja-msvc-debug\AzureRender.exe `
  --scene-type character `
  --asset .\assets_public\test_model.gltf `
  --smoke-frames 120
```

风格化检查预设：

```powershell
.\build\ninja-msvc-release\AzureRender.exe `
  --asset .\assets_public\test_model.gltf `
  --qa-camera full-body-front `
  --qa-light stylized-key
```

`stylized-key` 成组选择灯光与展示 Look。使用 `--no-stylized` 排除风格化分支，使用 `--no-inner-outline` 关闭屏幕空间内部描边。

## 运行黑洞场景

```powershell
.\build\ninja-msvc-release\AzureRender.exe `
  --scene-type blackhole `
  --blackhole-quality cinematic `
  --blackhole-camera front
```

质量档位有 `performance`、`balanced` 和 `cinematic`。相机预设有 `front`、`orbit-left`、`high`、`close` 和 `over-shoulder`。更换相机或质量后，渲染器会清空时间 History。这样旧采样不会混入新视图。

## 环境贴图

`--environment` 可以读取单张等距柱状 `.hdr/.png/.jpg`，也可以读取包含六个面的目录。六个文件名分别以 `_Right`、`_Left`、`_Up`、`_Down`、`_Front` 和 `_Back` 结尾。加载器会把六面图转换成内部使用的等距柱状图。

```powershell
.\build\ninja-msvc-debug\AzureRender.exe `
  --scene-type character `
  --environment D:\Assets\StudioEvening.hdr
```

发布配置中不能包含开发机的绝对路径。环境资源支持 Radiance HDR 和线性 OpenEXR。损坏的 OpenEXR 文件会报告文件路径与解码错误。

## 创建和打开编辑器场景

```powershell
.\build\ninja-msvc-debug\AzureRender.exe `
  --asset .\assets_public\test_model.gltf `
  --create-scene .\build\ninja-msvc-debug\public.azscene

.\build\ninja-msvc-debug\AzureRender.exe `
  --editor .\build\ninja-msvc-debug\public.azscene
```

编辑器包含 Scene Outliner、Inspector 和 Asset Browser。它也支持 Renderer 选择、Undo/Redo、手动重载资产和语义化 Capture。详细数据格式见[资产、场景与编辑器](assets-and-editor.md)。

## 确定性截图

捕获目录必须不存在或为空：

```powershell
.\build\ninja-msvc-release\AzureRender.exe `
  --scene-type blackhole `
  --width 2560 --height 1440 `
  --capture-dir .\captures\blackhole\front-cinematic `
  --capture-frames 1 --capture-fps 24
```

捕获使用固定时间步长。输出包含 PNG、状态 manifest 和性能信息。尺寸范围是 64x64 到 7680x4320，帧率范围是 1 到 240。`--capture-dir` 与 `--capture-frames` 必须同时使用。

五章技术序列：

```powershell
.\build\ninja-msvc-release\AzureRender.exe `
  --portfolio --technical-sequence `
  --capture-dir .\captures\character\technical `
  --capture-frames 1920 --capture-fps 24 `
  --width 2560 --height 1440
```

技术序列帧数至少为 5 且必须能被 5 整除。它与 `--qa-*` 隔离模式互斥。

## GPU Timing

```powershell
.\build\ninja-msvc-release\AzureRender.exe `
  --scene-type blackhole `
  --gpu-timing `
  --gpu-timing-output .\captures\blackhole\timing.json `
  --smoke-frames 300
```

Timing 来自 Vulkan Timestamp Query。它只统计 Query 包围的 GPU Pass，不包含 CPU 更新、Present 等待、PNG Readback 和视频编码。比较结果时，请使用相同的 GPU、驱动、分辨率、场景、质量和构建类型。

## 诊断与 QA

```powershell
.\build\ninja-msvc-debug\AzureRender.exe `
  --diagnostic-view normal `
  --hud --smoke-frames 120
```

角色还提供效果和缓冲隔离。例如观察 Face SDF：

```powershell
.\build\ninja-msvc-debug\AzureRender.exe `
  --qa-camera face-three-quarter `
  --qa-light stylized-key `
  --qa-effect face-sdf `
  --qa-effect-state isolation
```

完整枚举见[参数与接口参考](reference.md)。

## 安装树与手动运行

```powershell
cmake --install .\build\ninja-msvc-debug --prefix .\build\install-debug
cmake --install .\build\ninja-msvc-release --prefix .\build\install-release

.\build\install-debug\bin\AzureRender.exe --check-resources
.\build\install-release\bin\AzureRender.exe --smoke-frames 120
```

Windows MinGW 安装树的 `bin/` 必须包含 `AzureRender.exe` 和 `glfw3.dll`。它还需要 `libgcc_s_seh-1.dll`、`libstdc++-6.dll` 和 `libwinpthread-1.dll`。不要单独移动 EXE。

## 常见问题

### 缺少 DLL

运行安装树内的 EXE，并执行：

```powershell
.\tools\verify_windows_runtime.ps1 `
  -Executable .\build\install-release\bin\AzureRender.exe
```

### 找不到 Shader 或资产

```powershell
.\build\ninja-msvc-release\AzureRender.exe --check-resources
```

开发树、构建树与安装树由 `ResourceLocator` 统一探测。只有特殊部署才应传 `--resource-root`，不要把本机路径写入源码。

### Vulkan Validation 不可用

确认 SDK 安装完整，并检查 `VK_LAYER_KHRONOS_validation` 是否可见。Validation 需要 Debug 构建。`AZURERENDER_ENABLE_VALIDATION` 只在 Debug 配置中启用。

### MinGW 编译器无诊断退出

检查 `cc1plus.exe --version`。若退出码为 `0xC0000135`，把对应 MinGW 的 `bin` 放入当前会话 `PATH`，确保前端与后端来自同一工具链。

### 捕获尺寸不正确

确定性捕获会创建隐藏的无边框 Surface。Manifest 中的宽高必须和命令一致。视频或图片不能在后期做非等比拉伸。如果目录非空，或者 Surface 无法满足请求尺寸，捕获会直接失败。
