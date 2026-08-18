# Blackhole 场景展示

> 生成日期：2026-08-18
> GPU：NVIDIA GeForce RTX 4060 Laptop GPU

## 设置

全部媒体使用 Release、Cinematic 质量、1800 最大步数、4 spp、near step scale 0.48。截图为 1920x1080，并在 temporal history 积累后选取末帧。

| 文件 | 相机 / 取帧 | SHA-256 |
| --- | --- | --- |
| `images/blackhole_front_cinematic_v1_1920x1080.png` | front / 59 | `0182EF777D6D187C9F38C7A5FF6C52819783D68BFADBA08A954F29B75632E0B8` |
| `images/blackhole_orbit_left_cinematic_v1_1920x1080.png` | orbit-left / 59 | `28D308D72328DF27C90831BCFBE25F91508C2729DA824DBC8CF11FF4B3EAEBB9` |
| `images/blackhole_high_cinematic_v1_1920x1080.png` | high / 52 | `CD868AC2B8E30D010B460072F431043A7ED76F048666AA302E41B1560DE0195B` |
| `images/blackhole_close_cinematic_v1_1920x1080.png` | close / 35 | `F07D27EB0F3B3185781DECAA72D70E3C05241D5203D3A4CD59DFDC0E373C8FCA` |

High 的原计划为 60 帧，但该次 1080p PNG readback 在完成第 52 帧后停止推进；进程被终止，已完成帧有效且 history 已收敛。Close 随后独立完成 36/36，最终程序回归正常退出。该现象记录为长序列 readback 风险，不伪装为完整 60 帧证据。

## 视频

`video/blackhole_four_views_cinematic_v1_1920x1080_60fps.mp4`：front 60 帧、orbit-left 60 帧、high 53 帧、close 36 帧顺序拼接，共 209 帧 / 3.48 秒，H.264/yuv420p/BT.709。SHA-256：`4FF6CF8E3C7D36F60084911258DF8AB2B2C6A99DCB40ABA45EC75CCEE02C3296`。

## 复现单视角

```powershell
.\build\ninja-release\AzureRender.exe `
  --scene-type blackhole --blackhole-quality cinematic `
  --blackhole-camera front --width 1920 --height 1080 `
  --capture-dir .\build\showcase-captures\blackhole-front `
  --capture-frames 60 --capture-fps 60
```
