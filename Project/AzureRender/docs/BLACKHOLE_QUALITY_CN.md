# Blackhole 质量与视觉回归

## 2026-08-18 吸积盘视觉基线 v2

吸积盘密度参考 `D:\Assigment\temp\BufferA.txt` 的 `GenerateAccretionDiskNoise`、`Shape`、SpiralTheta 和 Keplerian velocity。实现使用物体空间乘法 octave、旋转径向坐标、动态厚度、旋臂遮罩和 dust cloud，不再为噪声设置非零密度保底，因此盘面允许形成真实稀疏间隙。噪声随盘连续旋转而不是逐帧随机生成，TAA 半衰期缩短为 55 ms，以保留细节并避免闪烁。

多普勒效果同时驱动色温、接近/远离侧 RGB 偏色和 `doppler^3` 相对论 beaming。接近侧必须明显偏蓝白且更亮，远离侧必须偏红且更暗；引力红移继续按观测者与采样半径修正。`shiftMax` 从 1.25 提高到 2.4，只限制极端峰值，不再抹平主要不对称。

正式视频契约为三个独立机位各 300 帧、60 fps、完整 5.0 秒：`front`、`high`、`over-shoulder`。其中 `over-shoulder` 必须让黑洞中心位于画面右下区域，并让左上主要保留星空。三段按该顺序拼接为 900 帧、15.0 秒总览。

## 1. 质量档位

| 档位 | 最大积分步数 | 每像素 trace | 近光子球步长比例 | 用途 |
|---|---:|---:|---:|---|
| `performance` | 600 | 1 | 0.85 | 快速预览 |
| `balanced` | 1100 | 1 | 0.65 | 交互与 timing |
| `cinematic` | 1800 | 4 | 0.48 | 截图与视频，默认 |

积分步长保持连续，并在 1.5Rs 到 8Rs 的高曲率区域逐渐细化，不使用会造成色块边界的离散距离分段。

```powershell
.\build\ninja-release\AzureRender.exe `
  --scene-type blackhole `
  --blackhole-quality cinematic `
  --blackhole-camera orbit-left
```

相机档位为 `front`、`orbit-left`、`high`、`close`、`over-shoulder`。质量或相机变化、resize、capture 开始及非连续旋转都会自动使 TAA history 失效；capture manifest 记录档位、视角、步数、采样数、近场步长和 reset 次数。

## 2. 图像回归

```powershell
python .\tools\compare_images.py reference.png candidate.png `
  --max-mean-error 0.005 `
  --max-changed-ratio 0.01 `
  --output comparison.json
```

工具比较线性归一化后的 RGB 像素，维度不一致直接失败。默认像素变化阈值为 `2/255`；正式基准应同时保存命令、capture manifest 和 comparison JSON。

R3 本机验收（RTX 4060 Laptop GPU，1280x720，Release）：

- 两次独立 Cinematic/Front 12 帧捕获的末帧平均 RGB 误差 `0.0`，变化像素比例 `0.0`。
- Character Endfield 公共全身基线逐像素一致，证明黑洞 shader/设置未污染角色 renderer。
- Balanced/Orbit-left 300 个 timing 样本：Main scene `3.573 ms`，Total render 平均 `3.616 ms`、最小 `3.298 ms`、最大 `6.064 ms`。该值是 GPU pass timing，不包含 CPU、present、readback 或编码。
