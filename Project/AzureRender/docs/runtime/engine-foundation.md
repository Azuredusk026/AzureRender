# 引擎基础与独立 Player

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-10-04
> 适用范围：Windows 项目、运行时与 Player
> 源码入口：`CMakeLists.txt`、`src/runtime/`、`src/player/main.cpp`
> 关联测试：`ProjectTests.cpp`、`RuntimeLifecycleTests.cpp`、`test_player_cli.py`

## 职责与使用场景

G0 提供模块库、项目创建、版本化配置、资源挂载和独立 Player。AzureRender 提供现有编辑预览与渲染验收入口，AzurePlayer 提供项目运行入口。

| 构建目标 | 职责 | 项目内依赖 |
| --- | --- | --- |
| AzureFoundation | 诊断、资源定位和公共构建约束 | AzureBuildOptions |
| AzurePlatform | GLFW 窗口和表面生命周期 | AzureFoundation |
| AzureRenderCore | RHI、帧图、着色、资产解析和场景渲染器 | AzureFoundation |
| AzureRuntime | 项目、关卡、资产、物理、输入和脚本 | AzureRenderCore、AzureReflection |
| AzureEditor | 编辑会话、相机控制和 Dear ImGui 界面 | AzureRuntime、AzurePlatform |
| AzureRenderHost | 编辑预览的 GPU 宿主 | AzureRuntime、AzurePlatform、AzureEditor |
| AzurePlayerHost | Player 的 GPU 宿主 | AzureRuntime、AzurePlatform |

宿主以 `AZURE_WITH_EDITOR` 编译变体隔离编辑器实现。公共渲染库由两个宿主共享。项目内新功能按所属模块加入显式源码清单，模块链接方向由 CMake 维护。

## 数据与所有权

Project 持有规范化项目文件路径、项目标识、名称、资源挂载和启动场景。项目创建生成 UUID 格式标识，项目移动保持该标识。

RuntimeLifecycle 持有 World、场景节点映射、初始场景描述和延迟操作队列。实体的变换与可见性进入逐帧场景快照，节点身份组件保护节点映射免受实体编号复用影响。关闭清空系统、组件和待执行操作。

`SceneDocument::renderDescription()` 将节点和灯光转换为渲染描述。Player 每帧以共享只读描述传入 `SceneFrameData`，角色渲染器在更新统一缓冲、剔除和绘制快照前读取节点与灯光。已有节点的变换、隐藏和删除在该帧生效，资源集合按加载时顺序持有。

GPU 宿主持有设备、交换链、帧槽、任务池和分配器。场景渲染器持有其资源，并在 GPU 工作完成后卸载。所有权和同步依据 [RHI 同步](rhi-synchronization.md)与 [并行提交](parallel-submission.md)。

## 生命周期与时序

运行时状态为 Created、Running、Paused、Stopped。`start()` 从 Created 进入 Running。`pause()` 和 `resume()` 切换运行与暂停。`step()` 在 Paused 状态请求推进下一帧。

`stop()` 清空运行期数据，允许重复调用。

项目运行使用 60 Hz 固定步，每步先消费延迟操作，再执行 World、脚本和物理。暂停保持模拟时间，单步执行一个固定步。接口见 [物理与输入](physics-input.md)和 [脚本与玩法](scripts-gameplay.md)。

Player 的 P 键切换暂停，O 键推进一个暂停帧。

Player 读取项目及启动场景后创建 GPU 宿主。启动场景初始化运行时节点。主线程处理窗口事件和场景变更，渲染录制读取已生成的帧快照。结束后等待 GPU 完成并按依赖顺序释放资源。

## 接口契约

项目文件名为 `project.azureproject`，`schemaVersion` 为整数 1。字段包括 `id`、`name`、`mounts` 与 `startupScene`。

```json
{
  "schemaVersion": 1,
  "id": "de59f8f0-37df-4c61-bb6c-2b63b9da7b84",
  "name": "MyGame",
  "mounts": [{"name": "assets", "path": "assets"}],
  "startupScene": "assets:/startup.azscene"
}
```

挂载名使用小写字母、数字和下划线，`engine` 为内置资源入口。项目挂载路径相对于项目目录，规范化后的路径须位于项目目录。虚拟资源使用 `挂载名:/相对路径`。`engine:/assets_public/` 和 `engine:/shaders/` 通过 ResourceLocator 定位安装资源。

`Project::load()` 检查字段、版本、挂载目录和启动场景路径。`loadStartupScene()` 解析场景并检查资源文件。场景资源可使用虚拟路径或相对场景文件的路径。缺失文件、重复挂载、未知挂载和越界路径产生异常及路径诊断。

`AzurePlayer --create-project <目录>` 要求目录为空，创建项目和 Sample 启动场景，并声明内置公开模型的虚拟资源引用。`--project <文件> --check-project` 验证配置及启动资源。运行项目时，启动场景提供渲染设置，命令行渲染参数覆盖其对应字段。`--scene <文件>` 提供独立场景运行入口。

## 线程与同步

World 与 RuntimeLifecycle 由主线程操作。延迟操作在帧边界执行，工作线程读取帧快照并写入各自的命令缓冲。

录制任务数量在一至五之间时，自适应入口在调用线程录制二级命令缓冲。较大批次通过工作池并行录制，调用线程同时参与。提交顺序由帧图确定，任务异常在任务完成后向调用线程传播。显式串行验收路径保留。

GPU 时间报告的 `cpuFrame` 分别记录帧槽等待、取图、提交、呈现和 CPU 整帧总时间。`attempts` 表示尝试帧数，`completedFrames` 表示完成帧数。等待与取图累计包含交换链重建前的重试，CPU 整帧累计包含已完成帧的更新、录制、截图等待和尺寸重建。窗口事件处理位于该计时区间之外。

`submission` 的绘制计数描述场景渲染器工作。黑洞三个全屏场景绘制均进入计数，宿主后处理与 HUD 的绘制具有独立职责。CPU 和 GPU 指标分别解释。

## 序列化与兼容

项目使用 JSON，整数版本 1 为当前契约。读取拒绝未知版本。启动资源支持 `.azscene` 场景和 `.azurelevel` 关卡，组件由反射注册表校验。

格式与兼容规则见 [反射与序列化](reflection.md)和 [资产与关卡](assets-levels.md)。

## 平台行为

Windows 使用 GLFW 与 Vulkan，Debug 启用 Validation。Release 发布目录包含 Player、渲染验收应用、运行库、着色器、公开资源、文档和许可证。Player 资源定位支持安装树迁移。Android 为 Deferred。

## 使用示例

在安装目录之外的工作目录运行以下命令，替换可执行文件路径。

```powershell
& 'D:/AzureEngine/bin/AzurePlayer.exe' --create-project './MyGame'
& 'D:/AzureEngine/bin/AzurePlayer.exe' --project './MyGame/project.azureproject' --check-project
& 'D:/AzureEngine/bin/AzurePlayer.exe' --project './MyGame/project.azureproject'
```

运行时测试可追加 `--scene-type character` 或 `--scene-type blackhole`。`--smoke-frames 30` 提供自动退出，`--gpu-timing --gpu-timing-output <新文件>` 输出性能记录。

## 诊断与排错

项目错误输出配置或资源路径。先使用 `--check-project` 核验项目，再用 `--check-resources` 核验引擎资源。运行期日志默认位于工作目录的 `captures/azureplayer.log.jsonl`。

Player 接受运行时命令。编辑命令使用 AzureRender 的编辑入口。加载失败时进程返回非零退出码，工具可据此判断结果。

## 验收与证据

契约测试注册为 `AzureEngine.Project`、`AzureEngine.RuntimeLifecycle` 与 `AzureEngine.PlayerCli`。现有 NullRHI 测试覆盖逐帧节点变换、隐藏、删除对绘制实例的影响，以及黑洞绘制计数。任务测试覆盖调用线程路径和异常传播。

Release 门禁包含隔离环境中的 Player 项目创建、目录移动、校验及三场景运行。阶段结果见 [G0 验收](../acceptance/g0/2026-10-03.md)。

## 参考来源

模块实现基于项目现有 `src/app/`、`src/ecs/`、`src/render/`、`src/rhi/` 与 `src/resources/`。JSON 使用已固定的 nlohmann/json，窗口使用已固定的 GLFW，GPU 后端沿用 Vulkan、VMA 与 OpenEXR。版本和许可证由安装清单及 `THIRD_PARTY_NOTICES.md` 记录。
