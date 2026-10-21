# AzureRender 工程

三个公开模型工程已交付，支持编辑、保存和独立发布。
PT0 至 PT7 已完成，历史 P8 待项按原清单维护。
打开方法见[公开光追演示](docs/tutorials/public-pathtracing-demos.md)。
本轮范围见[路径追踪实施计划](docs/plans/2026-10-15-pathtracing-completion.md)。
WC1 至 WC6 完成，安装入口与演示项目已更新。
操作步骤见[世界参照、相机与路径追踪](docs/tutorials/world-camera-rendering.md)。

界面操作与模块归属见[编辑器界面说明](docs/tutorials/editor-interface.md)。

界面改进方向见[实施方案](docs/plans/2026-10-14-ui-modernization.md)。
可直接打开[视觉提案](docs/design/ui-modernization/preview.html)。
VD0 研究与设计完成，VD1 至 VD5 完成，VD6 为 Ready。
自动交付已验证，可进入集中人工复核。
启动入口与证据见[界面交付记录](docs/acceptance/vd/2026-10-14.md)。

构建目录、临时输出和定期清理见[产物管理](docs/development/artifact-retention.md)。

UX-A 至 UX-D 完成，UX-E 为 Ready。
自动检查已完成，新版使用体验等待集中确认。
交付入口与结果见[本轮交付](docs/acceptance/ux/2026-10-14.md)。
范围与阶段记录见[交互优化计划](docs/editor-usability-audit.md)。
用户已反馈人工复核基本可用，交互体验需要优化。

ED0 至 ED7 开发交付完成，P8 状态为 Ready。
开发任务见[编辑器实施计划](docs/plans/2026-10-14-editor-generalization.md)。
本轮机器批次已收尾，实体与历史条件待满足。

阶段状态见[当前状态](docs/plans/current-stage-status.md)。
阶段收尾与验收顺序见[执行说明](docs/acceptance/phase-closure.md)。

工作区迁移记录见[UI8 交付结果](docs/acceptance/ui8/2026-10-12.md)。
本轮检查见[P8 收尾记录](docs/acceptance/p8/2026-10-14.md)。

相机、变换与场景视口见[视口指南](docs/tutorials/viewport-workspace.md)。

对象选择与属性操作见[编辑指南](docs/tutorials/selection-properties.md)。

资源查找与日志操作见[面板指南](docs/tutorials/browser-console.md)。
资源与日志迁移结果见[阶段验证](docs/acceptance/ui5/2026-10-12.md)。

安装版入口、UI 与发布能力见[产品化状态](docs/ui-productization-status.md)。
自动工作流与剩余复核见[交付验证](docs/acceptance/ui4/2026-10-12.md)。

AzureRender 是使用 C++17 与原生 Vulkan 的 Windows 引擎。编辑器、运行时和独立 Player 提供内容制作、游戏运行与发布能力。公开探索项目用于验证这些公共能力。

## 从哪里开始

| 想了解什么 | 阅读入口 |
| --- | --- |
| 现在完成了什么，还有什么待验 | [项目现状](docs/project-status.md) |
| 接下来按什么顺序开发 | [开发总计划](docs/plans/azure-engine-plan.md) |
| 自动执行怎样推进，检查怎样安排 | [协作与验收](docs/development-workflow.md) |
| 构建并运行项目 | [构建与使用](docs/getting-started.md) |
| 从空项目制作并发布游戏 | [制作教程](docs/tutorials/editor-first-game.md) |
| 从安装版 GUI 创建、重开与发布项目 | [项目中心使用指南](docs/tutorials/project-center.md) |

## 当前可用能力

双击安装目录中的 `bin/AzureRender.exe` 进入项目中心。
新建或打开工程后进入编辑器，项目目录独立保存。
GUI 发布产生项目命名的 exe 与完整游戏目录。
启动和 UI 分层见[应用架构](docs/architecture-ui-productization.md)。
项目打开与资源初始化记录见[启动诊断](docs/development/startup-diagnostics.md)。

编辑器支持场景层级、属性编辑、选择变换、内容导入、撤销重做和运行预览。运行时提供 ECS、物理、脚本、动画、声音及游戏界面。独立 Player 按项目数据运行，并通过 Windows 游戏包交付。

最近一轮实现了类型化材质画布、像素坐标二维文档及 GIF、PSD、SVG、ASCII USD 导入。格式注册覆盖 29 种输入、20 种输出，各格式有明确支持边界。二维内容和材质图的范围见[项目现状](docs/project-status.md)。

本轮 Debug 与 Release 各核齐 188 项非 GPU 标记检查。
首轮各有 186 项通过，两项修复后定向复测通过。
图形回归、移动交付和正式任务见本轮收尾记录。
实体键鼠、听感与制作效率由人工复核确认。

| 制作任务 | 操作说明 |
| --- | --- |
| 编辑三维关卡 | [编辑器交互](docs/tutorials/editor-interaction.md) |
| 使用材质画布 | [材质制作](docs/runtime/material-authoring.md) |
| 接入二维内容 | [二维内容与投影标签](docs/runtime/scene-2d.md) |
| 导入与导出资产 | [资产交换](docs/runtime/asset-exchange.md)、[扩展格式范围](docs/assets/extended-formats.md) |
| 发布独立游戏 | [Windows 游戏发布](docs/runtime/game-publishing.md) |

![编辑器工作区与公开探索关卡](docs/media/editor-workspace.png)

图中为公开探索项目。图片用于展示编辑工作区，当前检查范围以[集成验收](docs/acceptance/p9/2026-10-12.md)为准。

## 构建编辑器

Windows 工具链为 MSVC、CMake、Ninja、Vulkan SDK 和 vcpkg。本机验收使用 MSVC 14.44 与 Vulkan SDK 1.4.350。

```powershell
$env:VULKAN_SDK = "C:/VulkanSDK/1.4.350.0"
$env:VCPKG_ROOT = "C:/path/to/vcpkg"
./tools/msvc_env.bat cmake --preset msvc-release
./tools/msvc_env.bat cmake --build --preset msvc-release
./build/ninja-msvc-release/AzureRender.exe --editor-project assets_public/exploration/project.azureproject
```

将 vcpkg 路径替换为本机目录。公开示例制作流程使用可写工作副本。安装与发布以完整目录为单位，操作见 [游戏发布](docs/runtime/game-publishing.md)。

## 工程与证据

`src/` 保存模块实现，`shaders/` 保存着色器，`tests/` 保存契约测试。`tools/` 保存构建与验收工具，`docs/` 保存使用说明、计划和证据。架构入口为[引擎基础](docs/runtime/engine-foundation.md)和[渲染器架构](docs/architecture.md)。

本轮证据见[P8 集中收尾](docs/acceptance/p8/2026-10-14.md)。延期条件见[P8 清单](docs/plans/p8-deferred-acceptance-manifest.json)。阶段交付记录见[变更记录](CHANGELOG.md)，整理前的索引见[历史快照](docs/history/2026-10-12-planning/readme-snapshot.md)。

## 资产与许可

`assets_public/` 包含公共示例、测试资产和许可。`assets_private/` 保存本机授权素材。公开包与截图使用许可明确的资源，依赖许可见[第三方声明](THIRD_PARTY_NOTICES.md)。

本批最终候选与收尾进度见[P8 执行记录](docs/acceptance/p8/2026-10-14.md)。

## 资源验证开发

原生可玩还原采用 NR0 至 NR12 阶段体系。
目标涵盖近乎原生画面、WASD 与原生运动动画混合。
场景还原覆盖本机观测到的全部可见对象与效果。
引擎缺失能力及验收见[可玩还原计划](docs/plans/2026-10-17-native-playable-restoration.md)。
实景、公开资料与抓帧见[证据执行计划](docs/plans/2026-10-21-native-render-evidence-execution.md)。
捕获与重放使用[RenderDoc 证据工具](docs/acceptance/nr/renderdoc-capture.md)。
夹具已取得原生浮点输出，抓帧布局校验仍待修复。
场景分项诊断与实景参考见[原生参考基准](docs/acceptance/nr/reference-baseline.md)。
参考校验读取真实图像编码，并核验正式参考的像素尺寸。
公开算法线索见[渲染资料核查](docs/research/2026-10-21-endfield-public-rendering.md)。
后处理支持[版本化颜色查找表](docs/runtime/color-grading-lut.md)。
主视口与辅助视口共享线性输出和曝光契约。
数值、编辑帧及资源回归见[调色验收](docs/acceptance/nr/color-grading-lut.md)。
材质支持独立[影色查找表](docs/runtime/shadow-color-lut.md)及 GPU 诊断。
无纹理基色采用最近字节量化，详见[量化验证](docs/acceptance/nr/base-color-quantization.md)。
原生动作与 WASD 的短路径结果见[操控验证](docs/acceptance/nr/playable-integration.md)。
速度驱动的多片段混合与步态同步已接入公共姿态入口。
双角色与原生动作证据见[混合验证](docs/acceptance/nr/locomotion-blending.md)。
原生走跑完整时域及端点来源见[动作证据](docs/acceptance/nr/native-locomotion.md)。

莱万汀资源验证按 EV0 至 EV8 推进。
通用形变、表情编辑、场景交换与材质恢复已交付。
原生联合展示采用编队待机 01 与中性表情。
原生目录与资源包读取已通过本机验证。
原生读取支持[显式资源分层](docs/acceptance/nr/layered-content.md)。
更新目录与共享基础块可组合读取，并保留来源收据。
场景转换支持[原作最高细节选择](docs/acceptance/nr/source-geometry-detail.md)。
原生待机候选已进入引擎，材质与物理效果待完善。
完整原生走跑已通过[两组速度混合回放](docs/acceptance/nr/native-locomotion.md)。
原生表情源已取得，演示保持中性。
帝江号中心切片已与编队待机 01 完成联合技术采样。
场景实例编辑与保存重开已验证。
47 个场景材质支持表面通道烘焙。
当前版本可读取 232 个静态灯。
联合候选使用 45 个点光及聚光。
颜色、GI 与反射的十二帧结果见[联合验证](docs/acceptance/nr/joint-colour-lighting.md)。
完整着色与原生环境光照还原仍待验收。
通用聚光与显式场景照明通过公开 GPU 夹具。
分项证据见[直接光机制](docs/acceptance/nr/local-lighting.md)。
局部灯支持[显式距离衰减](docs/runtime/local-light-attenuation.md)。
原生指数来源与公共验证见[衰减证据](docs/acceptance/nr/light-attenuation.md)。
场景[方向光](docs/runtime/directional-lights.md)接入光栅及路径追踪。
场景文档保存完整形状与衰减参数。
显式场景照明按主方向光生成级联阴影。
灯光支持独立世界空间的[盒状作用体积](docs/runtime/light-influence-boxes.md)。
独立局部反射支持原作层级、盒投影与体积混合。
公共 GPU 夹具及中庭候选见[反射接入](docs/plans/2026-10-18-local-reflection-probe.md)。
中庭反射候选已完成四帧采样，整体照明仍待还原。
方向辐照网格支持世界插值及场景资源定位。
公共 GPU 验证见[网格契约](docs/runtime/irradiance-volumes.md)。
中枢物流已读取 21 个原生样条描述。
运行语义与实例接入见[物流样条证据](docs/acceptance/nr/logistics-evidence.md)。
单发射器已验证实际运动，支持[刚体节点动画](docs/runtime/rigid-node-animation.md)。
53 个上层对象已完成[联合采样](docs/acceptance/nr/upper-geometry.md)。
角色表面及显式切线见[角色通道接入](docs/acceptance/nr/character-surfaces.md)。
脸部支持[双图距离场](docs/runtime/dual-face-sdf.md)与独立线性色带。
蓝色通道法线用于局部漫反射与轮廓光。
分支范围与数值验证见[脸部法线证据](docs/acceptance/nr/face-sdf-normal.md)。
局部场景灯逐灯消费脸部色带及头部动画。
灯光支持[逐灯镜面倍率](docs/runtime/light-specular-response.md)，原生中庭源倍率已进入联合候选。
原生局部算法已核验，完整角色着色仍待还原。
透明表面支持[场景颜色折射](docs/runtime/scene-color-refraction.md)。
中庭一个原生玻璃材质已进入[联合候选](docs/acceptance/nr/native-glass.md)。
展示记录见[帝江号材质与光照](docs/acceptance/ev/dijiang-lighting-video.md)。
该展示提供 1080p、24 帧、16 秒的实际引擎录像。
获取主线见[原生资产计划](docs/plans/2026-10-17-endfield-native-assets.md)。
实际证据见[原生资产验收](docs/acceptance/ev/native-assets.md)。
历史画面基准与 GPU 性能验收保留待项。
实施范围见[资源验证计划](docs/plans/2026-10-16-endfield-resource-validation.md)。
当前证据见[联合验收](docs/acceptance/ev/2026-10-16.md)。
