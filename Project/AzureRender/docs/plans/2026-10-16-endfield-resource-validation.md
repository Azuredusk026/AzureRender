# 单角色、场景、动画与表情验证实施计划

日期：2026-10-16。
状态：EV0 完成，EV1 诊断交付且原生读取待输入。
EV2 至 EV4 已交付通用实现与局部验证。
EV5 至 EV7 集成验证中，EV8 正在执行。
目标：莱万汀在一个可编辑场景中播放动作与表情。
本轮按自主实施模式执行全部阶段。
场景、动作和表情候选由用户选择。

## 目标与架构

本计划交付可复现的离线资源管线。
引擎提供通用形变、动画、场景与材质能力。
游戏读取器与 Blender 负责源格式解释和烘焙。
角色映射、场景选择与表情名称属于项目数据。

技术采用 Blender 5.1.2、Python、glTF 2.0 和 GLB。
引擎侧沿用 C++、Vulkan 与现有资源注册接口。
Ruri 桥接采用匹配版本的 DLL 与 .NET 10。
需求依据为本次请求与[调查记录](../research/2026-10-16-endfield-resource-validation.md)。

实施按任务逐项验证，使用复测通过的阶段提交。
执行者先读取仓库 `AGENT.md` 和上述调查记录。
新增公共能力同时验证莱万汀与公共测试夹具。
正式执行采用仓库自主实施规则。

## 验收对象与范围

| 对象 | 选取规则 | 必需结果 |
| --- | --- | --- |
| 角色 | 现有莱万汀修复版为外观基线 | 保留 468 关节与 13 材质的有效绑定 |
| 身体动作 | 首先使用现有 `idle` | 可播放、暂停、定位与循环 |
| 原生动作 | 可读资源中选同角色短待机 | 记录 clip、Avatar 与容器身份 |
| 场景 | 选已读取、依赖完整的最小室内或平台切片 | 保留层级、静态实例与空间关系 |
| 表情 | 中性、闭眼、张嘴及混合 | 身体动作与表情同时播放 |
| 第二用法 | 公共机械夹具与共享实例场景 | 验证通用契约及自动回归 |

场景目标限定一个连续空间。
优先选择约 20 米范围、200 个可见实例以内的切片。
数据选定后记录真实统计和边界框。
超预算时选择完整的较小切片并记录选择依据。

当前游戏场景与原生动作尚无可读样本。
EV1 负责确定实际资源 ID 和依赖闭包。
原生数据待项保持开放，引擎任务继续推进。
公共场景与重定向动作仅承担各自验证职责。

## 全局约束

- 源资产只读，派生产物写入独立候选目录。
- 本机角色与游戏资源存入 `assets_private/`。
- 通用源码和公共夹具使用可分发内容。
- 输入、工具、配置和输出均记录 SHA-256。
- 坐标统一为米制、Y 向上，正向写入资产配置。
- 每项损失注明来源、影响、处置和验收状态。
- 编辑、脚本和自动测试共用生产操作。
- 导入通过暂存、验证和原子提交完成。
- GPU 缓冲按在途帧持有，关闭等待所属工作完成。
- 解析与转换在工作线程执行，文档提交回到主线程。
- Ruri 与游戏专用映射属于离线工具依赖。
- 运行时通过通用资源契约消费转换产物。
- 公开代码采用 AGPL-3.0，复用前登记许可义务。
- 正式性能、双设备与长跑沿用 P8 集中验收。

## 重点失败条件

| 输入或状态 | 必需行为 | 负责阶段 |
| --- | --- | --- |
| sparse accessor、无基础缓冲、非法索引 | 正确展开合法数据并拒绝越界 | EV3 |
| 两实例共用资源且表情不同 | 可变状态隔离 | EV4、EV8 |
| 同名骨骼、不同绑定矩阵 | 骨架指纹不匹配时拒绝自动合并 | EV2 |
| 镜像与非均匀缩放 | 法线、绕序、实例变换保持一致 | EV6 |
| 重导入期间取消或依赖缺失 | 文档和资产数据库保持完整 | EV6 |

## 阶段顺序

| 阶段 | 优先级 | 依赖 | 独立交付 |
| --- | --- | --- | --- |
| EV0 基线与清单 | P0 | 本机样本 | 来源清单与可重复基线 |
| EV1 读取器与 REA | P0 | EV0 | 兼容结论、原生资源候选或明确待项 |
| EV2 角色与动作转换 | P0 | EV0 | 当前角色的确定性播放资产 |
| EV3 通用形变数据 | P1 | EV0 | 多目标和完整动画采样 |
| EV4 形变渲染集成 | P1 | EV3 | CPU、GPU 与全部消费者一致 |
| EV5 表情组合与编辑 | P1 | EV2、EV4 | 骨骼和形态键表情叠加 |
| EV6 场景交换 | P1 | EV0；原生分支依赖 EV1 | 可编辑场景与共享实例 |
| EV7 材质映射 | P1 | EV2；原生分支依赖 EV1 | 显式通道与效果差异报告 |
| EV8 联合验收 | P2 | EV2 至 EV7 | 单场景演示、公共回归与交付记录 |

EV1 的外部输入待项不改变后续公共能力验收。
原生验证完成必须具备真实游戏数据证据。
各阶段提交采用 `feat(evN): <中文摘要>`。
提交前核对差异、暂存范围、测试与文档状态。

## EV0：固定样本与环境

修改 `tools/character_intake.py`。
新增 `tools/test_resource_validation_manifest.py`。
新增 `assets_public/resource_validation/` 公共夹具目录。
运行证据写入 `build/runs/` 的本次任务目录。

清单版本为 `schemaVersion: 1`。
必需字段为 `sources`、`tools`、`settings`、`outputs`。
另含 `counts`、`losses`、`evidence` 和 `sourceKind`。
来源类别为游戏原生、重定向、程序生成或人工制作。

- [x] 为清单缺哈希、未知版本和缺依赖编写失败用例。
- [x] 为目标数、skin 数及骨架指纹增加统计。
- [x] 统计 JOINTS_1、WEIGHTS_1 和 sparse accessor。
- [x] 固定修复版外观与现有 idle 的五帧基线。
- [x] 使用 Blender 5.1.2 独立配置验证导入。
- [x] 记录 CPU、GPU、驱动、内存和磁盘预算。
- [x] 运行清单测试与现有角色准入测试。
- [x] 保存来源清单、截图配置及测试结果后提交。

骨架指纹覆盖父子关系、稳定路径和绑定矩阵。
表情检查同时记录面部骨骼与形态目标。
全部统计来源为解析结果，而非文件名推断。

## EV1：读取器与 REA 验证

诊断与有界样本结果见[EV1 验收](../acceptance/ev/ev1.md)。
原生读取需要兼容游戏 1.0.14 的专用输入。

新增 `tools/probe_resource_reader.py`。
新增 `tools/test_resource_reader_probe.py`。
诊断输出保存在本机清单与 REA 证据包中。
工具入口接受 `--bin-dir`、`--game-root`、`--output`。

按以下顺序形成有界调查。

- [x] 固定 Importer、读取器及其依赖的版本和哈希。
- [x] 用 REA `inspect_managed_artifact` 检查读取器 DLL。
- [x] 记录目标框架、依赖、资源和可用适配线索。
- [ ] 用受控探针验证桥接加载与一个标准公共 Bundle。
- [ ] 验证目标游戏索引及一个候选资源的依赖闭包。
- [ ] 输出角色、场景、动作、Avatar 和表情候选 ID。
- [ ] 对具体解析失败保存偏移、错误和输入哈希。
- [ ] 依据失败进入单个托管方法或本地函数分析。
- [x] 将观察、推断和未知项分别写入诊断报告。
- [x] 验证缺 DLL、版本不符和缺依赖的受控失败后提交。

REA 静态结果只证明其实际观察范围。
方法存在与元数据可读均不能证明游戏资源可解析。
每个失败最多追加一次具有新假设的诊断。
之后登记缺失输入及重试触发条件，继续独立任务。

当确需本地二进制分析时，使用对应目标与函数入口。
保存 Evidence ID、输入哈希、工具版本及覆盖限制。
每次原生会话完成后关闭分析目标。
游戏运行观察另行定义最小场景与操作范围。

## EV2：角色与单动作转换

沿用 `tools/retarget_animation.py` 与 `tools/prepare_character.py`。
新增 `tools/blender/export_validation_asset.py`。
新增 `tools/test_validation_asset_export.py`。
新增 `tools/compare_deformation_reference.py`。

转换接口接受输入、配置与独立输出目录。
输出 `character.glb`、`manifest.json` 和参考采样。
参考采样包括时间、骨骼世界矩阵及关键顶点位置。
采样空间、单位和变换顺序必须写入文件。

- [ ] 验证修复版与动作源的骨架指纹。
- [ ] 为同名异骨架、绑定差异和缺曲线编写失败用例。
- [ ] 建立保留修复材质的独立动作候选。
- [ ] 保留输入 GLB 的自定义材质元数据。
- [ ] 对原生候选记录 Humanoid、Avatar 与根运动规则。
- [ ] 将已求解的动作采样为 60 Hz 骨骼曲线。
- [ ] 在五个固定时刻及半帧时刻比较参考姿态。
- [ ] 验证单动作播放、暂停、定位和循环边界。
- [ ] 运行转换、重定向及 `AnimationPose` 回归后提交。

初次采样比较采用最大位置误差 1 mm。
旋转误差上限为 0.1°，权重误差上限为 0.001。
误差超限时提高离线采样精度或修复转换。
源动作本身的循环跳变单独报告并保留原片段。

已有重定向动作和游戏原生动作分别记录结果。
原生来源待项只能由真实原生候选关闭。
所有输出保留素材名称与源容器标识映射。

## EV3：通用形变与动画数据

修改 `src/assets/GltfLoader.hpp`、`GltfLoader.cpp`。
新增 `src/assets/MorphTargetData.hpp`。
新增 `src/animation/AnimationCurve.hpp` 与 `.cpp`。
新增 `tests/MorphTargetTests.cpp` 与 `AnimationCurveTests.cpp`。

新增 `MorphTargetData`，持有名称与三类形变增量。
增量包括 POSITION、NORMAL 与 TANGENT。
网格持有只读目标数据，实例持有独立权重。
`MorphWeightState` 使用 `std::vector<float>` 保存权重。

`AnimationCurve` 保存时间、插值、值宽度与连续值数组。
采样接口为 `sampleCurve(const AnimationCurve&, float)`。
返回 `std::vector<float>`，宽度由通道契约验证。
旋转采用四元数语义，权重采用逐分量语义。

- [ ] 增加 0、1、2、8、64、65 个目标的边界测试。
- [ ] 将每网格默认预算设为 64 个目标。
- [ ] 超预算返回包含来源和数量的诊断。
- [ ] 支持 sparse accessor 和合法缺省基础缓冲。
- [ ] 实现节点权重优先于网格权重的默认值规则。
- [ ] 实现 TRS 与可变宽度 weights 通道。
- [ ] 实现 STEP、LINEAR 与 CUBICSPLINE。
- [ ] 按时间间隔缩放三次曲线切线并归一化旋转结果。
- [ ] 拒绝非有限值、非法索引、错误宽度及时间序列。
- [ ] 验证旧两目标资产与公共机械形变夹具后提交。

合法曲线权重保留其数值，界面范围属于编辑元数据。
自动夹紧须由资源显式配置并记录损失。
新增 CTest 名称为 `AzureAssets.MorphTargets`。
曲线测试名称为 `AzureRender.AnimationCurve`。

## EV4：统一形变消费者

修改 `src/assets/AssetDeformation.hpp`。
修改 `src/render/DeformedBounds.hpp` 与 `SkinningBatch.hpp`。
修改 `src/scenes/CharacterSceneRenderer.cpp` 及其数据声明。
修改 `src/render/pathtracing/TraceScene.cpp`。

更新 `skin.comp`、`mesh.vert`、`shadow.vert`。
更新 `outline.vert`、`inner_outline.vert` 及共享着色声明。
复用 `AssetDeformationTests.cpp` 和 `SkinningGpuTests.cpp`。
新增 `tools/test_morph_consumer_consistency.py`。

统一计算顺序为形变、蒙皮、实例世界变换。
CPU 与 GPU 使用相同目标索引和权重布局。
GPU 权重按实例和在途帧分配，资源目标只读共享。
零权重目标可以跳过，非零目标须完整计算。

- [ ] 写入第三个目标才产生明显位移的失败夹具。
- [ ] 写入非零法线与切线增量的照明夹具。
- [ ] 建立可变权重缓冲和版本化描述符布局。
- [ ] 更新计算蒙皮及顶点着色器回退路径。
- [ ] 对照 CPU 参考，验证全部目标与四权重蒙皮。
- [ ] 更新阴影、描边、拾取和动态包围盒。
- [ ] 更新路径追踪几何刷新及加速结构生命周期。
- [ ] 验证两实例独立表情、连续换资源和关闭释放。
- [ ] 运行形变、蒙皮、同步及相关画面回归后提交。

公共单位夹具的 CPU/GPU 最大位置差为 0.00001 米。
莱万汀与 Blender 的参考差沿用 EV2 标准。
动态包围盒须包含全部验证姿态的顶点。
路径追踪与光栅化分别记录支持模式和结果。

## EV5：骨骼表情与编辑操作

修改 `src/ecs/GameComponents.hpp`。
修改 `src/runtime/PresentationRuntime.cpp`。
修改 `src/runtime/AnimationStateMachine.hpp` 与 `.cpp`。
修改 `src/scene/SceneDescription.hpp` 的动画帧契约。

修改 `src/editor/animation/AnimationPreviewService.hpp` 与 `.cpp`。
修改 `src/editor/panels/AnimationPanel.cpp`。
新增 `src/animation/ExpressionPose.hpp` 与 `.cpp`。
新增 `tests/ExpressionPoseTests.cpp`。

`ExpressionPose` 使用版本、骨架指纹和目标映射。
骨骼数据保存相对中性姿态的局部变换增量。
形态数据保存稳定目标 ID 与权重。
项目内预设通过 UUID 引用，名称用于展示。

求值顺序为身体动作、面部遮罩层、实例手动覆盖。
面部层以参考中性姿态定义加法旋转与位移。
资源声明混合方式、顺序、权重范围与缺项策略。
核心通过骨骼路径和目标 ID 处理任意角色。

- [ ] 为身体动作与闭眼层共存编写失败测试。
- [ ] 用 Blender 验证现有眼睑、下颌的实际变形作用。
- [ ] 制作明确标注人工来源的中性、闭眼和张嘴预设。
- [ ] 接入可用原生表情，记录其骨骼或形态通道。
- [ ] 注册按目标 ID 设置权重与应用预设的编辑操作。
- [ ] 实现撤销、重做、保存、重开和 Play/Stop 恢复。
- [ ] 将旧 morph0/morph1 配置迁移为前两个权重。
- [ ] 验证闭眼与张嘴混合及三秒表情时间段。
- [ ] 运行 `ExpressionPose` 和 `AnimationAuthoring` 后提交。

新增 CTest 名称为 `AzureRender.ExpressionPose`。
面部骨骼测试以网格变形和参考图确认结果。
若现有骨骼无法完成目标，记录缺失权重或曲线。
原生表情取得前，来源状态保持人工制作或待输入。

## EV6：保留层级的场景交换

新增 `tools/blender/export_scene_exchange.py`。
新增 `src/assets/scene/SceneExchange.hpp` 与 `.cpp`。
新增 `src/editor/scene/SceneImportService.hpp` 与 `.cpp`。
修改 `src/assets/AssetTransfer.cpp` 与相关请求契约。

接入 `src/runtime/SceneDocument.cpp` 和编辑事务。
新增 `tests/SceneImportTests.cpp`。
新增 `tools/test_scene_exchange.py`。
新增 `tools/test_scene_import_workflow.py`。

交换描述采用版本 1，包含资源表和节点表。
节点字段为 ID、parent、localTransform 与 meshRef。
材质覆盖、相机、灯光和来源 ID 使用显式字段。
重复实例引用同一网格资源，节点保持独立状态。

解析接口为 `readSceneExchange(path)`。
返回纯数据 `SceneImportProposal` 与结构化诊断。
`SceneImportService` 通过编辑事务提交该提案。
源 ID 与引擎 UUID 的映射用于重导入差异计算。

- [ ] 构造双实例、父子层级、镜像与非均匀缩放夹具。
- [ ] 验证重复 ID、循环父链和缺资源的原子失败。
- [ ] 导出一个完整小场景及依赖资源。
- [ ] 将候选转换为 `.azurelevel` 与共享模型资源。
- [ ] 复用既有相机和灯光描述，缺失语义写入损失表。
- [ ] 验证插入、撤销、重做和重导入冲突提示。
- [ ] 验证取消、失败恢复、保存重开与移动目录。
- [ ] 验证角色动作及表情可在导入场景运行后提交。

新增 CTest 名称为 `AzureAssets.SceneImport`。
工作流名称为 `AzureEditor.SceneImportWorkflow`。
原生场景的验收包含源节点与引擎节点映射表。
公共夹具用于验证导入机制及发布可移植性。

## EV7：材质语义与外观

沿用 `src/runtime/MaterialDocument.cpp`。
沿用 `src/editor/material/MaterialEditingService.cpp`。
新增 `tools/convert_material_semantics.py`。
新增 `tools/test_material_semantics.py`。

映射输入包含 shader 身份、贴图角色与通道语义。
输出采用现有 `.azurematerial` 和材质配置契约。
基色、法线、粗糙度、金属度与自发光分别定义色彩空间。
特殊头发、脸部与透明参数通过显式配置接入。

- [ ] 为 AO、粗糙度、发光通道互不串用编写夹具。
- [ ] 固定现有 13 个材质的稳定槽位与来源。
- [ ] 导出角色及场景的完整材质映射表。
- [ ] 验证 Blender 导出后自定义材质信息可恢复。
- [ ] 对深度、场景颜色及多 Pass 依赖逐项分类。
- [ ] 为首个场景必需的缺失能力编写独立实现与测试。
- [ ] 验证正面、侧面、背面和表情近景。
- [ ] 运行材质文档、角色修复与透明回归后提交。

完整画面还受灯光与后处理影响。
数值验收使用固定灯光的公共材质夹具。
艺术外观使用固定镜头对照并由操作者复核。
每项近似效果单独登记，原生还原状态以证据为准。

## EV8：联合验证与交付

新增 `tools/run_resource_validation.py`。
新增 `docs/acceptance/ev/2026-10-16.md`。
同步 `docs/assets-and-editor.md` 与 `docs/character-rendering.md`。
同步 README、开发总计划及生成来源清单。

验证入口接受项目、角色、场景、动作及表情配置。
输出机器可读结果、固定帧图像和短视频。
公开夹具与本机角色各有独立 manifest。
所有结论关联源码、产物和设备指纹。

- [ ] 在一个场景中播放一个角色的指定动作。
- [ ] 播放中性、闭眼、张嘴及混合表情。
- [ ] 录制 20 秒片段并保存五个姿态采样点。
- [ ] 验证编辑器操作、独立 Player 和目录移动。
- [ ] 执行相关旧场景、双实例及公共机械夹具回归。
- [ ] 执行 Debug、Release 与受影响 GPU 路径检查。
- [ ] 记录加载峰值、帧时间、显存和内存。
- [ ] 在冻结候选上执行 P8 集中性能与长跑。
- [ ] 复核所有损失、来源待项和人工结果后提交。

初始局部预算为 1280×720、RTX 4060、光栅模式。
热身后 60 秒的第 95 百分位帧时间目标为 16.7 ms。
进程工作集目标小于 12 GiB，显存目标小于 6 GiB。
以上为新增目标，现行更严格预算仍然有效。

完整完成需要引擎验收与原生来源验收同时通过。
缺少游戏原生场景、动作或表情时逐项标注待输入。
既有重定向与人工表情结果保留自身完成状态。
剩余问题列出责任输入、失败证据和重试条件。

## 构建与验证命令

命令从 `Project/AzureRender` 执行。
下列命令为实施阶段入口，本次编制未运行构建。

```powershell
./tools/msvc_env.bat cmake --build --preset msvc-release
ctest --test-dir build/ninja-msvc-release --output-on-failure -R 'AnimationPose|AssetDeformation|SkinningBatch|SkinningGpu|IndependentAnimationGpu|AnimationAuthoring|MaterialDocument|AssetLevel'
```

新增测试在所属阶段注册到 `CMakeLists.txt`。
每个新增能力先验证失败用例，再实现并复测。
新测试运行如下，缺少匹配项视为验证配置失败。

```powershell
ctest --test-dir build/ninja-msvc-release --output-on-failure --no-tests=error -R 'MorphTargets|AnimationCurve|ExpressionPose|SceneImport'
python -m unittest discover -s tools -p 'test_resource*.py'
python -m unittest discover -s tools -p 'test_validation_asset_export.py'
python -m unittest discover -s tools -p 'test_scene_exchange.py'
python -m unittest discover -s tools -p 'test_material_semantics.py'
```

## 按样本触发的扩展

多 skin 与八权重属于明确的后续扩展候选。
EV0 统计或 EV1 原生样本需求触发其设计。
多 skin 需要 primitive、节点与 skin 的绑定契约。
八权重需要完整更新 CPU、GPU 与参考误差测试。

当转换需裁剪权重时，必须记录累计损失与顶点误差。
超出 EV2 精度时，扩展支持后再完成该样本验收。
布料与头发物理先通过离线烘焙验证可见运动。
实时物理、完整大世界与战斗特效另立专项计划。
