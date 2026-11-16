# 窗景参照采集

> 状态为 P0 局部验收。窗景身份和现有参照图已锁定。

## Recovery 窗景

`WindowViewType` 定义编号 0 至 28 的 29 种窗景。
Recovery 的 `EveryWindowViewActivatesAndRenders` 隔离用例通过。
29 张 PNG 均为 2078 × 1169，每张的摘要唯一，按枚举编号和名称对账。
该记录覆盖窗景激活和现有画面参照。

## 原版游戏采集

Steam 启动的原版游戏记录了 13 张画面。
采集配置为 1280 × 720、高画质和最高渲染比例。
现有文件保存为 JPEG，十二张的实际尺寸为 856 × 511，一张为 1707 × 912。
文件身份采用实际编码、尺寸和内容摘要。
固定条件的像素对照使用同步相机、时相和粒子条件的成对帧。

## 复验

在 AzureRender 根目录运行命令，输出写入受管任务中的新文件。

```powershell
python -B -m tools.npr_room.window_references --recovery $recovery --enum-source $windowViewTypeSource --output $windowReferences
```

清单位于本机私有目录的 `source-lock/window-references.json`，SHA-256 为 `454e116ec6bc9ff59eb75ce2c3aff49521bb41db5a702af4a635607a9e22347d`。
