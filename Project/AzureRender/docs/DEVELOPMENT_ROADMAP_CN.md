# AzureRender 未来开发路线

> 路线版本：2026-09-20
> 当前状态：P0/P1/P2 与 R1-R5 全部完成；E0 完成，E1-E7 为 `Ready` 队列，当前没有 Active 阶段。

黑洞 P1 已于 2026-08-19 进入 `Final / Frozen`，最终基线为 16 秒双机位展示及周期噪声无接缝实现。只有用户主动重新启用后才能继续变更；当前角色收尾不得触碰黑洞渲染路径。

本文是未来开发的唯一队列。它描述优先级和准入条件，不把尚未实现的内容写成当前能力。

## 1. 产品原则

- AzureRender 继续作为多场景、多 shader 的可插拔 renderer，而不是单一 demo。
- 公共核心只提供跨场景设施；算法、资源和美术逻辑留在场景 renderer。
- 所有正式功能必须有公共资产路径、自动化测试和可重复视觉证据。
- 私有角色可以补充本地 QA，但不能成为 CI 或发布依赖。
- 不为短期画面修改在 `AzureRenderApp` 增加资产或场景专属分支。

## 1.1 引擎化方向

AzureRender 在保持多场景可插拔 renderer 定位的同时，向可扩展的渲染器-引擎架构演进。演进先替换承重结构，再在新结构上叠加功能与性能：结构阶段不产出画面但解开能力上限，功能阶段验证结构的实际承载力。

引擎化的参考实现是同级目录的 Piccolo（GAMES104 教学引擎），技术栈为 C++17/Vulkan/GLFW/ImGui/VMA，与本项目一致。采用其中已验证的分层方式，不照搬其游戏引擎特有的关卡、物理、脚本和反射代码生成。

| 参考点 | Piccolo 做法 | AzureRender 采纳形式 |
|---|---|---|
| 显存分配 | VMA 统一分配，`VMA_DYNAMIC_VULKAN_FUNCTIONS` 动态加载 | E1 全部显存分配迁入 RHI 分配器 |
| API 抽象 | `RHI` 纯虚接口 + `VulkanRHI` 实现，pass 只依赖接口 | E1 窄接口加 RAII 句柄，并提供可 mock 实现 |
| 上传缓冲 | global upload ring buffer，按 in-flight frame 复位偏移 | E1 统一上传路径与对齐处理 |
| 场景与资源分离 | `RenderScene` 持可见性，`RenderResource` 持 GPU 缓存 | E2 可见性集合与资源缓存分层 |
| 剔除 | frustum plane 提取、`BoundingBox` 变换、tiled frustum 相交 | E2 视锥剔除与包围体数学 |
| Pass 组织 | `RenderPassBase` / `RenderPipelineBase`，pass 独立文件 | E3 render graph，pass 声明资源读写 |
| Compute | `particle_kickoff/emit/simulate` 三段 compute + indirect dispatch | E4 compute 通路，E6 间接绘制 |
| IBL | irradiance/specular cubemap + BRDF LUT + mipmap sampler | E4 改为运行期 GPU 生成 |
| Shader 构建 | 递归 glob 含 `.comp`，统一 include 目录 | E4 扩展编译列表与 include |
| 级联阴影 | 由 frustum 角点计算级联包围体 | E5 级联阴影 |
| 逻辑/渲染解耦 | `RenderSwapContext` 双缓冲 swap data | E6 多线程录制的前置结构 |

不采纳：`render_type.h` 单文件 108 KB 的集中类型定义、`main_camera_pass.cpp` 175 KB 的单 pass 体量、裸指针 `RHI*` 资源所有权、约 120 个虚函数的全覆盖式 RHI、硬编码 pass 顺序与手写 barrier、以及缺失自动化测试（其 `source/test` 为空）。本项目以窄接口加 RAII、render graph、密集 ECS 存储与可 mock 后端替代这些取舍。

## 2. 下一优先级候选

### R1：发布工程硬化

状态：`Complete`（2026-08-18）。

- 增加 `--help` 和稳定 CLI reference 输出。
- 修复现有 strict-aliasing 与空字符编译警告，启用更严格的 warning gate。
- 在 CI 生成 Windows/Linux 安装包并执行干净环境启动测试。
- 为 portfolio manifest 增加 CI 哈希校验。
- 建立版本号、changelog 和 tag 发布流程。

验收：干净 clone 能通过配置、构建、测试、安装、打包、资源检查和公共 renderer smoke；包内无私有资产、本机路径或缓存。

结果：Windows Debug/Release 均完成 12/12 CTest；两个安装树在仅保留 Windows 系统 PATH 时通过 `--version` 与 `--check-resources`，并从安装目录实际运行 Character/Blackhole 各 120 帧。Release Gate 的构建、测试、安装、移动、哈希、隔离运行和 TGZ 打包全部通过。

### R2：Renderer SDK 可用性

状态：`Complete`（2026-08-18）。

- 提取内置 renderer 注册表和示例模板。
- 为 `RenderContext`、capabilities、生命周期和资源所有权增加契约测试。
- 增加 settings schema migration 与未知 renderer 的明确错误路径。
- 建立 shader variant/feature catalog，避免组合爆炸散落在 App。
- 编写最小第三场景示例，但不恢复旧工业科幻场景需求。

验收：新增示例场景不修改公共帧主循环；卸载、resize、capture 和另两个场景回归全部通过。

结果：内置 factory 与 shader feature 已集中到 catalog；新增无工业美术含义的 `sample` renderer；capabilities/lifecycle/registry/settings migration 契约已自动化。Debug/Release 12/12 CTest 通过，三个 renderer 各完成 120 帧实机回归。

### R3：黑洞质量与自动化

状态：`Complete`（2026-08-18）。

- 评估自适应积分或更高阶积分器，控制性能与轨迹误差。
- 增加可配置相机、黑洞质量/自旋扩展研究和质量档位。
- 建立离屏 GPU 图像测试、history reset 测试和容差型图像比较。
- 对 TAA ghosting、吸积盘采样和星场频谱进行专项测量。
- 发布 Release GPU timing，区分 pass timing 与完整帧时间。

验收：画面提升必须有相同设备/分辨率对比、确定性证据和角色 renderer 隔离回归。

结果：三档质量和四个相机已数据化；近光子球使用连续自适应细化；质量/相机变更触发可单测的 history reset。双 capture 末帧像素完全一致，Character 公共基线零差异；RTX 4060 Balanced 720p/300 样本 Total render 平均 3.616 ms。

### R4：角色渲染与美术工具

状态：`Complete`（2026-08-18）。

- 把 showcase look 从 C++ 常量演进为版本化数据资产和编辑器面板。
- 提供更具代表性的自有公共角色资产，替代当前几何测试模型作为公开 Beauty 基准。
- 改进 Face SDF authoring、材质 profile 检查和灯光预览。
- 增加背景/地台模块化组件，但仍属于 Character renderer 展示层。
- 补充透明、头发、皮肤和 outline 的 GPU 图像回归。

验收：全身、近景、背面、动画、Stylized A/B 与 Material Check 均有公共基准；现有场景文件向后兼容。

结果：五套 Look 已迁移为 Showcase Look v1 JSON；编辑器可调整 Look、背景、地台和 Face SDF；公共材质改为显式 Material Profile v1，RenderSettings v6 保持旧场景迁移。公共资产承担自动化基线，私有角色仅保留本机补充展示，不进入版本库或发布包。

### R5：编辑器生产力

状态：`Complete`（2026-08-18）。

- Command pattern Undo/Redo。
- 资产热重载、依赖图和缩略图。
- 多实体场景保存、prefab/instance 和更完整 inspector。
- 捕获与视觉基准管理 UI。

验收：编辑器操作有 session/scene round-trip 测试，失败保存不破坏已有文件。

结果：命令式 Undo/Redo、显式资产热重载、资源依赖/状态视图、`.azscene v2` 多节点 transform 与 prefab/instance 引用、语义化 Capture 面板均已落地。Session 和 SceneModel 测试覆盖历史、请求消费、v1 迁移、v2 round-trip 与失败保存。

## 3. 无限期 Deferred

以下内容只有用户主动启用后才能进入 `Ready`：

- Traditional/Subpasses/Dynamic Rendering Local Read 正式论文实验。
- Android、移动端功耗和热稳定性实验。
- 统计分析、论文写作和学校交付物。
- 跨 DLL 稳定插件 ABI 或脚本运行时。
- 独立工业科幻场景。

Deferred 不表示取消已有原型代码，但不得作为默认下一任务，也不得在产品文档中宣称完成。

## 3.1 E0-E7 引擎化队列

目标是可扩展的渲染器-引擎架构，判据为三条：新增场景、材质、光源、pass 时不需要改公共核心；绘制成本随对象数量亚线性增长；渲染后端可替换。

当前架构有四个结构性约束写在接口前提里，补丁式改良无法绕过：`RenderContext` 的单资产单场景假设、绘制路径无批次概念、ECS 未驱动渲染、Vulkan 调用散布各层。因此队列先替换承重结构，再在新结构上加功能。

E0-E3 为结构替换，产生大幅 diff 且不直接产出画面；E4-E7 在新结构上做功能与性能。顺序不可颠倒：在旧结构上叠加 compute 与多光源，会把错误前提固化进更多代码。

| 阶段 | 性质 | 产出 | 依赖 |
|---|---|---|---|
| E0 视觉与性能基线 | 安全网 | 像素回归与性能基准（`Complete` 2026-09-20） | — |
| E1 RHI 与内存层 | 结构替换 | 后端可替换、VMA、bindless | E0 |
| E2 场景图与渲染数据流 | 结构替换 | 统一场景表示、多对象、批次 | E0 |
| E3 Render Graph 与 pass 库 | 结构替换 | 声明式 pass、自动 barrier | E1、E2 |
| E4 Compute 与着色质量 | 功能 | compute 通路、GPU IBL、新 bloom | E3 |
| E5 光照与阴影体系 | 功能 | 多光源、CSM、聚簇光照 | E3 |
| E6 并行与 GPU 驱动提交 | 性能 | 多线程录制、间接绘制 | E3 |
| E7 复杂场景验证 | 验收 | 压测全部子系统 | E4、E5、E6 |

### 准入与验收要点

- **E0**：`Complete`（2026-09-20）。七个隔离视图有公共基线并接入 CTest 与 CI；性能基准工具产出固定格式 JSON；故意引入的 shader 改动能被检出。基线 `character` 每帧 6 次 draw 与 6 次 descriptor 绑定，作为 E1、E2 的对比起点。
- **E1**：Vulkan 调用全部收拢到 RHI，后端为纯虚接口且提供可 mock 实现使 pass 逻辑无 GPU 可测；VMA 替换全部显存分配；bindless 描述符使绑定次数显著下降。E0 五视图像素零差异。
- **E2**：ECS 成为唯一运行期场景表示，组件改为密集存储；场景支持多对象与实例化；批次使同网格多实例的 draw call 不随实例数线性增长。单对象场景 E0 零差异。
- **E3**：pass 声明读写资源，图负责排序、瞬态资源分配与 barrier；新增 pass 不需修改公共帧代码；三个场景全部经由图渲染。
- **E4**：compute 通路建立；bloom 改为多级下采样上采样；IBL 改为 GPU 端运行期生成；skinning 与 morph 迁入 compute；支持 OpenEXR。
- **E5**：光源成为场景实体；采用聚簇或分块光照使成本与屏幕分块相关而非光源总数；级联阴影落地且保留现有 PCSS 半影特征。
- **E6**：逻辑与渲染数据双缓冲；多线程录制 secondary command buffer；剔除与绘制参数由 compute 写入间接缓冲。画面与单线程像素一致，确定性捕获仍逐帧一致。
- **E7**：对象与光源数量明显超过当前角色场景的压测场景，不改公共核心完成接入，规模化性能曲线符合亚线性预期。

每阶段必须产出与 E0 同格式的性能数据；没有数据支撑的性能主张不予接受。各阶段的改动面、逐步实施顺序、风险与提交标题见 [引擎化实施计划](ENGINE_EVOLUTION_PLAN_CN.md)。

## 4. 阶段执行规则

1. 一次只允许一个 `Active` 阶段。
2. 开始前冻结范围、公共验收资产、性能设备和提交标题。
3. 每阶段独立实现、测试、文档同步和 Git commit。
4. 视觉功能必须保存语义化命名的公共证据与 SHA-256。
5. Debug/Release、CTest、Validation、安装包资源检查和既有场景回归不可跳过。
6. 不通过降低验收标准或删除测试来完成阶段。

当前架构边界见 [架构文档](ARCHITECTURE_CN.md)，具体开发命令见 [开发指南](DEVELOPMENT_GUIDE_CN.md)。
