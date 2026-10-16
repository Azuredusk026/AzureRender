# 资源验证基线验收

日期：2026-10-16。阶段：EV0。
状态：清单与独立导入通过，着色外观留待 EV7。

## 已验证的内容

角色采用本机 `laevat_repaired_v2.glb`。
动作来源采用 `laevat_g5_source_animated.glb`。
两者均为 468 关节、一个蒙皮和 13 个材质。
解析统计为 81198 个顶点与 94603 个三角形。

两份资源的骨架指纹相同。
指纹覆盖节点路径、层级、局部变换和逆绑定矩阵。
逆绑定矩阵的最大数值差为零。
形态目标、第二组权重和稀疏访问器数量均为零。

清单版本为 1，输入与工具记录 SHA-256。
依赖缺失、哈希缺失和未知版本均受控拒绝。
来源类型采用原生、重定向、程序生成或人工制作。
本机派生角色的游戏原生来源由 EV1 核验。

公共机械面板包含三个独立形态目标。
其网格为工具夹具，可用于通用加载回归。
夹具与清单位于 `assets_public/resource_validation/`。

## 自动验证

清单与骨架用例共四项通过。
既有角色准入用例共两项通过。
初始缺功能失败与修复后结果分别保存。

Blender 5.1.2 使用后台工厂配置完成两次导入。
每次保存五张图与全部骨骼世界矩阵。
采样位于片段起点、四分之一、中点及终点附近。
精确位置为总时长的 0、0.25、0.5、0.75、1 倍。

图像采用 640×640 正交镜头和工作台渲染。
其职责为几何、姿态及材质颜色基线。
游戏着色器和最终外观由 EV7 单独验证。
修复版首帧已进行图像检查。

## 环境与待项

处理器为 i9-13900HX，内存约 31.7 GiB。
显卡为 RTX 4060 Laptop，显存为 8188 MiB。
NVIDIA 驱动为 595.79。
C、D、E 盘空余约为 17.8、119.4、239.2 GiB。

修复版导入报告存在未分配骨骼的顶点。
导入和五帧输出完成，告警保留于原始日志。
EV2 需定位顶点及影响范围。
动作源导入日志未出现同一告警。

## 证据入口

[来源清单](../../../build/runs/20261016-140642-ev0-baseline-891c98/manifest.json)
记录本机文件位置、输入指纹与输出指纹。

[角色统计](../../../build/runs/20261016-140642-ev0-baseline-891c98/intake.json)
保存完整骨架与面部骨骼候选。

[修复版基线](../../../build/runs/20261016-140642-ev0-baseline-891c98/repaired/baseline.json)
和[待机基线](../../../build/runs/20261016-140642-ev0-baseline-891c98/idle/baseline.json)
保存镜头、动作、帧位置与姿态矩阵。

复现命令从引擎根目录执行。

```powershell
python -m unittest discover -s tools -p test_resource_validation_manifest.py
python -m unittest discover -s tools -p test_character_intake.py
```

离线基线脚本为 `tools/blender/capture_validation_baseline.py`。
传入 `--input`、`--output` 和可选 `--action`。
Blender 启动参数使用 `--background --factory-startup`。
