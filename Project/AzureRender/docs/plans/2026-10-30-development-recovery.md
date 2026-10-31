# 环境恢复与后续开发计划

日期：2026-10-30。工作目录为 `D:/Projects/FYP`。
本页用于恢复开发环境，并安排后续任务。
状态：自主实施中，按 P0 至 P3 连续推进。

本轮记录见[恢复与实施验收](../acceptance/recovery/2026-10-30.md)。

## 项目与任务依据

主工程位于 `Project/AzureRender`。
技术栈为 C++17、Vulkan、CMake 和 Python。
运行时、编辑器、Player 与离线工具具有独立职责。
公共能力需要公开夹具及另一种使用方式。

`AfterglowRender` 保留独立仓库和构建内容。
当前主线以 AzureRender 的配置与文档为准。

| 依据 | 用途 |
| --- | --- |
| [仓库规则](../../../../AGENT.md) | 模块边界、验证与产物规则 |
| [恢复交接](2026-10-29-reinstall-handoff.md) | 候选、工具版本和原始故障 |
| [开发总计划](azure-engine-plan.md) | 工作顺序和历史交付 |
| [统一状态表](current-stage-status.md) | 正式阶段状态 |
| [主场景预算](2026-10-25-main-scene-shading-budget.md) | 材质特化与性能任务 |
| [动态反射计划](2026-10-28-reflection-filter-integration.md) | 原生射线及历史接入 |
| [可玩还原计划](2026-10-17-native-playable-restoration.md) | NR0 至 NR12 的最终验收 |

## 当前状态

分支为 `engine-dev`，恢复起点为 `a109b7e`。
渲染验证基线为 `53a6c11`。
材质类别特化候选位于 `8eca1e9`。

双配置构建与公共运行检查已通过。
新机基线与候选的八帧 PNG 全部一致。

编辑器、运行时、发布及路径追踪已有实现和证据。
原生还原的公共机制已有多项局部验证。
正式状态仍为 NR0 开发中、NR1 至 NR12 待完成。
局部测试结果按对应验收页的范围解释。

原始四样点启动访问异常保持独立调查。
交接记录中的异常码为 `0xc0000005`。
转储指向 TinyGLTF 的 JSON 字符串解析。
根因仍待确定，Release 五次正常启动已通过。

Debug 完整场景的资源扫描与加载成本继续诊断。
紧凑依赖索引已通过双配置及七百三十九条记录核对。
公共夹具及完整 Release 图像见[索引验证](../acceptance/recovery/asset-dependency-index.md)。

性能需同时核对配置、输入与统计口径。
交接记录的模式一平均约为 22.97 毫秒。
该结果属于历史候选短测。
持续 1080p60、完整 SSIM 与连续序列仍待验。

## 本机检查

| 项目 | 2026-10-30 检查结果 |
| --- | --- |
| 工程路径 | `D:/Projects/FYP/Project/AzureRender` |
| 系统工具 | Git 与 winget 可用 |
| Python 初始状态 | WindowsApps 商店占位入口 |
| C++ 初始状态 | 未发现 Visual Studio 安装器及 MSVC |
| 构建工具初始状态 | 当前 PATH 未发现 CMake、Ninja |
| Vulkan 初始状态 | 运行时可枚举双 GPU，SDK 待安装 |
| 独显 | RTX 4060 Laptop，驱动 592.82 |
| 核显 | Intel UHD，驱动 32.0.101.6556 |
| D 盘初始剩余空间 | 约 702.56 GiB |
| 开发证据归档 | 62,901,352,215 字节，SHA-256 核对通过 |
| 运行资产 | `build/runs` 已恢复，候选及故障资料指纹通过 |
| 工作区 | 四个 P5 证据文件已有修改，保留现场 |

归档检查使用 `Restore-Evidence.ps1 -CheckOnly`。
归档位于 `build/archives`。
本次校验只证明压缩包字节与回执一致。
解压后的资源、硬链接和运行结果分别验收。

## 环境配置清单

工具安装目录使用 `D:/Tools`。
下载使用 `D:/Tools/Downloads`。
编译、依赖和缓存使用工程的 `build` 目录。
系统安装器的共享组件按官方安装规则存放。

| 顺序 | 组件 | 版本与配置 | 完成条件 |
| --- | --- | --- | --- |
| E0 | 备份与工作区 | 保留 `.git`、原始资产和已有修改 | 归档哈希一致，记录 Git 状态 |
| E1 | Python | 3.11.9，安装于 `D:/Tools/Python311` | 解释器、pip、虚拟环境可运行 |
| E2 | Visual Studio Build Tools | 2022，C++ 桌面工具、Windows SDK、英文诊断 | `cl /Bv` 可运行，记录实际工具集 |
| E3 | CMake / Ninja | CMake 3.20+，优先使用 VS 自带工具 | 两者版本查询成功 |
| E4 | Vulkan SDK | 1.4.350.0，含 glslc 与验证层 | 头文件、库、着色编译和设备枚举通过 |
| E5 | vcpkg | 提交 `d186940b2f0c4e8e5b4ff804af57d2a26fbfa091` | 引导成功，版本数据库覆盖基线及显式覆盖项 |
| E6 | C++ 依赖 | `vcpkg.json`，`x64-windows` | 清单配置成功，版本符合锁定项 |
| E7 | Python 依赖 | 四份工具清单及文档工具 | `pip check` 与模块导入通过 |
| E8 | 运行资产 | 恢复归档中的 `build/runs` | 当前候选资源与故障转储可读 |
| E9 | 抓帧工具 | RenderDoc 1.46，优先检查保留目录 | 实际 RDC 加载、查询与图像导出通过 |
| E10 | 文档工具 | `requirements-docs.txt` | 文档校验及需要时的站点构建通过 |
| E11 | .NET SDK | 9.0.319，启用托管脚本原型 | CoreCLR 与 NativeAOT 构建和测试通过 |
| E12 | 扩展工具 | Blender、视频工具按任务安装 | 对应工具链任务可以运行 |

vcpkg 内置基线为 `2bc124dac8436f5d8e108273aacf3f68d9824185`。
工具检出提交同时覆盖 `libwebp 1.6.0#3` 等修订。
依赖版本由 `vcpkg.json` 和覆盖端口共同确定。
Assimp 为 6.0.4，Manifold 为 3.5.2，Lua 为 5.4.8。

GLFW、Jolt、RmlUi、音频和图像库由清单安装。

.NET SDK 9.0.319 用于托管工具及当前候选复现。
本轮构建启用 `AZURE_ENABLE_MANAGED_SCRIPT_PROTOTYPE`。
Blender 5.1.2 用于原生离线转换任务。
视频工具优先使用 `imageio-ffmpeg` 提供的程序。

Node、uv 和 GitHub CLI 按具体开发任务补齐。

### 安装与配置

以下命令在工程目录执行。
每项执行后检查退出码与实际工具版本。
安装状态见本页末尾的实施记录。

- [x] 核对开发证据归档的 SHA-256。
- [x] 完成 Python、C++ 构建工具与 Vulkan SDK 安装。
- [x] 设置本机 `VCPKG_ROOT` 和 `VULKAN_SDK`。
- [x] 固定 vcpkg 提交并完成引导。
- [x] 创建 Python 虚拟环境并检查依赖。
- [x] 按需恢复运行资产并核对活动路径。
- [x] 配置 Debug 与 Release，完成公共冒烟。
- [x] 验证 RenderDoc 与当前真实场景入口。

安装 C++ 工具可使用以下命令。
安装内容包括推荐组件和 CMake 工具。

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget --silent --accept-source-agreements --accept-package-agreements --override '--quiet --wait --norestart --nocache --installPath D:\Tools\VS2022BuildTools --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --add Microsoft.VisualStudio.Component.VC.14.44.17.14.x86.x64 --add Microsoft.VisualStudio.Component.VC.CMake.Project --addProductLang en-US'
winget install --id KhronosGroup.VulkanSDK --version 1.4.350.0 --exact --source winget --silent --location D:\Tools\VulkanSDK\1.4.350.0 --accept-source-agreements --accept-package-agreements
```

安装参数依据 [Microsoft 安装说明](https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio?view=vs-2022)。
SDK 来源为 [LunarG 下载入口](https://vulkan.lunarg.com/sdk/home)。
Python 来源为 [3.11.9 发布页](https://www.python.org/downloads/release/python-3119/)。

本机系统代理为 `127.0.0.1:7897`。
GitHub 直连失败后，通过该代理完成源码获取。
vcpkg 仓库配置使用该本机代理。
代理端口变化时先核对系统配置。

```powershell
git -c http.proxy=http://127.0.0.1:7897 clone --no-checkout https://github.com/microsoft/vcpkg.git D:/Tools/vcpkg
git -C D:/Tools/vcpkg config http.proxy http://127.0.0.1:7897
git -C D:/Tools/vcpkg checkout d186940b2f0c4e8e5b4ff804af57d2a26fbfa091
& D:/Tools/vcpkg/bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = 'D:/Tools/vcpkg'
$env:VULKAN_SDK = 'D:/Tools/VulkanSDK/1.4.350.0'
$env:PATH = 'D:/Tools/Python311;D:/Tools/Python311/Scripts;' + $env:PATH
$env:PYTHONUTF8 = '1'
$env:DOTNET_ROOT = 'D:/Tools/dotnet9'
$env:PATH = $env:DOTNET_ROOT + ';' + $env:PATH
$env:NUGET_PACKAGES = 'D:/Tools/nuget-packages'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/reinstall/Restore-Environment.ps1 -PreparePython
& build/reinstall-environment/venv/Scripts/python.exe -m pip check
```

Python 包版本以保存清单为准。
本机 pip 24 读取中文清单时需要 UTF-8 模式。
下载失败时记录具体包、索引与解释器版本。
兼容版本调整需要单独验证工具行为。

技能已由当前会话提供，可按需核对环境包副本。

### 资产与路径

历史根路径为 `D:/Assigment/2609/FYP`。
当前根路径为 `D:/Projects/FYP`。
活动配置中的绝对路径需逐项核对和修复。
历史日志、转储和来源清单保留其原始路径。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/reinstall/Restore-Evidence.ps1
```

该命令验证压缩包后恢复 `build/runs`。
首次解压前确认目标尚未创建。
运行证据为恢复目标时，应先执行解压再创建任务目录。
整包历史展开的逻辑大小约为 312.14 GiB。

恢复后读取当前任务的 `continuation.json`。
任务目录见[恢复交接](2026-10-29-reinstall-handoff.md)。
核对原始转储、主入口配置、模型和材质依赖。
依赖齐全后再开始真实场景验证。

### 构建与环境验收

主构建复用 `msvc-debug` 和 `msvc-release` 预设。
现有备份缺少这两份主构建产物，需要重新配置。
恢复脚本的 `-Configure` 可用于独立迁移诊断。
常规开发保持每种配置一份构建目录。

```powershell
$env:PATH = 'D:/Tools/VulkanSDK/1.4.350.0/Bin;' + $env:PATH
.\tools\msvc_env.bat cmake --preset msvc-debug -DAZURE_ENABLE_MANAGED_SCRIPT_PROTOTYPE=ON -DPython3_EXECUTABLE=D:/Projects/FYP/Project/AzureRender/build/reinstall-environment/venv/Scripts/python.exe
.\tools\msvc_env.bat cmake --build --preset msvc-debug -j 2
.\tools\msvc_env.bat cmake --preset msvc-release -DAZURE_ENABLE_MANAGED_SCRIPT_PROTOTYPE=ON -DVCPKG_INSTALLED_DIR=D:/Projects/FYP/Project/AzureRender/build/ninja-msvc-debug/vcpkg_installed -DPython3_EXECUTABLE=D:/Projects/FYP/Project/AzureRender/build/reinstall-environment/venv/Scripts/python.exe
.\tools\msvc_env.bat cmake --build --preset msvc-release -j 2
vulkaninfo --summary
& build/ninja-msvc-release/AzureRender.exe --scene-type character --asset assets_public/test_model.gltf --smoke-frames 120
```

配置显式选择虚拟环境的 `Python3_EXECUTABLE`。
MSVC 环境通过 `tools/msvc_env.bat` 装配。
GPU 程序串行运行，构建并行度为二。
首次验收记录编译器、SDK、驱动与实际 GPU。

## 后续开发顺序

P0 至 P3 为本页的任务优先级。
正式阶段状态继续使用统一状态表中的编号。

### P0：恢复可验证基线

依赖为 E0 至 E8，以及公共场景可运行。
产出为本机环境记录、当前候选及基线构建。
故障输入和冻结图像均绑定源码与资源指纹。

1. 读取 `continuation.json` 和原始崩溃分析。
2. 从当前候选复现四样点初始化异常。
3. 对照 `53a6c11`，划定触发条件与影响范围。
4. 检查加载器、JSON 解析、资源所有权和缓存。

5. 根因明确后增加相应回归，再实施修复。

定位入口为 `src/assets/GltfLoader.cpp`。
同时核对角色初始化及完整材质加载调用。
基线对照使用独立检出目录，保留当前工作区。
历史诊断已执行多轮，追加诊断需有明确新假设。

- [x] 公共 glTF 与完整真实资产均可加载。
- [x] Debug、Release 和四样点路径完成受影响验证。
- [x] 实际 Player 的重复启动和正常退出通过。
- [ ] 保存转储、日志、根因及回归结果。

资产扫描采用[紧凑依赖索引计划](2026-10-31-asset-dependency-index.md)。
双配置构建及每配置二十九项受影响检查通过。
Release 完整入口八帧与同机控制一致。

共享启动快照按[专项计划](2026-10-31-project-startup-context.md)实施。
双配置各三十五项检查及完整入口均通过。
完整 Debug 四样点在六百秒内完成八帧和卸载。

来源及范围见[启动验证](../acceptance/recovery/project-startup-context.md)。

托管发布验证采用[目录恢复计划](2026-10-31-managed-workflow-cleanup.md)。
两种配置及后端均通过实际脚本与发布工作流。

### P1：材质特化与主场景预算

依赖为 P0 的稳定加载与可重现图像。
范围沿用[主场景预算](2026-10-25-main-scene-shading-budget.md)。
入口为 `shaders/mesh.frag` 及角色材质管线。
生命周期覆盖创建失败、热重载和卸载。

- [x] 复核 `8eca1e9` 的有限类别及动态回退。
- [x] 核对新机冻结八帧，记录归档严格门禁待项。
- [x] 验证主辅视图、模式一、四样点和 Player。
- [x] 执行材质策略、GPU 和资源释放短回归。
- [x] 固定预热、机位与帧数，记录模式一平均值及 P95。
- [ ] 满足有效热状态后完成四样点成对性能。

性能调整须同时满足图像和资源预算。
正式 1080p60 沿用 NR12 的完整验收条件。
阶段结果同步预算计划、验收页及统一状态表。
功能位策略与当前结果见[特化验证](../acceptance/nr/material-class-feature-specialization.md)。

### P2：反射、角色与抗锯齿闭环

优先推进投影射线在主场景的深度与历史装配。
入口为 `src/render/ProjectedReflectionRay.*` 及所属装配。
颜色、过滤足迹和实际 GPU 输出分别核对。
公共反射、历史接收和多样点路径共同回归。

随后按 NR7-AA4 至 AA6 补齐动态画质。
范围包括透明发丝、服饰、显露与镜头运动。
角色脸部、眼睛、对象阴影及完整照明按专项推进。
来源约束沿用[角色技术清单](2026-10-22-character-technique-closure.md)。

- [x] 原生射线冻结输入和 GPU 输出完成对照。
- [ ] 主场景、公开镜面及取消重试通过回归。
- [ ] 动态薄透明与服饰满足既有细节门禁。
- [ ] 六组正式机位完成相机及照明条件核对。
- [ ] 联合图像、性能与实际资源峰值完成记录。

原生射线的冻结数值参考见[顺序验证](../acceptance/nr/native-ray-arithmetic-order.md)。
三份完整输出逐字节一致，主场景装配继续实施。

Vulkan 来源参考按[专项验证](../acceptance/nr/native-ray-vulkan-reference.md)验收。
双配置、正逆瓦片顺序及三份完整输出均通过。
生产接入继续核对深度约定和历史所有权。

公共投影射线及双配置资源契约见[接口验证](../acceptance/nr/projected-ray-contract.md)。
主场景继续装配深度、运动及前帧颜色。

相机、线性运动及双配置镜面控制见[适配验证](../acceptance/nr/projected-camera-adapter.md)。
表面深度、法线和反射资格按公共输入契约准备。

### P3：原生可玩交付

按 NR0 至 NR8 的依赖补齐联合渲染门禁。
NR9 核对原生移动动作的循环和脚接触。
NR10 验证 WASD、相机及场景移动。
NR11 验证速度混合、共享步态和统一姿态消费。

- [ ] 完成静止、走跑、启停与转身序列。
- [ ] 核对 Player、编辑预览和独立发布结果。
- [ ] 按 NR12 执行完整图像与持续性能验收。
- [ ] 完成发布包移动、许可清单及实录。
- [ ] 集中复核双设备、实体操作及听感。

P8 保留性能热状态、人工复核和历史归因待项。
正式长跑与完整双设备批次在冻结交付候选后安排。
新机硬件和驱动差异进入结果来源记录。
归档兼容与四样点热状态见[环境复核](../acceptance/p8/2026-10-31-environment.md)。

历史通过结果按原配置保留。

## 文档与交付规则

每个可验收阶段同步实现、测试及相关文档。
更新开发总计划、统一状态表和专项验收页。
涉及内容来源时同步 manifest 与许可清单。
文档变更核对本地链接和中文定稿文风。

后续完整实施按阶段创建独立提交。
标题使用 `feat(<phase>): <中文摘要>`。
提交前核对差异、暂存范围与实际测试结果。
当前四个 P5 文件的已有修改保持独立。

## 安装实施记录

本节记录本次实际安装结果及待项。
检查通过与尚未执行的项目分别登记。
详细日志使用 D 盘下载目录和工程任务目录。

- 证据归档：SHA-256 检查通过。
- Python：3.11.9 安装通过，解释器与 pip 已验证。
- Python 工具包：四份清单安装通过，模块导入通过。
- Python 一致性：`pip check` 通过。
- 源码检索：`code_query.py read` 实际查询通过。
- C++ 工具：Visual Studio 2022 Build Tools 安装通过。
- 编译器：19.44.35229，工具集目录为 14.44.35207。
- 构建工具：CMake 3.31.6-msvc6、Ninja 1.12.1。
- C++ 验证：经 `msvc_env.bat` 编译、链接及运行通过。
- Vulkan SDK：1.4.350.0 安装通过，头文件与库齐全。
- Vulkan 验证：双 GPU、验证层及 Vulkan 1.3 编译通过。
- vcpkg：固定版本数据库，完整清单安装与配置通过。
- 运行资产：恢复通过，候选及故障资料指纹通过。
- C++ 依赖：五十五个包编译安装通过，Debug 配置通过。
- .NET：9.0.319 安装通过，CoreCLR 与 NativeAOT 编译通过。
- 文档工具：mkdocs-material 9.8.0 安装通过，依赖检查通过。
- Blender：5.1.2 便携版校验通过，后台 GLB 导出通过。
- 抓帧工具：官方 1.46 跨接口重放及原生图像一致性通过。
- 引擎构建：Debug 与 Release 完整构建通过。
- 真实入口：Release 五次四样点启动通过，资源释放通过。
- 公共运行：双配置各十项材质回归及七条 Player 路径通过。
- 图像对照：新机基线与候选八帧一致，归档兼容待复核。
- 启动快照：双配置七十项受影响检查通过。
- 完整主场景：双配置十六帧与本机冻结对照一致。
- Debug 四样点：521.251 秒内完成八帧、验证层及卸载检查。
- 紧凑索引四样点：一百五十帧功能通过，热状态复核未通过。

结构化检查记录位于以下本机文件：

```text
build/reinstall-environment/setup-20261030.json
```

`VCPKG_ROOT` 与 `VULKAN_SDK` 已保存到用户环境。
新终端可读取这些变量。
当前会话执行命令时可按上文显式设置。
MSVC、CMake 与 Ninja 通过项目批处理入口调用。
