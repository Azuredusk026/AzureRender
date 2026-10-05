# R6 与 G7 可玩表现实施计划

> 文档类型：阶段计划
> 状态：R6 Active，G7 Planned
> 更新日期：2026-10-05
> 适用范围：公开角色、场景可见性、角色输入与独立 Player
> 需求依据：[编辑器与可玩体验优化计划](2026-10-05-quality-round.md)

## R6 复现与因果定位

先使用 P1 独立包和标准负载复现用户症状。记录对象身份、相机位置、帧号和材质。保存默认路径、诊断对照和错误区域标注。

| 对照 | 输入 | 判定目标 |
| --- | --- | --- |
| 默认与直接提交 | `--disable-gpu-culling` | GPU 可见性与间接命令 |
| 默认与全部提交 | `--disable-culling --disable-gpu-culling` | CPU 视锥及包围盒 |
| 正反面诊断 | 新增 `--qa-disable-face-culling` | 绕序、变换奇偶性与材质双面 |
| 深度诊断 | 新增 `--qa-disable-depth-test` | 遮挡、近远裁剪与深度状态 |
| 固定对象与动画对象 | 相同相机、固定时间、不同姿态 | 变形包围盒与轮廓扩张 |

两个新增开关用于诊断捕获。正式通过证据使用默认渲染状态。每种问题在修复前由独立测试失败复现。记录反例后再选择对应修复。

公共主角与立方体的基础绕序已通过 CPU 检查。下一步检查导入后的变换、投影 Y 方向及运行期状态。图像验收覆盖近景、远景、墙内、屋顶、门和坡道。

## R6 文件与拟定契约

| 操作 | 路径 | 职责 |
| --- | --- | --- |
| 修改 | `src/rhi/Rhi.hpp`、`src/rhi/VulkanRhi.cpp` | 显式正面绕序与管线状态 |
| 修改 | `src/scenes/CharacterSceneRenderer.cpp` | 变换奇偶性、可见性、主投影与关联通道 |
| 修改 | `src/assets/GltfLoader.cpp` | 导入变换、绕序、法线与切线一致性 |
| 修改 | `src/scene/Frustum.hpp`、`src/render/DeformedBounds.hpp` | 边界保守性与 CPU/GPU 一致性 |
| 修改 | `src/render/RenderSettings.*`、`src/runtime/LevelRenderSettings.hpp` | 相机与阴影范围的保存及校验 |
| 修改 | `shaders/mesh.vert`、`shaders/shadow.vert`、`shaders/outline.vert` | 依据复现结果同步变换与裁剪契约 |
| 新增 | `tools/create_visibility_fixture.py`、`tools/test_scene_visibility_gpu.py` | 公共最小场景与图像断言 |
| 修改 | `tests/SceneGraphTests.cpp`、`tests/NullRhiPassTests.cpp`、`CMakeLists.txt` | CPU、管线与 GPU 回归登记 |

拟增加 `GraphicsPipelineDesc::frontFace`，默认顺时针。运行期镜像变换选择对应正面绕序，材质保留其剔除语义。导入时烘焙的镜像变换单独核验索引。描边、阴影与透明排序使用同一变换契约。

拟增加关卡字段 `cameraNear`、`cameraFar` 和 `shadowDistance`。默认值分别为 0.1、100 和 100。探索关卡候选值为 0.1、500 和 100。主视锥、聚簇灯光、编辑器拾取与 GPU 剔除读取一致的投影。

校验要求 0.01 ≤ near ≤ 10，near < far ≤ 5000。阴影距离满足 near < shadowDistance ≤ far。缺少字段的关卡使用默认值。无效候选保留活动场景与历史。

## R6 实施步骤

- [ ] 保存用户场景与六组定点复现，登记对象及区域掩码。
- [ ] 增加失败断言，覆盖负缩放、动画边界、近裁剪与远物体。
- [ ] 逐项验证剔除、深度及变换假设，记录实际根因。
- [ ] 实现对应修复与相机范围契约，复测默认通道。
- [ ] 运行 CPU/GPU、透明、阴影、描边、固定描述符与性能回归。
- [ ] 同步 `docs/runtime/scene-visibility.md`、渲染文档和 R6 证据。
- [ ] 审核参考图、源码、清单与暂存范围，提交 R6。

专项测试拟注册为 `AzureRender.SceneVisibilityGpu`，GPU 串行。公共夹具包含正缩放、单轴镜像、双轴镜像与非均匀缩放。另包含双面薄片、透明叠层、蒙皮角色和视锥边界对象。

每个目标记录预期可见区域、可见面和深度顺序。无遮挡且相距足够的对象必须出现。比较 CPU 与 GPU 默认输出，报告物体身份与失败区域。诊断开关造成的预期差异单独登记。

## G7 方向与动作契约

先区分模型前轴、角色世界朝向与步态相位。捕获同一动作的正面、侧面和俯视视角。记录脚尖方向、实际位移、角色 yaw 与动画时间。

公开生成器脚部沿局部 −Z 延伸。控制器使用 `atan2(vx,vz)-forwardYaw`。候选修复先验证公共角色 `forwardYaw=180` 的结果。私有主角根据实际前轴单独登记参数。

若前轴一致而步态仍反向，再检查生成器曲线与片段映射。前向动画需与位移一致，循环交界保持连续。真实 Idle、Walking 的来源与重定向证据保留。

## G7 文件与拟定接口

| 操作 | 路径 | 职责 |
| --- | --- | --- |
| 修改 | `src/runtime/InputActions.hpp` | 左右 Shift 共用 `sprint` 动作 |
| 修改 | `src/runtime/GameComponents.hpp`、`src/reflection/Registry.cpp` | Character 版本 3 与 `sprintMultiplier` 迁移 |
| 修改 | `src/runtime/GameRuntime.cpp` | 冲刺目标速度、平滑速度与朝向 |
| 修改 | `src/app/AzureRenderApp.cpp`、`src/runtime/GameInputReplay.hpp` | 实际键盘与回放、焦点、暂停边界 |
| 修改 | `tools/generate_third_person_assets.py` | 按方向证据调整公共步态 |
| 修改 | `tools/create_third_person_project.py`、`tools/create_playable_project.py` | 朝向、冲刺与操作说明 |
| 修改 | `assets_public/third_person/`、`assets_public/exploration/` | 已核验的模型、Prefab 与玩法资源 |
| 修改 | `tests/ThirdPersonTests.cpp`、`tests/PhysicsInputTests.cpp`、`tests/ReflectionTests.cpp` | 多键、速度、迁移与固定步断言 |
| 新增 | `tools/test_locomotion_direction.py` | 真实输入、方向与侧面动作验收 |

拟定接口 `InputActions::bindAdditional(const std::string& action, int key) -> void`。`bind` 使用替换绑定语义。左右 Shift 使用 GLFW 键值 340、344。任一键按住时冲刺有效，两键均释放后结束。

`Character::sprintMultiplier` 默认 2.5，允许范围为 1 至 4。版本 1、2 迁移时补该默认值。正常目标速度为 `speed`，冲刺为 `speed*sprintMultiplier`。公开项目步行保持 2，冲刺为 5 米每秒。

加减速沿用固定步与现有可配置参数。松开 Shift 时平滑回到步行目标。动画速率由真实水平速度与参考速度计算。当前任务使用 Idle、Walking，冲刺通过 Walking 节奏适配验收。

## G7 实施步骤

- [ ] 保存 W 前进的侧面失败证据，断言角色前轴与位移方向。
- [ ] 增加左右 Shift、组合键、释放、失焦、暂停与重开失败测试。
- [ ] 验证朝向假设，修复对应参数或已确认的步态曲线。
- [ ] 实现冲刺、Character 迁移和编辑器字段，更新公开项目。
- [ ] 对照实际键盘与输入回放，完成四方向、相机旋转及完整任务。
- [ ] 同步第三人称、输入、教程操作表和 G7 证据，提交 G7。

匀速阶段模型前轴与实际速度方向点积至少为 0.95。转向阶段按已有角速度收敛。步行与冲刺匀速误差至多为 2%，斜向速度等于目标速度。相同总时长的 30、60、144 Hz 输入结果保持固定步一致。

仅按 Shift 时角色保持静止。碰撞、跳跃、坡道和相机遮挡仍须通过。失焦释放全部按键，重开恢复初始状态。公开与私有主角的侧面录像均需人工核对动作方向。

## 验收命令与提交

以下两个专项工具与对应测试属于待实施产物。

```powershell
python tools/test_scene_visibility_gpu.py --executable build/ninja-msvc-release/AzureRender.exe --output build/r6/visibility
python tools/test_locomotion_direction.py --player build/ninja-msvc-release/AzurePlayer.exe --output build/g7/locomotion
ctest --test-dir build/ninja-msvc-debug --output-on-failure
```

R6 提交标题为 `feat(r6): 完成场景可见性与裁剪验收`。G7 提交标题为 `feat(g7): 完成角色方向与冲刺验收`。每阶段附带两配置回归、默认路径 Validation 和性能证据。
