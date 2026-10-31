# 源码导航与检索

本页用于定位当前实现、相关测试和构建目标。
命令在 `Project/AzureRender` 目录执行。
模块契约见[引擎基础](../runtime/engine-foundation.md)。

## 目录职责

| 内容 | 目录 | 使用方式 |
| --- | --- | --- |
| 当前实现 | `src/`、`shaders/`、`managed/` | 定位类型、生产调用和着色逻辑 |
| 回归与开发工具 | `tests/`、`tools/` | 查找验证入口和生成流程 |
| 构建与数据契约 | `CMakeLists.txt`、`cmake/`、`schemas/` | 核对目标、依赖和数据格式 |
| 使用与设计说明 | `docs/runtime/`、`docs/tutorials/`、`docs/architecture/` | 理解现行契约和操作 |
| 任务与历史证据 | `docs/plans/`、`docs/acceptance/`、`docs/history/` | 查询范围、来源和实际验收 |
| 构建与本机内容 | `build/`、`showcase/`、`captures/` | 按任务明确的路径查询 |

## 模块定位

下表列出源码入口和主要构建目标。
具体文件归属与依赖以 `CMakeLists.txt` 为准。

| 任务 | 优先目录 | 主要目标 |
| --- | --- | --- |
| 配置、诊断与资源定位 | `src/foundation/`、`src/diagnostics/`、`src/resources/` | `AzureFoundation` |
| 平台与进程 | `src/platform/` | `AzurePlatform` |
| 渲染与设备抽象 | `src/render/`、`src/rhi/`、`src/scenes/`、`shaders/` | `AzureRenderCore` |
| 模型与材质数据 | `src/assets/` | `AzureRenderCore` |
| 世界、场景文档与材质文档 | `src/runtime/`、`src/ecs/` | `AzureRuntime` |
| 类型注册 | `src/reflection/` | `AzureReflection` |
| 脚本服务与后端 | `src/scripting/` | `AzureScriptingHost`、`AzureLuaScripting` |
| 编辑器与公共界面 | `src/editor/`、`src/ui/` | `AzureEditor`、`AzureUi` |
| 应用装配与帧循环 | `src/app/`、`src/player/` | `AzureRenderHost`、`AzurePlayerHost` |
| 自动验证服务 | `src/validation/` | `AzureValidation` |

## 检索顺序

`tools/code_query.py` 提供有界查询。
默认范围为 `src/`、`shaders/` 和 `managed/`。
结果上限为 20 项及 12,000 个 Unicode 字符。
字符预算覆盖完整 JSON 与末尾换行。

```powershell
python tools/code_query.py search RenderGraph --module src/render --json
python tools/code_query.py read src/render/RenderGraph.hpp --start 1 --lines 80 --json
python tools/code_query.py search MaterialDocument --scope tests --json
```

`--scope` 选择生产、测试、工具、说明、证据或计划。
对应值为 `source`、`tests`、`tools`、`docs`、`evidence`、`plans`。
`--module` 在该范围内指定子目录。
`private` 与 `artifacts` 范围支持明确文件路径的读取。

查询包含未跟踪且符合 Git 忽略规则的源码。
单文件快照上限为 4 MiB。
结果包含当前文件 SHA-256，截断带有明确提示。
超长行使用 `partialLine` 标记。

`--max-chars` 可在 700 至 12,000 之间设置。
`--timeout` 默认十秒，上限三十秒。
退出码零表示命中，一表示空结果，二表示执行错误。
目录联接与越界路径按错误处理。

1. 按模块定位表选择目录。
2. 用文件名或符号找出候选文件。
3. 读取声明、相关函数和调用位置。
4. 在 `tests/` 与 `tools/` 中查找验证入口。
5. 根据问题进入设计说明或指定验收目录。

查找材质文档相关文件：

```powershell
rg --files src tests -g '*MaterialDocument*'
```

检索渲染图在生产代码中的使用：

```powershell
rg -n -F 'RenderGraph' src/render src/rhi src/scenes
```

核对对应测试的构建入口：

```powershell
rg -n -F 'RenderGraphTests' CMakeLists.txt
```

检索结果先查看文件名与行号。
需要实现细节时，再读取相关函数与邻近上下文。
结果被截断时，应缩小目录或补充符号。
错误和空结果分别判断。

## 按需符号地图

安装固定版本的语法解析依赖：

```powershell
python -m pip install --only-binary=:all: --target build/qa-python/code-intelligence -r tools/requirements-code-intelligence.txt
python tools/code_query.py symbols RenderGraph --json --timeout 30
python tools/code_query.py map RenderGraph --module src/render --depth 1 --json
```

`symbols` 按名称返回类型、别名、函数及签名。
默认返回八项，`--limit` 上限为二十。
`map` 返回定义文件和本地包含关系的邻接文件。
默认返回五个文件，每个文件至多四项符号。

地图深度为零至二，结果共享完整响应预算。
名称已知时使用 `symbols`，明确文本使用 `search`。
需要理解文件关系时再读取地图。
首次索引可使用三十秒期限。

索引支持 C/C++、Python 与 C#。
着色器采用 C++ 兼容语法近似提取。
`syntaxErrors` 标记语法恢复，相关结果需读取源码核对。
Lua 等未覆盖文件通过文本检索定位。

缓存位于 `build/code-intelligence/`。
查询逐文件重新核对内容哈希，复用有效的解析结果。
同长度、同修改时间的内容变化仍触发更新。
解析器版本和提取实现变化会使缓存更新。

目录边界、未跟踪源码和删除重命名共用查询契约。
索引字段记录指纹、解析数量、覆盖范围和后端版本。
宏、重载匹配和真实调用关系由编译器语义决定。
当前地图的 `precision` 为 `syntax`。

依赖版本、来源与许可见[依赖记录](code-intelligence-dependencies.json)。
原生依赖不可用时，工具报告安装入口。
文本检索与文件读取可独立运行。

## 证据与索引边界
历史源码快照用于解释对应候选的验证结果。
当前实现从 `src/` 等生产目录定位。
证据检索按阶段、日期或报告给出的路径展开。
私有素材与本机参考输入按任务范围访问。

源码导航描述模块职责。
行号与符号位置通过当前文件查询。
后续代码索引需记录来源指纹与输出预算。
索引结果用于定位，修改依据来自当前源码。

目录规模和实装建议见[结构与检索优化计划](../plans/2026-10-26-repository-structure-and-retrieval.md)。
产物保留与清理见[产物管理](artifact-retention.md)。
证据盘点、归档与恢复见[操作说明](evidence-operations.md)。

## 仓库结构审计

`tools/audit_repository.py` 按 Git 工作区根目录审计。
`--source` 可指定仓库或其子目录。
文档、跟踪状态及忽略规则共用同一根目录。
结果记录于调用方指定的输出文件。
