# Windows 窗景大气来源

> 状态为 P5 局部验收。Windows 的原始着色器程序、资源绑定和四个时段的运行参数已核对。白天与傍晚的天空通道已通过原生采样和 Vulkan 房间对照。

`RoomScene` 中的 `MyCozyWeatherService` 绑定 `BaseCozy Weather Sphere` 和四个时段的时间模块。Windows 实例使用天空、云层和雾三个网格渲染器。三个原材质分别引用 `Stylized Sky Desktop`、`Stylized Clouds (COZY Desktop)` 和 `Stylized Fog (Desktop)`，基准天气组件引用十张原图。移动端备用天空的四个场景对象按独立引用登记。

| 时段 | 窗景编号 | 时间模块 | 原当日比例 |
| --- | ---: | ---: | ---: |
| 白天 | 11 | 22587 | 0.52083330 |
| 傍晚 | 12 | 22526 | 0.77083330 |
| 夜晚 | 13 | 25873 | 0.90921134 |
| 多云 | 14 | 25805 | 0.52083330 |

## 原程序与参照

从 Recovery 1.17.3 `ReferenceProbe` 的 `resources.assets` 读取五个原着色器对象。每个原始对象的 SHA-256 与 Recovery 归档一致。Microsoft Direct3D 反汇编器验证五组首通道的共十段着色器程序。记录包含顶点与片元程序的原条目编号、常量缓冲区偏移、纹理槽与采样器槽。两次独立提取的清单摘要一致。

四份原实机大气记录提供采样后的月亮方向、天空及云层颜色、雾、散射和滤色值。原窗景截图与运行参数使用相同的编号。首通道绑定与源资源清单用于复验 Windows 绘制参数。

| 记录 | SHA-256 |
| --- | --- |
| Direct3D 程序和绑定 | `dc42aaa993c5d494d88e7b4ac7e026b4ca41e6b4e874962a9fbdc7d42b1bca49` |
| 实机大气来源及资源绑定 | `44811daf277a26a5e6def1c1bb26ef5b9ed0f5a49514a54ced7609966badc011` |

## 傍晚天空离屏采样

任务 1790 和 1791 使用 Direct3D 11 硬件设备执行原傍晚天空顶点和片元程序。采样使用原天空网格的 675 个顶点和 672 个三角形、五张原贴图及傍晚实机记录的颜色和太阳方向。照相机置于天空球中心，以固定时刻从六个轴向各取 256 × 256 像素。两个进程的 393,216 个 RGBA32F 像素逐字节一致，全部具有有限分量和完整覆盖。线性 RGB 范围为 0.02131 至 1.05969。

原始采样的 SHA-256 为 `b0682e513746c5dd32b86307f66ce277cc9df6740c0af5cd73d77033e1759457`。本机 `windows-atmosphere-reference/sunset-native-probe.json` 记录坐标面顺序、运行编号和源资产。该采样对应天空通道，云、雾、室内光照与色彩输出使用合成画面的独立验收。

## Vulkan 天空通道

六面浮点采样按 Unity 与 AzureRender 的坐标关系换算为 1024 × 512 线性 HDR 环境图。编码最大绝对误差为 0.00701。任务 1794 在同一环境光条件下，从同一机位各采两帧，分别选择纯色和原采样天空。两条路线均隐藏室外大气占位网格与城市景物。左侧可见天空区域的 11,204 个像素发生变化，椅背对照区的 7,000 个像素逐一相同。两次运行均确认 GPU 图像和缓冲区退出后归零。

环境图 SHA-256 为 `5e0e2c4e448495ff00932d17fa8db7d0064abe65e09433781f4f36e9b90f4c83`。本机 `windows-atmosphere-reference/sunset-vulkan-transfer.json` 记录两张原生画面和实际 Player 的摘要。房间合成画面的原版对照、实时天空切换及其他时段仍按完整窗景矩阵验收。

## 白天天空通道

采样工具按时段的原始时钟和整组颜色绑定选择对应的桌面绘制记录。白天记录的时钟为 12:30，运行时间为 7.253 秒。夜晚和多云采样需要补齐对应的桌面绘制记录。

任务 1852 使用同一天空网格、原 Direct3D 11 程序和纹理，执行两个独立进程。六个方向的 393,216 个 RGBA32F 像素逐字节相同，线性 RGB 最大值为 10.95267。纹理输入使用无损压缩，解码内容逐一核对摘要。任务 1853 使用同一解码路径复验傍晚，输出摘要与傍晚记录相同。

任务 1854 在 1280×720 的 Vulkan 房间进行对照。白天环境图使可见窗区 11,352 个抽样位置中的 11,205 个发生变化，椅背对照区的 7,000 个像素逐一相同。两个 Player 正常退出后，缓冲区和图像的计数均为零。

| 记录 | SHA-256 |
| --- | --- |
| 白天原始浮点采样 | `7800ad27038b8d836d5a2050dd2c673e9034df90dd9f85527e820e86b091f096` |
| 白天 HDR 环境图 | `d9cc56a9fd8bf9d6c01622afd7cb8efe29a978abf85faa5d659e328c267d231d` |
| 白天 Vulkan 对照报告 | `c5c81408136474ebc8d6de90069baa73a7da38c32513768d5cb0889de9f21770` |

白天的采样输入、环境图、构建记录和对照画面位于本机 `windows-atmosphere-reference/day-validation/`。天空通道验证使用独立背景，云层、雾和不同天气的合成使用窗景矩阵。

## 复验

原 DXBC、反汇编和绑定清单写入本机私有目录。`PYTHONPATH` 指向本项目的 UnityPy 转换工具目录。

```powershell
python -B -m tools.npr_room.unity_d3d11_programs --source $resourcesAssets --manifest $originalShaderInventory --output $programFolder
python -B -m tools.npr_room.windows_atmosphere --source $unitySource --catalog $assetCatalogRoot --captures $windowMatrixIsolated --d3d-programs "$programFolder/windows-atmosphere-programs.json" --output $atmosphereReceipt
python -B -m unittest tools.npr_room.test_unity_d3d11_programs
```

在 Visual Studio x64 开发者命令行编译和运行天空采样工具。`$zlibRoot` 指向包含 `include/zlib.h`、`lib/z.lib` 和 `bin/z.dll` 的依赖目录。`$skyMode` 选择 `Day` 或 `Sunset`，源记录使用 Recovery `ReferenceProbe/reference-sky.log`。压缩纹理和浮点数据写入本机工作目录。

```powershell
$env:PATH = "$zlibRoot/bin;" + $env:PATH
cl.exe /nologo /EHsc /std:c++20 /O2 /W4 "/I$zlibRoot/include" tools/npr_room/WindowsSkyProbe.cpp d3d11.lib "$zlibRoot/lib/z.lib" "/Fe:$skyProbe"
python -B -m tools.npr_room.windows_sky_probe --source $unitySource --catalog $assetCatalogRoot --references $atmosphereReceipt --programs $programFolder --original-log $originalSkyLog --mode $skyMode --output $preparedProbe
& $skyProbe $preparedProbe $skyCube
python -B -m unittest tools.npr_room.test_windows_sky_probe
```

```powershell
python -B -m tools.npr_room.windows_sky_environment --capture $packedSkyCube --reference $skyProbeReceipt --output $skyHdr
python -B tools/npr_room/test_windows_sky_environment_player.py --player $player --room $room --resource-root $resources --environment $skyHdr --references $atmosphereReceipt --output $vulkanReplay
```
