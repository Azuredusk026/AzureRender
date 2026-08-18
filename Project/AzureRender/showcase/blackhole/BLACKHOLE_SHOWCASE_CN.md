# Blackhole 场景展示

## v2 相对论吸积盘

本版使用 Release/Cinematic（1800 最大步数、4 spp、near step scale 0.48）生成。盘面使用物体空间乘法噪声、动态厚度、旋臂和 dust gap；多普勒因子同时控制色温、方向偏色与 `doppler^3` beaming。接近侧为蓝白高亮，远离侧为红暗，稀疏纹理随盘连续旋转。

| 截图 | 相机 / 取帧 | SHA-256 |
| --- | --- | --- |
| `images/blackhole_front_relativistic_v2_1280x720.png` | front / 299 | `97C88862968F5E14E3CF2DE68B43067B923BBEAA968C002F31A3CB47CC14DA3D` |
| `images/blackhole_high_relativistic_v2_1280x720.png` | high / 299 | `9BBCDABE9285FDA4D4AA10DE9046C1188B0EE98619D60F041A15A3E1F2BFE5F2` |
| `images/blackhole_over-shoulder_relativistic_v2_1280x720.png` | over-shoulder / 299 | `0A0C8C930A4A3A39AE996D5474CBD55826D7ACA85629B49137235CDF8D38335A` |

每个独立视频均为 300 帧、60 fps、完整 5.0 秒：

| 视频 | SHA-256 |
| --- | --- |
| `video/blackhole_front_relativistic_v2_1280x720_60fps.mp4` | `00398AB3FC9A548DBEA426478C7EEC72C2AD2FADF50813189C9036E0412A2891` |
| `video/blackhole_high_relativistic_v2_1280x720_60fps.mp4` | `8368B9550EFBC3833DE712ECCC6F1207F2888880843A40229BA49985F2013772` |
| `video/blackhole_over-shoulder_relativistic_v2_1280x720_60fps.mp4` | `1959B4BF53E91EB38A793CC05D0B4E6A770792C61CF39A7131EE030E2D9AC9BA` |

总览 `video/blackhole_three_views_relativistic_v2_1280x720_60fps.mp4` 按 front、high、over-shoulder 拼接，为 900 帧、15.0 秒，SHA-256 为 `1F9DFB204706FE21C5760FBD96F944CFC01A7077B37658DA276B971B76272B8B`。四个 MP4 均已完整解码验证。

```powershell
.\build\ninja-release\AzureRender.exe `
  --scene-type blackhole --blackhole-quality cinematic `
  --blackhole-camera over-shoulder --width 1280 --height 720 `
  --capture-dir .\captures\blackhole\blackhole_over-shoulder_relativistic_v2_1280x720 `
  --capture-frames 300 --capture-fps 60
```

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
