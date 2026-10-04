---
type: "Fragment"
id: Project/build_release
title: "构建、资源定位与发布门禁"
description: "工程如何配置与编译、运行期如何跨开发树/构建树/安装树定位资源、什么证据才算通过发布门禁，以及哪些资产禁止随发行物分发。"
parent: /Project/_overview.md
fragment: build_release
entity_names:
  constants:
    - name: 项目版本（CMake PROJECT VERSION）
      value: "0.1.0"
      source: Project/AzureRender/CMakeLists.txt
    - name: 应用版本宏 AZURERENDER_VERSION
      value: "0.1.0-rc1"
      source: Project/AzureRender/CMakeLists.txt
    - name: cmake_minimum_required
      value: "3.20"
      source: Project/AzureRender/CMakeLists.txt
    - name: CMAKE_CXX_STANDARD
      value: "17"
      source: Project/AzureRender/CMakeLists.txt
    - name: vcpkg manifest baseline
      value: "2bc124dac8436f5d8e108273aacf3f68d9824185"
      source: Project/AzureRender/vcpkg.json
    - name: vcpkg 依赖清单
      value: "glfw3, stb, openexr, tinygltf, vulkan-memory-allocator"
      source: Project/AzureRender/vcpkg.json
    - name: MinGW vcpkg triplet
      value: "x64-mingw-dynamic"
      source: Project/AzureRender/CMakePresets.json
    - name: MSVC vcpkg triplet
      value: "x64-windows"
      source: Project/AzureRender/docs/getting-started.md
    - name: 发布门禁阶段（Release）
      value: "configure, build, test, install, version, resources, isolated-runtime, package, manifest"
      source: Project/AzureRender/tools/run_release_gate.cmake
    - name: 发布门禁阶段（Debug）
      value: "configure, build, test, install, write-install-manifest, verify-install-manifest（结果标记 development-only）"
      source: Project/AzureRender/tools/run_release_gate.cmake
    - name: 视觉回归严格容差
      value: "maxMeanError 0.0 / maxChangedRatio 0.0 / pixelThreshold 0.0"
      source: Project/AzureRender/tools/visual_regression_cases.json
    - name: 视觉回归宽松容差
      value: "maxMeanError 0.006 / maxChangedRatio 0.02 / pixelThreshold 0.0078431373"
      source: Project/AzureRender/tools/visual_regression_cases.json
    - name: 视觉回归默认输出
      value: "1280 × 720，captureFrames 1，qa-light stylized-key"
      source: Project/AzureRender/tools/visual_regression_cases.json
    - name: 自动化基线目录
      value: "assets_public/baselines/character/"
      source: Project/AzureRender/docs/assets-and-editor.md
    - name: AZURERENDER_DEVELOPMENT_ROOT
      value: "构建期注入的开发树源码根，仅开发构建存在"
      source: Project/AzureRender/src/resources/ResourceLocator.cpp
    - name: 环境变量资源根覆盖
      value: "AZURERENDER_RESOURCE_ROOT"
      source: Project/AzureRender/src/resources/ResourceLocator.cpp
    - name: 编辑器布局文件
      value: "azurerender-editor.ini（本机文件，不入库）"
      source: Project/AzureRender/.gitignore
retrieval_hints:
  - "安装树里为什么不能单独复制 exe？"
  - "怎么运行发布门禁，它检查哪些阶段？"
  - "视觉回归用哪些用例，容差在哪里定义？"
  - "哪些目录禁止进入安装包和作品集？"
  - "新增一个 Shader 要怎么接入构建？"
  - "开发树、构建树和安装树的资源是怎么找到的？"
  - "⚠️ 你要找的是 Shader 算法与画面效果本身，不在这里，在 Project_character / Project_blackhole"
  - "⚠️ 你要找的是运行时命令行参数语义，不在这里，在 Project_host"
  - "本子系统也叫构建与发布 / CI 门禁，需求里的『打包』『回归测试』『安装验证』都归这里"
  - "新增文档页面必须挂进 mkdocs.yml 导航，站点构建使用 --strict，未挂载页面等于不可检索"
  - "新增 Shader 必须同时接入 compile_shader 调用与对应 Renderer 的 Shader Feature 归属"
architectural_role: "工程与交付层，唯一允许定义验收判据的地方，禁止在此处放画面逻辑"
---

## 业务意图

这一层回答"怎样证明这次改动是可交付的"：它把配置、编译、测试、安装、资源定位、隔离运行、打包与清单串成一条可执行门禁，并用固定资产与固定机位的像素比对替代口头结论。它解决的业务问题是把"看起来没问题"变成可复现的证据，同时把私有授权资产挡在公开分发之外。

## 对外接口

| 接口 | 方向 | 关键字段 | 业务说明 | 入口符号 |
|------|------|---------|---------|---------|
| `AzureRender` 可执行目标 | 构建→交付 | `src/main.cpp` 与 `src/**` 源文件清单 | 唯一主目标；`AZURERENDER_VERSION`、`AZURERENDER_DEVELOPMENT_ROOT`、`AZURERENDER_ENABLE_VALIDATION` 由它注入 | `Project/AzureRender/CMakeLists.txt` |
| `AzureRenderShaders` 自定义目标 | 构建→交付 | 每个 GLSL 一条 `compile_shader` 调用 | GLSL → `glslc` → `build/shaders/*.spv`，再进 `VkShaderModule` | `Project/AzureRender/CMakeLists.txt` |
| Shader 变体 | 构建→运行时 | `AZURE_COMPUTE_SKINNING`、`AZURE_BINDLESS`、`AZURE_COMPUTE_BLOOM` | 生成 `mesh_compute.vert`、`mesh_bindless.frag`、`shadow_bindless.frag`、`background_bindless.frag`、`outline_compute.vert`、`blackhole_taa_compute.frag` 等 | `Project/AzureRender/CMakeLists.txt` |
| CTest 套件 | 门禁→CI | 33 个 `AzureRender.*` 用例（含 NullRhi/NullRhiPass/VisualRegression/\*Gpu/InstallManifestRoundTrip） | 无窗口契约测试与 GPU 用例分层，`ctest --output-on-failure` 为默认入口 | `Project/AzureRender/CMakeLists.txt` |
| `ResourceLocator` | 运行期→资源 | `--resource-root`、`AZURERENDER_RESOURCE_ROOT`、可执行文件目录、`../share/AzureRender`、开发树、当前目录 | 按顺序查找；找不到时抛异常并列出所有已尝试路径 | `src/resources/ResourceLocator.cpp` |
| `--check-resources` | CLI→门禁 | 无 | 检查开发树/安装树资源是否完整 | `src/resources/ResourceLocator.cpp` |
| `run_release_gate.cmake` | 门禁→报告 | `BUILD_DIR`, `CONFIG`, 结果 JSON | 逐阶段执行并在失败处停止；Debug 结果标记 `development-only` | `tools/run_release_gate.cmake` |
| `write_install_manifest.cmake` / `verify_install_manifest.cmake` | 门禁→安装树 | 文件级 SHA-256 | 安装清单写入与校验，缺失或哈希不符即失败 | `tools/*.cmake` |
| `verify_windows_runtime.ps1` | 门禁→Windows | `-Executable` | 在隔离 PATH 下证明 EXE 能找到同目录 GLFW 与 MinGW Runtime | `tools/verify_windows_runtime.ps1` |
| `run_visual_regression.py` + `visual_regression_cases.json` | 门禁→像素 | 用例名、`isolation`、`qaCamera`、`ciEnabled`、容差档 | 公共资产上逐用例比图，`--update-baseline` 才允许重写基线 | `tools/run_visual_regression.py` |
| `compare_images.py` | 门禁→像素 | `--max-mean-error`, `--max-changed-ratio`, `--output` | 黑洞与通用对比入口；阈值必须在测试前确定 | `tools/compare_images.py` |
| `validate_material_profiles.py` | 工具→资产 | glTF 路径 | 按 Schema 校验公共 Material Profile | `tools/validate_material_profiles.py` |
| `build_toon_ramp_atlas.py --check` | 工具→资产 | `--check` | 校验生成的 Ramp 图集与 JSON 源数据一致 | `tools/build_toon_ramp_atlas.py` |
| `audit_face_sdf_compatibility.py` | 工具→资产 | `--require-compatible` | Face SDF 纹理、通道、方向与 Head Node 审计 | `tools/audit_face_sdf_compatibility.py` |
| `audit_brow_mesh.js` | 工具→资产 | 网格拓扑与骨骼影响 | 眉毛/睫毛小岛拓扑、退化三角形与权重审计 | `tools/audit_brow_mesh.js` |
| `verify_portfolio.ps1` | 工具→证据 | 无参数 | 校验 `portfolio/` 图片命名、Manifest 与 SHA-256 | `tools/verify_portfolio.ps1` |
| `inspect_capture_stats.py` / `build_qa_contact_sheet.py` / `encode_capture.ps1` | 工具→证据 | 捕获目录 | 捕获统计、接触表与视频编码 | `tools/` |
| `check_doc_style.py` / `check_docs.sh` | 门禁→文档 | 无参数 | 文档行文与链接检查 | `tools/check_docs.sh` |
| `mkdocs build --strict` + `.github/workflows/docs.yml` | 门禁→站点 | `nav` 顺序 | PR 严格构建、main 分支构建部署 Pages | `Project/AzureRender/mkdocs.yml` |
| `fix_msvc_deps_prefix.py` | 工具→构建 | 构建目录 | 修复 `/showIncludes` 前缀失配导致的头文件依赖失效 | `tools/fix_msvc_deps_prefix.py` |

## 跨模块依赖

| 依赖 | 引用原因 | 关键符号 | confidence |
|------|---------|---------|------------|
| `Project_host` | 门禁通过 CLI 参数驱动 smoke/Capture/Timing | `parseCommandLine`, `--smoke-frames` | extracted |
| `Project_character` / `Project_blackhole` | 视觉回归与性能采集的用例对象 | `visual_regression_cases.json` | extracted |
| `Project_render_core` | Shader 变体宏（`AZURE_COMPUTE_SKINNING`/`AZURE_BINDLESS`/`AZURE_COMPUTE_BLOOM`）与 Descriptor 路径一一对应 | `RenderContext::bindlessTextures` | extracted |
| `Project_scene_editor` | `.azscene` 与 `assets_public/scenes/*.azscene` 参与场景门禁 | `SceneDocument::load` | extracted |
| `Project_renderer_sdk` | `shaderFeatures()` 与构建的 Shader 清单需一致 | `ShaderFeatureDescriptor` | inferred |
| 仓库根 `.github` | 根级 `ci.yml` / `documentation.yml` 复用本工程的构建与文档门禁 | — | inferred |

> 反向依赖（谁调用了本子系统）：

| 调用方 | 调用场景 | 关键符号 |
|--------|---------|---------|
| `Project_host` | 运行期经 `ResourceLocator` 定位 shader/资产/海报目录 | `ResourceLocator::shaderDirectory` |
| CI 与发布流程 | 严格执行配置、构建、测试、安装、清单与打包 | `run_release_gate.cmake` |
| 文档站 | `mkdocs.yml` 的 `nav` 决定页面可检索范围 | `mkdocs.yml` |

## 典型调用链

```
tools/configure_windows.ps1 -Config Release
  → cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=<vcpkg> -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic
    → compile_shader(...)（mesh/shadow/outline/background 的 AZURE_COMPUTE_SKINNING 与 AZURE_BINDLESS 变体）
      → ctest --output-on-failure
        → AzureRender.exe --smoke-frames 120（Debug Validation）
cmake -DBUILD_DIR=... -DCONFIG=Release -P tools/run_release_gate.cmake
  → 阶段 configure → build → test → install → version → resources
    → isolated-runtime（verify_windows_runtime.ps1）→ package → manifest
      → verify_install_manifest.cmake（文件级 SHA-256）
视觉变更：
  tools/run_visual_regression.py（用例来自 tools/visual_regression_cases.json）
    → AzureRender.exe --qa-* --capture-dir … → 与 assets_public/baselines/character/ 比较
```

## 契约测试路由

> 改动先查此表定位必须跑的测试；测试名即 CTest `add_test` 名。所有用例不创建可见窗口，因此不能代替 GPU 验收。

| 测试文件 | CTest 名 | 断言的契约 | 归属主题 |
|----------|---------|-----------|---------|
| `tests/CommandLineTests.cpp` | `AzureRender.CommandLine` | 参数值域、互斥项与错误码 | `Project_host` |
| `tests/RuntimeDiagnosticsTests.cpp` | `AzureRender.RuntimeDiagnostics` | 诊断级别、退出码与事件流 | `Project_host` |
| `tests/GpuCapabilityReportTests.cpp` | `AzureRender.GpuCapabilityReport` | JSON 结构与 Schema 一致 | `Project_host` |
| `tests/SurfaceLifecycleTests.cpp` | `AzureRender.SurfaceLifecycle` | 零尺寸等待与关闭退出路径 | `Project_host` |
| `tests/NullRhiTests.cpp` | `AzureRender.NullRhi` | RHI 调用序列与屏障参数 | `Project_render_core` |
| `tests/NullRhiPassTests.cpp` | `AzureRender.NullRhiPass` | 级联 viewport 与阴影 Pass 录制；`Registration must not record commands` | `Project_render_core` |
| `tests/RenderGraphTests.cpp` | `AzureRender.RenderGraph` | 写者先于读者、状态转换顺序、非法 ID 拒绝 | `Project_render_core` |
| `tests/TransientResourcePoolTests.cpp` | `AzureRender.TransientResourcePool` | 瞬态资源复用与退休时机 | `Project_render_core` |
| `tests/ComputePassTests.cpp` | `AzureRender.ComputePass` | 工作组向上取整与禁用空录制 | `Project_render_core` |
| `tests/LightBufferTests.cpp` | `AzureRender.LightBuffer` | 按 `stableId` 排序与上限截断 | `Project_render_core` |
| `tests/ClusteredLightGridTests.cpp` | `AzureRender.ClusteredLightGrid` | 聚簇分配与对数深度分层 | `Project_render_core` |
| `tests/CascadedShadowTests.cpp` | `AzureRender.CascadedShadow` | 级联分割参数合法与 `splits.back()==farPlane` | `Project_render_core` |
| `tests/UploadRingTests.cpp` | `AzureRender.UploadRing` | 环分段、对齐与越界/非 2 次幂拒绝 | `Project_render_core` |
| `tests/FrameTaskSchedulerTests.cpp` | `AzureRender.FrameTaskScheduler` | `order` 稳定排序的确定性执行 | `Project_render_core` |
| `tests/EnvironmentAssetTests.cpp` | `AzureRender.EnvironmentAsset` | HDR/EXR/LDR 通道、数据窗口与错误信息 | `Project_render_core` |
| `tests/SceneModelTests.cpp` | `AzureRender.SceneModel` | `.azscene` 迁移、往返与原子保存 | `Project_scene_editor` |
| `tests/SceneGraphTests.cpp` | `AzureRender.SceneGraph` | 局部/父子变换、循环节点与光源序列化 | `Project_scene_editor` |
| `tests/EcsTests.cpp` | `AzureRender.Ecs` | Entity 与 Component 生命周期 | `Project_scene_editor` |
| `tests/EditorSessionTests.cpp` | `AzureRender.EditorSession` | 命令可用性、Undo/Redo 与 Capture 标签 | `Project_scene_editor` |
| `tests/EditorCameraControllerTests.cpp` | `AzureRender.EditorCameraController` | 相机输入（含 inactive 输入不更新相机） | `Project_scene_editor` |
| `tests/PickMathTests.cpp` | `AzureRender.PickMath` | 视口→节点反投影数学 | `Project_scene_editor` |
| `tests/ExtensionRegistryTests.cpp` | `AzureRender.ExtensionRegistry` | 重复 ID、依赖与 API 版本拒绝 | `Project_renderer_sdk` |
| `tests/ResourceLocatorTests.cpp` | `AzureRender.ResourceLocator` | 开发树与安装树搜索顺序 | `Project_build_release` |
| `tests/SceneGraphTests.cpp`（实例与剔除部分）、`tests/NullRhiPassTests.cpp` | `AzureRender.ClusteredLightingGpu`、`AzureRender.CharacterMorphComputeGpu`、`AzureRender.VisualRegression*`、`AzureRender.BlackholeComputeGpu`、`AzureRender.CharacterIblComputeGpu`、`AzureRender.CharacterSkinningComputeGpu` | GPU/视觉用例：需真实设备或 Lavapipe，按固定 Capture 与基线比对 | `Project_character` / `Project_blackhole` |
| `tools/test_install_manifest_roundtrip.cmake` | `AzureRender.ReleaseGateMissingBuild`、`AzureRender.InstallManifestRoundTrip` | 构建缺失即失败；清单往返一致 | `Project_build_release` |

## 实现约束清单

### 必须定义的常量/枚举

| 标识符 | 值 | 所在文件 | 说明 | 约束由来 |
|-------|----|---------|------|---------------------|
| `AZURERENDER_VERSION` | `0.1.0-rc1` | `CMakeLists.txt` | 应用版本宏 | 包名与版本报告以此为准；`--version` 输出同源 |
| `AZURERENDER_ENABLE_VALIDATION` | Debug 定义 / Release 不定义 | `CMakeLists.txt` | Validation Layer 开关 | Release 默认不启用，最终用户无需安装 SDK Layer |
| `AZURERENDER_DEVELOPMENT_ROOT` | 开发构建注入 | `CMakeLists.txt` | 开发树资源根 | 安装构建不得依赖源码绝对路径（来源 commit：`建立渲染设置与 Face SDF 资产契约`） |
| `AZURERENDER_TEST_SOURCE_DIR` | 测试编译定义 | `CMakeLists.txt` | 测试定位源树资产 | 让 CTest 在不依赖当前工作目录的前提下找到公共资产 |
| 视觉回归严格/宽松容差 | `0 / 0 / 0`；`0.006 / 0.02 / 0.0078431373` | `visual_regression_cases.json` | 像素判据 | 阈值必须在测试前确定，不能为了让候选通过而放宽 |
| vcpkg baseline | `2bc124dac8436f5d8e108273aacf3f68d9824185` | `vcpkg.json` | 依赖版本锚点 | 功能开发不顺便升级 SDK 或依赖基线 |

### 必须实现的函数/脚本

| 名称 | 所在文件 | 说明 |
|------|---------|------|
| `compile_shader` | `CMakeLists.txt` | 每个 GLSL 与每个宏变体各一次；新增 Shader 必须同时登记 |
| `ResourceLocator::find` | `src/resources/ResourceLocator.cpp` | 失败时抛异常并列出全部已尝试路径，禁止静默回退到当前目录 |
| `write_install_manifest` / `verify_install_manifest` | `tools/*.cmake` | 文件级 SHA-256；校验失败即门禁失败 |
| `run_release_gate` 各阶段 | `tools/run_release_gate.cmake` | 阶段失败立即停止，结果 JSON 记录阶段清单 |
| `verify_windows_runtime.ps1` | `tools/verify_windows_runtime.ps1` | 隔离 PATH；单独复制 EXE 不受支持 |

### 存档/产物字段索引（不可裁减）

| 产物 | 字段 | 说明 |
|------|------|------|
| Install Manifest | 每个文件的相对路径 + SHA-256 | 安装树完整性依据 |
| Capture Manifest | 尺寸、帧、场景、RenderSettings、诊断状态、Renderer 自定义字段 | 让单张 PNG 可还原生成条件 |
| `portfolio_manifest.json` | 图像路径、参数、设备/构建信息、SHA-256 | 公开证据的可校验入口 |
| `run_release_gate` 结果 JSON | `version`、`platform`、`config`、`build_dir`、`install_tree`、`stages[]`、`status` | Debug 结果固定 `development-only` |

### 边界约束（能做 / 禁止）

- 禁止：安装树包含 `assets_private/`、构建缓存、IDE 文件、`captures/`、私有角色截图/视频/派生模型、本机绝对路径、凭据与临时日志（来源：`docs/development-and-release.md`）。
- 禁止：手工把 `final`、任务号（`P1`/`S36`/`CQ0`）或日期加进包名与作品集文件名；版本号只在有意改变画面基准时递增（来源：`portfolio/README_CN.md`）。
- 禁止：提交第三方角色网格、纹理、派生导出或可再分发归档（来源：`assets_private/README.md`）。
- 禁止：为绕过警告全局降低警告等级；所有编译器启用严格警告且警告即错误。
- 禁止：为了让候选通过而临时放宽视觉阈值，或用"模糊/降对比度"掩盖画面缺陷。
- 禁止：把自动生成的 API 页面或纯示例教程当作权威接口；接口冲突判定顺序为源码与 Schema/CMake/测试 > 现行文档站 > `CHANGELOG.md` > Archive 与旧 Git 历史。
- 禁止：文档撰写中把待实现功能写成现有能力。
- 边界：Linux CI 在 Mesa Lavapipe/Xvfb 下运行，隔离 PATH DLL 检查只在 Windows 适用；私有角色 QA 为可选补充，公共 CI 与作品集必须使用可再分发资产。
- 边界：二进制包不含 `portfolio/`；源码仓库保留公共视觉证据。
- 允许：Debug 构建用于 Smoke、编辑器与资源重载检查；Release 用于 GPU Timing、正式 Capture 与发布树。Release 通过不代表可跳过 Debug Validation。

### 设计决策

| 决策点 | 选定方案 | 备选方案 | 选定理由 |
|--------|---------|---------|---------|
| 资源定位 | 一个 `ResourceLocator` 服务开发树、构建树与安装树 | 开发与安装各走一套路径逻辑 | 同一套逻辑保证开发态与发布态行为一致；失败时列出候选路径便于定位 |
| Shader 编译 | CMake 显式列出每个 GLSL 与宏变体 | 运行时加载并编译源码 | 构建期失败即阻断，避免发布包含未编译 Shader |
| 安装验证 | 内容清单 + 文件级 SHA-256 | 只校验文件是否存在 | 可检出静默替换与遗漏 |
| 测试分层 | 无窗口契约测试进 CTest，GPU 与视觉用例单独命名 | 全部塞进默认 CTest | 无 GPU 环境仍可跑契约测试，同时不把 GPU 结果误当契约 |

## 变更风险

- 改 Shader 变体宏或 `compile_shader` 调用：运行时按名取 `.spv`，遗漏即 Pipeline 创建失败；同时 `shaderFeatures()` 与文档需同步。
- 改安装内容或清单规则：`ARC`/安装门禁与 `verify_windows_runtime.ps1` 会失败，发布包可能缺 DLL、SPIR-V 或 Schema。
- 改 `ResourceLocator` 搜索顺序：开发态能跑但安装态找不到资源，`--check-resources` 与隔离运行门禁是唯一有效检出手段。
- 改视觉基线或容差：把真实回归误判为通过；有意改画面时必须保存对比图、说明原因并更新清单中的 SHA-256。
- 改 `vcpkg.json` 或工具链基线：Windows MinGW 与 MSVC 两条 ABI 路径都要重新验证；imgui 由源码编译（1.92.8，docking）以保证与编译器 ABI 一致，改用包管理器版本会导致链接失败。
- 改 `mkdocs.yml` 的 `nav`：新文档页面不会出现在站点，`--strict` 也不会因未挂载而失败，信息静默丢失。

> 📄 本节内容来源于仓库内置文档：`Project/AzureRender/docs/development-and-release.md`、`Project/AzureRender/docs/getting-started.md`、`Project/AzureRender/docs/assets-and-editor.md`、`Project/AzureRender/docs/documentation-standard.md`、`Project/AzureRender/README.md`、`Project/AzureRender/portfolio/README_CN.md`、`Project/AzureRender/assets_private/README.md`（原文已提炼，非完整转录）
