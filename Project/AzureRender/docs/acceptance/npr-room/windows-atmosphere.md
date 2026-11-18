# Windows 窗景大气来源

> 状态为 P5 局部验收。四个时段均有原 Direct3D 11 天空通道采样和 Vulkan 房间对照。云层、雾、室内光照和完整窗景按各自的画面条件验收。

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

环境图 SHA-256 为 `5e0e2c4e448495ff00932d17fa8db7d0064abe65e09433781f4f36e9b90f4c83`。本机 `windows-atmosphere-reference/sunset-vulkan-transfer.json` 记录两张原生画面和实际 Player 的摘要。房间合成画面的原版对照和实时天空切换按完整窗景矩阵验收。

## 白天天空通道

采样工具按时段的原始时钟和整组颜色绑定选择对应的桌面绘制记录。白天记录的时钟为 12:30，运行时间为 7.253 秒。

任务 1852 使用同一天空网格、原 Direct3D 11 程序和纹理，执行两个独立进程。六个方向的 393,216 个 RGBA32F 像素逐字节相同，线性 RGB 最大值为 10.95267。纹理输入使用无损压缩，解码内容逐一核对摘要。任务 1853 使用同一解码路径复验傍晚，输出摘要与傍晚记录相同。

任务 1854 在 1280×720 的 Vulkan 房间进行对照。白天环境图使可见窗区 11,352 个抽样位置中的 11,205 个发生变化，椅背对照区的 7,000 个像素逐一相同。两个 Player 正常退出后，缓冲区和图像的计数均为零。

| 记录 | SHA-256 |
| --- | --- |
| 白天原始浮点采样 | `7800ad27038b8d836d5a2050dd2c673e9034df90dd9f85527e820e86b091f096` |
| 白天 HDR 环境图 | `d9cc56a9fd8bf9d6c01622afd7cb8efe29a978abf85faa5d659e328c267d231d` |
| 白天 Vulkan 对照报告 | `c5c81408136474ebc8d6de90069baa73a7da38c32513768d5cb0889de9f21770` |

白天的采样输入、环境图、构建记录和对照画面位于本机 `windows-atmosphere-reference/day-validation/`。天空通道验证使用独立背景，云层、雾和不同天气的合成使用窗景矩阵。

## 夜晚与多云天空通道

夜晚和多云使用 Windows 1.17.3 原版顶点与片元程序、天空网格和贴图执行受控离屏采样。颜色、月亮方向与标量取自已有大气快照。太阳方向使用各时段时间模块的修正日比例和 `CozyWeather` 的父子旋转计算。原程序集登记遮挡方向的着色器标识，三份可匹配白天和傍晚的桌面日志均记录零向量。

白天与傍晚先按各自实录重建输入。任务 2071 和 2072 的颜色及方向分量与桌面日志相差至多 0.000002，六面天空与实录输入对应采样的线性 RGB 最大误差分别为 0.00000191 和 0.00000132。

夜晚的原场景时钟为 21:49，采样固定时间为 9.302 秒。多云的原场景时钟为 12:30，采样固定时间为 7.253 秒。任务 2073 和 2074 各启动两个独立进程，每种时段的 393,216 个 RGBA32F 像素均逐字节一致，所有分量有限且透明度均为 1。完整房间的动态实录对照仍使用对应天气与镜头的运行证据。

任务 2075 至 2078 在 RTX 4060 和 Intel 核显的 1280×720 Vulkan 房间验证环境图。夜晚和多云在 RTX 4060 上分别改变左窗抽样区的 11,201 和 11,206 个像素，在 Intel 核显上分别改变 11,202 和 11,205 个像素。各组椅背对照区的 7,000 个像素逐一相同。八个进程通过 Vulkan 同步校验并正常释放图像和缓冲区。

| 记录 | 夜晚 SHA-256 | 多云 SHA-256 |
| --- | --- | --- |
| 原程序浮点采样 | `fa84449c2f77b4279953d2bd9a0f14d79e916f92afcfea0d05743e64312454dc` | `228815e2439172f9ed69b17b62e5e55467bbec434331dfdc45007a697c5101f7` |
| HDR 环境图 | `e9fdf63b3235cba045bebc7b0fa0c3d0adbf1640a177808442fed0ad7cf1248a` | `570793e72a6ec220a98c71584c9b0be942eff75f46d3816ba36ef39e660c70ad` |
| RTX 4060 房间对照 | `8a7f8e063ad887f8aedd604de708a0100a8646fac4ed05b25484940a0e46ae30` | `8eee3b0cda74464a3edfabc623745a3dd39fd42bb2dc56dea0f219444df9f13c` |
| Intel 核显房间对照 | `ecccf44228c9274df39f4f8045536f322626bf404c6f4605ac4d3f5562882bbd` | `08cbb19f60130195ff511dc02f2214b5df224d3d9ba36f202f14b23607dc0b7a` |

四个时段的受控采样输入、浮点归档、环境图及房间对照位于本机 `windows-atmosphere-reference/night-cloudy-validation/`。

## 原版云层与雾的受控采样

任务 2080 至 2083 在同一 Direct3D 11 硬件设备执行桌面云层和雾的原版首通道。每个时段和通道按两个相隔 0.5 秒的时刻采样。每个样本包含六个方向共 98,304 个 RGBA32F 像素，两次独立进程的结果逐字节一致。云层使用原网格、四张原贴图及固定单位风向，雾使用原网格、原噪声贴图及六档反向深度。受控输入的视点位于天空球中心。

夜晚云层的合成贡献在两个时刻之间改变 3,638 个像素。多云的云层在这组时间与风向下保持全覆盖，两帧相同。雾的合成贡献分别改变 57,555 和 52,954 个像素。首个时刻的中央采样在深度 1 至 150 米的范围内，夜晚透明混合系数从 0.0221 升至 0.3797，多云从 0.0581 升至 0.6897。原版白天天空回归的浮点输出摘要与归档完全一致。

| 通道 | 夜晚报告 SHA-256 | 多云报告 SHA-256 |
| --- | --- | --- |
| 云层 | `e287089c87bb939c40a80b86deb25af54fa079b03186f8dba24d4e53da9eb9ae` | `b036568921d6dd86f4bc9a146d428a59f5400126a28c4f224301fee9644c11ab` |
| 雾 | `ccf6bcdd8010ca7fa8f6af0a541e1f122d3a27780b1bc5b66eba47499ee60bd8` | `b4fc4129e25e861827898292ac05e5f55cea27a1155f7ca34e647a2f89680a08` |

本机 `windows-atmosphere-reference/weather-pass-validation/` 保存浮点采样、常量缓冲区、命令和校验。完整房间的动态对照需要对应镜头、时间、窗景状态与逐帧深度。

## 固定镜头房间合成

天空与云层按相同天气和时刻配对。云层的线性 RGB 先乘透明混合系数，再插值并叠加到天空，输出 1024×512 HDR 环境图。雾的原程序按固定房间相机、16:9 视口和 24 档对数深度输出颜色与混合系数，再按原混合公式换算为透射与散射。体积为 128×72×24 的 RGBA16F 数据，四组数据的量化最大绝对误差均为 0.00024414。

两种显卡在同一房间、相机与固定模拟帧分别加载两个天气时刻。启用雾效的场景均通过捕获相机匹配。重复载入同一时刻的资源时，整帧逐像素一致，计时器不透明区域在所有对照中保持原色。多云的天空与云层环境图保持一致。

| 任务 | 显卡 | 天气 | 雾效开关变化像素 | 两时刻变化像素 |
| --- | --- | --- | --- | --- |
| 2092 | RTX 4060 | 夜晚 | 800,279 | 260,237 |
| 2093 | RTX 4060 | 多云 | 806,356 | 110,260 |
| 2094 | Intel Raptor Lake-S 核显 | 夜晚 | 801,013 | 268,716 |
| 2095 | Intel Raptor Lake-S 核显 | 多云 | 806,362 | 112,202 |

现有发行版截图和 Recovery 截图分别登记单帧及对应天气。角色服装、桌面道具和界面状态随采集环境变化。完整原版房间的逐帧对照需要同一镜头、角色与道具状态、天气时刻和相机深度。固定镜头合成使用原版天气通道和移植版房间画面，雾体积匹配该相机并作用于有几何深度的表面。

本机 `windows-atmosphere-reference/weather-pass-validation/` 提供源通道、雾体积、房间帧和原版截图清单。采样、构建与回放记录的摘要登记在 `room-weather-verification.json`。

## 复验

原 DXBC、反汇编和绑定清单写入本机私有目录。`PYTHONPATH` 指向本项目的 UnityPy 转换工具目录。

云层和雾使用 `tools.npr_room.windows_weather_probe` 准备输入。两类采样分别使用 `--role cozy-cloud` 和 `--role fog`，采样时间由 `--time` 指定。云层另指定 `--wind 0.6 0 0.8`。运行采样程序时在输出路径之后传入对应的通道名称。

固定镜头对照使用 `tools.npr_room.windows_weather_environment` 合成天空和云层。本机存档的 `capture-room-fog-volumes.py` 调用 `tools.npr_room.windows_room_fog` 准备固定相机的输入和体积，采样程序的通道参数为 `fog-volume`。使用 `tools.npr_room.test_windows_weather_room_player` 回放同一时刻的雾效开关、资源重载及两个时刻的合成。

```powershell
python -B -m tools.npr_room.unity_d3d11_programs --source $resourcesAssets --manifest $originalShaderInventory --output $programFolder
python -B -m tools.npr_room.windows_atmosphere --source $unitySource --catalog $assetCatalogRoot --captures $windowMatrixIsolated --d3d-programs "$programFolder/windows-atmosphere-programs.json" --output $atmosphereReceipt
python -B -m unittest tools.npr_room.test_unity_d3d11_programs
```

在 Visual Studio x64 开发者命令行编译和运行天空采样工具。`$zlibRoot` 指向包含 `include/zlib.h`、`lib/z.lib` 和 `bin/z.dll` 的依赖目录。白天和傍晚读取 Recovery `ReferenceProbe/reference-sky.log`。夜晚和多云分别指定 `Night`、`Cloudy` 以及对应的固定采样时间。`$cozyAssembly` 指向 Recovery `UnityProject/AuxiliaryFiles/GameAssemblies/DistantLands.Cozy.Runtime.dll`。压缩纹理和浮点数据写入本机工作目录。

```powershell
$env:PATH = "$zlibRoot/bin;" + $env:PATH
cl.exe /nologo /EHsc /std:c++20 /O2 /W4 "/I$zlibRoot/include" tools/npr_room/WindowsSkyProbe.cpp d3d11.lib "$zlibRoot/lib/z.lib" "/Fe:$skyProbe"
python -B -m tools.npr_room.windows_sky_probe --source $unitySource --catalog $assetCatalogRoot --references $atmosphereReceipt --programs $programFolder --original-log $originalSkyLog --mode $skyMode --output $preparedProbe
& $skyProbe $preparedProbe $skyCube
python -B -m tools.npr_room.windows_sky_probe --source $unitySource --catalog $assetCatalogRoot --references $atmosphereReceipt --programs $programFolder --replay-time $sampleSeconds --cozy-assembly $cozyAssembly --mode $skyMode --output $controlledProbe
& $skyProbe $controlledProbe $firstSkyCube
& $skyProbe $controlledProbe $secondSkyCube
Get-FileHash -LiteralPath $firstSkyCube,$secondSkyCube
python -B -m unittest tools.npr_room.test_windows_sky_probe
```

```powershell
python -B -m tools.npr_room.windows_sky_environment --capture $packedSkyCube --reference $skyProbeReceipt --output $skyHdr
python -B tools/npr_room/test_windows_sky_environment_player.py --player $player --room $room --resource-root $resources --environment $skyHdr --references $atmosphereReceipt --output $vulkanReplay
```
