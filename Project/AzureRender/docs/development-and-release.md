# 开发、测试与发布

AzureRender 按修改风险选择测试。纯数据或 CLI 变更通常可以用单元测试覆盖。Vulkan 生命周期变更需要运行 Debug Validation。

R3 工作线程录制可用 `--disable-parallel-recording` 切换回主线程录制，用于同条件视觉和性能比较。当前工作线程路径覆盖 Character 蒙皮、GPU 剔除、阴影和主场景。主线程按图顺序执行这些命令缓冲。

Character 不透明绘制使用 GPU 剔除与间接参数。`--disable-gpu-culling` 切回 CPU 可见性和直接绘制；缺少 Compute 或 drawIndirectFirstInstance 能力时自动使用该路径。透明排序和轮廓仍使用 CPU 可见列表。

`--disable-multi-draw-indirect` 强制每次提交一个间接命令，用于验证不支持批量间接的设备路径。

性能对照使用 `--fixed-frame-step` 固定模拟步长为 `1/capture-fps`，并给出相同相机、实例数和帧数。该开关不生成截图，适合独占运行的吞吐采样。

固定步长与 Capture 模式同时使用累计模拟时间作为 SceneFrameData.timeSeconds，首帧为零。普通交互运行继续使用平台时钟。

视觉算法变更还要生成固定 Capture，并进行人工对照。发布候选必须检查安装树和隔离运行时。

## 开发环境

Windows 基线：

- Windows 10/11。
- Vulkan SDK 1.4.350.0，目标 Vulkan 1.3。
- CMake 3.20+、Ninja 1.13+。
- MinGW GCC 13.1、C++17。
- vcpkg manifest 和 `x64-mingw-dynamic`。

Linux CI 使用 Ubuntu 24.04、GCC、Ninja、Mesa Lavapipe/Xvfb 和 Vulkan Validation。`vcpkg.json` 固定依赖版本。功能开发不要顺便升级 SDK 或依赖基线。

## 构建配置

Debug：

- 启用 `AZURERENDER_ENABLE_VALIDATION`。
- 保留诊断和断言，作为生命周期与 API 正确性基线。
- 适合 Smoke、编辑器和资源重载检查。

Release：

- 关闭 Validation Layer 运行时依赖。
- 用于 GPU Timing、正式 Capture 和发布安装树。
- Release 通过不代表 Debug Validation 可以跳过。

配置命令见[构建与使用](getting-started.md)。所有编译器都启用严格警告，并把警告视为错误。不要为了绕过一个警告而全局降低等级。

### MSVC 头文件依赖追踪

MSVC 构建通过 `tools/msvc_env.bat` 配置和编译。Ninja 用 cl 的 `/showIncludes` 输出追踪头文件依赖，CMake 记录的匹配前缀一旦与编译器实际输出失配，改动头文件就不会触发重建。配置阶段会自动验证这个匹配。出现前缀警告时，运行 `python tools/fix_msvc_deps_prefix.py <build 目录>` 再重新配置。

## 代码边界

### 修改宿主

适用于 Swapchain、公共 Attachment、Frame Sync、Capture、最终合成和 Renderer 接口。修改前回答：

- Handle 的创建者和销毁者是谁？
- 是否依赖 Swapchain Size/Format？
- 是否影响全部 Renderer？
- 是否需要 History Reset 或 Descriptor 重写？
- 安装树是否还包含所需资源？

宿主变更至少回归 Character、Blackhole 和 Sample。

### 修改场景 Renderer

每个场景管理自己的 Pipeline、Descriptor、Buffer/Image 和算法状态。不要把场景私有资源放回 `AzureRenderApp`。如果多个场景都需要新的 Attachment，先扩展 `SceneRendererCapabilities` 和 `RenderContext`。公共资源由宿主统一创建。

### 修改 Shader Binding

必须同步：

1. GLSL `layout(binding=...)`。
2. Descriptor Set Layout。
3. Descriptor Pool 数量。
4. Descriptor Write。
5. 回退资源。
6. Shader 编译和 Debug Validation。
7. [参数与接口参考](reference.md)。

只改 Shader 或只改 C++ 都可能在部分驱动上表现为黑图、错误采样或 Validation Error。

### 修改数据 Schema

Schema 版本只在语义不兼容时递增。变更需要：

- 新字段默认值。
- 旧版本迁移。
- 未知未来版本拒绝。
- Round-trip 测试。
- JSON Schema、Loader 和文档同步。

不能通过默默忽略未知字段来伪造兼容性。

## 新增 Renderer

以 `SampleSceneRenderer` 为起点：

1. 实现稳定小写 `name()`。
2. 用 `capabilities()` 声明 Depth/Normal 和诊断视图。
3. 在 `onLoad()` 创建自有资源。
4. 在 `updateFrame()` 更新 CPU/Uniform 状态。
5. 在 `recordScene()` 只记录到当前 Command Buffer。
6. 在 `onSwapchainRecreate()` 重建尺寸相关对象。
7. 在 `onUnload()` 清理部分或完整初始化资源。
8. 注册到 `BuiltinRendererCatalog`。
9. 声明 Shader Feature 归属。
10. 添加 Registry、生命周期、Smoke 和安装资源测试。

普通帧禁止 `vkQueueWaitIdle`。Renderer 不能销毁 RenderContext 借出的任何 Handle，也不能假定 `sceneFramebuffer` 跨帧有效。

## 测试分层

### CTest

```powershell
ctest --test-dir .\build\ninja-debug --output-on-failure
ctest --test-dir .\build\ninja-release --output-on-failure
```

当前测试覆盖：

- CLI 值域、互斥项和错误码。
- ECS Entity/Component 生命周期。
- Editor Camera、Picking 和 Session 历史。
- `.azscene` v1 迁移与 v2 Round-trip。
- Runtime Diagnostics 与 GPU Capability Report。
- Resource Locator 开发/安装树搜索。
- Extension Registry 的重复 ID、依赖和 API 版本。
- 安装 Manifest Round-trip 与失败门禁。

这些测试不创建完整可见 Vulkan 场景，因此不能证明 Shader 和驱动路径正确。

### Debug Validation Smoke

```powershell
.\build\ninja-debug\AzureRender.exe --smoke-frames 120

.\build\ninja-debug\AzureRender.exe `
  --scene-type blackhole --smoke-frames 120
```

编辑器相关变更还要创建/打开公共 `.azscene`。Smoke 必须正常退出、无 Validation Error、无资源泄漏导致的销毁顺序错误。

### Shader 与资产验证

- CMake 编译全部 Shader。
- `build_toon_ramp_atlas.py --check` 验证生成资产。
- `validate_material_profiles.py` 验证公共 Material Profile。
- Face SDF 与 Brow 工具验证专用角色契约。
- `--check-resources` 验证开发树和安装树。

### 视觉回归

先用公共资产生成固定 Capture，再比较候选：

```powershell
python .\tools\compare_images.py reference.png candidate.png `
  --max-mean-error 0.005 `
  --max-changed-ratio 0.01 `
  --output comparison.json
```

阈值必须在测试前确定，不能为了让候选通过而临时放宽。视觉算法有意变化时，保存对比图、解释变化，再更新基准。

私有角色可以补充人工检查，但不能替代公共回归，因为其他开发者和 CI 无法访问它。

### GPU 性能

性能测试使用 Release 和 Timestamp Query。报告要写明 GPU、驱动、分辨率、Renderer、质量或 Look，以及 Render Path。还要记录帧数、Warm-up 和平均、最小、最大值。GPU Pass Timing 不是完整的 Frame Time。

## Release Gate

统一门禁：

```powershell
cmake -DBUILD_DIR="$PWD/build/ninja-release" `
  -DCONFIG=Release `
  -P .\tools\run_release_gate.cmake
```

发布门禁会检查配置、Shader、目标构建和 CTest。它也检查安装、Manifest、版本、资源和运行时。修改 GPU 路径或视觉效果后，还要在真实 GPU 上运行 Validation 和 Capture。

Windows Release 构建会生成可分发的压缩包，并完成隔离运行检查。Windows Debug 构建仅用于开发；Debug 门禁验证配置、构建、CTest 和安装清单，并在结果中标记 `development-only`。调试运行库依赖本机 Visual Studio 工具链。

## 安装树

```powershell
cmake --install .\build\ninja-release `
  --prefix .\build\install-release
```

安装内容：

- `bin/AzureRender.exe`。
- 平台运行时 DLL。
- 编译后的 SPIR-V Shader。
- `assets_public/`。
- 必需 Schema、许可和运行文档。
- 文件级 SHA-256 Install Manifest。

禁止包含：

- `assets_private/`。
- Build Cache、IDE 文件和 `captures/`。
- 私有角色截图、视频或派生模型。
- 本机绝对路径、凭据和临时日志。

Windows 使用：

```powershell
.\tools\verify_windows_runtime.ps1 `
  -Executable .\build\install-release\bin\AzureRender.exe
```

该检查应在隔离 PATH 下证明 EXE 能找到同目录 GLFW 和 MinGW Runtime。单独复制 EXE 不支持。

## 打包

文档和代码达到候选状态不等于必须立即打包。只有明确准备发行物时运行 CPack：

```powershell
cpack --config .\build\ninja-release\CPackConfig.cmake
```

包名由版本、系统和架构生成。不要手工添加 `final`、任务号或日期。生成后要记录文件大小、SHA-256、Commit 和构建环境，也要附上门禁摘要。

二进制包不包含 `portfolio/`。源码仓库可以保留公共视觉证据。

## 发布验收矩阵

| 门禁 | Windows | Linux CI |
| --- | ---: | ---: |
| Debug/Release 构建 | 必须 | 必须 |
| CTest | 必须 | 必须 |
| 公共 Character/Blackhole Smoke | 必须 | 必须 |
| 公共 Editor Smoke | 必须 | 必须 |
| Debug Validation | 必须 | 必须 |
| 安装树资源检查 | 必须 | 必须 |
| 隔离 PATH DLL 检查 | 必须 | 不适用 |
| 私有角色 QA | 可选补充 | 不执行 |
| 固定视觉 Capture | 视觉变更必须 | 可选 |
| GPU Timing | 性能声明必须 | 按 Runner 能力 |

发布证据优先使用 CI Artifact、JSON、Capture Manifest 和 SHA-256，不把整段终端日志粘贴进活动文档。

## Git 与文档工作流

开发开始前运行 `git status --short --branch`，保留用户已有改动。一次提交只覆盖一个可验收主题：

```text
feat(<phase>): <summary>
fix(<scope>): <summary>
docs(<scope>): <summary>
```

提交前：

1. 检查 `git diff` 和 `git diff --check`。
2. 运行与风险匹配的测试。
3. 更新唯一权威文档，不复制同一事实。
4. 确认没有 Capture、私有资产和本机路径。
5. 只暂存本任务文件。

文档站本地验证：

```powershell
python -m pip install -r .\requirements-docs.txt
mkdocs build --strict
mkdocs serve
```

GitHub Actions 在 Pull Request 中执行严格构建，在 `main` 分支构建成功后部署 Pages。

## Changelog 与历史

`CHANGELOG.md` 只记录用户或开发者能观察到的版本变化。不要把每次参数微调都写进去。长期有效的实现原理放在主题文档中。已完成阶段、旧验收和过程日志保留在 Git 历史或 `docs/archive/`，不进入网站主导航。

判断信息冲突时遵循：

1. 源码、Schema、CMake 和自动化测试。
2. 当前 GitHub Pages 文档。
3. `CHANGELOG.md`。
4. Archive 和旧 Git 历史。

## 后续路线

项目当前优先级是巩固可发布渲染器，而不是扩张无关场景。合理的后续候选包括：

- 系统化 Vulkan Debug Marker 和 Object Name，改善 RenderDoc 分析。
- 更明确的 Render Graph/Attachment 能力声明。
- GPU 内存预算、子分配和资源生命周期统计。
- 自动生成部分 CLI/Schema 参考，减少文档漂移。
- 使用公开授权角色资产建立可发布的高质量 Character 基线。
- Renderer 动态插件仅在 ABI、版本和卸载隔离方案完成后再启用。

论文相关的 Render Path 扩展目前暂停，等待项目所有者主动恢复。工业科幻场景不在当前需求中。Blackhole P1 已冻结。Character P2 继续处理材质正确性、光照稳定和公开证据。
