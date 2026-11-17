# 开场全屏特效

> 状态为 P5 局部验收。五个 Windows 原版程序及其材质已锁定。开场单色噪声和黑屏通过原生房间回放。

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

## 色差硬件参照

色差使用两张原版 sRGB 纹理，均以线性过滤和循环寻址采样。输入画面使用 320×180 的线性 RGBA32F 固定图案。原 Direct3D 11 程序读取不同位置的颜色，并按原时间种子、纹理和扫描线参数输出替换画面。

任务 2007 在 Intel 核显和 RTX 4060 各执行两次相同采样。四帧包含零比率、教学比率 0.2、半秒后的 0.2 和比率 0.5。每台设备的重复采样逐字节一致，零比率帧与输入逐像素一致，所有帧的透明度均为 1。比率 0.2 下，半秒间隔改变 Intel 的 57,595 个像素与 NVIDIA 的 57,600 个像素。单色噪声及黑屏回归与各自归档的 Direct3D 参照逐字节一致。

本机证据位于 `windows-opening-reference/chromatic-validation/`。硬件参照用于对应设备的 Vulkan 逐帧对照。

| 记录 | SHA-256 |
| --- | --- |
| 色差源绑定清单 | `dd116548b12edb0edba44e305d48f10f22ce56fa18dcde074cde2d3b6178efca` |
| Intel 原始浮点画面 | `b58a463dab43dad075fe3d9dd68e147a6de10140507ae19d6927a945893fc15d` |
| NVIDIA 原始浮点画面 | `1a2b0536af1b467d49baa8c95b26344737f42fa39ae83803e2ed94507d06f264` |
| Intel 回放报告 | `0b458b877a369810ea4fe87d2e490a68ea1189f5ba7a167735255a73d935d4fe` |
| NVIDIA 回放报告 | `5cc67f4c47ad2c59d7ba53da390b3d9eb114a360ea224ad2a1cc2ccf57e5b01d` |

## Vulkan 色差合成

Vulkan 着色器以原程序的时间种子计算红色通道位移，读取当前画面，并使用两张原纹理计算颜色扰动和扫描线。零比率的输出采用当前画面。

RTX 4060 的任务 2008 和 2009 各执行四帧。Intel 核显的任务 2011 和 2012 各执行相同四帧。每台显卡的 Vulkan 回放与同一显卡的 Direct3D 11 原程序结果逐字节一致，分别核对 230,400 个 RGBA32F 像素。四次回放均通过 Vulkan 校验与资源释放检查。任务 2010 的单色噪声使用同设备工具对照，黑屏使用同设备归档，两项回归均逐字节一致。

本机报告与编译程序位于 `windows-opening-reference/chromatic-validation/`。常规房间的画面输入与剧情命令另行验收。

| 记录 | SHA-256 |
| --- | --- |
| Intel Vulkan 对照 | `e8c57c143c2f61bba2b890d2a0f0e66cdc41e506ef89fd9efdd9556ba4cdef35` |
| NVIDIA Vulkan 对照 | `b908546625216695623c36e8ac670bf6c96f960883498c2181ced28ea5efef4d` |
| Vulkan 色差程序 | `e363362f77c2eeccf60bca00745ab3d3e76482e5aa68ef8e059895909af254d7` |

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

## Vulkan 黑屏合成

Vulkan 着色器按原程序计算时间种子、块遮罩和百叶窗。渐变纹理按对应显卡的 D3D11 点采样规则处理垂直边界。每台显卡分别运行原始 D3D11 程序和 Vulkan 着色器，保持纹理、目标底色及五组时间和比率一致。

Intel 核显的任务 1952 记录最大通道误差 1.49×10⁻⁸。RTX 4060 的任务 1953 记录最大通道误差 1.86×10⁻⁹。两种显卡均重复采样两次，Vulkan 校验层与资源释放检查通过。零比率和全黑帧逐像素一致。RTX 4060 的单色噪声回归逐字节匹配归档的 Vulkan 参照。

独立全屏采样使用固定的相机不透明颜色输入。房间画面、通道顺序和剧情触发各自按正常入口采证。

| 记录 | SHA-256 |
| --- | --- |
| Intel 核显对照 | `a010db01ee67dd265315ea14fc6e9552bb44281f7b08d3839e836cd4147a63ac` |
| RTX 4060 对照 | `073b9c11287f28843ee64a93951e711054daae7ca2393e519feda6df92bf2ad9` |
| RTX 4060 原始浮点画面 | `1a8b8efcbe5aab34b73b04e4005df7263a563a8d5bbca94af939ecdcba1dc5a8` |
| Vulkan 黑屏程序 | `b15133e7ed89a1c27ea43a90492194bde82926470f187d050dd17d7af0757405` |

## 房间黑屏时序与合成

`azure.screen-blackout` 读取当前相机的不透明颜色及房间装配的两张原纹理。黑屏在游戏界面之前合成，单色噪声叠加在其上。每个在途帧使用独立的图像绑定，切换场景和调整窗口时按渲染资源生命周期重建。

剧情命令按源延迟启动。持续时间为 −1 时保持至下一次变更或剧情结束。指定持续时间时，按原命令调度清屏。教学首次播放依次使用 1、0.5、0，第一章使用正常画面。

任务 1954 从正常入口核对命令时序。任务 1968 使用原生诊断构建回放教学和第一章，半黑到清屏间隔 0.8 秒。全黑帧的场景区只包含黑底与单色噪声，半黑帧保留 40,896 个彩色场景像素，83,266 个彩色界面像素跨两个状态保持一致。身体动作、视线、相机、语音、嘴型、成就及存档通过回归。

本机回放、画面和诊断构建位于 `windows-opening-reference/blackout-validation/room-roundtrip/`。

| 记录 | SHA-256 |
| --- | --- |
| 常规房间回放 | `b682a8c9b62d0292159565425cbd654b58727c068376bb544940b7ff58527d8a` |
| 原生 Player | `88cc278567c6508fbd6f080ed7bd0a53b22258912294fc5b417e3beca2ea3d95` |
| 诊断构建清单 | `679d4267260d15940f69f35c1b1bd272733a00d32f55e4d1cbf444f5daf17433` |

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
cl.exe /nologo /EHsc /std:c++20 /O2 /W4 tools/npr_room/WindowsOpeningNoiseProbe.cpp d3d11.lib dxgi.lib
python -B -m tools.npr_room.windows_noise_probe prepare --effects $programDirectory --catalog $imageCatalog
& $noiseProbe $programDirectory $originalCapture
& $noiseProbe $programDirectory $repeatCapture
python -B -m tools.npr_room.windows_noise_probe verify --first $originalCapture --repeat $repeatCapture --output $noiseReport
python -B -m tools.npr_room.windows_noise_probe prepare-chromatic --effects $programDirectory --catalog $imageCatalog --source $unitySource --output $chromaticInputs
& $noiseProbe $chromaticInputs $chromaticCapture chromatic $dxgiAdapterIndex
& $noiseProbe $chromaticInputs $chromaticRepeat chromatic $dxgiAdapterIndex
python -B -m tools.npr_room.windows_noise_probe verify-chromatic --first $chromaticCapture --repeat $chromaticRepeat --output $chromaticReport
& $glslc --target-env=vulkan1.3 shaders/inner_outline.vert -o $fullscreenVertex
& $glslc --target-env=vulkan1.3 shaders/opening_chromatic.frag -o $chromaticFragment
& $vulkanNoiseProbe $fullscreenVertex $chromaticFragment $vulkanChromatic $chromaticInputs chromatic
& $vulkanNoiseProbe $fullscreenVertex $chromaticFragment $vulkanChromaticRepeat $chromaticInputs chromatic
python -B -m tools.npr_room.windows_noise_probe compare-chromatic --reference $chromaticCapture --candidate $vulkanChromatic --repeat $vulkanChromaticRepeat --output $vulkanChromaticReport
python -B -m tools.npr_room.windows_noise_probe prepare-blackout --effects $programDirectory --catalog $imageCatalog --source $unitySource --output $blackoutInputs
& $noiseProbe $blackoutInputs $blackoutCapture blackout
& $noiseProbe $blackoutInputs $blackoutRepeat blackout
python -B -m tools.npr_room.windows_noise_probe verify-blackout --first $blackoutCapture --repeat $blackoutRepeat --output $blackoutReport
& $noiseProbe $blackoutInputs $blackoutAdapterCapture blackout $dxgiAdapterIndex
& $glslc --target-env=vulkan1.3 shaders/inner_outline.vert -o $fullscreenVertex
& $glslc --target-env=vulkan1.3 shaders/opening_blackout.frag -o $blackoutFragment
& $vulkanNoiseProbe $fullscreenVertex $blackoutFragment $vulkanBlackout $blackoutInputs
& $vulkanNoiseProbe $fullscreenVertex $blackoutFragment $vulkanBlackoutRepeat $blackoutInputs
python -B -m tools.npr_room.windows_noise_probe compare-blackout --reference $blackoutAdapterCapture --candidate $vulkanBlackout --repeat $vulkanBlackoutRepeat --output $vulkanBlackoutReport
python -B tools/npr_room/test_story_signal_room_player.py --player $player --room $deskRoom --resource-root $runtimeResources --catalog $bodyCatalog --graph $animatorGraph --chapters $openingChapters --source-receipt $tutorialSource --models $bodyMorphModels --camera-trace $cameraTrace --camera-audio-source $unitySource --face-catalog $plankCatalog --blackout-source-lock $effectsReceipt --image-catalog $imageCatalog --achievements --output $blackoutRoomReplay
& $glslc --target-env=vulkan1.3 shaders/monochrome_screen_noise.frag -o $noiseFragment
& $vulkanNoiseProbe $fullscreenVertex $noiseFragment $vulkanCapture
& $vulkanNoiseProbe $fullscreenVertex $noiseFragment $vulkanRepeatCapture
python -B -m tools.npr_room.windows_noise_probe compare --reference $originalCapture --candidate $vulkanCapture --repeat $vulkanRepeatCapture --output $vulkanComparison
python -B tools/test_screen_noise_player.py --player $player --resource-root $runtimeResources --output $playerNoiseReplay
```

Lua 5.4 依次载入 `assets/story-noise.lua` 和 `tests/story-noise.lua`。
