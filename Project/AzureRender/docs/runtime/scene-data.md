# 场景数据运行时说明

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-09-26
> 适用范围：R0 场景数据收口与 R2 光源变换集成
> 源码入口：`src/scene/SceneDescription.hpp`、`src/scene/TransformSystem.hpp`、`src/scenes/CharacterSceneRenderer.cpp`
> 关联测试：`tests/SceneGraphTests.cpp`

## 职责与使用场景

场景文档保存资源引用、节点稳定标识、父节点关系和局部变换。运行期渲染器读取 `SceneDescription`，将节点解析为世界变换，再生成当前帧的实例和包围体。文档对象负责序列化，渲染器只消费场景快照。

## 数据与所有权

`SceneNodeDesc::id` 是节点在场景文档中的稳定标识，`parentId` 为空表示根节点。节点描述不持有 GPU 资源。角色渲染器在加载期间持有场景快照，并以资源 ID 选择共享的 GPU 资源。

`SceneInstance` 属于渲染器当前帧，包含模型矩阵、世界包围体、来源顺序和资源键。实例列表在下一帧重建，命令录制只读取已经完成的列表。

`SceneLightDesc` 通过 `nodeId` 关联场景节点。应用在创建渲染上下文时解析节点层级，将光源颜色、强度、半径、启用状态和世界位置复制到渲染场景快照。

## 生命周期与时序

1. 应用从 `SceneDocument` 复制资源和节点描述到 `RenderContext::scene`。
2. 渲染器加载资源并保存场景快照。
3. 每帧更新动画、相机、方向光矩阵与点光源聚簇。
4. 按节点 ID 解析父子变换，计算世界包围体。
5. 分别按主相机和阴影相机筛选可见实例。
6. 主场景使用主相机实例顺序，阴影 pass 使用同一实例缓冲中的连续区段。

没有父节点的节点直接使用局部变换。发现循环时保留循环节点的局部变换，并由测试或诊断层报告循环计数；渲染流程不会递归崩溃。

## 接口契约

`resolveNodeWorldTransforms(scene, cycleCount)` 返回与 `scene.nodes` 等长的世界矩阵数组。矩阵索引与节点索引一致。`cycleCount` 可为空；非空时返回检测到的循环节点计数。

节点 ID 为空时不能建立父链接。父 ID 找不到时按根节点处理。资源 ID 找不到时使用现有资源加载错误路径，不创建隐式资源。

## 线程与同步

场景快照和实例列表在渲染线程更新。命令录制期间不能修改实例列表。GPU 实例缓冲按 in-flight frame 分配，CPU 在对应帧可写入前等待已有提交完成。

## 序列化与兼容

`.azscene` Schema v3 保存光源 ID、节点关联、颜色、强度、半径和启用状态。v1 与 v2 文档读取时使用空光源列表；从资产创建的根节点使用稳定的 `root` ID。保存和读取保持节点 ID、父链接与光源关联不变。

## 平台行为

Windows 和 Android 使用相同的节点解析和剔除逻辑。平台差异由渲染能力和资源预算处理，不改变场景变换结果。

## 验收与证据

运行 `SceneGraphTests` 与 `SceneModelTests` 验证局部变换、父子变换、循环、光源序列化和场景解析。GPU 验收要求父节点移动后子节点与附属光源随动，保存重开后结果一致。

## 参考来源

[Piccolo RenderScene](https://github.com/BoomingTech/Piccolo/blob/main/engine/source/runtime/function/render/render_scene.h) 用独立可见集合组织渲染数据。Azure Engine 采用相同职责分离，并保留自己的场景格式、测试和资源所有权规则。
