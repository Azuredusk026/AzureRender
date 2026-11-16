# 静态网格目录

> 状态为 P2 局部验收。静态网格已通过 Release Player 读取。

工具根据锁定的场景与目录候选索引网格。
每个 Unity GUID 对应一个稳定资源槽。
目录记录来源摘要、转换类别、输出路径和内容摘要。
源文件在转换后再次核对摘要。

本轮有 241 个网格来源。
其中 207 项转换为静态 GLB。
另有 33 项带绑定关系，一项带独立形变。
对应变体见[角色与窗景资源转换](character-assets.md)。

静态输出合计 31,547,468 字节。
全部 GLB 在移动后的私有项目内通过 Player 校验。
完整材质、图集、实例覆盖和渲染评估按专项计划验收。

## Windows 静态场景

房间场景启用 Windows 原版子树。
静态转换按各渲染器的索引范围读取九个合批网格，生成 142 个独立渲染实例。
三个引用共用一个 Unity 内置 Quad，该网格取自 Recovery 对应的内置资源容器。 [UnityPy 1.25.0](https://pypi.org/project/UnityPy/1.25.0/)

房间交换文件含 274 个节点，其中 201 个节点含静态几何，共引用 194 项资源。
生产编辑器异步导入任务完成后保存文档，重开并核对节点及资源数量，再使用 Release Player 校验。
其余四个参考场景在此入口的直接静态网格计数为零，预加载、界面和运行时装配按各场景的行为路线验收。
原材质及动态场景状态进入各自的验收项。

## 复验

在 AzureRender 根目录执行以下命令。
目标使用受管任务内的新目录。

```powershell
python -B -m tools.npr_room.mesh_catalog --source $unitySource --inventory $referenceInventory --output $meshCatalog
python -B -m tools.npr_room.static_batch_catalog --source $unitySource --scene $roomScene --catalog $meshManifest --output $batchOutput
python -B -m tools.npr_room.scene_catalog --scene $roomScene --catalog $meshManifest --batches $batchManifest --builtins $builtinManifest --activate 1439 --deactivate 5531 --output $roomExchange
```

在 Player 中以项目挂载加载生成的 GLB。
清单摘要见[静态网格清单](static-meshes.json)。
[场景校验记录](scene-assets.json)登记实际导入数量和产物摘要。
