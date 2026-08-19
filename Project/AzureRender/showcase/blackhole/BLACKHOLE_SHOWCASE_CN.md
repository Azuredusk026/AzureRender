# 黑洞场景展示

## 当前交付

正式总片严格包含四段，每段 192 帧/8.00 秒，顺序为正面、侧上方、移动视角、参考近景。全部使用 Cinematic（1800 最大步数、4 spp、near step scale 0.48）、1600×900、24 fps、SAR 1:1。近景让黑洞位于画面右侧附近，近处吸积盘斜向扫过画面。

| 文件 | 内容 | SHA-256 |
| --- | --- | --- |
| `images/20260819-151955_黑洞正面.png` | `front` 中段帧 | `8037B8D3A400F5BA02F3D9B557CA9FFD99D001437BDE3EA0C405C151D7BDD228` |
| `images/20260819-151956_黑洞侧上方.png` | `high` 中段帧 | `6F0378C80F0C45E309CDC348884998E795EAFBFA2736E7A543AC53872C602C1C` |
| `images/20260819-151957_黑洞移动视角.png` | `orbit-left + portfolio` 中段帧 | `EFA623C3871B43CACDCC26DD9D4DC47EC1A46AC3A17185566896AAE4C0E9C228` |
| `images/20260819-151958_黑洞参考近景.png` | `close` 中段帧 | `996E93F92DAA7D95D0636E0087DD103E0A374FAB600CE63A2A667AFA879852F0` |
| `video/20260819-152000_黑洞四机位展示.mp4` | 768 帧、32.00 秒、四段顺序拼接 | `168BCF3B026551604FA3F6DA6D0C87F7895A910CAC963C2991A3EBFA895BC10A` |

视频已完整解码至 null sink，无损坏帧；容器报告 H.264 High、yuv420p/BT.709、1600×900、24 fps、SAR 1:1、DAR 16:9。

## 复现

每个相机使用相同参数单独捕获，移动段额外增加 `--portfolio`：

```powershell
.\build\ninja-debug\AzureRender.exe `
  --scene-type blackhole --blackhole-quality cinematic `
  --blackhole-camera front --width 1600 --height 900 `
  --capture-dir .\build\blackhole_front `
  --capture-frames 192 --capture-fps 24
```

依次将 `--blackhole-camera` 替换为 `high`、`orbit-left`、`close`。四段编码后按该顺序无重编码拼接，最终必须为 768 帧/32.00 秒。
