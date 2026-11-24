# 阴天房间光照

> 状态为 P5 局部验收。房间的原版阴天灯具通过原生渲染与整合回归。

来源报告登记 `RoomScene` 的 56 个灯具及所属变换。阴天候选接入启用且照射默认渲染层的三盏灯。灯具位置由完整父级变换计算，方向由旋转链计算。灯具色值、强度、范围和锥角均取源序列化字段。

| 源组件 | 灯型 | 强度 | 范围 |
| --- | --- | ---: | ---: |
| 9420 | 聚光灯 | 0.8 | 2.21 |
| 9423 | 方向光 | 0.3 | 19.93 |
| 9467 | 点光源 | 0.25 | 0.8 |

局部灯具使用[引擎平滑反平方衰减](../../runtime/local-light-attenuation.md)。来源报告保留阴影和烘焙模式。原版衰减、原阴影、天空环境量、窗景配准与色彩阈值按完整画面对照验收。

## 回放

任务 1727 固定同一房间、相机和时步，采集各两张 1280×720 图。加入三盏灯具改变 605,441 个像素，整幅平均线性亮度为 0.163618，基准为 0.137248。两侧保持城市窗景与相同室内资产。退出时图像和缓冲区归零。

任务 1728 通过 850 帧启动复验，消费 23 次输入并触发五次计时提示音与两次音乐启动。七个角色部件保持同步动作。

| 证据 | SHA-256 |
| --- | --- |
| 灯具来源报告 | `f9ac1c9d06dbb6b2ef792aa67632b7fb604a4b8d074a2d4276db0e4141889512` |
| 成对渲染报告 | `5c300659d70ac7855badbf8f8844a633b6dd93e04bf255356e6b828bf4ce5868` |
| 房间回归报告 | `c4c506e3ce4de53b21f9daf587b9fa99c7c36b9f07ddd70adf4bf9643e9202da` |
| 灯具画面 | `366eae61200eba599caf66d11012b8f7c131ec2f584ddfb48c543588c177aebb` |

本机工程位于 `assets_private/npr-room-dev/room-cloudy-lights`，画面位于 `reference-preview/cloudy-lights.png`。

## 复验

```powershell
python -B -m tools.npr_room.room_lights --room $room --output $cloudyRoom --source $unitySource
python -B tools/npr_room/test_cloudy_room_player.py --player $player --base $room --cloudy $cloudyRoom --resource-root $resources --output $pair
python -B -m unittest tools.npr_room.test_room_lights
```
