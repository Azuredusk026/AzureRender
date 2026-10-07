# 构建复现与素材准入

> 文档类型：运行说明
> 适用范围：Windows、MSVC、Ninja 与独立 Player

## 构建与产物关联

在工程目录设置 `VULKAN_SDK` 与 `VCPKG_ROOT`。`msvc_env.bat` 初始化 MSVC，并保留调用者指定的 vcpkg 路径。

```powershell
.\tools\msvc_env.bat cmake --preset msvc-debug
.\tools\msvc_env.bat python tools/build_provenance.py build --build-dir build/ninja-msvc-debug --config Debug
.\tools\msvc_env.bat cmake --preset msvc-release
.\tools\msvc_env.bat python tools/build_provenance.py build --build-dir build/ninja-msvc-release --config Release
python tools/build_provenance.py verify --build-dir build/ninja-msvc-release
```

构建记录保存源码与四个可执行文件的哈希。
产品包含编辑宿主、Player、元数据与几何编译器。
记录还包含 Git 状态和 CMake 工具链。
构建期间源码变化会导致记录失败。

验证时源码或产物变化会返回非零退出码。

记录覆盖源码、着色器、工具、模式与公开资产。
它还覆盖 `managed`、构建配置和托管许可来源。
启用托管模块时，记录包含程序集与原生库。
私有素材通过单独的准入报告关联。

## 回归入口

```powershell
ctest --test-dir build/ninja-msvc-debug -C Debug --output-on-failure
.\tools\msvc_env.bat cmake -DBUILD_DIR=build/ninja-msvc-release -DCONFIG=Release -P tools/run_release_gate.cmake
ctest --test-dir build/ninja-msvc-release -C Release --show-only=json-v1
ctest --test-dir build/ninja-msvc-release -C Release -LE gpu --output-on-failure
```

公共回归使用 `-LE gpu`。完整本机验收使用实际 Vulkan 设备，执行全部测试和发布门禁。Release 测试清单包含 `AzureEngine.GamePackage`，适用于 Ninja 与多配置生成器。

Windows CI 分别构建 Debug 和 Release，检查测试发现、公共回归和安装清单。真实 GPU 的视觉、游戏包与性能结果记录在本机阶段证据中。

## 素材准入

可选离线工具使用固定版本 ufbx。准备脚本下载经过哈希校验的源码和许可证，再通过 MSVC 编译。

```powershell
.\tools\msvc_env.bat python tools/prepare_fbx_tool.py
python tools/character_intake.py --asset "$env:AZURE_CHARACTER_ASSET" `
  --idle "$env:AZURE_IDLE_SOURCE" --walk "$env:AZURE_WALK_SOURCE" `
  --output build/f1/character-intake.json
```

三个环境变量分别指向主角 GLB、待机 FBX 和行走 FBX。报告记录来源哈希、骨架指纹、复杂度、动画时长和采样率。许可范围由素材来源记录确认，用户提供的素材默认用于本机验收。

FBX 工具输出米制、Y 轴向上的局部姿态。每段动画以 30 Hz 采样，保留结尾。骨架不同的片段进入离线重定向，运行时读取 glTF 2.0。
