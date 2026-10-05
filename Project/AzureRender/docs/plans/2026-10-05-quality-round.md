# 编辑器与可玩体验优化计划

> 文档类型：开发计划
> 状态：F3、R6、G7、U2 Complete，P2 Active
> 更新日期：2026-10-05
> 适用范围：Windows 编辑器、公开探索关卡与独立 Player
> 需求依据：仓库整理、README、UE 风格界面、动画方向、Shift 与显示异常

## 目标与架构

本轮交付整洁的仓库、完整的编辑器工作区和可玩包。角色前向、冲刺与场景可见性进入独立验收。教程、展示图片和发布清单对应同一构建。

编辑器沿用 Dear ImGui、EditorSession 和生产编辑命令。角色沿用固定步、Jolt 和实体动画。渲染沿用现有 Vulkan 通道与 CPU/GPU 可见性路径。界面组织参考 UE，样式与资源由本项目实现。

## 调查依据

本表记录基线提交 `aa9e0f1` 的调查结果。检查日期为 2026-10-05。用户附件展示仓库页面，用于目录整理需求。动画与显示症状由 R6、G7 进行 GPU 复现。

| 项目 | 已核对的事实 | 所属阶段 |
| --- | --- | --- |
| 工作分支 | `engine-dev`，基线提交 `aa9e0f1` | F3 |
| 根目录工具状态 | `.codemaker/codemap` 的五个文件被跟踪，约 6.7 MB | F3 |
| 本地远端引用 | `origin/main` 为 `c881a8f`，树中含 `.claude`、`.codemaker` 和 `opencode.json` | F3、P2 |
| 当前工作树 | `.claude` 与 `opencode.json` 未被跟踪 | F3 |
| 展示入口 | 根 README 与站点首页引用黑洞展示图 | P2 |
| UI 初始化 | 默认深色主题，默认停靠布局为左右与底部面板 | U2 |
| 新增面板停靠 | Build Game、Animation Preview、Gameplay Debug 缺少默认停靠登记 | U2 |
| 冲刺 | `InputActions` 无 Shift 动作，公开主角 `speed=2` | G7 |
| 模型朝向 | 公开角色脚部沿局部 −Z 延伸，转向公式以 +Z 为基准 | G7 |
| 相机范围 | 主投影与级联划分使用 0.1 至 100，关卡目标约在 Z=−330 | R6 |
| 几何绕序 | 公共主角 180 个三角形、场景立方体 12 个三角形与法线方向一致 | R6 |
| 显示路径 | 已有 CPU/GPU 视锥剔除、材质背面剔除与深度测试 | R6 |
| 自动化覆盖 | P1 覆盖任务和稳定性，本轮增加方向与定点可见性断言 | R6、G7 |
| CI 分支 | 推送触发范围为 `dev`、`main`，工作分支为 `engine-dev` | F3 |

源码检索尚未发现基于深度金字塔的遮挡剔除通道。显示问题按视锥、背面、裁剪与深度测试分别取证。模型朝向与步态相位也分别验证。

## 阶段与顺序

| 顺序 | 阶段 | 状态 | 优先级 | 交付物与完成条件 |
| --- | --- | --- | --- | --- |
| 1 | F3 仓库与文档基础 | Complete | P0 | 删除清单、忽略规则、准确 README 结构、CI 与链接检查 |
| 2 | R6 场景可见性与裁剪 | Complete | P0 | 最小复现、默认渲染修复、镜像与边界 GPU 验收 |
| 3 | G7 动画方向与冲刺 | Complete | P0 | 前向一致、双 Shift、组件兼容、真实操控验收 |
| 4 | U2 编辑器工作区 | Complete | P1 | 停靠、样式、面板交互、DPI、教程与使用验收 |
| 5 | P2 展示与独立交付 | Active | P1 | 最终截图、README、公开游戏 ZIP、完整回归与证据清单 |

阶段按表中顺序进入 Active。F3 先登记问题与复现输入。R6 提供稳定画面，供 G7 与 U2 验收。P2 使用最终界面与角色表现生成展示媒体。

文件级步骤见[仓库整理计划](2026-10-05-repository-plan.md)、[可玩表现计划](2026-10-05-playable-quality-plan.md)和[编辑器工作区计划](2026-10-05-editor-ui-plan.md)。本文负责范围与阶段状态，子计划负责实施任务。

## 全局约束与审查重点

Windows x64、C++17、Vulkan、Dear ImGui、Jolt、Lua 和 RmlUi 为实施基础。Android 状态为 Deferred。用户私有素材保留在本机授权范围。公开媒体与包使用许可明确资源。

每项修复先保存可复现失败，再实现并回归。计划中的新增文件与接口均为待实施产物。运行时说明在行为验证后同步。每阶段单独提交 `feat(<phase>): <中文摘要>`。

| 审查重点 | 期望行为 | 验证归属 |
| --- | --- | --- |
| 名称含 legacy 的活动回退路径 | 保留仍被设备降级与回归使用的能力 | F3 |
| 镜像、非均匀缩放、动画边界 | 默认路径可见，材质正反面语义正确 | R6 |
| 按住 Shift 后失焦、暂停、重开 | 输入释放，速度与任务状态恢复 | G7 |
| 小窗口、高 DPI、损坏布局 | 关键操作可达，布局恢复，输入命中正确 | U2 |
| 最终截图与独立包来源 | 源码、产物、素材、捕获参数与清单关联 | P2 |

## P2 展示与交付步骤

- [ ] 从空项目按 U2 教程制作公开关卡，记录实际步骤与耗时。
- [ ] 在最终构建捕获编辑器工作区、机器人侧面动作与探索关卡。
- [ ] 将三张精选图放入 `portfolio/images/editor/` 与 `portfolio/images/gameplay/`。
- [ ] 更新根 README、工程 README、`docs/index.md` 与 `portfolio/portfolio_manifest.json`。
- [ ] 完成全部 Debug、Release、视觉、性能、安装与公开包检查。
- [ ] 用公开包执行 20 次切关、20 次重开与真实 30 分钟长跑。
- [ ] 在 RTX 4060 Laptop 的 1080p 和 Intel 核显的 540p 验证既定范围。
- [ ] 生成游戏 ZIP，解压至含空格路径，隔离 PATH 并完成全部任务。
- [ ] 写入 `docs/acceptance/p2/<date>.md` 与证据 manifest。
- [ ] 检查源码、图片、许可、包哈希、文档与暂存范围，提交 P2。

README 首屏包含项目定位、编辑器截图和三个入口。入口为运行演示、构建编辑器、阅读教程。后续章节依次为能力、验证范围、结构、开发路线和许可。根入口使用仓库相对图片路径。

展示图只包含公开资产，旁边注明构建与场景。截图捕获后检查文字、布局和物体可见性。哈希、尺寸、源码与捕获参数进入展示 manifest。仍被验收证据引用的图片保留稳定路径。

## 完整完成门禁

全部测试要求零失败，新增测试进入 CTest 发现。GPU 验收串行运行，源码在采样期间固定。现行预算沿用[第三人称计划](third-person-playable-plan.md)。标准负载三轮均预热 300 帧，再采样 1800 帧。

GPU P95 至多为 16.6 ms，Player CPU 工作 P95 至多为 8 ms。物理单步 P95 至多为 2 ms。提交额外耗时 P99 至多为 8 ms，提交帧 P99 至多为 33.3 ms。稳定驻留与工作集增长至多为 5%。

编辑器 CPU 工作 P95 沿用 10 ms 门禁。实测条件记录分辨率、面板状态、负载和设备。透明材质、阴影、描边与固定描述符路径分别回归。视觉参考更新须附带问题证据和对应变更说明。

## 验证入口

以下为现有验证入口。新增专项命令在各子计划中标为待实施。文档计划通过检查仅表示结构有效。

```powershell
ctest --test-dir build/ninja-msvc-debug --output-on-failure
cmake -DBUILD_DIR=build/ninja-msvc-release -DCONFIG=Release -P tools/run_release_gate.cmake
python tools/run_playable_performance.py --player build/ninja-msvc-release/AzurePlayer.exe --editor build/ninja-msvc-release/AzureRender.exe --output build/quality/performance
python tools/test_editor_playable.py --executable build/ninja-msvc-release/AzureRender.exe --output build/quality/authoring --install build/ninja-msvc-release/release-gate/install-moved
```

## 来源

[UE 5.8 编辑器界面](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-editor-interface)于 2026-10-05 核对。参考范围为菜单、工具栏、视口、Outliner、Details 和内容浏览器。界面资源、控件实现与许可见 U2 子计划。
