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

Vulkan 全屏测试使用相同的底色、合成系数和时间输入。任务 1827 与 1828 的 172,800 个像素逐次一致，校验层错误数和退出后的分配数均为零。与 D3D11 对照时，控制帧逐像素一致，两帧活动噪声分别有 6,712 和 6,223 个像素位置存在差异，平均 RGB 绝对误差为 0.00583 和 0.00540。精确噪声位置及完整房间合成继续按 P5 验收。报告位于 `vulkan-noise-comparison.json`。

## 黑屏特效硬件参照

任务 1929 使用原 Windows 顶点和片元程序、原材质常量及两张原纹理。坡度纹理使用点采样，时间纹理使用线性采样，两张纹理均按 sRGB 解码及循环寻址。测试为相机不透明颜色和目标底色分别提供固定输入，采集五张 320×180 RGBA32F 画面。

比率为 0 时控制帧逐像素一致。固定时间从 9.302 秒推进至 9.802 秒，比率 0.5 的画面有 53,653 个像素变化。同一时刻开启百叶窗有 17,916 个像素变化。比率为 1 时全帧 57,600 个像素均参与替换。原程序的浮点中间值最低为 −0.75，最终显示和原生房间组合需按完整画面对照验收。重复采样的摘要一致。单色噪声回归逐字节匹配已归档的 D3D11 参照。

本机证据位于 `windows-opening-reference/blackout-validation/`。

| 记录 | SHA-256 |
| --- | --- |
| 原材质与纹理输入 | `3fd3ba0742a743eae8eb238c0ce548190e9de78c2806556cf5bbb2cbc13cb699` |
| 原始浮点画面 | `2d7f6d4d12d2ef97d5bb048a2c515969fca254805c5d524e62a182b95e3d6a80` |
| 回放报告 | `cd8f42b29019a6c093afd747dc8f538caf1f25be8e324922d188cd5646b30371` |

## 原生画面合成

版本 1 的 `azure.screen-noise` 组件接受 0 至 1 的透明度。默认值 0 对应原画面。活动场景包含多个组件时，取其中的最大透明度，以帧时间推进图案。全屏绘制位于色彩处理和抗锯齿之后、游戏界面之前。

任务 1837 使用 640×360 的四个独立 Player 进程。未声明组件和使用默认值时，三帧的像素逐帧一致。脚本设置 0.25 后，首个固定模拟步生成的图像与同透明度的场景数据一致。39,084 个背景像素在相邻采样之间改变噪声图案。不透明界面抽样区保持原色，四个进程正常退出后的分配数均为零。

精简证据位于 `windows-opening-reference/player-validation/`。
同一 Player 的房间回归覆盖两个进程的笔记编辑、排序、删除、重开及存档版本保护。

## 剧情噪声时序

教程首次播放按源行 3 在 2 秒内升至 0.05。第一章和历史回放读取准备段最后一条噪声命令作为初始值。后续命令保留各自的延迟，开始时接续当时的透明度。结束剧情清空待执行命令。

原版 `Ease` 枚举确认 `isSine` 走线性回退。参数 `0` 使用 `DOTweenSettings.asset` 的 `OutQuad`。脚本测试覆盖延迟先后、重叠、即时变更、取消和结束清理。

任务 1839 使用原生房间夹具。教程录得 589 个非零帧，峰值为 0.05。第一章录得 80 个非零帧，峰值为 0.03。章节完成写入前核对组件已归零。报告位于 `room-opening-voices-verified/validation/opening-route-noise/`。

本次房间回放覆盖单色噪声、32 段原语音和背景音乐。场景淡化、对应环境音和五个特效的组合进入完整画面对照。

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
python -B -m tools.npr_room.windows_noise_probe prepare-blackout --effects $programDirectory --catalog $imageCatalog --source $unitySource --output $blackoutInputs
& $noiseProbe $blackoutInputs $blackoutCapture blackout
& $noiseProbe $blackoutInputs $blackoutRepeat blackout
python -B -m tools.npr_room.windows_noise_probe verify-blackout --first $blackoutCapture --repeat $blackoutRepeat --output $blackoutReport
& $glslc --target-env=vulkan1.3 shaders/inner_outline.vert -o $fullscreenVertex
& $glslc --target-env=vulkan1.3 shaders/monochrome_screen_noise.frag -o $noiseFragment
& $vulkanNoiseProbe $fullscreenVertex $noiseFragment $vulkanCapture
& $vulkanNoiseProbe $fullscreenVertex $noiseFragment $vulkanRepeatCapture
python -B -m tools.npr_room.windows_noise_probe compare --reference $originalCapture --candidate $vulkanCapture --repeat $vulkanRepeatCapture --output $vulkanComparison
python -B tools/test_screen_noise_player.py --player $player --resource-root $runtimeResources --output $playerNoiseReplay
```

Lua 5.4 依次载入 `assets/story-noise.lua` 和 `tests/story-noise.lua`。
