# 第三人称探索关卡

> 文档类型：使用指南与接口参考
> 适用范围：公开项目与本机动画主角项目

## 启动与操作

公开项目位于 `assets_public/exploration/`。关卡使用四个动画角色和自制实体资源。玩家向起点的引导角色领取任务，收集三个物件。随后开启门，进入室内目标区完成探索。

在工程根目录启动 Player：

```powershell
build/ninja-msvc-release/AzurePlayer.exe --project assets_public/exploration/project.azureproject
```

| 操作 | 行为 |
| --- | --- |
| WASD | 按相机方向移动 |
| 左右 Shift | 按住冲刺，松开回到步行速度 |
| 鼠标、滚轮 | 旋转相机、调整距离 |
| Space | 跳跃 |
| E | 触发当前提示目标 |
| R、Restart 按钮 | 重新加载当前探索关卡 |
| Esc | 释放鼠标，操作界面 |
| 点击游戏区域 | 恢复相机控制 |
| Pause / Resume 按钮 | 暂停或恢复固定模拟 |

路线经过开放区域、狭窄通道、检查点、坡道和室内。界面显示目标、收集数量、路径提示与交互目标。参考路线在第 221 秒完成，可连续重新开始。

## 创建本机项目

创建工具接收经过动画准入的 glTF 或 GLB。指定模型须包含 idle 和 walk 两个片段。工具复制模型、公共配角、规则和许可文本。

```powershell
python tools/create_playable_project.py --output build/my-exploration
python tools/create_playable_project.py --output build/local-exploration --character "$env:AZURE_CHARACTER_ASSET"
```

主角路径通过本机环境变量提供。私有主角与派生图像保存在忽略目录。公开项目采用 CC0 素材，项目内附 `CHARACTER-LICENSE.txt`。

## 组件与实体

组件使用反射存档，当前版本均为 1。Prefab 位于项目的 `assets/prefabs/`。主角、引导角色、收集物、门与检查点具有独立模板。

| 类型 | 字段与规则 |
| --- | --- |
| `azure.interactable` | `range` 为 0.1 至 20 米，`offset` 为目标偏移。`prompt` 提示，`enabled` 控制可用性 |
| `azure.collectible` | `category` 标识类别，`collected` 表示已收集 |
| `azure.door` | `requiredCount` 为 1 至 99，`open` 表示已开启 |
| `azure.checkpoint` | `activated` 表示已触发，`position` 保存检查点位置 |
| `azure.task-state` | `started`、`collected`、`doorOpened`、`completed` 保存任务状态 |

选择模块检查有效节点、可用状态、距离和视线。等距目标按稳定节点身份排序。触发器参与任务事件，视线查询忽略触发器与玩家自身。

输入按下沿触发一次交互。收集物计数后在下一帧边界删除。门满足数量要求后移除碰撞并隐藏。目标区检查数量和开门状态，检查点记录触发状态。

## Lua 实体接口

`self` 句柄保存实体编号、节点身份和关卡版本。编号复用或切关后，句柄访问产生失效诊断。脚本初始化采用组件与结构副作用事务。

| 接口 | 行为 |
| --- | --- |
| `self:find(id)` | 返回受控实体句柄。缺失节点返回 nil |
| `self:has(type)` | 查询反射组件 |
| `self:interaction_target()` | 返回当前目标节点。无目标返回空字符串 |
| `self:get(type, field)`、`set(...)` | 读写反射字段，执行类型和范围校验 |
| `self:remove_component(type)` | 在帧边界移除可选组件 |
| `self:spawn(id, resource, position)` | 使用现有关卡资源，在帧边界创建节点 |
| `self:destroy()` | 在帧边界销毁当前节点 |
| `interact(actor)` | 接收玩家稳定节点身份 |

结构身份依赖 Transform 与 Renderable。生成要求有效资源、父节点和唯一身份。身份长度至多 128 字符，关卡节点容量至多 4096。当前关卡内，已使用的稳定身份保持预留。

成功初始化提交跨实体字段和结构操作。失败初始化保持有效对象与现行脚本状态。回调错误停止对应脚本，错误记录提供素材路径与原因。

运行时模型实例按在途帧维护关节、蒙皮与实例缓冲。容量增长发生在对应帧栅栏完成后。扩容更新当前帧描述符，其他在途帧保持自身缓冲。

## 验证入口

`PlayableQuest` 验证真实任务脚本、重复事件和重开事务。`Interaction` 与 `ScriptEntities` 覆盖选择和句柄失效。`RuntimeSpawnGpu` 验证真实 Vulkan 生成与退出释放。

```powershell
python tools/test_playable_player.py --executable build/ninja-msvc-release/AzurePlayer.exe --output build/quest-qa --capture
python tools/test_runtime_spawn_gpu.py --executable build/ninja-msvc-release/AzurePlayer.exe --output build/spawn-qa
```

路线报告保存逐帧任务、对象、交互、相机遮挡和关卡版本。工具使用生产输入，检查完成时间、两次重开、界面绘制和退出释放。私有主角验收通过同一工具的 `--character` 参数运行。
