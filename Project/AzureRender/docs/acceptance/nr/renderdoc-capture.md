# 渲染抓帧链验证

日期：2026-10-21。
状态：引擎夹具捕获与重放链可用。
Debug 抓帧的宿主校验尚未通过。
原游戏抓帧尚待验证。

## 工具与来源

本机安装 RenderDoc 1.43。
目录为 `C:/Program Files/RenderDoc`。
版本提交为 `286e07140d96bf3acda4059e085e8f5eb0e92608`。
RenderDoc 采用 MIT 许可。

工具使用官方 `renderdoc.pyd` 接口。
匹配运行时为 Python 3.6.8 嵌入包。
运行时仅放在本次任务目录。
下载地址为 [Python 官方发布](https://www.python.org/ftp/python/3.6.8/python-3.6.8-embed-amd64.zip)。

已调查 [RenderDoc MCP](https://github.com/JiaboLi-GitHub/renderdoc-mcp)。
调查提交为 `176d12ba194eef5f6a4b7fc7d0ae90cda923afcd`。
该项目采用 MIT 许可。
其工具封装 RenderDoc 捕获和重放接口。

本轮使用同一官方接口验证数据链。
源码、许可、文件摘要见任务目录的 `sources.json`。
Python 包摘要见 `python-runtime-source.json`。
MCP 服务配置保持由工具接入任务管理。

## 捕获与重放

正式入口为 `tools/native_render/renderdoc_capture.py`。
宿主使用项目 Python 环境。
`--python` 指定兼容 RenderDoc 的外部运行时。
`--renderdoc-dir` 指定本机安装目录。

`renderdoc_worker.py` 管理捕获及接口生命周期。
`renderdoc_replay.py` 管理重放与数据导出。
这两个模块兼容 Python 3.6 绑定。
场景、程序和事件均由调用方指定。

任务目录为：
`build/runs/20261021-172156-native-render-evidence-623c52/renderdoc/`。

捕获使用官方启动接口。
控制连接使用接口返回的精确标识。
`inject` 接收调用方指定的进程编号。
目标程序的退出生命周期由调用方管理。

捕获选项启用 `apiValidation`。
子进程捕获关闭，资源范围限于帧内使用。
收据记录程序摘要、进程、参数和捕获摘要。
目标正常退出由宿主日志单独验证。

`replay` 接收捕获、输出目录和事件编号。
默认选择首个和末个绘制事件。
数据包含动作树、纹理、缓冲和调试消息。
指定事件导出着色器、常量和渲染目标。

每个事件记录动作类型和原始标记。
绘制事件导出顶点及像素阶段。
计算调度事件仅导出计算阶段。
保留的管线绑定以 `bound_` 字段记录。

阶段范围见事件的 `stage_coverage`。
曲面细分、几何、网格和光追阶段单列覆盖状态。
当前导出器检查 `Vertex`、`Pixel` 和 `Compute`。
计算事件的图形目标和视口记为空。

着色器导出保留原二进制和反汇编文本。
常量同时记录浮点和整数位解释。
实际类型以反射结果为准。
未取得调试名称的字段需要进一步分析。

每次运行使用新的输出目录。
`request.json` 记录输入摘要与参数。
`receipt.json` 记录实际版本、接口和输出摘要。
`worker.log` 保留外部运行时诊断。

成功收据需通过文件、摘要和来源校验。
缺失输出、错误摘要和空动作重放均拒绝。
损坏捕获或重放失败保留失败收据。
超时只终止工作进程，目标生命周期仍归调用方。

## 使用命令

以下命令在引擎目录执行。
先设置本次任务的运行时路径。

```powershell
$nrRun = 'D:/Assigment/2609/FYP/Project/AzureRender/build/runs/20261021-172156-native-render-evidence-623c52'
$nrPython = "$nrRun/renderdoc/python36/python.exe"
$nrRenderDoc = 'C:/Program Files/RenderDoc'
```

平面夹具的捕获命令为：

```powershell
python tools/native_render/renderdoc_capture.py capture `
  --python $nrPython --renderdoc-dir $nrRenderDoc `
  --executable build/ninja-msvc-release/AzurePlayer.exe `
  --output "$nrRun/renderdoc/candidate-capture-001" --frame 3 `
  -- --project "$nrRun/renderdoc/plane-fixture/lab.azureproject" `
  --width 256 --height 256 --fixed-frame-step `
  --capture-dir "$nrRun/renderdoc/candidate-capture-001/host-frames" `
  --capture-frames 8 `
  --runtime-report "$nrRun/renderdoc/candidate-capture-001/host-runtime.json"
```

既有捕获的重放命令为：

```powershell
python tools/native_render/renderdoc_capture.py replay `
  --python $nrPython --renderdoc-dir $nrRenderDoc `
  --capture "$nrRun/renderdoc/plane-capture/capture_frame3.rdc" `
  --output "$nrRun/renderdoc/replay-inspection-001" `
  --event 49 --event 131
```

动作与事件编号保存在 `actions.json`。
`resources.json` 列出实际纹理和缓冲。
`pipelines.json` 保存选定事件的绑定及常量。
着色器原字节、反汇编和 PNG 同目录输出。

CPU 回归命令为：

```powershell
python -m unittest discover -s tools -p 'test_native_render*.py'
```

## 本机结果

夹具来自 `tools/native_render/light_lab.py`。
内容为白色平面和一盏点光源。
宿主为主 Release 构建的 `AzurePlayer.exe`。
图像为 256×256，固定步长共运行八帧。

第 3 帧捕获文件为 `plane-capture/capture_frame3.rdc`。
文件大小为 4,656,324 字节。
捕获与重放收据均记录 SHA-256。
目标运行于 NVIDIA RTX 4060 Laptop GPU。

| 项目 | 实际结果 |
| --- | --- |
| 图形接口 | Vulkan |
| 本机重放 | 支持，未降级 |
| 动作 | 39 |
| 绘制 | 4 |
| 计算调度 | 3 |
| 纹理与缓冲 | 33 与 18 |
| 重放调试消息 | 0 |
| 第 49 事件 | 顶点及像素着色器，17 个顶层常量 |
| 第 131 事件 | 最终渲染目标，PNG 导出成功 |
| 图像一致性 | 重放与宿主截图逐 RGB 相等 |
| 正常退出 | 八帧完成，缓冲及图像计数归零 |

成功重放记录位于 `plane-replay-v2/replay-report.json`。
首轮 API 字段错误保留于 `plane-replay/`。
图像误差记录为 `replay-image-verification.json`。
最大通道误差为零。

正式工具复用此捕获完成重放。
收据为 `formal-replay-green/receipt.json`。
着色器原字节、常量和管线成功导出。
最终渲染目标与宿主截图逐 RGB 相等。

损坏捕获的失败收据位于 `formal-corrupt-rejection/`。
实际错误来自 `OpenFile` 的文件标识读取。
八项 CPU 测试覆盖来源、失败与阶段语义。
相关工具的 25 项 CPU 回归通过。

正式捕获驱动完成 Debug 影色表夹具取帧。
八帧运行正常退出，释放计数归零。
捕获链成功与宿主图形校验分别记录。
正式重放验证见 `formal-tool-verification.json`。

活动阶段复核使用同一影色表捕获。
记录为 `active-stage-verification.json`。
重放输出位于 `active-stage-replay/`。
原始捕获的 SHA-256 保持收据中的值。

| 事件 | 动作 | 实际导出阶段 |
| --- | --- | --- |
| 17 | 计算调度 | `Compute` |
| 49 | 索引绘制 | `Vertex`、`Pixel` |
| 141 | 最终绘制 | `Vertex`、`Pixel` |

事件 141 的 PNG 与既有导出逐字节相等。
事件 17 的图形目标为空。
该复核验证活动阶段筛选及图像保持。

## Debug 宿主校验

影色表夹具为 128×128，固定步长八帧。
宿主日志确认 Vulkan 验证层已启用。
正式捕获成功，重放包含 41 个动作、6 次绘制。
重放调试消息为空，宿主仍记录 12 条校验错误。

| 宿主错误类别 | 条数 | 状态 |
| --- | --- | --- |
| 着色器声明未启用的 `MultiView` 能力 | 2 | 官方修复依据已确认 |
| 提交命令预期呈现布局，实际为 `GENERAL` | 8 | 来源待定位 |
| 呈现图像实际为 `GENERAL` | 1 | 来源待定位 |
| 未取得交换链图像时执行布局转换 | 1 | 来源待定位 |

日志位于 `shadow-lut-capture/worker.log`。
分类记录位于 `validation-investigation/`。
分类文件为 `shadow-lut-1.43-host-error-summary.json`。
重放收据位于 `shadow-lut-replay/receipt.json`。

`MultiView` 错误对应官方 [PR 3831](https://github.com/baldurk/renderdoc/pull/3831)。
修复提交为 `ceeeaa5478e7e8ddb782d0269839b4039e57a6ce`。
该修复为内部着色器增加特性检查。
本机 1.43 源码的对应检查为 `NoCheck`。

官方 1.46 源码已包含该检查。
便携包已放入任务目录，系统安装保持 1.43。
来源与摘要见 `portable-1.46-source.json`。
捕获层版本实验按独立候选记录。

布局错误尚未找到对应的上游修复。
工具版本、层顺序及宿主截图路径需分别取证。
当前 Debug 抓帧不能作为无校验错误的证据。

屏障改写已定位至工具的呈现布局转换。
正常限帧捕获与重放导出保留一条取图校验错误。
源码、CPU 红绿检查及运行归因见[布局诊断](renderdoc-layout-audit.md)。

## 1.46 便携验证

官方便携包使用内嵌 Python 3.8.20。
探针通过 `qrenderdoc --python` 调用官方接口。
该入口由前端管理重放接口的初始化和关闭。
外部模块的构建要求见[官方说明](https://github.com/baldurk/renderdoc/blob/v1.46/docs/python_api/python_module.rst)。

输出目录为 `portable-1.46-experiment/`。
候选包含冻结的 Player 和 45 份已编译着色器。
运行参数、依赖与前后摘要保存在 `request.json`。
各版本结果分别关联其宿主候选。

进程环境的 `VK_IMPLICIT_LAYER_PATH` 指向便携目录。
该环境变量的规则见[官方加载器文档](https://github.com/KhronosGroup/Vulkan-Loader/blob/vulkan-sdk-1.4.350.0/docs/LoaderLayerInterface.md)。
实际加载的 DLL 路径与摘要进入收据。
系统 RenderDoc 注册保持指向原安装目录。

本次单帧捕获为 `capture_frame3.rdc`。
文件大小为 1,093,596 字节。
宿主完成八帧并结束进程。
缓冲与图像的释放计数均为零。

宿主日志确认 Vulkan 验证层启用。
宿主仍有 10 条校验错误。
重放消息与宿主校验分别记录。
该候选的无错误抓帧门禁尚未通过。

| 宿主错误类别 | 1.46 实测条数 |
| --- | --- |
| `MultiView` 着色器能力 | 0 |
| 提交时的图像布局 | 8 |
| 呈现时的图像布局 | 1 |
| 未取得图像时执行布局转换 | 1 |

宿主证据为 `host-diagnostics.jsonl`。
该文件摘取本次运行的 39 条结构化记录。
收据记录源日志摘要、行号与时间范围。
完整结果见 `summary.json`。

## 原生浮点取样

同次 1.46 重放使用官方 `PickPixel` 取样。
目标为事件 127 的首个颜色输出。
其格式为 `R16G16B16A16_FLOAT`。
目标大小为 128×128，取样点为 `(64, 64)`。

像素着色器摘要匹配冻结的 `mesh_bindless.frag.spv`。
`GetTextureData` 同时导出原生子资源。
原生半浮点值与 `PickPixel` 返回值逐值相同。
原始文件为 `event-127-target0-native.bin`。

独立参考取自 `expected_lookup` 的八邻点插值。
探针返回显示编码前的线性累加值。
衍生源码与原函数摘要均进入收据。
实际夹具颜色、模型与纹理摘要同时记录。

| 通道 | 原生线性值 | 独立参考 | 相对误差 |
| --- | --- | --- | --- |
| 红 | 0.2119140625 | 0.2116357143 | 0.1315% |
| 绿 | 0.0599670410 | 0.0598486568 | 0.1978% |
| 蓝 | 0.7377929688 | 0.7396844453 | 0.2557% |

三个通道均满足本样本的 1% 误差条件。
该结果验证单夹具的原生浮点查表输出。
原生完整着色组合与其他输入仍按所属门禁验收。

## 原游戏受控尝试

本轮对既有游戏进程执行一次标准注入请求。
调用为官方 `InjectIntoProcess`，目标编号为 40300。
返回结果为失败，控制连接标识为零。
本次请求未产生捕获文件。

官方错误为：

```text
RenderDoc injection failed: Failed to inject renderdoc.dll into process.
Check that the process did not crash or exit early in initialisation,
e.g. if the working directory is incorrectly set.
```

请求后同一游戏进程仍响应。
本次操作保持游戏运行和游戏目录只读。
收据位于 `endfield-attach/capture-receipt.json`。
运行日志位于 `endfield-attach.log`。

错误未给出保护机制或权限的具体归因。
该结果也未证明晚注入是失败原因。
后续正常启动捕获需独立安排目标生命周期。

## 验证边界

Release 日志记录 Vulkan 验证层关闭。
RenderDoc 的捕获验证选项另行记录。
Release 结果用于捕获、取数和重放链验收。
宿主 Vulkan 校验状态由独立日志判定。
性能测量需要独立运行条件。

原游戏的数据取得需通过正常捕获路径验证。
采集前记录游戏版本、画质和目标帧状态。
已初始化的图形设备需确认捕获工具支持范围。
失败收据与原生资源分析结果共同保留。

原生效果认定需要原游戏事件与资源证据。
夹具的着色器、常量和图像属于引擎证据。
NR0 的相机标定和正式参考仍按所属门禁验收。
