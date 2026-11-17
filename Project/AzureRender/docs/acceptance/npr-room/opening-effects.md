# 开场全屏特效

> 状态为 P5 源程序验收。Windows 原版程序、房间渲染配置、材质和纹理引用已锁定。下游按原始构图和时序验证 Vulkan 视觉结果。

`DirectionService` 引用三个画质档的房间渲染器。三个档位的开场渲染功能顺序均为色差、画面偏移、故障、黑屏和单色噪声。五个功能的挂载时机为 URP 事件 600，使用第 0 个 `DrawProcedural` 通道。

| 特效 | 原程序名称 | 原颜色合成 |
| --- | --- | --- |
| 色差 | `ScreenColorGlitch` | 替换 |
| 画面偏移 | `ScreenMoveGlitch` | 替换 |
| 故障 | `ScreenGlitch` | 按透明度混合 |
| 黑屏 | `ScreenBlackout` | 按透明度混合 |
| 单色噪声 | `NoiseShader` | 按透明度混合 |

从 Recovery 原版 Windows 1.17.3 的 `globalgamemanagers.assets` 读取五个 Shader 对象。每个对象的原始摘要与 Recovery 清单一致。五组顶点和片元程序均经过 Windows `D3DDisassemble` 解析。程序清单的 SHA-256 为 `74dc88e8fdc9d3f141a18612b62aae85e081577fcb1dea9d2e7560bb0909a78a`。

房间来源锁包含三个渲染器的有序通道、材质、默认浮点参数、纹理色彩空间、纹理原始摘要、着色器参数缓冲位置和混合状态。运行时纹理保留原槽位，分别对应相机不透明颜色、当前全屏输入和故障画面。来源锁的 SHA-256 为 `364c55422025e1ebaa19579062ecb0c497dc79a11aa79996abd033d25188c5ba`。本机编译程序和清单位于 `assets_private/npr-room-dev/windows-opening-reference/`。

## 单色噪声实机参照

任务 1816 使用原生 D3D11 顶点和片元程序，以及原材质的 64×1 sRGB 时间纹理，在固定底色上捕获三个 320×180 RGBA32F 帧。比率为 0 时控制帧逐像素一致。比率为 0.05 时，每个像素按原混合系数与黑色或白色合成，透明度保持为 1。时间从 9.302 秒移到 9.802 秒时，18,319 个像素变化。两次硬件回放的摘要一致，均为 `5a17e42cb1f1dfda269de9d61619865df57ffc4b8500f2e57809b53a96e86389`。

## 复验

```powershell
python -B -m tools.npr_room.unity_d3d11_programs --source $nativeArchive --manifest $recoveryShaderManifest --family opening-effects --output $programDirectory
python -B -m tools.npr_room.windows_opening_effects --source $unitySource --catalog $privateAssets --native $nativeArchive --programs $programReceipt --output $effectsReceipt
python -B -m unittest tools.npr_room.test_unity_d3d11_programs
cl.exe /nologo /EHsc /std:c++20 /O2 /W4 tools/npr_room/WindowsOpeningNoiseProbe.cpp d3d11.lib
python -B -m tools.npr_room.windows_noise_probe prepare --effects $programDirectory --catalog $imageCatalog
& $noiseProbe $programDirectory $originalCapture
& $noiseProbe $programDirectory $repeatCapture
python -B -m tools.npr_room.windows_noise_probe verify --first $originalCapture --repeat $repeatCapture --output $noiseReport
```
