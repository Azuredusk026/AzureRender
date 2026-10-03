# 脚本与玩法

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-10-04
> 适用范围：Windows Player 项目运行
> 源码入口：`src/runtime/ScriptRuntime.*`、`src/runtime/GameRuntime.*`
> 关联测试：`AzureEngine.ScriptRuntime`、`AzureEngine.ScriptPlayer`

## 职责与使用场景

ScriptRuntime 将 Lua 5.4.8 与 sol2 3.5.0 接入固定步运行时。脚本控制角色、访问反射属性、接收触发事件并请求关卡切换。它依赖 AssetDatabase、RuntimeLifecycle 和 GameRuntime。

## 数据与所有权

一个运行时持有一个 Lua 状态，每个脚本实体持有独立环境。基础函数和数学、字符串、表库进入环境，库表按实体复制。脚本通过 `self` 访问所属实体。

句柄包含实体、节点标识和场景版本，每次访问检查有效性。关卡替换或实体删除使句柄失效。延迟删除由 RuntimeLifecycle 持有，其执行独立于脚本对象的寿命。

宿主先销毁 ScriptRuntime，再销毁 GameRuntime，最后停止 RuntimeLifecycle。有效实体上的活动脚本在关闭时执行 `shutdown`。已失效实体释放环境，属性访问产生脚本错误。

## 生命周期与时序

固定步依次执行延迟操作、脚本更新、角色运动、物理和触发事件。Lua 的运动指令覆盖所属角色在该步的默认输入。暂停期间执行零个固定步，单步执行一次完整流程。

脚本提供可选的 `init()`、`update(dt)`、`trigger(other, entered)` 和 `shutdown()`。`dt` 单位为秒，当前为 1/60。`other` 是另一实体的节点标识，`entered` 表示进入或离开。

每 500 ms 检查脚本源文本。候选先编译、校验回调并执行暂存状态下的 `init`。候选初始化成功后执行当前脚本的 `shutdown`，提交属性和延迟副作用。

候选失败保留当前代码、属性与关卡请求。相同失败源文本仅尝试一次，再次修改后重试。Lua 源变化保留关卡 World，关卡数据、Prefab 和渲染资产变化通过关卡事务提交。

## 接口契约

所有调用使用 Lua 的冒号语法。读写目标为当前实体，组件必须存在，字段名称来自反射注册表。玩法修改在运行时回调中执行。

| 接口 | 输入与结果 |
| --- | --- |
| `self:alive()` | 返回句柄是否有效 |
| `self:action(name)` | 返回动作的持续按下状态 |
| `self:pressed(name)` | 返回当前固定步的按下沿 |
| `self:move(x, z, jump)` | 设置水平运动与跳跃请求；数值须有限 |
| `self:get(type, field)` | 返回反射字段值 |
| `self:set(type, field, value)` | 按反射范围与类型事务校验字段 |
| `self:destroy()` | 下一固定步删除当前实体 |
| `self:load_level(reference)` | 将资源引用交给宿主的关卡请求处理器 |
| `self:audio_play()` | 播放当前实体的音频源 |
| `self:ui_text(id, text)` | 设置游戏界面的纯文本 |

关卡请求处理器负责排队，主循环在固定步之外准备和提交关卡。修改接口在顶层代码中调用产生错误。数组需稠密，最多 1024 项，嵌套深度最多 16。

## 线程与同步

Lua 状态、回调、重载和关卡请求均在主线程执行。脚本使用运行时组件，渲染器消费场景快照。GPU 准备失败由关卡事务保留当前 World 与渲染器。

## 序列化与兼容

`azure.script` 组件版本为 1，字段为 `asset` 和 `enabled`。脚本资产支持 UUID 与虚拟路径引用。Lua 环境中的局部变量属于当前运行，热重载通过 `init` 建立候选状态。

## 平台行为

Windows Player 在项目运行中启用脚本，W/A/S/D 和空格由动作系统提供。P 控制暂停，O 控制单步。Android 为 Deferred。

## 使用示例

将公开示例复制到可写目录并启动 Player。示例包含角色、地板与传送门，角色进入传送门后加载第二关卡。

```powershell
Copy-Item 'D:/AzureEngine/share/AzureRender/assets_public/gameplay' './MyGame' -Recurse
& 'D:/AzureEngine/bin/AzurePlayer.exe' --project './MyGame/project.azureproject'
```

角色脚本位于 `assets/player.lua`，传送门脚本位于 `assets/portal.lua`。角色速度由初始化回调写入反射字段。测试将 `autoplay` 设为 true，验证持续移动和关卡切换。

```lua
function update(dt)
    local x = self:action("move-right") and 1 or 0
    self:move(x, 0, self:pressed("jump"))
end
```

## 诊断与排错

语法和运行错误记录资产路径与 Lua 堆栈，运行错误禁用对应脚本。诊断最多保存 128 项。`--runtime-report` 输出 `activeScripts`、`scriptErrors` 和模拟累计耗时。

每次顶层执行和回调设有十万条 Lua 指令预算，超限产生隔离错误。源文件上限为 1 MiB。脚本面向本地可信项目，内存和 C 函数耗时由宿主环境约束。

## 验收与证据

ScriptRuntime 覆盖错误、死循环、反射、初始化事务、重载和句柄失效。ScriptPlayer 使用真实 Vulkan 验证角色移动、触发器、切关和错误隔离。Release 门禁使用移动后的安装树运行同一公开示例。

执行 `ctest --test-dir build/ninja-msvc-debug -R AzureEngine.Script --output-on-failure`。完整结果与性能记录见 [G4 验收](../acceptance/g4/2026-10-04.md)。

## 参考来源

[Lua 5.4.8](https://www.lua.org/versions.html) 和 [sol2 3.5.0](https://github.com/ThePhD/sol2/tree/v3.5.0) 使用 MIT 许可证。依赖由 vcpkg 固定基线与 Lua 版本覆盖安装，许可证进入发布清单。

适配使用 sol2 的 `state`、`environment`、`protected_function` 和 Lua 指令钩子。参考安装头 `sol/environment.hpp` 与 `sol/protected_function.hpp` 的公开接口。绑定代码由项目实现。

MSVC 对 sol2 表情符号字面量产生 C5321。包含 sol2 头时局部关闭此警告，项目代码继续使用严格警告。对应验证为 Debug、Release 构建和真实 Player 测试。
