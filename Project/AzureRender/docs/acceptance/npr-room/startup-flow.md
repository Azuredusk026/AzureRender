# 房间启动与转场

> 状态为 P2 局部验收。独立 Player 验证五关卡顺序、预加载、取消、重进及模型修复后重试。

## 启动顺序

本地项目依次提交 `Boot`、`AssetLoad`、`Entry` 和 `Transition`。
`Transition` 请求后台准备 `RoomScene`。Player 在转场期间保留当前关卡。
转场至少运行 2 秒，目标就绪后再等待 0.5 秒并提交房间。
Backspace 取消待处理请求，Enter 在取消或失败后重新准备房间。
原版启动画面、叠加场景、主数据、音频服务和账户入口按各自的功能验收项实现。

## 独立 Player 验收

| 项目 | 实测结果 |
| --- | --- |
| 启动与重进 | 顺序进入五个关卡，随后经转场重进房间一次 |
| 取消与重试 | 首次转场持续 393 帧，重试后进入房间 |
| 输入与音频 | 850 帧消费 23 个事件，播放五次原音效 |
| 加载故障 | 破坏私有副本的 GLB，保留转场和加载诊断 |
| 修复与恢复 | 恢复模型字节后按 Enter，同一进程提交房间 |
| 正常退出 | 缓冲与图像计数均为零 |

固定步回放与故障恢复均无脚本或表现错误。
原生脚本单测覆盖关卡预加载、取消、错误读取和修复后提交。
本机开发工程位于 `assets_private/npr-room-dev/room-startup-probe`，其 `validation` 目录保留 Release Player、构建摘要及回放证据。
该 Player 通过工作树的相关源码和本机 Release 库链接。
项目移入私有目录后再次通过项目校验。
完整构建和发布验证按对应门禁执行。

| 证据 | 标识 |
| --- | --- |
| 生产回放与故障复测 | 共享队列 1624、1628 |
| 回放运行报告 SHA-256 | `544635c33c4d4b933cb9f7694b2a52dc0f08cbaa50293022baa20f8f7bc7eccc` |
| 修复前状态 SHA-256 | `6fda0b8c71d1a022973a293913c09ecd4fedd4adc5f3a0dcea7e92d9b32f45cb` |
| 修复后状态 SHA-256 | `8dc731e4415533794d8b68a92348185d9df17c3115ea408c1e6f7372525ca10c` |

## 复验

在 AzureRender 根目录执行命令。输出使用本机受管路径。

```powershell
python -B tools/npr_room/build_room_probe.py --source $unitySource --furniture $importedFurnitureProject --audio-playback $audioPlayback --startup-flow --output $roomStartupProbe
python -B tools/npr_room/test_pomodoro_runtime.py --lua $luaDll
python -B tools/npr_room/test_room_startup_player.py --player $player --project $roomProject --output $startupReplay --resource-root $releaseBuild
```
