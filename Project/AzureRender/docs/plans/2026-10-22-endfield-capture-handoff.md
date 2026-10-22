# 终末地抓帧接手说明

日期：2026-10-22。阶段：NR0-F1。
目标是取得原游戏帧，并导出真实渲染绑定。
已取得九份终末地原生帧，并核对 SHA-256。
九帧均已通过官方 RenderDoc 1.46 加载。
九帧均可读取渲染通道，图形接口为 D3D11。
工具安装、协议验证和原游戏抓帧分别验收。
当前实施暂停，下一项为已有帧的专项分析。
完整还原及工作区入口见[主交接](2026-10-22-native-render-restoration-handoff.md)。

## 当前采集结果

本轮产物位于以下目录：

```text
D:/Assigment/2609/FYP/Project/AzureRender/build/runs/20261022-201845-endfield-capture-954719
```

文件名前缀为 `Endfield-games-child_frame`。
文件扩展名为 `.rdc`。
用户手动选择机位并触发本轮九帧采集。

| 帧号 | 画面与用途 |
| --- | --- |
| 1058 | 游戏标题界面 |
| 2399 | 中庭、主角和界面 |
| 3534 | 中庭正面与中央物流装置 |
| 4697 | 中庭侧向视角与物流装置 |
| 5565 | 中庭仰视、顶部与物流装置 |
| 6834 | 莱万汀编队预览 |
| 8402 | 洁尔佩塔编队预览 |
| 8877 | 大潘编队预览 |
| 10401 | 莱万汀全身三维预览 |

哈希与字节数见 `capture-files-verified.json`。
九帧共 `5521101932` 字节，目录含 `.keep` 保留标记。
本轮重新计算的哈希与原始收据全部一致。
核对结果见[原生帧验证](../acceptance/nr/native-game-captures.md)。

逐帧加载收据为 `*-replay.json`。
渲染通道收据为 `*-passes.json`。
同名 `.png` 为官方工具提取的内嵌缩略图。
缩略图与实际重放输出分别验收。

`3534` 与 `10401` 已导出实际重放图像。
输出为 `*-replayed.png`，分辨率为 2560×1440。
图像已目视核对，分别覆盖中庭与莱万汀预览。
最终颜色纹理均为本帧的 `ResourceId::406`。
对应呈现事件分别为 `12945` 与 `3460`。
资源编号必须结合帧哈希使用。
导出收据为 `*-backbuffer-receipt.json`。

这两帧的资源清单为 `*-inventory.json`。
中庭帧含 1046 个动作及 1021 个绘制或调度。
角色帧含 272 个动作及 250 个绘制或调度。
动作统计来自官方重放资源清单。
其余七帧已验证加载和通道查询。
其逐帧颜色输出与专项资源分析仍待执行。

采集入口为 `E:/Hypergryph Launcher/1.6.0/Games.exe`。
修改版 CLI 以管理员权限启动该入口。
采集启用 `--opt-hook-children`。
用户在官方启动器选择 DX11 并启动游戏。
游戏进程的控制端口已核实为 `38922`。
进程号与端口仅适用于本轮运行。

中庭与角色预览使用独立照明来源标记。
现有中庭帧的机位不同。
同机位物流时序、画质配置仍待补充。
脸部附加帧、原始绑定与材质导出仍待分析。

## 目标与验收

场景目标为明亮白黄色的帝江号中庭。
画面覆盖广场、后方区域和中央上下物流装置。
角色目标为莱万汀的面部、头发、皮肤及服饰。
中庭角色出场限制与角色预览分别记录。

有效帧、官方加载及两张实际颜色图已有证据。
专项交付物为逐绘制事件及原始绑定清单。
shader、常量、纹理和输入流须对应源对象。
活动变体和输出像素共同证明真实消费。

现有莱万汀帧优先完成脸部及附加帧导出。
近景精度不足时补充脸部专用帧。
物流运动采用同机位的多个时刻采集。
每份帧文件记录哈希、API、相机和画质。
预览与中庭使用独立的照明来源标记。

## 工作目录

仓库为 `D:/Assigment/2609/FYP`。
引擎目录为 `Project/AzureRender`，分支为 `engine-dev`。
开始前读取根目录 `AGENT.md`。
接手会话按主交接安排源分析与引擎实施。
多个会话协作时，先声明文件与 GPU 使用归属。

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
本轮九份原游戏帧已通过官方 1.46 加载验证。
两帧实际颜色图验证重放导出链路。

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
该次普通启动未取得游戏帧。
当次 DX11 仅为请求参数，未取得实际 API 证据。

本次记录位于 `$captureTools/capture-attempt-01.json`。
原始输出位于 `game-captures/Endfield-20261022-054309-cli.log`。
同目录仅有本次日志和 CLI 收据，未产生 `.rdc`。
成功管理员启动和九帧结果见本页当前采集结果。

管理员入口为 `start-capture-as-administrator.cmd`。
该入口须通过 Windows 的管理员运行方式启动。
其调用脚本带有 `#Requires -RunAsAdministrator`。
普通双击不构成管理员执行的证据。

发行包、文件存在性和源码说明均属于准备证据。
具体失败的归因须结合新尝试的实际结果。
该次失败未确定保护机制是否参与。

## 已验证的采集入口

成功入口为捕获目录的 `launch-games-capture.ps1`。
管理员运行修改版 CLI，启用启动器子进程捕获。
启动器程序为 `E:/Hypergryph Launcher/1.6.0/Games.exe`。
用户在启动器选择 DX11，并启动游戏完成本轮采集。

关键参数与来源为：

```text
capture --opt-hook-children
--working-dir E:/Hypergryph Launcher/1.6.0
--capture-file <新产物目录>/Endfield-games-child
E:/Hypergryph Launcher/1.6.0/Games.exe
```

`games-launch-receipt.json` 记录管理员权限为真。
该轮 CLI 返回启动器目标标识 `38920`。
游戏子进程目标端口为 `38922`。
这些标识只对原运行有效，接手时重新枚举。

新增采集使用新产物目录，保存完整启动收据。
具体参数以脚本和当次程序帮助为准。
CLI 返回目标标识时可为非零值。
成功判断须结合控制连接、文件和官方重放结果。

当前已有帧优先用于分析，补帧由具体证据缺口触发。
游戏拍照入口为 `R`，隐藏角色及 NPC 后按 `X` 隐藏界面。
按键与自动输入以实际响应为准。

## 已有帧分析步骤

先检查现有收据和工具路径，再核对进程状态。
已有工具和独立环境按路径复用。
官方重放、游戏与引擎验证共享 GPU，串行执行。
接手会话保留全部未提交渲染源码及证据。

在 PowerShell 中设置路径并打开中庭锚帧：

```powershell
$captureTools = 'D:/Assigment/2609/FYP/Project/AzureRender/build/runs/20261022-051129-character-logistics-restoration-0f2e71/capture-tool-research'
$captureRoot = 'D:/Assigment/2609/FYP/Project/AzureRender/build/runs/20261022-201845-endfield-capture-954719'
& "$captureTools/launch-official-replay.ps1" `
  -CapturePath "$captureRoot/Endfield-games-child_frame3534.rdc"
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

先核对状态中的实际 `path` 与预期帧哈希。
`latest_capture_path` 表示目录中最新文件。
历史帧的 `stale` 标志须结合实际打开路径解释。
逐绘制资源分析必须使用对应帧的实际事件号。

两帧的 Present 输出槽查询留有失败收据。
成功输出来自 `ResourceId::406` 的显式纹理导出。
资源号只在对应帧有效，专项目标须重新定位。
每个导出结果核对格式、切片、尺寸和摘要。

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

> 继续终末地已有原生帧的专项分析。
> 先读根目录 AGENT.md 与本交接文档。
> 已取得九份原生 RDC，官方 1.46 加载与通道查询均通过。
> 先读取本文件的当前采集结果与逐帧收据。
> 使用官方 1.46 和已安装桥接分析中庭及角色帧。
> 复用现有工具与收据，保留主会话的渲染源码改动。
> 导出角色、脸部附加帧、GI、反射及物流的真实绑定。
