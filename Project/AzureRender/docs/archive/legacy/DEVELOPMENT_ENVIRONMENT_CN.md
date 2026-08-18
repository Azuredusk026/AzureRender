# AzureRender 开发与测试环境

本文件记录 2026-07-25 已验证的 Windows 开发环境。新成员应先复现这套基线，再升级 SDK、编译器或依赖。

## 已验证版本

- Windows：10.0.26200.6584
- GPU：NVIDIA GeForce RTX 4060 Laptop GPU
- Vulkan SDK：1.4.350.0
- Vulkan API target：1.3
- CMake：3.20.5
- Ninja：1.13.2
- C++：GCC/MinGW 13.1.0，C++17
- vcpkg baseline：`10ceb139a610ebf3c6aa49cdc4a4b7f3db5d3f2b`
- GLFW：3.4#1
- tinygltf：3.0.0
- stb：2024-07-29#1

参考仓库版本：

- Khronos Vulkan-Tutorial：`4780b519f078f9a7213c3b1dd27982ee0b9e5c71`

`AfterglowRender/` 仅为可选的本地学习参考目录，已加入 ignore，不属于
AzureRender 的 clone 或测试依赖。

## 首次获取

仓库包含 private 第三方测试资产，只能授予可信成员访问权限：

```powershell
git clone --recurse-submodules https://github.com/Azuredusk026/AzureRender.git
cd AzureRender
```

如果已经普通 clone：

```powershell
git submodule update --init --recursive
```

## 依赖安装

安装 Vulkan SDK 1.4.350.0，并设置：

```powershell
$env:VULKAN_SDK = "C:\VulkanSDK\1.4.350.0"
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
```

将 Ninja 与 GCC 13.1.0 加入 `PATH`。项目提供 `vcpkg.json` 锁定依赖基线；使用 manifest mode 安装 x64-windows 依赖：

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows
```

## 配置、构建与回归

```powershell
cd Project\AzureRender
cmake --preset ninja-debug
cmake --build --preset ninja-debug
cmake --preset ninja-release
cmake --build --preset ninja-release
```

公共资产 Debug Validation 回归：

```powershell
.\build\ninja-debug\AzureRender.exe --smoke-frames 120
```

私有角色 Release 回归：

```powershell
.\build\ninja-release\AzureRender.exe `
  --asset .\assets_private\laevat_static\laevat_static_material.glb `
  --smoke-frames 120
```

预期运行时统计：

- 公共资产：337 vertices、900 indices、3 primitives、4 materials；
- 私有角色：81,487 vertices、284,673 indices、14 primitives、15 materials。

构建目录、IDE 配置和截图是本机生成内容，不进入版本控制。测试资产、转换工具、源码、shader、开发日志和环境清单进入版本控制。

## 2026-08-13 Linux CQ-2 补充验证

本次 CQ-2 额外在 Ubuntu 24.04、GCC 13.3、CMake 3.28.3、Ninja 1.11.1、
Vulkan headers/Validation 1.3.275、glslc 2023.8 和 NVIDIA GeForce RTX 5070 Ti
Laptop GPU 上完成 Debug/Release 构建、Xvfb Swapchain、公共/私有 Validation Smoke、
Capture 和 GPU Timestamp 功能验证。该环境用于跨平台工程回归，不替代上述 Windows
正式发布基线。
