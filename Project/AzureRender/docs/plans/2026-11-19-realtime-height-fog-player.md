# 实时高度雾 Player 接入清单

生成器基线为 `f3b364e`，双设备证据来自队列 2231。
当前交付为接入清单，场景绑定及 Player 定向验收待执行。
正式回归和 Steam 实机验收维持暂缓。

## 接入顺序

1. 在 `ViewFogBinding` 增加版本三的显式生成配置。
   `generatedHeightVolume` 持有 `dimensions` 和 `farViewDepth`。
   生成模式要求 `analytic.height` 及空捕获资源和机位。
   解析、捕获以及关闭的绑定继续使用各自的版本契约。

2. 在 `ViewFogGpu::State` 为生成模式持有 `GeneratedHeightFogGpu`。
   沿用 `CharacterSceneRenderer::initializeLoad` 的缓冲准备阶段。
   生成器接收全部帧参数缓冲和同构建的计算程序。
   `writeDescriptors` 将片元绑定 35 指向当前槽位的三维纹理。

3. 在 `ViewFogGpu::updateFrame` 写入当前相机及视口。
   使用参数第 13 行的第三分量标记生成模式。
   第四分量写最远深度，沿用计算程序的读取位置。
   保留 `PackedViewFogFrame` 的 336 字节布局。

4. 在 `CharacterSceneRenderer::registerPasses` 登记生成。
   传入现有主机写入参数资源和当前槽位。
   主绘制声明对生成图像的 `Sampled` 读取。
   核对直接记录入口 `recordScene` 的写入及采样顺序。

5. 在 `view_fog.glsl` 复用 `view_fog_medium.glsl`。
   使用当前屏幕坐标和对数深度定位生成体积。
   在雾起点和最远深度对齐首尾深度体素中心。
   体积外使用解析高度雾，大气仍按片元求值。

6. 将生成配置纳入完整绑定的保存和资源身份。
   覆盖 `Level::load`、场景文档及关卡重载。
   在采集清单记录生成状态、尺寸和最远深度。
   诊断区分捕获匹配、解析介质和生成体积。

配置校验使用生成器的轴长和在途槽位上界。
横纵轴最长 64，深度为 2 至 128，槽位数最多 8。
最远深度必须大于高度雾起始深度。
核对设备支持线性读取及存储写入 RGBA16F。

绑定失败按关卡资源准备失败处理并保留已提交关卡。

生成体积按屏幕宽高归一化横纵坐标。
深度采样使用与生成端互逆的映射。
映射中 `d` 为当前深度，`s` 为起始深度，`f` 为最远深度。
应用公式时 `n` 为体积深度尺寸。

```text
t = log2(1 + d - s) / log2(1 + f - s)
z = (t * (n - 1) + 0.5) / n
```

## 槽位、同步与释放

| 边界 | 接入步骤 |
| --- | --- |
| 槽位复用 | `AzureRenderFrame.cpp` 等待当前帧栅栏后更新参数。 |
| 参数写入 | 写当前槽位的 336 字节缓冲并刷新主机写入。 |
| 体积生成 | `GeneratedHeightFogGpu::addPass` 读取参数并覆盖整个槽位。 |
| 图像发布 | 渲染图从 `GENERAL` 转为 `SHADER_READ_ONLY_OPTIMAL`。 |
| 片元读取 | 使用计算写入到片元采样读取的阶段及访问依赖。 |
| 关卡切换 | 通过 `AzureRenderGameplay.cpp` 的准备及提交边界切换资源所有者。 |
| 释放 | 等待最后一笔使用完成，依次释放生成器、参数及捕获资源。 |

每个在途槽位持有独立的图像、图像视图和生成描述符。
计算和片元描述符引用同一槽位。
直接记录路径使用同样的主机写入和图像发布顺序。
卸载核对活动图像和缓冲均为零。

定向夹具使用 48 × 32 × 48 和最远深度 8。
近端与远端超出体积范围的深度使用解析参考值。
验证时保持同一高度介质和镜头参数。

## 冻结对照与连续回放

| 对照 | 冻结输入与证据 | 核验范围 |
| --- | --- | --- |
| GPU 生成 | `20261119-142326-realtime-height-fog-647ebb/verification.json` | 两块 GPU 各 180 个体素查询和槽位交换。 |
| 移动相机 | `20261119-025202-npr-original-room-registration-04ae4a/full-fog-fixture/` | 同一关卡十帧，含相机离开及返回。 |
| 双机位关卡 | `20261119-025202-npr-original-room-registration-04ae4a/fog-baked-height-route/` | 两个离线高度雾体积及 72 帧往返。 |

冻结目录均位于主工程的 `build/runs`。
复用时登记输入引用并核对源文件摘要。
候选回放在受管目录生成同结构的原创夹具。
回放所需几何为 `surface.gltf` 和 `rear.gltf`。

移动相机沿用 `test_scene_fog_gpu.py` 的位移数组。
复用 `camera.lua`、默认空动作表及同一光照设置。
连续回放采用 128 × 128、60 FPS 和十帧固定步。
轨迹包含中间位移及两端静止帧。

关卡往返沿用 `route.azureproject`、`actions.json` 和两个关卡。
用例使用 `viewFog` 版本三和同一高度介质配置。
首尾帧使用同一生成绑定的起始机位对照图。
依据 `routeFrames` 的关卡提交状态逐帧选择对照图。

为每个机位另采一帧相同生成配置的独立画面。
与对应独立画面的像素应一致。
生成画面与解析参考值的比较使用线性误差门槛。

冻结报告摘要如下。

| 文件 | SHA-256 |
| --- | --- |
| `verification.json` | `65f42cdb798e788347f5abddf8c2acfe9c1c94114a262f9f11b66d1ddac835cc` |
| `full-fog-fixture/report.json` | `954ae8769f073101612e47e524ea34a8175caf2832c4a7ec1421072bf8df177c` |
| `fog-baked-height-route/sequence-verification.json` | `53d1e459f0b6aa69b110cfad481f425d7d617f7767b24729d53738869d20aa62` |
| `fog-baked-height-route/route.azureproject` | `a56b037482de3043e38a7fcea63b6a07e282613e160b92ef850ccf5d4d71b711` |
| `fog-baked-height-route/actions.json` | `063071fff57eedf6aa51fdf6e6e6f75d0eb094a33dfdbc7d9f520c9828932c93` |
| `fog-baked-height-route/assets/base.azfog` | `d83b09d8a25af39a2e5084f327d75024850bd0943dbd0e9820d391d2fa955aad` |
| `fog-baked-height-route/assets/moved.azfog` | `22f2b772c39b7939a36cce208744f1b9348f4e6dc83c35bf86bf4cc9af0fb01e` |

## 构建范围与实际占用

`AzurePlayer` 是连续画面的最小生产目标。
它通过 `AzurePlayerHost` 消费运行时与渲染库。
着色器由该目标现有的 `AzureRenderShaders` 依赖构建。
`AzureGeneratedHeightFogGpuTests` 可独立验证生成和同步。

现有 Release Player 可复放冻结对照。
当前源码的 `AzurePlayer` 目标用于接入后的候选。
既有安装依赖可由 `VCPKG_INSTALLED_DIR` 只读复用。
工作树内当前没有匹配的 Player 构建目录。

生成器已测的对象文件和计算程序合计 177065 字节。
连同独立测试对象、程序和采样着色器，共 1618544 字节。
该值来自封存的 Release 定向短检。
它对应生成单元的实际产物增量。

主 Release 的 `AzurePlayer.exe` 图有 940378356 字节现存产物。
统计范围为目标图中的对象、库、程序和着色器。
候选还需配置、链接临时量及验证输出。
独立候选保留 1.5 GiB 预约，实际峰值在构建时登记。

## 当前配额复核

2026-11-19 的只读检查显示主池余 642915483 字节。
1.5 GiB 预约还差 967697253 字节。
房间计时设置任务于 11 月 20 日 01:18:08 达到保留期。
该任务的登记占用为 1033570663 字节。

维护按到期时的引用、标记和证据摘要判定回收资格。
按快照推算，合格释放后主池余 1676486146 字节。
开跑以实时 `artifact_budget.py` 结果和任务原子预约为准。

## 最小命令

在 Visual Studio 开发者 PowerShell 中运行。
将 `$engine` 设为接入工作树的引擎根目录。
下列命令创建持有构建和采集输出的受管目录。

```powershell
$main = 'D:/Projects/FYP/Project/AzureRender'
$engine = 'D:/Projects/FYP-worktrees/npr-room-reproduction/Project/AzureRender'
$queue = 'D:/Projects/FYP-coordination/tools/dispatch.py'
$toolsDir = 'D:/Tools/VS2022BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake'
$cmake = "$toolsDir/CMake/bin/cmake.exe"
$ctest = "$toolsDir/CMake/bin/ctest.exe"
$ninja = "$toolsDir/Ninja/ninja.exe"
$referenceRun = '20261119-025202-npr-original-room-registration-04ae4a'
$generatorRun = '20261119-142326-realtime-height-fog-647ebb'
python -B "$main/tools/artifact_budget.py" --build "$main/build" --planned-bytes 1610612736
if ($LASTEXITCODE -ne 0) { throw 'Player reservation is unavailable' }
$task = python -B "$main/tools/artifact_cleanup.py" --new-run realtime-height-fog-player --planned-bytes 1610612736 --input-run $referenceRun --input-run $generatorRun
if ($LASTEXITCODE -ne 0) { throw 'Player reservation is unavailable' }
$runName = Split-Path $task -Leaf
$build = Join-Path $task 'player-release'
$deps = Join-Path $main 'build/ninja-msvc-debug/vcpkg_installed'
$toolchain = 'D:/Tools/vcpkg/scripts/buildsystems/vcpkg.cmake'

python -B "$main/tools/artifact_budget.py" --build "$main/build" --run "$task" --planned-bytes 1610612736
if ($LASTEXITCODE -ne 0) { throw 'Player reservation is unavailable' }
python -B $queue run --build "$main/build" --owner npr-room --task realtime-height-fog-configure --run $runName --kind build --priority P1 --estimate 60 --timeout 240 --command $cmake -S $engine -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_TOOLCHAIN_FILE=$toolchain" "-DVCPKG_INSTALLED_DIR=$deps" -DVCPKG_MANIFEST_INSTALL=OFF
if ($LASTEXITCODE -ne 0) { throw 'Player configuration failed' }
python -B $queue run --build "$main/build" --owner npr-room --task realtime-height-fog-player-build --run $runName --kind build --priority P1 --estimate 900 --timeout 1800 --command $cmake --build $build --target AzurePlayer AzureViewFogBindingTests AzureViewFogGpuTests AzureGeneratedHeightFogGpuTests
if ($LASTEXITCODE -ne 0) { throw 'Player build failed' }
```

绑定、所有权及生成短检沿用已有注册用例。
随后运行场景雾的移动用例和新生成用例。
下列命令用于接入后已登记的生成用例。
当前工具提供 `camera-reentry` 对照。

```powershell
$env:PATH = "$deps/x64-windows/bin;$env:PATH"
$env:VK_LAYER_PATH = 'D:/Tools/VulkanSDK/1.4.350.0/Bin'
$env:VK_INSTANCE_LAYERS = 'VK_LAYER_KHRONOS_validation'
$env:VK_LAYER_VALIDATE_SYNC = '1'
python -B "$main/tools/artifact_budget.py" --build "$main/build" --run "$task" --planned-bytes 8388608
if ($LASTEXITCODE -ne 0) { throw 'Fog capture reservation is unavailable' }
python -B $queue run --build "$main/build" --owner npr-room --task realtime-height-fog-targeted-tests --run $runName --kind gpu --priority P1 --estimate 80 --timeout 240 --command $ctest --test-dir $build -R '^AzureRender\.(ViewFogBinding|ViewFogGpu|GeneratedHeightFogGpu)$' --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Targeted fog tests failed' }
python -B $queue run --build "$main/build" --owner npr-room --task realtime-height-fog-camera-reentry --run $runName --kind gpu --priority P1 --estimate 70 --timeout 180 --command python -B "$engine/tools/test_scene_fog_gpu.py" --executable "$build/AzurePlayer.exe" --output "$task/camera-reentry" --expected-validation disabled --case camera-reentry --case generated-height-reentry
if ($LASTEXITCODE -ne 0) { throw 'Camera replay failed' }
```

关卡往返的参数沿用冻结的 `command.json`。
将 `$fixture` 设为受管目录中生成模式的双关卡工程路径。
复用动作表及固定步参数，采集到空的输出目录。
将回放的关卡名、机位和像素对齐到独立结果。

```powershell
python -B $queue run --build "$main/build" --owner npr-room --task realtime-height-fog-roundtrip --run $runName --kind gpu --priority P1 --estimate 45 --timeout 120 --command "$build/AzurePlayer.exe" --project "$fixture/route.azureproject" --game-actions "$fixture/actions.json" --runtime-report "$task/route-runtime.json" --capture-dir "$task/route-frames" --capture-frames 72 --capture-fps 60 --fixed-frame-step --qa-camera scene --qa-light scene --width 128 --height 128
if ($LASTEXITCODE -ne 0) { throw 'Level roundtrip failed' }
```

## 验收与收尾

- 生成数值沿用 0.0006 门槛。
- 连续场景按独立解析像素参考核对 0.008 门槛。
- 比较固定与索引描述符的完整序列。
- 检查连续机位、真实生成标志、首尾复现及零活动分配。
- 核对体积区间两端及区间外的解析采样。
- 四倍采样检查实际采样数、深度源及 Vulkan 同步结果。
- 冻结捕获与解析路径沿用既有画面兼容要求。
- 留存命令、程序和着色器摘要、运行报告及最小画面。
- 使用 `artifact_cleanup.py --finish` 封存受管任务。
