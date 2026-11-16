# 静态网格目录

> 状态为 P2 局部验收。静态网格已通过 Release Player 读取。

工具根据锁定的场景与目录候选索引网格。
每个 Unity GUID 对应一个稳定资源槽。
目录记录来源摘要、转换类别、输出路径和内容摘要。
源文件在转换后再次核对摘要。

本轮有 241 个网格来源。
其中 207 项转换为静态 GLB。
另有 33 项需要绑定关系，一项需要独立形变转换。
后两类保留在目录中，供后续场景装配。

静态输出合计 31,547,468 字节。
全部 GLB 在移动后的私有项目内通过 Player 校验。
完整材质、图集、实例覆盖和渲染评估按专项计划验收。

## 复验

在 AzureRender 根目录执行以下命令。
目标使用受管任务内的新目录。

```powershell
python -B -m tools.npr_room.mesh_catalog --source $unitySource --inventory $referenceInventory --output $meshCatalog
```

在 Player 中以项目挂载加载生成的 GLB。
清单摘要见[静态网格清单](static-meshes.json)。
