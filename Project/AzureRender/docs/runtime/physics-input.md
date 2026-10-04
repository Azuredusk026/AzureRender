# 物理、输入与固定步长

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-10-04
> 适用范围：Windows Player 运行时
> 源码入口：`src/runtime/PhysicsWorld.*`、`InputActions.hpp`、`GameRuntime.*`
> 关联测试：`PhysicsInputTests.cpp`、`test_physics_player.py`

## 职责与使用场景

PhysicsWorld 使用 Jolt 5.6.0 管理盒状刚体、胶囊角色、射线和触发器。InputActions 提供动作绑定、按住与按下沿。GameRuntime 将项目运行时按 60 Hz 推进并生成渲染所需变换。

## 数据与所有权

PhysicsWorld 持有独立 PhysicsSystem、临时分配器和任务系统，刚体与角色关联实体和节点身份。关卡版本变化重建物理状态，删除实体清除关联物理对象。关闭时先释放角色与刚体，再释放 PhysicsSystem。

## 生命周期与时序

固定步长为 1/60 秒。帧时间最多累计 0.25 秒，每帧最多推进 15 步。暂停清空累计时间，单步推进一次固定步。

每步先运行延迟操作与 World 系统，再更新角色、Jolt 刚体和触发事件，最后清空动作按下沿。动态物理变换写回 World，Player 随后生成只读场景快照。

## 接口契约

`azure.rigid-body` 声明盒体半尺寸、动态标记和触发器标记。`azure.character` 声明速度、跳跃、胶囊尺寸与控制参数。刚体与角色在单个实体上互斥，尺寸须为有限正数。

静态体读取 World 的位置、旋转和缩放。动态体写回位置与旋转。胶囊中心使用实体位置加 `centerOffset`。

`raycast()` 的第二个参数为射线位移，结果包含实体与比例。`sphereSweep()` 使用半径执行形状扫掠，并支持排除跟随角色与触发器。

触发器使用 Jolt 窄相位形状重叠查询，事件携带触发实体、另一实体和进入标记。离开或删除生成退出事件。

动作默认绑定 W/A/S/D、Space 与 E，`bind()` 可覆盖按键。`pressed()` 在首个固定步消费，重复按键保持一个按下沿。窗口失焦清除按住与按下状态，编辑器焦点由视口规则判断。

相机相对移动、加减速、转向、插值、台阶与跳跃窗口见[独立动画与第三人称控制](third-person-animation.md)。

## 线程与同步

当前 PhysicsSystem 使用 Jolt 的单线程任务执行器，World 修改与事件交付在主线程。Jolt 临时分配器为 16 MiB，世界容量为 4096 刚体，容量错误产生诊断。

## 序列化与兼容

刚体与变换使用版本 1 反射存档，角色使用版本 2。版本 1 角色存档通过迁移取得新增参数默认值。关卡组件在加载时校验，Jolt 内部句柄属于当前运行。

## 平台行为

Windows 通过 GLFW 采集输入，Player 项目运行接入 GameRuntime。渲染验收宿主保留其确定性相机与捕获时序。Android 为 Deferred。

## 使用示例

在关卡节点的组件对象中加入 `azure.character` 存档，设置 `speed` 与 `jumpSpeed`。地板节点加入 `azure.rigid-body`，`dynamic` 为 false，并通过 `halfExtent` 指定尺寸。

Player 启动后使用 W/A/S/D 移动，空格跳跃，P 暂停或恢复，O 在暂停时单步。`--runtime-report` 输出最终节点、固定步次数和模拟总时间。

## 诊断与排错

按模型脚底放置实体，并设置 `centerOffset` 使胶囊覆盖身体。碰撞体尺寸错误检查组件范围，物理容量错误检查关卡对象数量。

## 验收与证据

PhysicsInput 覆盖角色、查询、触发器、删除、暂停、单步、焦点与不同分帧下的固定步一致性。PhysicsPlayer 通过 Vulkan 检查实际关卡落地。

## 参考来源

[Jolt Physics 5.6.0](https://github.com/jrouwe/JoltPhysics/tree/v5.6.0) 通过 vcpkg 固定基线安装，协议为 MIT。适配使用公开的 PhysicsSystem、CharacterVirtual、过滤表和窄相位查询 API。

调用顺序参考 `HelloWorld/HelloWorld.cpp` 与 `Samples/Tests/Character/CharacterVirtualTest.cpp`，本地代码为接口封装。
