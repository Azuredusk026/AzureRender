# AzureRender 快速使用

AzureRender 是 C++17/Vulkan 桌面渲染器和编辑器。项目当前支持 `character` 与 `blackhole` 两种场景渲染器、HDR Scene Color、风格化角色材质、编辑器、确定性捕获和 GPU timing。

完整说明见[当前架构](docs/ARCHITECTURE_CN.md)、[使用手册](docs/USER_GUIDE_CN.md)和[未来开发路线](docs/DEVELOPMENT_ROADMAP_CN.md)。本文件只保留最短构建与运行入口。

## 环境要求

- Windows 10/11 或 Ubuntu 24.04
- Vulkan SDK，项目目标 Vulkan 1.3
- CMake 3.20+
- Ninja
- GCC/MinGW 13+ 或受 CI 支持的 MSVC
- vcpkg，依赖基线由 `vcpkg.json` 锁定

Windows 已验证基线：Vulkan SDK 1.4.350.0、GCC/MinGW 13.1、Ninja 1.13.2。

## 构建

```powershell
$env:VULKAN_SDK = "C:\VulkanSDK\1.4.350.0"
$env:VCPKG_ROOT = "C:\path\to\vcpkg"

.\tools\configure_windows.ps1 -Config Debug
.\tools\configure_windows.ps1 -Config Release
cmake --install build\ninja-debug --prefix build\install-debug
cmake --install build\ninja-release --prefix build\install-release
```

如果本机 preset 无法找到工具链，参考 [开发指南](docs/DEVELOPMENT_GUIDE_CN.md) 使用显式 `cmake -S/-B` 配置。

## 最短运行路径

公共角色 smoke：

```powershell
.\build\ninja-debug\AzureRender.exe --smoke-frames 120
```

黑洞场景：

该路径使用私有 raw trace、双 history TAA、受控 HDR bloom 和最终 Scene Color composite。

```powershell
.\build\ninja-debug\AzureRender.exe --scene-type blackhole --smoke-frames 120
```

P2 角色展示预设：

```powershell
.\build\ninja-debug\AzureRender.exe `
  --asset .\assets_public\test_model.gltf `
  --qa-camera full-body-front `
  --qa-light stylized-key `
  --smoke-frames 120
```

`stylized-key` 对应稳定预设 `Endfield Industrial`，会成套应用灯光、grade、Bloom 和描边设置；Capture manifest 同时记录数值 ID 与预设名称。

创建并打开编辑器场景：

```powershell
.\build\ninja-debug\AzureRender.exe `
  --asset .\assets_public\test_model.gltf `
  --create-scene .\build\ninja-debug\public.azscene

.\build\ninja-debug\AzureRender.exe `
  --editor .\build\ninja-debug\public.azscene
```

确定性截图：

```powershell
.\build\ninja-release\AzureRender.exe `
  --scene-type blackhole `
  --width 1280 --height 720 `
  --capture-dir .\captures\blackhole\blackhole_beauty_temporal_v1_1280x720_20260818 `
  --capture-frames 1 --capture-fps 60
```

GPU timing：

```powershell
.\build\ninja-release\AzureRender.exe `
  --scene-type blackhole `
  --gpu-timing `
  --gpu-timing-output .\captures\blackhole\blackhole_timing_rtx4060_1280x720.json `
  --smoke-frames 300
```

## 命令行入口

运行 `AzureRender.exe --help` 可查看稳定的命令行参考。常用参数如下：

| 参数 | 用途 |
|---|---|
| `--scene-type character|blackhole` | 选择场景渲染器 |
| `--asset <gltf/glb>` | 指定角色资产 |
| `--environment <hdr/png/jpg>` | 指定环境贴图 |
| `--scene <azscene>` | 加载场景 |
| `--create-scene <azscene>` | 创建场景文件 |
| `--editor <azscene>` | 启动编辑器 |
| `--smoke-frames <N>` | 运行固定帧数后退出 |
| `--width/--height` | 固定输出尺寸 |
| `--capture-dir/--capture-frames/--capture-fps` | 确定性捕获 |
| `--gpu-timing` | 启用 GPU pass timing |
| `--diagnostic-view beauty|normal|outline|shadow` | 诊断视图 |
| `--render-path traditional|subpasses|dynamic` | 论文三路径基准选择 |
| `--check-resources` | 检查安装/开发树资源 |
| `--version` | 输出版本 |
| `--help` | 输出命令行参考 |

参数范围和错误行为由 `src/app/CommandLine.cpp` 及自动化测试定义，不再另行维护一份容易漂移的参数手册。

## 角色展示控制

五套角色 Look 由版本化的 `assets_public/showcase_looks.json` 驱动，编辑器可独立控制背景、地台与 Face SDF。详细约束见 `docs/CHARACTER_LOOKS_CN.md`。

| 按键 | 行为 |
|---|---|
| `1`-`5` | 固定全身、方向和脸部机位 |
| `F1`-`F3` | 展示预设 |
| `F4` | 暂停/继续动画 |
| `F5`/`F6` | 调整 diffuse band |
| `F7`/`F8` | 调整 style mask |
| `F9` | 切换风格化光照 |
| `F10` | 切换内部描边 |
| `F11` | 重启动画 |
| `F12` | 保存 PNG |
| `H` | 显示/隐藏 HUD |

## 质量门禁

```powershell
ctest --test-dir build\ninja-debug --output-on-failure
cmake -DBUILD_DIR="$PWD/build/ninja-debug" -DCONFIG=Debug -P tools/run_release_gate.cmake
```

完整的 Debug、Release、Validation、视觉检查和提交规则见 [开发指南](docs/DEVELOPMENT_GUIDE_CN.md)。

## 资产与发布

- `assets_public/`：公共测试和发布资产。
- `assets_private/`：受限测试资产，不得进入公开发布包。
- `portfolio/`：按场景整理的公共代表图、证据 JSON 和 SHA-256 manifest。
- `captures/`、`build/`：可再生成内容，不进入版本控制。
- 第三方许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
