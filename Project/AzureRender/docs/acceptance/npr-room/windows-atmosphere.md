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

### 本机安装版参照

本机安装版的游戏版本为 1.18.1。两份 Windows 构建使用 Unity `2022.3.62f2)，`BuildSettings` 的五场景顺序一致。五个大气着色器的对象编号与原始字节均匹配 1.17.3 的 Direct3D 归档，可用于对应着色通道的身份核验。

两份构建的 `RoomScene` 均对应 `level3`，场景字节和应用程序集摘要各自登记。完整房间对照采用冻结版 1.17.3 的相机、帧色和深度，按同一帧配对。

本机 `windows-atmosphere-reference/installed-weather/installed-weather.json` 两次生成一致，SHA-256 为 `1752f6f2ce5a055f73eca0427c1fb0535414e5011a15bcbc921ba8585f1179e7`。

```powershell
python -B -m tools.npr_room.windows_weather_source_identity --reference $referencePlayerData --installed $installedPlayerData --program-receipt $atmospherePrograms --output $installedWeatherReport
```

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

固定镜头合成使用原版天气通道和移植版房间画面。雾体积匹配该相机并作用于有几何深度的表面。原版同步帧按下节记录采集。候选配准使用对应镜头、角色、道具与天气状态。

本机 `windows-atmosphere-reference/weather-pass-validation/` 提供源通道、雾体积、房间帧和原版截图清单。采样、构建与回放记录的摘要登记在 `room-weather-verification.json`。

## 雾效相机配准

`prepare_room` 可通过 `original_frame=True` 读取原版同步帧。
相机位置、朝向、视场角与裁剪范围来自同一份记录。
转换按世界 Z 轴镜像，核对视图与投影矩阵。
深度切片和体积绑定使用该相机的远裁剪面。

24 帧相机矩阵的最大转换误差为 0.000000125。
原版近裁剪面为 0.05，远裁剪面为 1000。
默认六面投影和默认深度切片与基线逐字节一致。
119 项房间检查通过。

任务 2162 使用原版 Direct3D 雾程序生成两份体积。
夜晚和多云均通过独立重复采样的字节一致检查。
半精度体积最大编码误差为 0.000244141。
两份体积各有 221,184 个非零雾效采样。

此批验证原版相机与受控天气回放的组合。
逐帧天气全局值和匹配 Player 的整帧画面继续验收。
本机证据位于 `original-room-dynamics/fog-camera/`。
报告 SHA-256 为 `3f253453eb0bdc6a04945df761ea74fb98090a89e6dca9b09cc3e1908f0fdbb1`。

## 原版房间同步帧

任务 2134 使用冻结的 Windows 1.17.3 参照程序。
夜晚与多云各采集 12 帧，分辨率为 1280×720。
固定模拟步为 1/60 秒，采样间隔为四帧。
每组首尾相隔 0.7333 秒。

每份颜色图与主相机深度属于同一渲染帧。
深度以小端 `R32_SFloat` 保存。
元数据包含相机矩阵、天气时钟和 38 个着色器全局值。
同时记录物件变换、九个蒙皮、骨骼路径与表情权重。

两种天气的相机位置和旋转均保持稳定。
夜晚首帧含 849 个变换，多云含 853 个。
变换记录包含完整父节点闭包。
各组均观测到颜色和几何深度的动态变化。

| 天气 | 颜色变化像素最大值 | 深度变化像素最大值 |
| --- | ---: | ---: |
| 夜晚 | 326,843 | 258,926 |
| 多云 | 285,530 | 249,605 |

诊断副本使用独立产品名和隔离存档目录。
诊断期间关闭成就统计写入，并启用主相机深度纹理。
原版 317 个输入文件和 37 个存档输入的摘要均一致。
此采集属于画面参照，Steam 实机验收按计划暂缓。

角色使用采集档的眼镜、猫耳耳机及杯子。
候选须按相同状态配准后核算画面误差。
静态区域 SSIM ≥0.95，关键区域亮度中位误差 ≤10%。
候选的整帧配准与正式回归仍在待验范围。

原始帧与来源收据位于本机私有目录
`windows-atmosphere-reference/original-room-dynamics/`。
两次校验及目录移动后的校验得到相同报告。
报告 SHA-256 为 `dc003ce6d26601aa1ff219b367647b378eec010ea22c0b51dda577809ac32ead`。

`OriginalRoomFrames.cs` 编译为诊断程序集。
它复用冻结参照的天气更新钩子和实际天气切换入口。
采集命令通过 `-azure-frame-output` 指定全新输出目录。
运行配方与受控副本的补丁收据保存在上述私有目录。

```powershell
python -B -m tools.npr_room.original_room_frames --frames $originalRoomFrames --output $frameVerification
```

## 同帧灯光与材质状态

任务 2144 对夜晚和多云各采集 12 帧。
每帧包含五盏灯光，以及颜色图、深度和姿态。
夜晚记录 263 个渲染器和 266 个材质状态。
多云记录 264 个渲染器和 267 个材质状态。

灯光记录颜色、强度、范围、锥角、阴影和裁剪掩码。
材质记录实际着色器、绘制队列及选定的颜色和标量。
纹理记录名称、缩放和偏移，并消费属性块中的覆盖值。
环境记录线性色彩空间、三色环境光及反射强度。

37 个天气标量按当前着色器全局值保存。
主相机、物件与渲染器的实例身份在同帧关联。
候选的灯光、材质与天气输入继续按这些状态配准。
完整画面的误差门槛保持在对应验收项中。

原版 317 个输入文件和 37 个存档输入的摘要均一致。
两次校验以及目录移动后的校验得到相同报告。
既有原版帧的校验报告也通过兼容复验。
采集期间的存档隔离与深度设置沿用上述诊断规则。

本机目录为 `original-room-dynamics/lighting-materials/`。
报告 SHA-256 为 `d1aa9c820799ccdeac889b20a668834c9ae355365bf28f570292aea35b1ba8e3`。

```powershell
python -B -m tools.npr_room.original_room_frames --frames $originalRenderFrames --require-render-state --output $renderStateVerification
```

## 原版天气全局值

天气参数诊断按夜晚、多云各采集三帧。
采样间隔为四帧，每组跨度为 0.1333 秒。
每帧保存同帧颜色、深度、40 个标量和六个向量。
记录包含雾深度倍率、月亮方向、风向及原渲染时钟。

任务 2167 使用隔离的冻结版程序完成采集。
云层与雾片元程序的全局输入名通过覆盖核对。
渲染时钟与记录的模拟时间逐帧一致。
122 项房间检查及目录移动校验通过。

317 个原程序文件及 37 个原存档文件摘要一致。
完整动态参照的每种天气十二帧报告通过字节核对。
三帧诊断用于参数接入，完整画面按十二帧口径验收。
实际消费与同帧画面继续验证。

本机证据位于 `original-room-dynamics/weather-globals/`。
报告 SHA-256 为 `281624c7b080f5a3f4bbc7f3409c03cd286264aa3fe61e7c4b41e1b78f3ee2d2`。

```powershell
python -B -m tools.npr_room.original_room_frames --frames $weatherDiagnosticFrames --samples-per-weather 3 --require-weather-globals --require-ambient-probe --output $weatherGlobalsVerification
```

## 原版环境光探针

任务 2147 在两种天气各采集 12 个同步帧。
每帧保存 RGB 三组共 27 个球谐系数。
Unity 同时计算 26 个单位方向的原版探针响应。
光色强度与色温策略也按当前图形设置保存。

三色环境光的方向响应包含二阶分量。
以下误差来自各天气 12 帧的 26 方向实算。
误差为线性 RGB 分量的最大绝对差。

| 天气 | 线性四项拟合误差 | 二阶九项拟合误差 |
| --- | ---: | ---: |
| 夜晚 | 0.0634984 | 0.0000000258 |
| 多云 | 0.498717 | 0.0000000659 |

候选继续接入二阶探针的方向响应。
原版图像、深度、姿态、灯光与探针保持同帧关联。
原输入和存档输入的摘要均通过核验。
两次报告与目录移动后的校验结果一致。

本机目录为 `original-room-dynamics/ambient-probe/`。
报告 SHA-256 为 `9daa311f19ad7c88b8e34e95fe35e372ebeb4d4e8f3130a564a8ccd9f1f573da`。

```powershell
python -B -m tools.npr_room.original_room_frames --frames $originalProbeFrames --require-ambient-probe --output $ambientProbeVerification
```

`original_ambient_probe` 将方向实算写为二阶辐照网格。
转换按世界 Z 轴镜像，并保存原帧与资源摘要。
任务 2153 使用项目的 C++17 配置加载 24 份原版网格。
624 个方向的 CPU 求值最大误差为 0.000000239。

通用二阶场、边界、有效权重及非法输入通过契约检查。
线性示例的编码与基线逐字节一致。
三层辐照资源的 CPU 回归通过。
场景图形入口按文件版本上传对应系数布局。

本机 CPU 证据位于 `ambient-probe/quadratic-cpu/`。

任务 2157 在 RTX 4060 与 Intel 核显上运行生产求值程序。
每台设备覆盖 26 份网格，共 672 次查询。
其中 24 份原版网格包含 624 个方向响应。
原版响应的最大误差为 0.000000239。

两种格式、边界、权重和最终钳位通过 GPU 对照。
同步验证、全部缓冲与图像释放检查通过。
任务 2158 通过场景入口的 C++17 严格编译。
任务 2159 编译通过 644 个受影响的生产片元分支。

任务 2160 核对两版上传字节与资源加载分派。
110 项房间 Python 检查及 Lua 运行夹具通过。
匹配 Player 的完整房间画面等待受管构建配额。
正式回归与 Steam 实机验收按当前安排延后。

本机 GPU 证据位于 `ambient-probe/quadratic-gpu/`。
报告 SHA-256 为 `d61cddf63a1d24186c3b6badc738bafcd57a89a3f64587d3dcd0e627579298b7`。

```powershell
python -B -m tools.npr_room.original_ambient_probe --frame $originalProbeFrame --output $quadraticGrid
```

配准命令的 `--ambient-probe` 使用同帧原版探针。
候选生成二阶资源、稳定资产标识及关卡绑定。
资源与元数据在暂存目录核验后共同发布。
原版帧和输入工程保留各自摘要。

独立小工程经正常命令入口转换 24 帧参照。
全部资源与已验证的 CPU、GPU 输入逐字节一致。
24 个相对资源路径通过目录移动检查。
116 项房间检查通过，完整房间画面继续验收。

本机绑定证据位于 `ambient-probe/binding/`。
报告 SHA-256 为 `67bdef10918450d041fe487d93a778d46da31c2250c4a7997ce8d13af031de6d`。

```powershell
python -B -m tools.npr_room.original_room_registration --room $room --frame $originalProbeFrame --source $unitySource --ambient-probe --output $registeredRoom
```

## 同步姿态配准

`original_room_registration` 按完整父子路径绑定角色。
七个部件均校验原蒙皮的骨骼顺序与表情数量。
本机工具在隔离项目冻结采样姿态，保存来源摘要。
角色轨迹、房间脚本与界面在该诊断项目中关闭。

配准入口分别保存角色模型与原版工程的路径。
省略原版工程参数时，仅执行几何姿态配准。
指定原版工程后，装配显隐、镜片及原版灯光。
输出发布前完成全部步骤，失败时清理暂存目录。

入口夹具验证两种参数方式与原输入摘要。
已有输出保护及失败收尾检查通过。
本机证据位于 `original-room-dynamics/registration-entry/`。
113 项房间检查通过，报告摘要为 `dd2eb098feadfd6fea36d5c0c8cf4f993d4eef7aa917fa448eb7e94ad2f63a27`。

24 帧共核验 6,864 个角色局部变换。
位移最大误差为零，旋转最大误差为 0.0000000024°。
房间与椅子的 2,760 个匹配变换也通过旋转复核。
直角旋转使用保持同一旋转矩阵的欧拉角分解。

夜晚与多云首帧分别绑定 112 和 118 个房间变换。
其余节点的路径差异登记于配准收据。
后续对照继续补齐物件显隐、合批身份和材质状态。
整帧画质误差以这些状态配齐后的候选计算。

任务 2137 在 RTX 4060 上分别载入两种首帧姿态。
两个独立 Player 的相机位置、视角和裁剪范围均匹配。
两次运行通过 Vulkan 同步校验，图像和缓冲归零。
本机证据位于 `original-room-dynamics/registration/`。

```powershell
python -B -m tools.npr_room.original_room_registration --room $room --frame $originalFrame --source $unitySource --output $registeredRoom
python -B -m unittest tools.npr_room.test_original_room_registration
```

## 原版灯光输入

配准工具消费原版逐帧灯光和完整变换层级。
位置按父级缩放合成，方向按父级旋转合成。
光色按冻结图形设置执行线性强度转换。
原版程序和恢复工程的该设置均已核对。

夜晚和多云各装配一盏方向光、一盏聚光灯及三盏点光源。
强度、半径、世界位置、方向与聚光锥角来自同步帧。
配准报告记录灯光来源和图形设置摘要。
输入非法时，配准工具保留当前关卡状态。

任务 2146 在 RTX 4060 上执行四个独立 Player 对照。
两个天气的原版输入均进入五灯的簇光照路径。
相机位置、方向和投影与各自控制帧逐项一致。
四个进程通过同步校验，并释放全部图像和缓冲。

夜晚与多云分别改变 672,022 和 666,758 个像素。
此对照验证直接灯光输入的实际消费。
三色环境光、原版硬软阴影和材质照明响应继续验收。
本机证据位于 `original-room-dynamics/direct-lighting/`。

## 原版物件显隐

配准工具从原场景文件编号恢复合批网格的完整路径。
路径与原版同步帧的活动物件记录共同决定显隐。
同名物件按完整世界变换核验身份。
身份存在歧义时，工具保留失败诊断。

夜晚与多云分别核对 202 个房间绘制节点。
原版活动节点分别为 177 和 180 个。
隔离候选隐藏对应的 25 和 22 个非活动节点。
七个角色部件继续使用已核验的姿态配准。

任务 2138 在两种天气各启动一个独立 Player。
相对同姿态的显隐对照，变化像素分别为 212,092 和 168,753。
椅背控制区的 1,200 个像素逐一相同。
两次运行通过同步校验，并释放全部图像与缓冲。

玻璃与转场节点按当前材质覆盖条件保存显隐。
天气背景、角色附件和材质细节继续按原版补齐。
显隐结果属于配准步骤，整帧画质门槛继续验收。
本机报告位于 `original-room-dynamics/activation/`。

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
