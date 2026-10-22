# 终末地抓帧接手说明

日期：2026-10-22。阶段：NR0-F1。
目标是取得原游戏帧，并导出真实渲染绑定。
现有已核验证据中，终末地原生帧数量为零。
工具安装、协议验证和原游戏抓帧分别验收。

## 目标与验收

场景目标为明亮白黄色的帝江号中庭。
画面覆盖广场、后方区域和中央上下物流装置。
角色目标为莱万汀的面部、头发、皮肤及服饰。
中庭角色出场限制与角色预览分别记录。

首个交付物为中庭的有效 `.rdc`。
官方 RenderDoc 须能打开并重放该帧。
事件、输出图像和资源须能证明原游戏身份。
成功注入、目标连接及帧文件取得分别记录。

后续保存莱万汀预览近景帧。
物流运动采用同机位的多个时刻采集。
每份帧文件记录哈希、API、相机和画质。
预览与中庭使用独立的照明来源标记。

## 工作目录

仓库为 `D:/Assigment/2609/FYP`。
引擎目录为 `Project/AzureRender`，分支为 `engine-dev`。
开始前读取根目录 `AGENT.md`。
抓帧会话负责采集与分析，渲染实现由主会话维护。

工具与已有日志的绝对路径为：

```text
D:/Assigment/2609/FYP/Project/AzureRender/build/runs/20261022-051129-character-logistics-restoration-0f2e71/capture-tool-research
```

下文将该目录记为 `$captureTools`。
接手时重新枚举游戏、GUI 和 GPU 使用进程。
进程号及桥接窗口标识仅在对应进程存活时有效。
新采集产物按仓库规则放入新的 `build/runs/` 目录。

## 工具与版本

| 工具 | 位置与状态 |
| --- | --- |
| 修改版 GUI | `$captureTools/riderduck-runtime/qriderduck.exe` |
| 修改版 CLI | `$captureTools/riderduck-runtime/riderduckcmd.exe` |
| 修改版 DLL | `$captureTools/riderduck-runtime/riderduck.dll` |
| MCP 源码 | `$captureTools/jinxiangw-source` |
| MCP 独立 Python | `$captureTools/mcp-env/Scripts/python.exe` |
| 官方桥接扩展 | `C:/Users/23587/AppData/Roaming/qrenderdoc/extensions/renderdoc_mcp_bridge` |
| 游戏程序 | `E:/Hypergryph Launcher/games/Endfield Game/Endfield.exe` |

官方分析环境的绝对路径为：

```text
D:/Assigment/2609/FYP/Project/AzureRender/build/runs/20261021-172156-native-render-evidence-623c52/renderdoc/portable-1.46/RenderDoc_1.46_64
```

系统安装版位于 `C:/Program Files/RenderDoc`，版本为 1.43。
本任务的官方重放入口使用便携版 1.46。
修改版负责采集，官方版本负责重放与 MCP 分析。
桥接扩展已安装在官方 GUI 的用户扩展目录。

修改版来源为用户指定的 [RenderDuck v1.2](https://github.com/LesFloraison/RenderDuck/releases/tag/v1.2)。
下载资产名为 `RiderDuck_v1.2.zip`。
包内目录名为 `RiderDuck_v1.3`，程序版本字段为 `v1.44`。
这些名称分别来自发行、压缩包和二进制。

压缩包大小为 `940241289` 字节。
SHA-256 为：

```text
7c45ed4001cfcefe78584d9cada3e5e2c09fa9b4c4b671787bcfd7dcf9b91f09
```

MCP 来源为 [JinxiangW/renderdoc-mcp](https://github.com/JinxiangW/renderdoc-mcp)。
固定提交为 `bb7d577421f740c5ae8f54bec0edb33e18a16742`。
独立环境使用 Python 3.11.9 和 FastMCP 4.1.3。
命令行入口可直接调用，客户端全局 MCP 配置保持原值。

## 现有验证

修改版的二十一个运行文件已核对哈希。
官方扩展的二十二个安装文件已重新核对。
MCP 的初始化与五十个工具枚举通过。
这些结果属于安装及协议验证。

官方 1.46 已重放 AzureRender 夹具。
夹具的线性浮点像素和资源导出已有证据。
夹具证据用于验证分析链，并保留其校验限制。
原游戏与修改版帧的重放兼容性仍待实际验证。

修改版导出使用 `RIDERDUCK_` 前缀。
官方捕获工作器具有独立接口约定。
本任务采用修改版自身的 CLI 与 GUI。
其 `.rdc` 文件由官方环境单独验证兼容性。

当前复核结果位于 `$captureTools/handoff-current-state.json`。
既有工具调查位于 `$captureTools/findings.md`。
安装和协议收据位于同目录。

## 失败记录

### 官方工具附加

官方 `InjectIntoProcess` 对一次既有游戏进程返回失败。
控制标识为零，未产生帧文件。
日志为 `Failed to inject renderdoc.dll into process`。
错误本身未确定权限、时序或保护机制的具体原因。

完整来源见[抓帧链验证](../acceptance/nr/renderdoc-capture.md)。
该记录使用当时的实际进程号，接手时重新枚举。

### 修改版普通启动

已执行一次修改版 CLI 启动尝试。
请求参数包含 `-force-d3d11`。
CLI 返回值为 `4`，错误原文为：

```text
Failed to create & inject: RiderDuck injection failed: Failed to launch process.
```

游戏 PE 清单声明 `requireAdministrator`。
本次记录判定为进程完整性级别阻塞。
管理员启动路径已准备，成功启动及注入仍待验证。
DX11 是请求参数，实际图形 API 尚未确认。

本次记录位于 `$captureTools/capture-attempt-01.json`。
原始输出位于 `game-captures/Endfield-20261022-054309-cli.log`。
同目录仅有本次日志和 CLI 收据，未产生 `.rdc`。
管理员启动记录与帧取得结果仍需后续采集。

管理员入口为 `start-capture-as-administrator.cmd`。
该入口须通过 Windows 的管理员运行方式启动。
其调用脚本带有 `#Requires -RunAsAdministrator`。
普通双击不构成管理员执行的证据。

发行包、文件存在性和源码说明均属于准备证据。
具体失败的归因须结合新尝试的实际结果。
当前证据尚未证明修改版已被识别或已成功避开检测。

## 接手步骤

先检查现有收据和工具路径，再核对进程状态。
已有工具和独立环境可直接复用。
原游戏与引擎验证共用 GPU，按单任务串行执行。
捕获会话保留主还原会话的源码与工作区改动。

在管理员 PowerShell 中执行下列启动入口。
先将 `$captureOutput` 设置为本轮实际产物目录。
这条管理员启动路径尚待原游戏实测。

```powershell
$captureTools = 'D:/Assigment/2609/FYP/Project/AzureRender/build/runs/20261022-051129-character-logistics-restoration-0f2e71/capture-tool-research'
$captureOutput = '本轮实际产物目录的绝对路径'
& "$captureTools/launch-riderduck-capture.ps1" `
  -Mode Launch `
  -GameArguments @('-force-d3d11') `
  -OutputDirectory $captureOutput
```

Windows 的管理员确认由用户按系统提示处理。
启动后检查 CLI 原文、游戏进程和目标控制标识。
CLI 成功时可返回非零的目标标识。
退出码的判断须结合消息，不能只按零或非零分类。

用修改版 GUI 连接实际捕获目标并确认图形 API。
沿用本机游戏登录状态进入帝江号中庭。
用户提供的操作是按 `R` 进入拍照模式。
隐藏主角与 NPC，再按 `X` 隐藏界面。

界面与按键以当次游戏实际响应为准。
通过修改版目标控制界面或匹配 API 触发帧。
启动成功后另行确认文件存在和有效大小。
保存该帧的 SHA-256 与采集条件。

取得文件后，用官方 1.46 打开：

```powershell
& "$captureTools/launch-official-replay.ps1" `
  -CapturePath '实际原游戏帧的绝对路径'
```

官方 GUI 保持打开目标帧，再查询桥接状态：

```powershell
& "$captureTools/invoke-jinxiangw-bridge.ps1" -Method list_live_windows
& "$captureTools/invoke-jinxiangw-bridge.ps1" `
  -Method ping -WindowId '实际枚举得到的窗口标识'
& "$captureTools/invoke-jinxiangw-bridge.ps1" `
  -Method get_capture_status -WindowId '同一个窗口标识'
```

多窗口时显式指定同一 `window_id`。
MCP 查询依赖官方 GUI 中实际打开的捕获上下文。
已有 `get-capture-status.params.json` 含历史窗口标识。
参数文件须按当次枚举结果填写。

## 优先导出数据

先确认实际事件、着色器身份和活跃变体。
常量布局以该帧的反射结果为准。
静态反汇编文件用于交叉核对来源。

| 对象 | 优先数据 |
| --- | --- |
| 脸部 | SDF、遮罩、RGBA 色带、影色表、常量和实际绘制 |
| 脸部照明 | 场景阴影与角色阴影、完整漫反射照明量 |
| 角色全局量 | `_CharacterParams0/1/6/7` 及实际启用分支 |
| 动画附加帧 | `_VertexSkinMatrices` 的基址、首三行及骨骼矩阵 |
| 服饰 | 原参数图 G、二维高光色带、`_CharacterParams13.w` |
| 头发 | RG/BA 法线、二维高光、眉毛遮罩、预深度和模板 |
| 场景 | 地面采样 UV、材质、聚光形状、GI 和反射绑定 |
| 全局照明 | `_IVDefaultSHAr/g/b`、方向场和活动体积常量 |
| 物流 | 实例身份、变换、时间、纹理滚动、深度软交界 |

脸部附加帧是当前面部 GI 还原的重要待项。
其首三行不能直接认定为头骨或模型根矩阵。
同帧对照角色根、头骨和渲染器矩阵可帮助定性。
源调查位于当前还原任务的角色审计目录。

```text
build/runs/20261022-051129-character-logistics-restoration-0f2e71/scene-audit/character-binding-audit/native-diffuse-normal-consumers.json
build/runs/20261022-051129-character-logistics-restoration-0f2e71/scene-audit/character-binding-audit/reserved-skin-frame-producer.json
build/runs/20261022-051129-character-logistics-restoration-0f2e71/scene-audit/character-binding-audit/character-volume-profile-trace.json
```

每份导出记录帧哈希、事件号、资源号及原始格式。
先保存原始 shader、buffer 和纹理，再生成分析图。
失败时保存具体启动级别、命令、错误和文件列表。
新尝试须说明环境变化或待验证假设。

## 新会话启动提示

可向新会话发送：

> 继续终末地原游戏抓帧任务。
> 先读根目录 AGENT.md 与本交接文档。
> 当前已核实原生 RDC 为零，工具及 MCP 环境已准备。
> 优先验证管理员级 RiderDuck 启动、目标连接和实际 DX11。
> 取得白黄色帝江号中庭帧后，用官方 1.46 和已安装桥接分析。
> 复用现有工具与收据，保留主会话的渲染源码改动。
> 导出角色、脸部附加帧、GI、反射及物流的真实绑定。
