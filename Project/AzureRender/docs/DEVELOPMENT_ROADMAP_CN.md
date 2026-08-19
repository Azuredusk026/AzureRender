# AzureRender 未来开发路线

> 路线版本：2026-08-18
> 当前状态：P0/P1/P2 与 R1-R5 全部完成；当前没有 Active 阶段。

黑洞 P1 已于 2026-08-19 进入 `Final / Frozen`，最终基线为 16 秒双机位展示及周期噪声无接缝实现。只有用户主动重新启用后才能继续变更；当前角色收尾不得触碰黑洞渲染路径。

本文是未来开发的唯一队列。它描述优先级和准入条件，不把尚未实现的内容写成当前能力。

## 1. 产品原则

- AzureRender 继续作为多场景、多 shader 的可插拔 renderer，而不是单一 demo。
- 公共核心只提供跨场景设施；算法、资源和美术逻辑留在场景 renderer。
- 所有正式功能必须有公共资产路径、自动化测试和可重复视觉证据。
- 私有角色可以补充本地 QA，但不能成为 CI 或发布依赖。
- 不为短期画面修改在 `AzureRenderApp` 增加资产或场景专属分支。

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

## 4. 阶段执行规则

1. 一次只允许一个 `Active` 阶段。
2. 开始前冻结范围、公共验收资产、性能设备和提交标题。
3. 每阶段独立实现、测试、文档同步和 Git commit。
4. 视觉功能必须保存语义化命名的公共证据与 SHA-256。
5. Debug/Release、CTest、Validation、安装包资源检查和既有场景回归不可跳过。
6. 不通过降低验收标准或删除测试来完成阶段。

当前架构边界见 [架构文档](ARCHITECTURE_CN.md)，具体开发命令见 [开发指南](DEVELOPMENT_GUIDE_CN.md)。
