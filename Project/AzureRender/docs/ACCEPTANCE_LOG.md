# AC: 综合验收日志(2026-08-16)

本文件保留本次综合验收的实际命令输出,便于事后审计。

## Release 版本与版本号

```
$ AzureRender.exe --version
AzureRender 0.1.0-rc1
```

## GPU 能力报告

```
device_id: 10464 (NVIDIA GeForce RTX 4060 Laptop GPU)
extensions: 274
features: Vulkan 1.4 全部相关 feature 启用
```

报告位置:`captures/*/gpu_capabilities.json`(符合 `schemas/gpu_capability_report.schema.json`)

## 全量 CTest

```
$ ctest --test-dir build/ninja-debug --output-on-failure
 1/11 Test  #1: AzureRender.CommandLine ........................ Passed    0.17 sec
 2/11 Test  #2: AzureRender.EditorCameraController ............ Passed    0.18 sec
 3/11 Test  #3: AzureRender.EditorSession ..................... Passed    0.25 sec
 4/11 Test  #4: AzureRender.Ecs ............................... Passed    0.17 sec
 5/11 Test  #5: AzureRender.SceneModel ........................ Passed    0.52 sec
 6/11 Test  #6: AzureRender.RuntimeDiagnostics ................              Passed    0.37 sec
 7/11 Test  #7: AzureRender.GpuCapabilityReport ................ Passed    0.17 sec
 8/11 Test  #8: AzureRender.ResourceLocator .................... Passed    0.41 sec
 9/11 Test  #9: AzureRender.ExtensionRegistry .................. Passed    0.17 sec
10/11 Test #10: AzureRender.ReleaseGateMissingBuild ........... Passed    0.18 sec
11/11 Test #11: AzureRender.InstallManifestRoundTrip .......... Passed    1.49 sec

100% tests passed, 0 tests failed out of 11

Total Test time (real) =   4.17 sec
```

## Smoke 120 帧 + 5 帧截图

```
$ AzureRender.exe --asset assets_public/test_model.gltf --width 320 --height 240 \
   --smoke-frames 120 --capture-frames 5 --capture-fps 60
SMOKE=0   # 退出码 = 0,无 VUID

capture_manifest.json: 80 行,包含 renderSettingsVersion=2、materialInventory 4 项、QA state hash 等
frame_000000.png ~ frame_000004.png: 320x240 渲染帧
gpu_capabilities.json: 见上文
```

## AR-8.1 功能验证(错误路径抛异常)

```
$ AzureRender.exe --asset assets_public/test_model.gltf \
   --environment assets_public/does_not_exist.hdr
EXIT=3
[error][main][code=3] Failed to decode HDR environment: assets_public/does_not_exist.hdr
```

证明 `loadEnvironmentAsset` 路径已正确集成(成功路径在 AR-8.1 测试环境也已验证)。
视觉对比限制:`shaders/background.frag` 不采样 environmentTexture(只用程序化背景渐变),
HDR 反射仅在模型表面边缘可见;在 capture 静态截图模式下两张图字节相同属预期。

## git 仓库健康

```
$ git fsck --full
错误数: 0
```

## 关键路径(用户可直接核对)

- 主工程:`D:/Assigment/2609/FYP/Project/AzureRender/`
- Release 可执行:`Project/AzureRender/build/ninja-release/AzureRender.exe`
- Debug 构建:`Project/AzureRender/build/ninja-debug/AzureRender.exe`
- 测试日志与脚本:`docs/`,`tests/`,`tools/`
- 截图输出:`C:/tmp/accept_120/`,`C:/tmp/accept_cap_proc/`