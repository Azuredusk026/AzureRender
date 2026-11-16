# 房间启动与转场

> 状态为 P2 局部验收。独立 Player 验证五关卡顺序、预加载、取消、重进及模型修复后重试。

## 启动顺序

本地项目依次提交 `Boot`、`AssetLoad`、`Entry` 和 `Transition`。
`Transition` 请求后台准备 `RoomScene`。Player 在转场期间保留当前关卡。
转场至少运行 2 秒，目标就绪后再等待 0.5 秒并提交房间。
Backspace 取消待处理请求，Enter 在取消或失败后重新准备房间。
叠加场景、主数据、音频服务和账户入口按各自的功能验收项实现。

## 桌面启动画面

从原 `Boot` 的 `LoadingCanvas` 提取桌面布局和七张图像，使用 `anim_ui_loading` 的十条已绑定曲线。
循环时长为 1.5 秒，按 60 Hz 生成 91 个采样时刻。图标和电话沿原位置及旋转曲线播放，光晕和图像透明度沿原曲线变化。
前四个关卡共用一个 RmlUi 文档，保持加载动画连续。动画使用 RmlUi 的可动画属性和视口单位。 [RmlUi 动画](https://mikke89.github.io/RmlUiDoc/pages/rcss/animations_transitions_transforms.html) [RmlUi 长度单位](https://mikke89.github.io/RmlUiDoc/pages/rcss/syntax.html)

游戏界面着色器在混合前按标准 sRGB 传递函数转为线性颜色。共享队列任务 1674 采集 100 张 1280×720 画面，中心区域有 48 种像素状态，每帧的背景抽样均为原场景的 RGBA `[22,20,35,255]`。
同一着色器的任务 1675 通过 850 帧房间回放、三档计时控件和笔记重开。
本机工程使用 `assets_private/npr-room-dev/room-splash`，预览位于 `reference-preview/splash.png`。
游戏揭幕、错误动画和带原服务初始化的同步回放进入后续验收。

| 证据 | SHA-256 |
| --- | --- |
| 启动画面验证 | `0ab187224a426e521304fc72f2fc06671dbeae523f4f5e84da21ade4e942b052` |
| 房间回放 | `694f455ce1390c5f888b277190f1beb2e62d0e1774da12b73b4b866db12333cb` |
| 界面片元着色器 | `bea138531fd66483a0440bc8d42ef5a429a05f68f74bb8f83c2ed6c3e737cc56` |

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
CoreCLR 9.0.22 的独立 Player 也通过了预加载、取消、错误读取与再次进入目标关卡。
本机开发工程位于 `assets_private/npr-room-dev/room-startup-probe`，其 `validation` 目录保留 Release Player、构建摘要及回放证据。
该 Player 通过工作树的相关源码和本机 Release 库链接。
项目移入私有目录后再次通过项目校验。
完整构建和发布验证按对应门禁执行。

## 房间几何集成

集成工程使用已保存的房间关卡、主相机和三个不透明材质槽。
通过原变换标识定位桌椅，保留场景节点及资源 UUID。
桌椅使用带原贴图的 GLB，椅子使用按原采样生成的窗边片段。
房间包含 283 个节点，其中六个节点用于计时界面及声源。模型资源为 194 项。
装配时复用同卷的网格文件。

集成工程完成 850 帧回放，消费 23 个输入事件，播放五次原音效。
Player 在原主相机取景下生成两张 1280×720 画面。
浏览预览可使用本机私有目录中的 `reference-preview/room-integrated.png`。
集成工程位于 `assets_private/npr-room-dev/room-integrated`，移入私有目录后通过项目校验。
全材质、灯光与角色动作按对应验收项实现。

| 证据 | 标识 |
| --- | --- |
| 生产回放与故障复测 | 共享队列 1624、1628 |
| 回放运行报告 SHA-256 | `544635c33c4d4b933cb9f7694b2a52dc0f08cbaa50293022baa20f8f7bc7eccc` |
| 修复前状态 SHA-256 | `6fda0b8c71d1a022973a293913c09ecd4fedd4adc5f3a0dcea7e92d9b32f45cb` |
| 修复后状态 SHA-256 | `8dc731e4415533794d8b68a92348185d9df17c3115ea408c1e6f7372525ca10c` |
| 托管脚本回放 | 共享队列 1631 |
| 托管运行报告 SHA-256 | `7a81abec20fb05f6937486e7ac9cb235638602bd7414593563c494d6c5e2f6bc` |
| 集成回放与取景 | 共享队列 1632、1633 |
| 集成房间 SHA-256 | `ba67c2568d0d0c7cbbc94907007d33e1d6e386707cf4036f1ffbe8d2bc1db026` |
| 集成回放 SHA-256 | `2acd39e5c7fa82591296c980ae48afe3595b6718c16252c62c0bee36b81018ab` |
| 取景画面 SHA-256 | `0180fd238a669f741005f7bc98838f9e7c32d9288cb08d0c352cae69dd05d752` |

## 复验

在 AzureRender 根目录执行命令。输出使用本机受管路径。

```powershell
python -B tools/npr_room/build_room_probe.py --source $unitySource --furniture $importedFurnitureProject --audio-playback $audioPlayback --startup-flow --output $roomStartupProbe
python -B tools/npr_room/build_room_probe.py --source $unitySource --furniture $importedFurnitureProject --audio-playback $audioPlayback --room-geometry $importedRoomProject --startup-flow --output $integratedRoom
python -B tools/npr_room/test_pomodoro_runtime.py --lua $luaDll
python -B tools/npr_room/test_room_startup_player.py --player $player --project $roomProject --output $startupReplay --resource-root $releaseBuild
python -B -m tools.npr_room.startup_splash --source $unitySource --catalog $assetCatalog --room $cityRoom --output $splashRoom
python -B -m tools.npr_room.verify_splash_capture --source $unitySource --capture $splashFrames --report $splashReport
```
