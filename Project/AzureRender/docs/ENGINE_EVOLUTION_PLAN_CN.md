# AzureRender 引擎化实施计划

> 计划版本：2026-09-20
> 覆盖范围：`DEVELOPMENT_ROADMAP_CN.md` 中 E0-E7 的具体实施步骤

本文把路线中的引擎化阶段展开为可执行步骤。路线定义优先级与准入条件，本文定义改哪些文件、按什么顺序改、每步如何验证。阶段状态以路线为准。

## 0. 目标与当前架构约束

目标是可扩展的渲染器-引擎架构，判据是三条：新增场景、材质、光源、pass 时不需要改公共核心；绘制成本随对象数量亚线性增长；渲染后端可替换。

当前架构存在四个结构性约束，它们不是代码质量问题，而是设计前提写死在接口里，补丁式改良无法绕过：

| 约束 | 证据 | 后果 |
|---|---|---|
| 单资产单场景假设 | `RenderContext` 持 `std::string assetPath` 与单个 `const LoadedAsset*`；`AzureRenderApp` 持单个 `sceneRenderer_` | 无法表达多对象场景，多实例、关卡、流式加载都无处落地 |
| 绘制路径无批次概念 | `CharacterSceneRenderer::recordShadowPass` 与 `recordMainPass` 按 `asset_.primitives` 线性遍历，每 primitive 一次 descriptor 绑定加两次 push constants 加一次 `vkCmdDrawIndexed` | 绘制成本与 primitive 数严格线性，无实例化与合批空间 |
| ECS 未驱动渲染 | `ComponentArray` 用 `std::unordered_map<Entity, T>` 存储；`RenderableComponent` 仅被 `EditorContext::visibleRenderableCount` 统计 | 存在两套并行场景表示，ECS 是编辑器附属物而非渲染数据源 |
| Vulkan 调用散布各层 | `vkCmd*` 直接出现在 `AzureRenderFrame.cpp` 与各场景渲染器 | 后端不可替换，pass 无法独立测试 |

因此阶段划分从"补功能"改为"先换承重结构，再在新结构上加功能"。E0-E2 是结构替换，会产生大幅 diff 且不产出画面；E3-E7 在新结构上做功能与性能。这个顺序不可颠倒：在旧结构上加 compute 与多光源，会把错误的前提固化进更多代码。

### 阶段总览

| 阶段 | 性质 | 产出 | 依赖 |
|---|---|---|---|
| E0 视觉与性能基线 | 安全网 | 像素回归与性能基准 | — |
| E1 RHI 与内存层 | 结构替换 | 后端可替换、VMA、bindless | E0 |
| E2 场景图与渲染数据流 | 结构替换 | 统一场景表示、多对象、批次 | E0 |
| E3 Render Graph 与 pass 库 | 结构替换 | 声明式 pass、自动 barrier | E1、E2 |
| E4 Compute 与着色质量 | 功能 | compute 通路、GPU IBL、新 bloom | E3 |
| E5 光照与阴影体系 | 功能 | 多光源、CSM、聚簇光照 | E3 |
| E6 并行与 GPU 驱动提交 | 性能 | 多线程录制、间接绘制 | E3 |
| E7 复杂场景验证 | 验收 | 压测全部子系统 | E4、E5、E6 |

## 通用约定

- 每步完成后运行 Debug 与 Release 的 `ctest`，两者必须全绿。
- 每阶段结束运行 `tools/run_release_gate.cmake`，Debug 开启 Validation。
- 每阶段结束三个场景各运行 120 帧 smoke，退出码必须为 0。
- 公共接口变更必须更新 `ARCHITECTURE_CN.md`；阶段状态变更必须同步 `tools/check_docs.sh`。
- 结构替换阶段以「旧路径与新路径并存、逐场景迁移、删除旧路径」三步推进，禁止长期保留双实现。
- 每个阶段结束必须有性能数据，格式见 E0 第 5 步。

## E0：视觉与性能基线

没有这一层，后续任何结构替换都无法证明没有破坏功能，也无法证明性能改善。

### 现状

已具备：确定性捕获（`src/app/AzureRenderCapture.cpp`）、容差比较（`tools/compare_images.py`）、五视图 CLI（`--qa-isolation` 已含 `albedo`、`hair-kk`、`shadow-tint`、`face-sdf`）、多用例批处理（`tools/run_character_qa.ps1`）。

缺口：无版本库内基线、未接入 CTest 与 CI、无性能基准、长捕获在高分辨率下停滞。

### 实施步骤

1. **修复长捕获停滞并统一 readback 路径。** 当前每个捕获帧在 `drawFrame()` 内新建 host-visible buffer、等 fence、立即销毁（`AzureRenderFrame.cpp` 第 80-92 行与第 205-210 行）。1920×1080×4 约 8.3 MB/帧。先加 `VkResult` 与分配失败诊断确认失败点，再改为按 in-flight frame 复用常驻 staging buffer，尺寸变化时重建。这一步同时是 E1 ring buffer 的前置。
2. **建立五视图公共基线。** 全部基于 `assets_public/test_model.gltf`，固定相机与光照，单帧捕获。命名遵循 `ASSET_AND_VISUAL_QA_CN.md`。同时写 evidence JSON 记录设备、驱动、Vulkan 版本、CLI 参数与 SHA-256。
3. **建立性能基准工具。** 新增 `tools/run_performance_baseline.py`：固定场景、分辨率、帧数，输出 CPU 帧时间与 GPU pass 时间的 p50/p95/max、draw call 数、descriptor 绑定次数、分配次数与显存占用。计数类指标需在代码中加轻量计数器，它们是后续阶段的主要判据。
4. **单入口与分层容差。** 新增 `tools/run_visual_regression.py` 执行捕获、比较、汇总。严格档用于本机同设备，宽松档用于 CI 的 lavapipe。阈值写入配置文件而非脚本常量。CI 只跑 `beauty` 与 `albedo`。
5. **固定性能报告格式。** 每阶段结束产出同格式 JSON，字段为设备、分辨率、场景、帧数、上述全部指标。跨阶段可直接对比。这是"性能优先"的落地方式：没有数据的性能主张不予接受。
6. **接入 CTest 与 CI。** 无可用 Vulkan 设备时跳过而非失败，保持纯 CPU 环境既有测试不受影响。
7. **验证检出能力。** 故意改动 shader 常量，确认比较失败；还原后通过。这是 E0 的真正验收。

### 验收

五视图可重复且与基线一致；CI 两视图通过；故意改动被检出；长捕获在目标分辨率完成全部帧；性能基准 JSON 产出且指标完整；纯 CPU 环境 CTest 全绿。

提交标题：`feat(e0): 建立视觉回归与性能基准`

## E1：RHI 与内存层

把 Vulkan 调用收拢到单一后端层，同时替换显存与描述符模型。合并为一个阶段，因为分开做会导致 RHI 接口先按旧内存模型定型、随后再改一遍。

### 改动面

- `src/rhi/`（新增）：设备、队列、分配器、命令、描述符、管线抽象。
- `src/render/VulkanHelpers.cpp`：迁入 RHI 后移除。
- `src/app/AzureRenderSupport.cpp`、`AzureRenderFrame.cpp`：改为经 RHI 调用。
- 三个场景渲染器：绘制与资源创建改为经 RHI。
- `vcpkg.json`、`CMakeLists.txt`：VMA 依赖。
- `schemas/gpu_capability_report.schema.json`：能力与统计字段，版本递增。

### 实施步骤

1. **先定接口再动实现。** RHI 覆盖设备与队列、资源创建与销毁、命令录制、同步、描述符、管线与 shader、查询。接口按项目实际使用面设计，使用 RAII 句柄而非裸指针。Piccolo 的 `RHI` 是约 120 个虚函数返回裸指针的全覆盖式设计，本项目不采纳：所有权必须清晰，且接口要可被 mock。
2. **可测试性作为一等目标。** RHI 为纯虚接口，提供记录调用序列的 `NullRHI` 实现，使 pass 的录制逻辑可在无 GPU 环境单元测试。这是当前测试全在 CPU 侧、GPU 逻辑零覆盖的根本解法。
3. **VMA 接入。** 单独编译单元定义 `VMA_IMPLEMENTATION`，采用 `VMA_DYNAMIC_VULKAN_FUNCTIONS` 避免 MinGW 静态链接问题。当前 `vkAllocateMemory` 仅四处调用（`VulkanHelpers.cpp:97`、`VulkanHelpers.cpp:295`、`AzureRenderSupport.cpp:428`、`AzureRenderSupport.cpp:742`），全部迁入 RHI 分配器。
4. **Upload ring buffer。** 单个大 buffer，按 in-flight frame 记录区间并每帧复位，处理 `minUniformBufferOffsetAlignment` 与 `nonCoherentAtomSize` 对齐。替换零散 mapped buffer 与 E0 的 staging buffer。
5. **Bindless 描述符。** 当前 `CharacterSceneRenderer` 按 `descriptorCount * 11` 固定分配 descriptor pool（第 910-921 行），每 primitive 绑定一次 descriptor set。改为全局纹理数组加索引，材质数据进 storage buffer，绘制时只传索引。这是 E2 合批与 E6 间接绘制的前提，不做则后续阶段无法减少绑定次数。必须检测 `descriptorIndexing` 与 `runtimeDescriptorArray` 支持，不可用时保留固定表路径。lavapipe 支持情况需实测。
6. **逐场景迁移后删除旧路径。** 顺序为 `sample`、`blackhole`、`character`。每迁完一个跑 E0 回归，全部完成后删除 `VulkanHelpers` 与旧 descriptor 路径。

### 验收

E0 五视图像素零差异；Debug Validation 无新增警告；descriptor 绑定次数与分配次数相对 E0 基线显著下降且有数据；`NullRHI` 下 pass 录制单元测试通过；降级路径有测试覆盖；ring buffer 对齐与复位有单元测试。

### 风险

MinGW 下 VMA 与 bindless 的实际可用性需尽早实测；若 lavapipe 不支持 bindless，CI 只覆盖降级路径，完整门禁在本机。

提交标题：`refactor(e1): 建立RHI抽象层` 与 `feat(e1): 引入VMA与bindless描述符`

## E2：场景图与渲染数据流

替换单资产假设，建立统一场景表示。这是最大的一次结构改动，也是"可扩展"的核心。

### 改动面

- `src/scene/`（新增）：场景图、变换层级、可见性、批次。
- `src/ecs/`：`ComponentArray` 改为密集存储，成为渲染数据源。
- `src/render/RenderContext.hpp`：移除 `assetPath` 与单 `LoadedAsset*`。
- `src/editor/SceneModel.cpp`、`EditorContext.cpp`：改为操作统一场景图。
- `src/scenes/CharacterSceneRenderer.cpp`：改为消费批次而非遍历 primitive。
- `.azscene`：schema 递增，多对象与实例。

### 实施步骤

1. **统一场景表示，消除双轨。** 当前 `SceneModel` 的节点树与 ECS 并行存在，ECS 仅供编辑器计数。改为 ECS 是唯一运行期场景表示，`SceneModel` 退化为序列化层。这一步必须先做，否则后续可见性与批次会被迫实现两遍。
2. **ECS 存储改为密集。** 当前 `ComponentArray<T>` 用 `std::unordered_map<Entity, T>`，遍历时缓存不友好且指针不稳定。改为密集数组加稀疏索引，使组件可连续遍历。这是 E6 多线程分块与剔除吞吐的前提。
3. **分离渲染数据与 GPU 资源缓存。** 参考 Piccolo 的 `RenderScene` 与 `RenderResource` 分工：前者持每帧可见性集合，后者持按资产键缓存的 GPU 资源。多个实体可共享同一网格与材质资源。
4. **变换层级与包围体。** 局部与世界变换、脏标记传播、包围盒计算与变换。纯 CPU 逻辑，先写单元测试。
5. **视锥剔除。** 视锥平面提取与相交判定，Piccolo 的 `render_helper.cpp` 有可参考实现。提供开关，两侧画面必须一致。
6. **批次与实例化。** 按管线与材质分组，同网格多实例合并为实例化绘制。实例数据进 storage buffer，配合 E1 的 bindless 使每批次只绑定一次。这一步直接改变绘制成本的增长曲线。
7. **改造场景渲染器消费批次。** `CharacterSceneRenderer` 的 `recordShadowPass` 与 `recordMainPass` 由遍历 `asset_.primitives` 改为遍历批次。`RenderContext` 移除单资产字段。
8. **多对象场景落地。** `.azscene` 支持多资产引用与实例，编辑器可增删对象。这是可扩展性的可见证据。

### 验收

单对象场景下 E0 五视图像素零差异；多对象场景可保存与加载并有 round-trip 测试；剔除开关两侧画面一致且 draw call 有可测下降；同网格多实例的 draw call 数不随实例数线性增长且有数据；剔除与变换数学有单元测试；旧 `.azscene` 迁移通过。

### 风险

这是改动量最大的阶段，涉及编辑器、序列化与三个场景渲染器。必须按「新场景图与旧路径并存、逐场景迁移、删除旧路径」推进，且每步都跑 E0 回归。若单次 diff 过大无法审阅，按第 1 至 8 步分多次提交。

提交标题：按步骤分多次提交，统一前缀 `refactor(e2)` 与 `feat(e2)`

## E3：Render Graph 与 pass 库

当前 pass 顺序、attachment 生命周期与 barrier 硬编码在 `AzureRenderFrame.cpp` 的 `recordCommandBuffer` 中。新增 pass 需要改公共帧代码，违反可扩展判据。

### 实施步骤

1. **建立 render graph。** pass 声明读写的资源，图负责排序、分配瞬态资源、插入 barrier 与 layout 转换。手写 barrier 是 E4 引入 compute 后的主要错误来源，图化是前置条件而非优化。
2. **瞬态资源池。** attachment 按生命周期复用显存，避免每个中间目标常驻。
3. **拆解现有帧结构。** shadow、main、outline、post-process、HUD、editor UI 全部改为图中的 pass。按职责拆分，不按 render pass 对象拆分：Piccolo 的 `main_camera_pass.cpp` 达 175 KB，说明按对象拆分在 subpass 集中时仍会失控。
4. **pass 可独立测试。** 配合 E1 的 `NullRHI`，每个 pass 的录制逻辑与资源声明可单元测试。
5. **场景渲染器贡献 pass。** 场景不再只提供 `recordScene` 回调，而是向图注册自己的 pass。黑洞的 trace、TAA、bloom、composite 成为独立 pass。
6. **确定 `IRenderFeature` 去留。** 图中的 pass 是这个预留接口的合理承载对象；若仍无清晰用法则移除。

### 验收

E0 五视图像素零差异；三个场景全部经由图渲染；新增一个测试 pass 不需修改公共帧代码；图的排序、barrier 与资源复用有单元测试；显存占用相对 E0 有对比数据。

提交标题：`refactor(e3): 引入render graph` 与 `refactor(e3): 场景渲染器改为注册pass`

## E4：Compute 与着色质量

### 实施步骤

1. **扩展 shader 构建。** 加入 `.comp` 与 include 目录支持。保留显式列出 shader 的做法，避免意外纳入实验文件。
2. **建立 compute 通路。** 以 bloom 下采样为首个用例。barrier 由 E3 的图负责，参考 Piccolo 的 `particle_kickoff/emit/simulate` 三段式与 indirect dispatch。
3. **重写 bloom。** 多级下采样与上采样替换当前单 pass。画面会变，需先建立有意变更的新基线并记录原因。
4. **GPU 端 IBL。** 运行期生成 irradiance、GGX prefiltered specular 与 BRDF LUT，替换当前 CPU 侧等距柱状图加 mip 近似。不引入六面贴图资产管线。
5. **GPU skinning 与 morph 迁入 compute。** 当前逐帧上传 joint matrices，改为 compute 计算并输出到 storage buffer，配合 E6 的间接绘制。
6. **OpenEXR。** 当前 `.exr` 明确失败，加入解码以扩大环境资产来源。评估依赖体积对发布包的影响。

### 验收

新 bloom 与 IBL 有基线对比图与变更说明；compute 路径有契约测试；`.exr` 可加载且损坏文件有明确错误；三个场景 120 帧回归通过；着色相关 GPU 时间有前后对比。

提交标题：`feat(e4): 建立compute通路与GPU端IBL`

## E5：光照与阴影体系

### 实施步骤

1. **光源作为场景实体。** 光源进入 E2 的场景图与 ECS，可增删与变换，而非渲染设置中的固定字段。
2. **聚簇或分块光照。** 直接支持多光源会使 fragment 着色成本随光源数线性增长。采用 compute 构建光源聚簇，使成本与屏幕分块相关而非光源总数。这是"性能优先"在光照上的体现。注意 UBO 与 storage buffer 的显式对齐，数组对齐错误是常见故障源。
3. **级联阴影。** 当前单张 2048 方向光 shadow map 在场景尺度放大后精度不足。级联包围体由视锥角点计算，Piccolo 的 `render_helper.cpp` 有可参考实现。
4. **保留现有 PCSS 特征。** 当前两阶段 PCSS（12 点 blocker search 加 16 点 PCF）是已验收的视觉特征，级联化不得丢失半影表现。过滤参数与级联配置分离。
5. **点光源阴影。** 在聚簇体系内按需支持，评估 cubemap 与 tetrahedron 布局的成本。
6. **schema 迁移。** `RenderSettings` 当前为 v7，新增字段需保证旧 `.azscene` 可读。

### 验收

级联分割与漏光有对比图；PCSS 视觉特征保留并有对比说明；光源数增长时帧时间增长显著低于线性且有数据；旧场景迁移通过；有意变化有新基线。

提交标题：`feat(e5): 光源实体化与聚簇光照` 与 `feat(e5): 实现级联阴影`

## E6：并行与 GPU 驱动提交

### 实施步骤

1. **逻辑与渲染数据双缓冲。** 参考 Piccolo 的 `RenderSwapContext`，帧边界交换。先建立数据边界，不立即引入线程。
2. **多线程命令录制。** secondary command buffer 按批次分块并行录制，每线程独立 command pool。E2 的密集 ECS 存储使分块可按连续区间切分。
3. **GPU 驱动的绘制提交。** 剔除结果与绘制参数由 compute 写入间接缓冲，使用 `vkCmdDrawIndexedIndirect`，配合 E1 的 bindless 与 E2 的批次。这是绘制成本亚线性的最终形态。
4. **Timeline semaphore。** 替换当前二元 semaphore 与 fence 组合，简化跨队列同步。
5. **Timing 细化。** 区分 CPU 录制与 GPU 执行，覆盖新增的 compute 与级联阴影。

### 验收

多线程与单线程画面像素一致；CPU 帧时间相对 E0 基线有可测下降且有数据；间接绘制路径下 draw call 提交次数不随对象数线性增长；Debug Validation 在多线程路径无错误；确定性捕获仍逐帧一致。

### 风险

多线程会破坏确定性捕获的逐帧一致性。捕获路径需保持单线程或提供显式确定性模式，这一点必须在设计阶段确定而非事后补救。

提交标题：`feat(e6): 多线程录制与GPU驱动提交`

## E7：复杂场景验证

### 实施步骤

1. **构造压测场景。** 需同时压测多对象与实例化、剔除、聚簇光照、级联阴影、compute 后处理、IBL 与间接绘制。对象与光源数量需明显超过当前角色场景。资产全部可公开。
2. **不改公共核心完成接入。** 这是对全部前序阶段的真正验收。若必须改公共核心，说明抽象存在缺口，应记录为契约演进输入。
3. **规模化性能数据。** 给出对象数与光源数变化时的帧时间曲线，验证亚线性增长。
4. **纳入回归体系。** 新场景进入 E0 的视觉与性能基线。

### 验收

不改公共主循环完成接入；load/unload、resize、capture、timing 全部通过；另两个场景零回归；规模化性能曲线符合亚线性预期。

提交标题：`feat(e7): 新增复杂度验证场景`

## Piccolo 参考边界

采纳：VMA 动态函数加载的最小形式、upload ring buffer 的偏移复位与对齐处理、场景与 GPU 资源缓存分离、视锥与级联包围体数学、compute 多段式与 barrier 排布、逻辑与渲染数据双缓冲、shader include 组织。

不采纳：`render_type.h` 的 108 KB 集中类型定义、`main_camera_pass.cpp` 的 175 KB 单文件、裸指针资源所有权、约 120 个虚函数的全覆盖式 RHI、硬编码的 pass 顺序与手写 barrier、以及缺失自动化测试的工程方式。本项目以窄接口加 RAII、render graph、密集 ECS 存储与可 mock 后端替代这些取舍。
