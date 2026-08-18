# AzureRender 当前开发计划

> 计划版本：2026-08-18 P0-P2
> 执行模式：Autonomous Implementation Mode
> 当前阶段：P0-P2 Complete
> 本文件是任务顺序、范围和完成状态的唯一事实来源。

## 1. 产品方向

AzureRender 是面向多项目复用的可插拔 Vulkan 渲染器，而不是只服务单个角色或单个 shader 的演示程序。公共核心负责 Vulkan 生命周期、HDR 合成、Capture、GPU Timing、编辑器和扩展注册；每个场景渲染器拥有自己的资源、pass、shader 和场景逻辑。

本轮固定顺序为：

1. `P0`：工程、文档与可插拔架构收口。
2. `P1`：黑洞模拟完成度。
3. `P2`：现有角色场景的终末地式渲染美术表现。

三个阶段连续执行，分别验证并独立提交。新增需求不得插入当前阶段。

## 2. 范围决策

- 删除旧 `M3/SC 工业科幻场景` 需求，不再保留为候选任务。
- P2 不创建新工业场景；它只改善 `CharacterSceneRenderer` 的灯光、材质、背景、地台和最终调色。
- 论文三路径、正式实验、Android 与论文交付无限期 `Deferred`，只有用户主动启用后才能重新进入计划。
- 动态 DLL 插件、脚本系统、资产热重载和完整 Undo/Redo 不属于 P0-P2。
- 进程内 `SceneRendererRegistry` 是当前扩展边界；未来场景通过稳定 ID 和 factory 注册，不在 App 主循环增加场景专属分支。

## 3. 阶段队列

| 顺序 | 阶段 | 状态 | 交付 |
|---:|---|---|---|
| 1 | P0 | Complete | 目录与文档规范化；应用实际通过 `SceneRendererRegistry` 创建场景；固定 P1/P2 验收 |
| 2 | P1 | Complete | 黑洞私有 trace/history、TAA、单 pass bloom、lensing、星空蓝移、确定性 Capture |
| 3 | P2 | Complete | 终末地式角色展示预设、分层灯光/背景/地台/grade、公共资产与私有角色 QA |

## 4. P0：架构与计划收口

### 必须完成

- 活动文档收敛为索引、概览、当前计划、开发指南和三个专题规范。
- 旧日志、阶段审计、DOCX 和过时交接只保留在 `docs/archive/`。
- 根目录只保留工程入口和仓库配置。
- `SceneRendererRegistry` 支持按 ID 创建单个 renderer。
- `AzureRenderApp` 注册并按 `SceneType` 稳定 ID 创建内置场景，不再使用场景构造 `switch`。
- CI 文档检查和安装包文档清单指向新体系。

### 验收

- Markdown 链接、状态和归档边界检查通过。
- Debug/Release 构建与 CTest 通过。
- 公共角色和黑洞 smoke 均可启动。
- Commit：`feat(p0): consolidate renderer architecture and project plan`

## 5. P1：黑洞模拟

参考资料位于 `D:/Assigment/temp`。参考实现的可移植原则为：首步/亚像素抖动、连续球对称步长、历史累积、多尺度亮区重建；不照搬 ShaderToy 的通道布局或硬编码分辨率。

### 必须完成

1. 将 trace pipeline 绑定到私有单颜色附件 render pass。
2. 一张 RGBA16F raw trace 与两张 ping-pong history 分离职责，避免把原始上一帧误当累积历史。
3. 每个 in-flight frame 使用独立 TAA descriptor set 和 UBO。
4. 通过 subpass dependency/barrier 保证 color write -> fragment sample。
5. 首帧、resize、场景加载和 capture 开始时清除或旁路无效 history。
6. TAA 使用基于时间半衰期的混合权重；确定性 Capture 使用固定 delta。
7. 在 TAA pass 中完成受控 HDR bloom，不覆盖公共 tone mapper。
8. 按当前相机坐标重新实现初始角向 lensing 修正。
9. 逃逸光线的星空颜色根据累积引力/多普勒 shift 产生有限蓝移，避免碎片和过曝。
10. Capture manifest/HUD 记录 TAA、bloom 和性能状态。

### 验收

- 1280x720 无黑屏、离散色块、几何碎片、未初始化 history 或 resize 残影。
- 静态相机在若干帧内稳定收敛；运动时不产生不可接受的长拖影。
- 黑洞 120 帧 Debug Validation 无 VUID/warning/error。
- Capture 同参数重复运行可复现。
- 角色 S36 Beauty 哈希不变。
- 记录相对 BH-2.1 约 14.1 ms main scene 基线的 GPU 成本。
- Commit：`feat(p1): complete blackhole temporal rendering`

### 完成证据（2026-08-18）

- 修复全屏三角形 UV 重复缩放；公开 1280x720 输出不再出现离散四边形星块。
- 两组 36 帧、60 fps 固定捕获逐帧 SHA-256 `36/36` 一致。
- Debug Validation 180 帧无 VUID/warning/error；Debug/Release CTest 均为 `12/12`。
- RTX 4060 Laptop GPU Debug timing：Main scene `12.885 ms`，Total `12.932 ms`；未回退于 BH-2.1 约 `14.1 ms` 基线。
- 公共证据首帧 SHA-256：`E750F12A7D585CCBD0613ECC2D6A50BC95103E8F8F41B532FC444448F910DC0C`。

## 6. P2：终末地式角色渲染美术

P2 的目标是让现有角色展示具有安静、克制、工业感明确的终末地式画面语言，同时保持角色可读性和材质验证能力。它不是新场景资产生产任务。

### 必须完成

- 将现有 `Endfield Industrial` preset 升级为版本化美术预设，其他 Neutral/QA preset 保持可用。
- 背景形成冷灰环境、结构化水平/网格信息和有限暖色信号，不使用纯装饰噪声。
- 地台和接触区域强化角色落地感，不遮挡轮廓或脚部。
- Key/Fill/Rim 分离，冷环境与暖强调色兼容皮肤、头发、布料和金属材质。
- 调整 exposure、contrast、saturation、bloom 和 outline，使近景与全身镜头都不过曝、不糊边。
- 所有参数通过现有 `RenderSettings`/preset 路径驱动，不在 App 添加资产专属分支。
- 生成公共资产 1280x720 证据；私有角色仅作本地补充 QA，不进入发布包。

### 验收

- Beauty、face close-up、front/back、stylized A/B 和 material-check 截图可读。
- 公共角色与编辑器 120 帧 Debug Validation 无异常。
- 黑洞 P1 输出不受角色美术改动影响。
- Capture manifest 能识别 P2 preset 和 grade 状态。
- Commit：`feat(p2): refine endfield-inspired character presentation`

### 完成证据（2026-08-18）

- `RenderSettings` 提供 v1 稳定预设名称与整套 look 应用入口；F1-F3、Portfolio 和 QA 不再分别维护零散参数。
- `Endfield Industrial` 使用冷灰结构背景、受控环境混合、暖色信号、石墨地台及分离的暖 Key/冷 Fill/青色 Rim。
- Grade 固定为 exposure `-0.15 EV`、saturation `0.88`、contrast `1.10`；Bloom threshold/strength 为 `1.15/0.10`，outline strength 为 `0.46`。
- 公共资产 1280x720 全身与近景 SHA-256 分别为 `71A76FA98B9E291BBF119700E4DCE58C9E643D02E3A27F2BC75FED004BC6FAB5`、`7FB1D2C98BF87062E78CC7F92601B8616191CEC16A4AFAB13721DFB9CAC896AF`。
- 公共 back-detail、stylized A/B、neutral material 捕获完成；私有角色全身和近景仅作本地 QA，未进入仓库。
- P2 后黑洞基准首帧仍为 `E750F12A7D585CCBD0613ECC2D6A50BC95103E8F8F41B532FC444448F910DC0C`，与 P1 逐字节一致。

## 7. 每阶段固定门禁

```powershell
git diff --check
cmake --build --preset ninja-debug
ctest --test-dir build\ninja-debug --output-on-failure
cmake --build --preset ninja-release
ctest --test-dir build\ninja-release --output-on-failure
.\build\ninja-debug\AzureRender.exe --smoke-frames 120
.\build\ninja-debug\AzureRender.exe --scene-type blackhole --smoke-frames 120
```

视觉阶段额外执行 1280x720 Capture 和截图检查。测试失败必须修复后重跑，不修改验收标准绕过问题。

## 8. 无限期 Deferred

- Traditional/Subpasses/Dynamic Rendering Local Read 正式论文实验。
- Android 平台、功耗实验、统计分析和论文交付。
- 跨 DLL 插件 ABI 和脚本运行时。
- 资源热重载、缩略图、依赖图和完整 Undo/Redo。

Deferred 内容不自动恢复，也不因 P0-P2 完成而成为“下一步”。
