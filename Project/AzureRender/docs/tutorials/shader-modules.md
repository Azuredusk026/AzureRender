# 着色模块对照教程

> 文档类型：教程
> 范围：Windows、MSVC 与 Slang 2026.8

## 准备工具链

在工程目录运行下列命令。
Vulkan SDK 提供 `slangc` 与 `glslc`。
`VCPKG_ROOT` 指向项目依赖工具链。
编译器版本必须为 2026.8。

```powershell
& "$env:VULKAN_SDK/Bin/slangc.exe" -version
.\tools\msvc_env.bat cmake -S . -B build/ninja-msvc-debug -DAZURE_ENABLE_SHADER_MODULE_PROTOTYPE=ON
.\tools\msvc_env.bat cmake --build build/ninja-msvc-debug --target AzureShaderModuleTests AzureShaderCompositionTests
ctest --test-dir build/ninja-msvc-debug -C Debug -R '^AzureRender.Shader(Module|Composition)$' --output-on-failure
```

测试输出应报告两项通过。
图像与成本记录位于 `build/ninja-msvc-debug/shader-composition.json`。
该夹具验证真实 Vulkan 同步和资源释放。

## 校验共享类型

类型修改通过共享描述完成。
生成完成后检查 C++ 和着色声明。
需要改变成员类型时，同时更新使用方。

```powershell
python tools/generate_shader_types.py --description shaders/modules/shared-layout.json --cpp src/render/ShaderSharedTypes.hpp --slang shaders/modules/SharedTypes.slang
python tools/generate_shader_types.py --description shaders/modules/shared-layout.json --cpp src/render/ShaderSharedTypes.hpp --slang shaders/modules/SharedTypes.slang --check
python tools/test_shader_module.py
python tools/test_shader_composition.py
```

反射偏移或数组跨度不一致会导致编译失败。
失败候选保留既有有效产物。
诊断包含出错的编译或布局契约。

## 在场景中检视变体

构建编辑宿主后准备独立着色目录。
复制完整程序集合，再放入选定的 Bloom 变体。
资源入口会加载该目录中的程序。

```powershell
.\tools\msvc_env.bat cmake --build build/ninja-msvc-debug --target AzureRender AzureShaderModulePrototypes
New-Item -ItemType Directory -Force build/slang-preview/shaders
Copy-Item build/ninja-msvc-debug/shaders/* build/slang-preview/shaders
Copy-Item build/ninja-msvc-debug/shader-modules/bloom-direct.spv build/slang-preview/shaders/bloom_downsample.comp.spv -Force
build/ninja-msvc-debug/AzureRender.exe --resource-root build/slang-preview --scene-type blackhole --width 1280 --height 720
```

选择 `bloom-cached.spv` 可检视缓存策略。
算法、参数和描述符绑定使用相同契约。
正式对照应固定相机、时间与渲染质量。

下面的工具采集四帧图像与 120 帧计时。
它验证真实同步、图像等价和资源释放。
输出目录须为全新目录。

```powershell
python tools/run_shader_scene_comparison.py --executable build/ninja-msvc-debug/AzureRender.exe --shaders build/ninja-msvc-debug/shaders --modules build/ninja-msvc-debug/shader-modules --output build/shader-scene-comparison
```

使用 `--driver` 指定核显的驱动清单。
完整定义见[着色模块与共享类型](../runtime/shader-modules.md)。
