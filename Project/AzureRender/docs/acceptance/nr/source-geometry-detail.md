# 原生场景几何细节选择

日期为 2026-10-21，阶段索引为 NR1。
中央区与上层区的最高细节静态转换已验证。
原生镜头的实时细节切换与最终外观继续验收。

## 选择契约

入口为 `endfield_native/build_scene_preview.py`。
`--lod-policy metric` 按指定源阈值选择绘制项。
`--lod-metric-squared` 声明非负平方标量，默认值为零。
该标量与原生相机、包围盒的关系仍待消费者核验。

`--lod-policy highest` 选择最高的原作区间。
同一区间的全部子网格与共同部件一起保留。
相互重叠的独立区间触发拒绝，需先拆分绘制组。
选择过程排除源可见性契约中的仅阴影绘制项。

公共选择器为 `endfield_native/scene_lod.py`。
它核验有限区间、子网格编号及标志位。
源区间、绘制编号与对象身份进入转换收据。
原生输入保持只读，输出使用独立目录。

## 网格身份

网格由精确的包内路径及对象指针绑定。
指针身份包含序列化文件和对象编号。
`endfield_native/mesh_binding.py` 核验唯一导出对象。
同名对象、缺失载荷及非网格指针均触发拒绝。

导入名称与包内名称的别名分别记录。
中央区三处实例共享同一个名称别名。
对应六个绘制项均保留请求名称及实际对象身份。
名称别名以精确容器绑定为依据。

源可见性位定义取自固定版本读取器。
文件为 `endfield_scene_slot_contract.py`。
其中 `highest_detail_visible_meshes` 定义可见网格范围。
本地读取器及对象导出来源沿用原生资产获取清单。

## 本机结果

中央区 86 个实例导出 142 个绘制项。
其中共享绘制资源 60 份，材质 45 个。
上层区 53 个实例导出 87 个绘制项。
其中共享绘制资源 48 份，材质 40 个。

两组源矩阵回读最大误差均为零。
中央区包围盒溢出小于两微米。
上层区包围盒溢出小于五微米。
材质采用读取器近似节点，完整外观仍待验收。

两区最高细节资源已完成正式编辑器联合导入。
保存结果包含 418 个节点、110 份资源。
原生待机、完整走跑完成 180 帧联合短测。
范围及剩余门禁见[联合接入](highest-detail-integration.md)。

上层依赖闭包包含 114 个包，均完成分层导出。
读取器导出 93 个网格、40 个材质和 89 张纹理。
缺失高细节资源的首次转换记录单独保留。
完整依赖导出后的转换正常退出。

136 项原生导入 Python 测试通过。
其中十三项覆盖区间选择与精确网格绑定。
Release 的 `AzureTools.EndfieldNative` 检查通过。
该检查已纳入 CTest，阶段状态继续按正式条件维护。

最高细节护栏完成 Debug 引擎十二帧捕获。
Vulkan 校验层开启，未报告设备校验错误。
脚本与表现错误为零，卸载后资源计数归零。
该小场景验证几何接入，原生材质仍按分项验收。

## 几何调查范围

完整模型调查遍历活动场景的节点层级及索引图元。
它同时应用模型节点矩阵和关卡实体矩阵。
既有联合候选共检查 62 份模型、245 个图元。
右侧护栏区域内仍可见历史交互设施的几何。

该候选引用的历史设施不在当前源实例清单中。
源快照一致性及护栏的动态来源继续追踪。
静态绑定姿态调查与实际运行时可见性分别验收。
手工护栏对应点拟合未通过相机标定门禁。

## 本机证据

证据根为 `build/runs/20261021-125515-native-scene-lighting-5e36f1/`。

| 文件 | 内容 |
| --- | --- |
| `source-lod-selection-audit.json` | 两组源区间选择 |
| `high-detail-central-v2/audit.json` | 中央区转换收据 |
| `high-detail-upper-v2/audit.json` | 上层区转换收据 |
| `high-detail-upper-vfs-receipt.json` | 高细节依赖分层来源 |
| `scene-lod-binding-green.log` | 136 项测试结果 |
| `scene-lod-ctest.log` | Release 的 CTest 结果 |
| `high-detail-source-verification.json` | 两组产物摘要及源变换核验 |
| `high-detail-rail-player/verification.json` | 护栏引擎播放与设备校验 |
| `game-reference/all-model-geometry-inventory.json` | 完整模型静态调查 |
| `game-reference/manual-left-panel-dlt.json` | 未通过的相机拟合 |
