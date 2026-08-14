# 项目开发交接摘要

> 最后核对：2026-08-14（Asia/Shanghai）
> 主工程：`Project/AzureRender`
> 当前工程基线：**S36.2 HDR Scene Color + ACES fitted 已完成**
> 当前长期节点：**M2 Gate Review**

## 0. 长期路线入口

项目方向现由仓库根目录的 `MASTER_DEVELOPMENT_PLAN_CN.md` 统一管理，对应正式版为
`FYP_Master_Development_Plan_v2.0.docx`。该计划覆盖作品集、美术质量、Renderer 架构、
Multi-pass/Subpass/DRLR、Android、实验、论文与最终提交，优先级高于本文中的历史
“下一节点”记录。

CQ-0 与 CQ-1 已于 2026-08-02 通过，CQ-2～CQ-5 已于 2026-08-13 完成。
CQ-6 Outline/Grade 实现于 2026-08-14 完成，M2 技术门禁已于同日通过，当前执行顺序为：

`CQ-3 Face SDF/Overlays -> CQ-4 Hair KK -> CQ-5 Rim/Specular/Emissive/Bloom -> CQ-6 Outline/Lighting/Grade -> M2 Gate`

CQ-0 的操作说明见 `docs/CHARACTER_QA_HARNESS_CN.md`，CQ-1 的 Schema、分类、参数 ABI、莱万汀审计和证据见 `docs/MATERIAL_SYSTEM_V1_CN.md`。CQ-2 使用 `assets_public/toon_ramp_profiles.json` 与生成的 `toon_ramp_atlas.ppm`。CQ-3～CQ-5 已完成 Face SDF、Overlay、双层 Hair KK、Rim、Specular、Emissive 和 Bloom enabled/disabled/isolation 验收。CQ-6 新增 `RenderSettings v2`、Outline/Grade v1 和最终合成参数闭环；M2 技术门禁已完成，当前只需用户视觉确认。发布化/编辑器支线见 `docs/RENDERER_MODULARIZATION_PLAN_CN.md`。

## 0.1 任务前缀词典

- `M`（Milestone）：里程碑门禁，例如 `M2` 角色作品集质量 Gate、`M3` 工业场景整合。
- `CQ`（Character Quality）：角色画质垂直切片，例如 `CQ-3` Face SDF、`CQ-6` Outline/Grade。
- `AR`（Architecture / Renderer）：渲染器发布化与模块化，例如 `AR-1` Renderer Core Boundary、`AR-3` Editor Preview。
- `SC`（Scene）：工业科幻场景工作包，例如 `SC-0` Art Bible、`SC-5` 性能与裁剪。
- `S`（Stage / Historical Sequence）：早期 S1-S36.2 的历史实现阶段；用于追溯，不代表当前优先级。
- `D`（Decision / Design Record）：设计决策与风险记录，例如 `D12` Tone Mapping；不是可执行功能节点。

在 M2 通过前，S36.3 Exposure/正式性能、Subpass、DRLR、Android 和大型场景均保持
Pending/Deferred，不能成为主要开发节点。

## 1. 项目信息

项目名为 **AzureRender — Stylized Vulkan Character Renderer**。FYP 的研究题目是：

> Comparative Evaluation of Vulkan Subpasses and Dynamic Rendering Local Read for Real-Time NPR Rendering

最终研究目标是在相同 NPR 工作负载下，对比传统 Multi-pass、Vulkan Subpasses 和 Dynamic Rendering Local Read（DRLR）。当前代码仍处于“作品集优先”的桌面渲染器阶段，主要用于先建立稳定、可展示的 NPR 视觉基线。

技术栈：

- Windows 10/11、C++17、Vulkan 1.3、GLFW、CMake/Ninja；
- glTF 2.0 资产加载，依赖 tinygltf 与 stb；
- GLSL 由 Vulkan SDK 的 `glslc` 编译为 SPIR-V；
- 当前验证设备为 NVIDIA GeForce RTX 4060 Laptop GPU。

`AfterglowRender/` 是 MIT 许可的学习与架构参考，不是主工程，也不应被整体合并进 FYP 主代码。

## 2. 当前真实实现状态

### 已完成

- Vulkan instance/device/surface/swapchain、Debug Validation Layer、双帧并行、resize/recreate；
- device-local 顶点/索引缓冲、per-frame UBO、descriptor、深度测试；
- glTF/GLB 场景节点与 TRS/Matrix、多个 mesh primitive 和多个材质；
- Base Color、Normal、Metallic-Roughness、Specular/Emissive、自定义 Style Mask、Matcap、Hair Data；
- OPAQUE/MASK/BLEND、double-sided、透明 primitive 按视空间深度排序；
- 自动切线生成、模型 bounds 居中/取景、环境漫反射与近似反射；
- 倒壳描边、toon band、材质阴影色、脸部 Matcap、Kajiya–Kay 风格头发高光；
- 程序化圆形地台、接触压暗、全屏渐变背景、主/辅/轮廓三点式灯光；
- 固定全身/四向/脸部近景机位、风格开关与参数热键；
- 2048×2048 Alpha-aware Shadow Map、3×3 PCF 与地台接收阴影；
- GPU Skinning、四秒程序化 Idle、确定性 Portfolio Orbit；
- 深度/法线内部描边、诊断视图、GPU Timestamp 与 Renderer 原生 HUD；
- 固定时间步 PNG 捕获、Manifest、20 秒 Beauty/Technical H.264 视频；
- 公共自有测试资产与私有莱万汀动态角色均可运行。

S34 的作品集交付包已经可用，S35 已完成第一阶段 AzureRender 命名迁移。
当前私有角色运行时统计为 **81,487 vertices、284,673 indices、14 primitives、
15 materials**；公共测试资产为 **337 vertices、900 indices、3 primitives、
4 materials**（均包含运行时追加的地台）。

### 当前渲染架构

当前不是最终论文需要的三路径 deferred benchmark。实际代码使用：

- 独立 Directional Shadow Map Pass；
- Main Scene Render Pass，写入颜色、深度和世界法线；
- Post-process Render Pass，采样深度/法线合成内部描边或诊断视图；
- 最终叠加 Renderer 原生 HUD、章节标题和转场几何；
- 十二个 shader：mesh、outline、background、shadow、inner outline、HUD。

因此目前仍然**没有**可切换的 Traditional Multi-pass/Subpass/DRLR 三路径、
DRLR feature probe、正式实验 CSV 或 Android 端。现有 GPU Timestamp JSON 是
作品集性能诊断，不能描述成论文三路径比较已经完成。

### 已知限制

- 环境贴图是程序生成的 LDR equirectangular texture，不是 HDR IBL；
- 没有 HDR framebuffer、tone mapping、bloom、mipmap/prefiltered specular；
- 已支持 GPU Skinning 和骨骼动画，但没有 morph target；
- BLEND 只按 primitive 排序，没有 per-triangle sorting/OIT；
- Unreal Hair `_HN` 的 RG/BA 语义是基于资产证据的兼容还原，不是母材质逐节点复刻；
- 私有莱万汀资产不能提交或公开分发，授权未确认前只可本地验证；
- `Afterglow PNG sequence v1`、`afterglow*` glTF extras 和旧动画名属于暂时
  保留的 Legacy Schema，不应在普通品牌替换中破坏；
- 工作树包含连续开发产生的未提交修改，禁止用 reset/checkout 丢弃。

## 3. 目录与事实来源

- `src/app/AzureRenderApp.*`：主生命周期、渲染资源创建与交互；
- `src/app/AzureRenderInternal.hpp`：只含 inline 的矩阵数学与 Vulkan 结果检查；
- `src/app/AzureRenderFrame.cpp`：Draw、UBO/HUD 更新与 Command Recording；
- `src/app/AzureRenderPipeline.cpp`：Render Pass、Graphics Pipeline 与 Framebuffer 创建；
- `src/app/AzureRenderDescriptors.cpp`：Descriptor Layout、Pool 与 Set 创建/写入；
- `src/app/AzureRenderResources.cpp`：Image、Texture 与 GPU Buffer 资源创建；
- `src/app/AzureRenderSupport.cpp`：Swapchain 重建、设备查询、Buffer/Image、
  Shader Module、Framebuffer/Debug Callback 支持；
- `src/app/AzureRenderCapture.cpp`：捕获目录、Manifest、GPU Timing 与 Screenshot；
- `src/assets/GltfLoader.*`：glTF 数据与自定义材质 extras；
- `src/render/RenderSettings.*`：CLI、运行时、Capture 与未来 GUI 共用的版本化渲染设置；
- `shaders/`：当前十二个 GLSL shader；
- `tools/`：Unreal 导出、纹理转换与 glTF 注入工具；
- `assets_public/test_model.gltf`：可公开、可回归的自有测试资产；
- `assets_private/`：本地第三方角色及派生资产，禁止提交；
- `docs/DEVELOPMENT_LOG_CN.md`：S7–S35 的实现与 QA 记录；
- `docs/LAEVAT_ASSET_EXPORT_CN.md`：私有角色导出、材质映射和限制；
- `docs/HDR_TONEMAPPING_DESIGN_CN.md`：S36 HDR Scene Color、Tone Mapping 与验收方案；
- `docs/RENDERER_MODULARIZATION_PLAN_CN.md`：AR-0 至 AR-4 的 Renderer/Scene/Editor 发布化路线；
- 根目录 `FYP_Development_Plan_v1.3.docx`：完整研究路线；其中早期“starter app”描述已过时，实际代码状态以本文件、源码和最新开发日志为准。

## 4. 构建、运行与本次验证

工作站现用 Vulkan SDK：

```text
C:\VulkanSDK\1.4.350.0
```

现有构建目录：

```powershell
cd D:\Assigment\2609\FYP\Project\AzureRender
cmake --build build/ninja-debug
cmake --build build/ninja-release
```

公共资产回归：

```powershell
.\build\ninja-debug\AzureRender.exe --smoke-frames 120
```

私有角色回归：

```powershell
.\build\ninja-release\AzureRender.exe `
  --asset .\assets_private\laevat_static\laevat_static_material.glb `
  --smoke-frames 120
```

2026-07-25 的重新验证结果：

- Debug/Release：构建成功，`ninja: no work to do`；
- 公共资产 Debug：Validation Layer 开启，120 帧成功，进程退出码 0；
- 私有角色 Release：120 帧成功，进程退出码 0；
- 使用 GPU：NVIDIA GeForce RTX 4060 Laptop GPU。

交互键位见 `README.md`。最常用的是 `1` 全身、`5` 脸部近景、`F9` 风格开关、`F12` 截图。

## 5. 接下来建议按此顺序继续

1. **仓库与文档收口**
   - 确认 Git 仓库应放在 workspace root 还是 `Project/`，修复/初始化后再开发；
   - 更新 README 的当前节点为 S21，并保留公共/私有资产边界；
   - 不修改 `AfterglowRender/`，除非任务明确要求查阅参考实现。

2. **S22：有边界地完成展示预设**
   - 增加可重复的灯光/背景预设和一个终末地风格构图；
   - 使用固定热键切换，避免破坏 `1`、`5`、`F9` 和现有截图流程；
   - 同时用公共资产 Debug Validation 与私有角色 Release 各跑 120 帧；
   - 记录全身、近景、F9 对照图，然后停止继续无上限视觉调校。

3. **冻结作品集里程碑 D0**
   - 补 renderer 架构图、pass 图、公开截图/视频和第三方资产声明；
   - 把 S21/S22 作为可展示的 forward NPR 基线，不宣称已完成论文 benchmark。

4. **进入 FYP 研究主线**
   - 先做设备/扩展 feature probe，确认桌面与目标 Android 设备的 DRLR 支持；
   - 定义三路径共享的场景、shader 公式、attachment 格式、分辨率和测量协议；
   - 实现真正的 G-buffer Traditional Multi-pass 基线；
   - 再实现 Subpass 和 DRLR 路径；
   - 加入 GPU timestamps、metadata、CSV/JSON 输出与公平性回归测试。

## 6. 给接手 Agent 的工作规则

- 开始任务前先读本文件、README、开发日志最后一个节点和对应源码；
- 只把源码和实际运行结果当作完成依据，不把旧计划中的未来项当作已实现；
- 每个节点保持小改动：构建 Debug/Release，跑公共资产 Validation，再跑私有资产回归；
- 新功能必须有公共资产 fallback，不能让公开版本依赖 `assets_private/`；
- 不提交私有 GLB、纹理、截图或 Unreal 派生资源；
- 完成节点后同步更新 `README.md`、`DEVELOPMENT_LOG_CN.md` 和本交接摘要。

## 7. S35.3 最新结构与验证

本轮继续执行“只移动实现、不改变逻辑”的重构：

- `AzureRenderInternal.hpp`：唯一的 inline 数学 Helper 与 `vkCheck`；
- `AzureRenderFrame.cpp`：`drawFrame`、UBO/HUD 更新和 Command Recording；
- `AzureRenderApp.cpp`：降至 2,545 行，继续负责主生命周期、资源创建与交互；
- `AzureRenderSupport.cpp`、`AzureRenderCapture.cpp`：职责与 S35.2 相同；
- 所有文件仍实现同一个 `AzureRenderApp`，没有新增资源所有者或运行时组件。

2026-08-02 的验收证据：

- Ninja Debug 与 Release 构建成功；
- 公共测试资产 120 帧 Debug Validation 成功，退出码 0；
- `captures/s35_frame_beauty_regression/frame_000000.png` 与 S30 Beauty 基准
  SHA-256 完全相同：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- `captures/s35_frame_technical_probe` 包含 25 帧与完整五章 Manifest；
- 技术探针收集 25 个 GPU Timestamp 样本，无 Validation warning/error；
- 私有角色统计、468 个 Joint Matrix、Legacy Animation/Manifest 名称均未改变。

上述 S35.3 验收门槛已在 S35.4 原样复用。Pipeline Creation 拆分的实际结果与
下一开发节点见下节；工程根目录改名仍放在绝对路径清理之后。

## 8. S35.4 Pipeline 拆分与下一节点

`AzureRenderPipeline.cpp` 现在包含以下五个原成员函数：

- `createRenderPass`；
- `createPostProcessRenderPass`；
- `createGraphicsPipeline`；
- `createFramebuffers`；
- `createPostProcessFramebuffers`。

Pipeline Layout、Pipeline、Render Pass 和 Framebuffer Handle 仍全部是
`AzureRenderApp` 成员，初始化调用顺序、Shader Module 销毁路径、Swapchain 重建
流程及最终 Cleanup 顺序未改变。`AzureRenderApp.cpp` 已由 S35.3 的 2,545 行降至
1,900 行，`AzureRenderPipeline.cpp` 为 658 行。

2026-08-02 验证：

- Ninja Debug/Release 均成功；
- 公共资产 120 帧 Debug Validation 成功；
- `captures/s35_pipeline_beauty_regression/frame_000000.png` 与 S30 基准
  SHA-256 完全一致：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- `captures/s35_pipeline_technical_probe` 完成 25 帧、五章节和 25 个 Timestamp
  样本，无 Validation warning/error；
- 本次短 Debug 探针 GPU 总耗时为 10.481 ms 平均、0.633 ms 最小、20.798 ms
  最大，波动明显，不能与预热 Release 长序列作性能结论比较。

上述 Pipeline 验收门槛已继续用于 S35.5。Descriptor 与 GPU Resource Creation
拆分的实际范围、验证结果和下一节点见下节。

## 9. S35.5 Descriptor/Resource 拆分

本轮分为两批并在批次之间单独编译：

1. `AzureRenderDescriptors.cpp`（377 行）：主/Post-process Descriptor Set
   Layout、主 Descriptor Pool/Sets、Post-process Descriptor Pool/Sets；
2. `AzureRenderResources.cpp`（497 行）：Swapchain Image View、Depth、Normal、
   Shadow、Vertex/Index、Texture、Uniform、Joint 与 HUD Buffer 创建。

以下职责仍留在 `AzureRenderApp.cpp`：Instance/Device/Swapchain、Command Pool/
Buffer、Semaphore/Fence、Timestamp Query、主生命周期与输入交互。主文件现在为
1,054 行。所有 Vulkan Handle 仍由同一个 `AzureRenderApp` 持有，Cleanup 代码没有
移动，Descriptor Binding、Image Layout、分配数量及上传顺序均未修改。

2026-08-02 验证：

- Descriptor 第一批移动后 Debug 单独编译成功；
- 完整资源移动后 Ninja Debug/Release 均成功；
- 公共资产完成 120 帧 Debug Validation；
- `captures/s35_resource_beauty_regression/frame_000000.png` SHA-256 为
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`，
  与 S30 基准完全一致；
- `captures/s35_resource_technical_probe` 包含 25 张 PNG、五章 Manifest 和
  25 个 Timestamp 样本；
- 本次短 Debug Timing 为 5.065 ms 平均、0.627 ms 最小、18.570 ms 最大，
  仍只作为查询功能探针，不作为性能回归数据；
- 无 Validation warning/error、Descriptor 错配、Shadow attachment 错误、
  半透明、隐藏结构穿透、材质丢失或蒙皮撕裂。

上述 S35.5 结构验收已经完成。路径审计、迁移探针结果与正式目录改名前的剩余事项
见下节。

## 10. S35.6 路径审计与迁移就绪状态

审计范围包括 CMake、CMake Presets、C++、Shader、PowerShell/Node/Python 工具、
README、交接文档、资产文档和作品集清单，并排除 `build/`、`captures/`、
`assets_private/` 与 Git 元数据中的生成/私有内容。

发现与处理：

- CMake 使用 `${CMAKE_CURRENT_SOURCE_DIR}`，Presets 使用 `${sourceDir}`，无需修改；
- `build_portfolio_package.ps1` 已使用 `$PSScriptRoot`，无需修改；
- `encode_capture.ps1` 接受调用者路径，不依赖工程名；
- 七个 Unreal Python 工具原本硬编码
  `D:\Assigment\2609\FYP\Project\MyVulkanApp`，现统一改为脚本位置向上一级解析；
- Unreal 不提供 `__file__` 或需要输出到另一工作副本时，可设置
  `AZURERENDER_PROJECT_ROOT`；
- 资产文档中的活动输出目录改为相对路径 `assets_private\laevat_static`；
- 开发日志中的旧 `MyVulkanApp → AzureRender`、历史根目录说明继续保留；
- 本文顶部和构建命令仍使用当前真实目录，待实际改名后同步更新。

验证：

- 七个 Unreal 工具全部通过 Python AST 语法检查；
- 七个工具的脚本相对根目录解析均指向当前项目根目录；
- `AZURERENDER_PROJECT_ROOT` 覆盖路径测试通过；
- 临时复制公开构建输入到 `Project/AzureRender_s35_path_probe`；
- 在该不同名称根目录下使用全新 Cache 完成 Debug/Release 配置和各 22 步构建；
- 改名探针 Debug 构建完成公共资产 120 帧 Validation；
- 改名探针 Release 构建读取原工作区私有 GLB，输出
  `captures/s35_path_probe_beauty_regression/frame_000000.png`；
- Beauty SHA-256 仍为
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 临时探针目录验证后已安全删除，不保留重复源码/构建树。

全新 Preset 配置还揭示了两个工作站前置条件：应设置
`VCPKG_ROOT=C:\Users\23587\.vcpkg-clion\vcpkg`，并把
`D:\JetBrains\CLion 2026.1\bin\ninja\win\x64` 加入 `PATH`。沙箱内 vcpkg
Manifest 获取全局 buildtrees lock 被拒绝，因此探针改用已安装的
`x64-windows` prefix 做只读配置；源码、Shader 和链接均验证成功。

上述迁移前检查已经全部落实。正式目录移动、新 Cache 与完整运行回归结果见下节。

## 11. S35.7 正式根目录迁移

项目已在同一卷内从：

```text
D:\Assigment\2609\FYP\Project\MyVulkanApp
```

移动到：

```text
D:\Assigment\2609\FYP\Project\AzureRender
```

迁移步骤与结果：

- 移动前解析并校验源/目标绝对路径，确认目标不存在；
- 从 `Project/` 父目录执行同卷 `Move-Item`，旧目录随后不存在；
- 原 Debug/Release Cache 先保留为 `*-pre-rename`，不参与任何新验证；
- 设置 `VCPKG_ROOT` 与 Ninja PATH 后，通过标准 Presets 重新配置；
- vcpkg Manifest 依赖安装/恢复成功；
- 新 Debug 与 Release 各完成 22 步全新构建；
- 新 Cache 的 `AzureRender_SOURCE_DIR` 与 `CMAKE_HOME_DIRECTORY` 均指向
  `D:/Assigment/2609/FYP/Project/AzureRender`；
- 完整回归通过后，两个 `*-pre-rename` 旧缓存已删除；它们只有可重建产物，
  没有源码、资产或捕获结果。

改名后回归：

- 公共 `assets_public/test_model.gltf`：120 帧 Debug Validation，退出码 0；
- 私有 Release Beauty：
  `captures/s35_root_rename_beauty_regression/frame_000000.png`；
- Beauty SHA-256：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`，
  与 S30/S35 全部基准完全一致；
- 私有 Debug Technical Sequence：
  `captures/s35_root_rename_technical_probe`；
- Manifest 声明 25 帧，目录包含 25 张 PNG，五章完整；
- 25 个 Timestamp 样本正常，本次短 Debug 平均 8.380 ms、范围
  0.628–19.744 ms，不作为正式性能比较；
- 81,487 vertices、284,673 indices、14 primitives、15 materials、468 Joint
  Matrix 与 `Afterglow_ProceduralIdle` 均正常；
- 无 Validation warning/error、路径丢失、Descriptor/Shader 加载错误、半透明、
  隐藏结构穿透、材质丢失或蒙皮撕裂。

S35 至此结束：可执行文件、CMake Target、C++ 类型、运行时品牌、源码结构和工程
根目录均已完成 AzureRender 迁移，Legacy Capture/glTF Schema 仍兼容。

S35 已完整结束。S36.1 的设计、能力探针与冻结基线结果见下节。

## 12. S36.1 色彩管线设计与冻结基线

当前 Main Pass 直接写 `VK_FORMAT_B8G8R8A8_SRGB` Swapchain，Post-process 使用
`LOAD` 在同一 Image 上叠加描边/HUD，没有独立 Scene Color。S36.1 已完成：

- 冻结 `captures/s36_ldr_color_baseline/frame_000000.png`；
- SHA-256 仍为
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 量化 1080p PNG 的 RGB 范围、均值、线性亮度分位与高光裁切比例；
- 选择 `VK_FORMAT_R16G16B16A16_SFLOAT` Linear HDR Scene Color；
- 选择 0 EV 固定曝光与 Narkowicz ACES fitted；
- 明确 SRGB Swapchain 自动编码，Shader 不手动执行 Gamma；
- 明确 Beauty 在 HDR 中合成描边后 Tone Map，诊断视图绕过 Tone Mapping，HUD 在
  Tone Mapping 后绘制；
- 新增运行时格式探针，检查 Sampled Image、Color Attachment 与 Color Attachment
  Blend 三项 optimal tiling 能力；
- RTX 4060 Laptop GPU 启动报告该候选格式 `supported`；
- Debug/Release 构建、公共 120 帧 Validation 与 Release LDR Beauty 回归通过，
  证明能力探针没有改变像素。

完整实施规范见 `docs/HDR_TONEMAPPING_DESIGN_CN.md`。下一节点为 **S36.2**：新增
per-Swapchain RGBA16F Scene Color、改写 Main/Final Composite Attachment、增加
Post-process Binding 3，并在 Final Composite 中实现 0 EV ACES fitted。S36.2 的
Beauty 变化是预期行为，必须建立新哈希和视觉基准，不能继续以 S35 哈希作为通过条件。

## 13. S36.2 HDR Scene Color 与固定 Tone Mapping

S36.2 已完成设计落地，当前活动色彩路径为：

```text
Main Scene -> RGBA16F Scene Color -> HDR Outline Composite
           -> 0 EV ACES fitted -> SRGB Swapchain -> HUD/Title/Fade
```

实现内容：

- 每张 Swapchain Image 对应一组 `sceneColorImages_`、Memory 与 Image View；
- Scene Color 使用 `VK_FORMAT_R16G16B16A16_SFLOAT`，具备 Color Attachment 与
  Sampled Image Usage；
- 初始化、Resize/Recreate 与 `cleanupSwapchain()` 已覆盖完整资源生命周期；
- GPU 缺少 Sampled/Color Attachment/Blend 必需能力时明确失败，不静默降级；
- Main Render Pass attachment 0 改写 Scene Color，结束布局为
  `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`；
- Main Framebuffer attachment 0 不再引用 Swapchain Image View；
- Final Pass 使用 `DONT_CARE/UNDEFINED` 覆盖写 Swapchain；
- Post-process Descriptor 增加 Binding 3，Descriptor Pool 每 Set 从 3 个采样器增至 4 个；
- `PostProcessPushConstants` 从 16 bytes 扩展为 32 bytes，固定
  `exposureEv = 0.0`、`toneMappingEnabled = 1.0`；
- Beauty 在线性 HDR 中混合描边后执行 Narkowicz ACES fitted；
- Final Composite 关闭 Alpha Blend 并固定输出 Alpha 1；HUD Pipeline 继续 Alpha Blend；
- World Normal、Internal Outline 与 Shadow Map 直接输出，不经过 Exposure/ACES；
- Capture 仍读取最终 8-bit RGBA Swapchain，不更改 Legacy Manifest Schema。

验证结果：

- Ninja Debug 与 Release 编译、链接成功；
- 公共测试模型 120 帧 Debug Validation，退出码 0；
- 交互生命周期检查将窗口从 856×511 最大化到 1707×912，并执行最小化/恢复；
  三次状态变化后渲染均继续更新，随后正常关闭；
- 私有 1080p Release Beauty：
  `captures/s36_hdr_beauty_v1/frame_000000.png`；
- 新 SHA-256：
  `5E8BF8B507FE07F385EAADF563DF40CD3C23FA6A2433156DEFD1BFD6AB829357`；
- RGB min 5/11/13，max 235/217/222，mean 29.000/60.084/69.169；
- Linear Luminance P50/P90/P95/P99/P99.9 为
  0.037679/0.062127/0.120969/0.394080/0.457723；
- 任一 RGB 通道等于 255 为 0%，Alpha 非 255 为 0%；
- 视觉检查确认头部、口腔、手臂、衣服和机械结构遮挡正确，没有重新出现半透明；
- 私有 Debug 技术序列位于 `captures/s36_hdr_technical_probe`，包含 25 张 PNG、
  五章与 25 个 Timestamp Sample；
- 代表帧 2/7/12/17/22 分别验证 Beauty、World Normal、Internal Outline、
  Shadow Map 与 Beauty+HUD；
- 本次短 Debug 平均 Shadow/Main/Final 为 1.301/3.262/0.142 ms，合计
  4.706 ms，仅作功能探针，不与正式 Release 性能数据比较；
- 无 Validation warning/error、Descriptor/Layout 错误、材质丢失、蒙皮撕裂或
  Capture Alpha 回归。

S36.2 至此完成。这里原定的 S36.3 已被长期主计划重新排序为 Deferred。当前下一节点
不是曝光或正式性能测量，而是 **M1/CQ-0 固定视觉 QA Harness**；只有角色质量通过 M2
Gate 后，才按 Master Plan 恢复后续工程与研究节点。

## 13. CQ-2 Toon Ramp / Shadow v1 与当前恢复入口

CQ-2 已于 2026-08-13 完成：

- `assets_public/toon_ramp_profiles.json` 是版本化 Ramp 源；
- `assets_public/toon_ramp_atlas.ppm` 是 10x64 线性 UNORM Atlas；
- Skin/Face 使用软 Ramp，Hair/Fabric/Metal/Eye 使用阶梯 Ramp；
- Direct Diffuse、Ambient、Shadow Visibility、AO、Shadow Tint 和 Style Mask
  已拆分并可单独 Isolation；
- Descriptor binding 11 持有 Renderer-owned Ramp，128-byte Push Constant 未扩展；
- Manifest 和 QA Index 保存 Ramp 文件 Hash；
- 详细设计与证据见 `docs/TOON_RAMP_SHADOW_V1_CN.md`。

最终验证在 NVIDIA GeForce RTX 5070 Ti Laptop GPU 上完成：Debug/Release 构建、
公共/私有 120 帧 Smoke、60 帧 Lighting Sweep、1920x1080 Release Beauty 和 Alpha/
Manifest 检查通过。评审图位于 `captures/cq2_review_v1/cq2_review_sheet.png`。

当前恢复入口为 **CQ-3 Face SDF 与脸部 Overlay**。第一步只验证公共 Face SDF 与
莱万汀 UV、方向和镜像规则的兼容性；不提前实现最终 Hair KK、Bloom 或最终调色。
