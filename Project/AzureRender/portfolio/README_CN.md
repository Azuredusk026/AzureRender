# AzureRender 作品集交付索引

项目：AzureRender - Stylized Vulkan Character Renderer  
作者：Wu Chenfeng  
当前里程碑：S34

这是投递和面试展示的统一入口。交付包不重复复制大型视频，而是引用
`captures/` 中经过验证的最终文件；`portfolio_manifest.json` 记录路径、
文件大小、SHA-256、技术视频章节和 Release GPU Timing 摘要。

## 建议展示顺序

1. 先展示 `images/azurerender_cover_1920x1080.png`，用一句话说明这是自主
   开发的 Vulkan 风格化角色渲染器。
2. 播放纯 Beauty 视频
   `../captures/Afterglow_S28_Portfolio_1080p60_20s.mp4`，重点展示最终画面、
   GPU 蒙皮、Idle 动画和确定性环绕镜头。
3. 播放技术拆解视频
   `../captures/Afterglow_S33_TechnicalTitles_1080p60_20s.mp4`，依次解释
   World Normal、Internal Outline、Shadow Map 与实时 GPU HUD。
4. 展示 `images/technical_contact_sheet_1920x1080.png`，用于静态项目页或
   面试中快速回顾五个渲染视图。
5. 最后引用 `../captures/s29_gpu_timing_release_1080p.json`，说明 GPU
   Timestamp 的测量边界和 1080p Release 数据。

## 一句话项目说明

AzureRender 是一个面向风格化角色展示的 Vulkan Renderer，支持 glTF
材质、GPU 蒙皮与动画、方向光 Shadow Map、倒壳外轮廓、深度/法线内部描边、
分层风格化光照、运行时 GPU Timing HUD，以及确定性的 1080p60 视频捕获。

## 技术亮点

- Vulkan 多 Pass 渲染与显式同步；
- glTF 多材质、法线、金属度/粗糙度、透明模式与双面材质；
- Storage Buffer 驱动的 GPU Skinning；
- 四秒循环 Idle Animation 与连续 Portfolio Orbit；
- 2048×2048、Alpha-aware、3×3 PCF Directional Shadow Map；
- 采样 Scene Depth 与 World Normal 的屏幕空间内部描边；
- Renderer 原生文字 HUD、章节标题和淡入淡出；
- Vulkan Timestamp Query 分别统计 Shadow、Main Scene 和 Outline；
- 固定时间步 PNG 序列和 BT.709 H.264 输出。

## 已验证性能

测试设备：NVIDIA GeForce RTX 4060 Laptop GPU，1920×1080，Release，
600 个 GPU Timestamp 样本。

- Shadow：0.189631 ms
- Main Scene：0.620023 ms
- Internal Outline：0.071526 ms
- 三个 GPU Pass 合计：0.881180 ms

这里的合计仅表示命令缓冲中的三个 GPU 渲染阶段，不包含 CPU 更新、
Swapchain Present、截图 Readback 或 PNG/MP4 编码，不应描述为完整帧耗时。

## 最终媒体

- 纯 Beauty：`../captures/Afterglow_S28_Portfolio_1080p60_20s.mp4`
- 技术拆解：`../captures/Afterglow_S33_TechnicalTitles_1080p60_20s.mp4`
- 封面：`images/azurerender_cover_1920x1080.png`
- 五章联系表：`images/technical_contact_sheet_1920x1080.png`
- GPU Timing：`../captures/s29_gpu_timing_release_1080p.json`
- 机器可读清单：`portfolio_manifest.json`

## 资产与公开说明

Renderer 源码、着色器、工具和程序化 Idle Animation 是本项目开发内容。
角色模型和原始贴图用于非商业技术展示与学习，不应随公开源码仓库重新分发。
公开仓库应保留 `assets_public/test_model.gltf` 作为可运行测试资产，并让私有
角色 GLB、原始贴图和大型捕获序列继续由 `.gitignore` 排除。

## 重新生成

在项目根目录运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\build_portfolio_package.ps1 `
  -FfmpegExecutable `
  "C:\tmp\afterglow_ffmpeg_812\ffmpeg-8.1.2-essentials_build\bin\ffmpeg.exe"
```

脚本会验证所有源文件、重新生成封面和联系表，并刷新包含 SHA-256 的
`portfolio_manifest.json`。
