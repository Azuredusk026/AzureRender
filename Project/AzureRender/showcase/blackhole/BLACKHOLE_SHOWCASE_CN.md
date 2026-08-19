# 黑洞场景展示

> 最终归档状态：`Final / Frozen`（2026-08-19）。当前媒体、参数、复现命令和 SHA-256 为黑洞 P1 最终验收基线；除非用户主动重新启用，不再修改黑洞 shader、机位或展示片。

## 当前交付

正式总片只包含两个连续段落：`front` 正面和 `close + portfolio` 近距离移动构图，每段 192 帧/8.00 秒。全部使用 Cinematic（1800 最大步数、4 spp、near step scale 0.48）、1600×900、24 fps、SAR 1:1。吸积盘角度噪声已改为圆周周期坐标，正面与移动近景中均无固定径向接缝。

| 文件 | 内容 | SHA-256 |
| --- | --- | --- |
| `images/20260819-170018_黑洞正面无接缝.png` | `front` 中段接缝回归帧 | `91DE8FE261CE5A8018BE64696B049D0F9B18E7B9F429CE82ADFC5E7A159DB55D` |
| `images/20260819-170018_黑洞越肩移动近景.png` | `close + portfolio` 中段帧 | `262E6E9218C52C11CBCDEDA759628734AFDBA997427A484C21B19564AFBA988B` |
| `video/20260819-170018_黑洞双机位展示.mp4` | 384 帧、16.00 秒、正面后接移动近景 | `8FEDE9E6C01D8FF6CA366373B617CBD88C2AE14D93C05D9FD8759834B95A826C` |

视频已完整解码至 null sink，无损坏帧；容器报告 H.264 High、yuv420p/BT.709、1600×900、24 fps、SAR 1:1、DAR 16:9。

## 复现

```powershell
.\build\ninja-debug\AzureRender.exe `
  --scene-type blackhole --blackhole-quality cinematic `
  --blackhole-camera front --width 1600 --height 900 `
  --capture-dir .\build\blackhole_front `
  --capture-frames 192 --capture-fps 24

.\build\ninja-debug\AzureRender.exe `
  --scene-type blackhole --blackhole-quality cinematic `
  --blackhole-camera close --portfolio `
  --width 1600 --height 900 `
  --capture-dir .\build\blackhole_close_move `
  --capture-frames 192 --capture-fps 24
```

两段分别编码后按上述顺序无重编码拼接，最终必须为 384 帧/16.00 秒。
