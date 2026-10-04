# 独立动画与第三人称控制

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-10-04
> 源码入口：`GltfLoader.*`、`AnimationStateMachine.*`、`GameRuntime.*`、`ThirdPersonCamera.hpp`

## 资源、姿态与录制

`LoadedAsset` 保存绑定骨架、原始顶点与动画片段。`sampleAnimationPose()` 从只读资源生成局部变换、世界矩阵和关节矩阵。每个实体独立保存状态、时间、淡化与 Morph 权重。

`NodeAnimationFrame` 通过节点身份关联实例。渲染准备阶段采样当前片段与前一片段，按局部位移、缩放和四元数混合。头部材质方向、透明排序与变形包围盒读取该实例的姿态。

原始顶点和索引由同一资源的实例共享。每个在途帧持有独立关节缓冲和蒙皮输出，实例通过关节偏移与顶点偏移读取分片。Compute 输出通过绑定 17 提供给主体、阴影和轮廓着色器。录制闭包保存已经准备好的派发参数，图调度建立存储写入到顶点读取的依赖。

各几何 Pass 使用同一投影矩阵。姿态准备、GPU 上传与包围盒计算完成后，录制阶段只读帧快照。容量不足产生诊断，关卡资源准备负责建立容量。

## 动画配置

`azure.animator` 使用版本 2。版本 1 存档通过迁移获得新增字段的默认值。

| 字段 | 作用 |
| --- | --- |
| `asset` | 动画状态图的资源引用 |
| `state`、`enabled` | 当前语义状态与启用状态 |
| `startTime` | 实体初始片段时间 |
| `locomotion` | 由角色实际速度选择 idle 或 walk |
| `referenceSpeed` | 行走片段的参考速度，单位为米每秒 |
| `crossfade` | 状态淡化时长，默认 0.18 秒 |
| `morph0`、`morph1` | 实例 Morph 权重 |

状态图版本为 1。`states` 指定片段索引与循环标记，`initial` 指定初始状态，`transitions` 指定布尔条件和可选 `blendSeconds`。时间键与片段引用在导入、图解析和采样时校验。

淡化时长按模拟时间推进，片段时钟按播放速率推进。混合期间的新状态请求排队，在当前淡化完成后衔接。暂停冻结状态与时钟，单步推进一个固定步。

主角动作通过 `retarget_animation.py` 离线处理。工具读取 ufbx 0.17.1 的 30 Hz 采样，将世界绑定基底的旋转差映射到 Bip001，保留目标骨长，并将根水平位移配置为原地动作。导出记录动作来源哈希、映射、采样率、循环接缝与结果哈希。

蒙皮输入须包含有效、归一化的源权重。`unreal_export_source_skin.py` 使用兼容源资产版本的 Unreal 导出 FBX。导出命令启用渲染支持。`fbx_skin.c` 读取源顶点和骨骼影响。

`restore_skin_weights.py` 按位置、UV 与骨骼名称恢复权重。缺失或歧义匹配产生错误。恢复产物保存源数据与结果哈希，原素材保留在私有目录。

## 角色与相机

`azure.character` 使用版本 2，配置速度、加速度、制动、转向、胶囊尺寸、中心偏移、坡度与台阶高度。`controlled` 选择玩家输入驱动，`forwardYaw` 声明模型正向。胶囊中心为实体位置加 `centerOffset`，角色可按模型脚底坐标放置。

输入按相机水平朝向转换，斜向运动归一化。实际物理速度驱动状态与播放速率。固定步长为 1/60 秒，渲染位置和角度在相邻模拟姿态间插值。暂停显示当前模拟姿态。

跳跃使用 100 ms 落地宽限与 100 ms 输入缓冲，一次按下沿消费一次跳跃。角色上升时使用空中规则，坡道与台阶由 Jolt CharacterVirtual 处理。

`azure.third-person-camera` 配置跟随目标、距离范围、肩偏移、目标高度、俯仰、灵敏度、碰撞半径与响应速度。组件存档校验距离与俯仰的交叉范围。相机球体扫掠排除角色自身和触发器。

遮挡立即收缩距离，释放后平滑恢复。目标删除时释放跟随状态，关卡提交时重建目标与插值。

## 操作与验收入口

WASD 移动，Space 跳跃，鼠标旋转相机，滚轮调整距离。Esc 释放游戏视口和鼠标，点击视口获取焦点。界面控件、文本输入与编辑器面板按焦点接收输入。失焦清理按键和鼠标增量。

公开项目位于 `assets_public/third_person/game/`，角色由项目工具生成，许可为 CC0-1.0。两种角色均包含待机与行走片段。私有项目通过参数化工具创建，并保存在本机忽略目录。

```powershell
python tools/create_third_person_project.py --output build/local-character --character <角色 GLB>
python tools/test_independent_animation_gpu.py --executable build/ninja-msvc-release/AzurePlayer.exe
python tools/test_third_person_player.py --executable build/ninja-msvc-release/AzurePlayer.exe
ctest --test-dir build/ninja-msvc-debug -R "ThirdPerson|AnimationPose|AnimationRetarget|SourceSkinRestore" --output-on-failure
```

`--game-actions` 读取按帧排序的按键、焦点与相机输入，交给生产控制器执行。`--runtime-report` 保存角色位置、动画、相机、固定步和回放结果。GPU 验收比较多实例、不同资源、时钟偏移、Morph、轮廓以及 CPU 和 Compute 输出，并检查 Validation 与退出释放。
