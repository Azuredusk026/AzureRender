# AzureRender 公开视觉证据

本目录保存公开图片、捕获来源和 SHA-256 清单。精选图用于 README 与文档站。完整帧序列保存在本机生成目录。

## 目录与来源

| 目录 | 内容 |
| --- | --- |
| `images/editor/` | 编辑器工作区 |
| `images/gameplay/` | 机器人侧面行走与探索关卡 |
| `images/character/` | 公开角色全身与近景 |
| `images/blackhole/` | 黑洞渲染 |
| `evidence/` | 角色与黑洞捕获摘要 |
| `portfolio_manifest.json` | 图片尺寸、字节数、来源与哈希 |

工作区、动作和探索图由 Release 构建捕获。设备为 RTX 4060 Laptop，尺寸为 1920×1080。每张图记录源码、运行文件、场景、输入与捕获参数。

公开图片使用项目公开资源。私有角色、授权动画与派生媒体保存在本机范围。图片路径包含场景、视角或用途及分辨率。哈希清单核验文件身份。

## 图片索引

- `editor_workspace_1920x1080.png` 展示菜单、工具栏与停靠面板。
- `robot_walk_side_1920x1080.png` 展示公开机器人的侧面行走姿态。
- `exploration_1920x1080.png` 展示公开关卡与任务界面。
- `character_endfield_public_fullbody_v1_1280x720.png` 展示全身视角。
- `character_endfield_public_closeup_v1_1280x720.png` 展示近景视角。
- `blackhole_temporal_beauty_v1_1280x720.png` 展示黑洞渲染。

## 验证与阅读

在工程目录执行图片与证据清单验证。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/verify_portfolio.ps1
```

完整交付结果见[P2 验收](../docs/acceptance/p2/2026-10-05.md)。关卡制作见[使用教程](../docs/tutorials/editor-first-game.md)。渲染说明见[架构文档](../docs/architecture.md)。
