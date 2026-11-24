# 场景数据运行时说明

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-10-23
> 适用范围：R0 场景数据收口与 R2 光源变换集成
> 源码入口：`src/scene/SceneDescription.hpp`、`src/scene/TransformSystem.hpp`、`src/scenes/CharacterSceneRenderer.cpp`
> 关联测试：`tests/SceneGraphTests.cpp`

## 职责与使用场景

场景文档保存资源引用、稳定标识、父关系和局部变换。
运行期渲染器读取 `SceneDescription`。
节点解析为世界变换，再生成当前帧实例和包围体。
文档对象负责序列化，渲染器消费场景快照。

## 数据与所有权

`SceneNodeDesc::id` 是场景文档中的稳定标识。
`parentId` 为空表示根节点。
渲染器持有场景快照，以资源 ID 选择共享 GPU 资源。
节点描述负责内容身份和变换。

`SceneInstance` 属于渲染器当前帧。
它包含模型矩阵、世界包围体、来源顺序和资源键。
实例列表在下一帧重建，录制期间只读。

`SceneLightDesc` 通过 `nodeId` 关联场景节点。
创建渲染上下文时，应用解析节点层级。
光色、强度、半径、启用状态和世界位置进入渲染快照。

## 生命周期与时序

1. 应用从 `SceneDocument` 复制资源和节点描述到 `RenderContext::scene`。
2. 渲染器加载资源并保存场景快照。
3. 每帧更新动画、相机、方向光矩阵与点光源聚簇。
4. 按节点 ID 解析父子变换，计算世界包围体。
5. 分别按主相机和阴影相机筛选可见实例。
6. 主场景使用主相机实例顺序，阴影通道使用连续实例区段。

根节点直接使用局部变换。
循环节点保留局部变换，诊断层报告循环计数。

## 接口契约

`resolveNodeWorldTransforms(scene, cycleCount)` 返回世界矩阵数组。
矩阵索引与 `scene.nodes` 索引一致，数组长度相同。
`cycleCount` 可为空，非空时接收循环节点计数。

建立父链接要求节点具有稳定 ID。
父 ID 缺失时，节点按根节点处理。
资源 ID 缺失时，报告资源加载错误。

## 线程与同步

场景快照和实例列表在渲染线程更新。
命令录制期间使用稳定的实例列表。
GPU 实例缓冲按在途帧分配。
CPU 写入前等待所属帧的提交完成。

## 序列化与兼容

`.azscene` 当前格式版本为十二。
光源保存 ID、节点关联、颜色、强度、半径和启用状态。
每个节点保存稳定源槽位到材质 UUID 的覆盖。
读取兼容版本一至十一。
角色局部灯字段见[颜色响应](character-local-light-response.md)。

版本 1 与 2 使用空光源列表。
版本 1 至 3 的材质覆盖默认为空。
从资产创建的根节点使用稳定的 `root` ID。
保存和读取保持节点 ID、父链接与光源关联。

## 平台行为

Windows 和 Android 共用节点解析和剔除逻辑。
平台差异由渲染能力和资源预算处理。
场景变换具有统一规则。

## 验收与证据

`SceneGraphTests` 与 `SceneModelTests` 验证变换及序列化。
范围包括父子关系、循环、光源和场景解析。
GPU 验收要求子节点和附属光源随父节点移动。
保存和重开须保持完整结果。

## 参考来源

[Piccolo RenderScene](https://github.com/BoomingTech/Piccolo/blob/main/engine/source/runtime/function/render/render_scene.h) 用独立可见集合组织渲染数据。Azure Engine 采用相同职责分离，并保留自己的场景格式、测试和资源所有权规则。
