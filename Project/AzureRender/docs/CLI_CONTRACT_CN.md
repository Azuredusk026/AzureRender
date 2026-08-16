# AzureRender 命令行契约

> 契约版本：1
> 冻结节点：AR-5.1

## 1. 解析边界

命令行由 `src/app/CommandLine.*` 独立解析，不初始化窗口、Vulkan 或资产。
所有参数先完成语法与组合校验，再执行 `--version`、资源检查、场景操作或渲染。

- 参数按从左到右解析；重复的值参数使用最后一次提供的值；重复开关保持幂等；
- 需要值的参数不得缺值，且下一个 `--` 开头的参数不会被当作值；
- 无符号整数必须完整匹配十进制数字，拒绝负数、尾随字符和类型溢出；
- 未知参数不会被忽略。

## 2. 稳定错误类型

`CommandLineError` 继承 `std::invalid_argument`，并公开稳定的错误分类和相关参数：

| 类型 | 含义 |
|---|---|
| `UnknownOption = 1` | 参数名称未知 |
| `MissingValue = 2` | 值参数没有后续值 |
| `InvalidValue = 3` | 值格式、范围或枚举不合法 |
| `InvalidCombination = 4` | 参数之间的依赖或互斥关系不满足 |

以上错误统一记录到 `cli` 诊断子系统，并返回进程退出码 `2`。资产/场景错误返回
`3`，Vulkan/GLFW 初始化错误返回 `4`，其他运行错误返回 `5`。

## 3. 数值与组合规则

| 参数 | 默认值 | 有效范围/约束 |
|---|---:|---|
| `--width` | 1280 | 64～7680 |
| `--height` | 720 | 64～4320 |
| `--smoke-frames` | 0（关闭） | 提供时必须大于 0 |
| `--capture-frames` | 0（关闭） | 提供时必须大于 0 |
| `--capture-fps` | 60 | 1～240 |
| `--diagnostic-view` | `beauty` | `beauty`、`normal`、`outline`、`shadow` |
| `--scene-type` | `character` | `character`、`blackhole`（可插拔场景渲染器选择） |

- `--capture-dir` 与 `--capture-frames` 必须同时使用；
- `--technical-sequence` 要求至少 5 帧，且帧数必须能被 5 整除；
- `--qa-effect-state` 要求同时提供 `--qa-effect`；
- 任意 `--qa-*` 参数不得与 `--technical-sequence` 组合；
- `--scene`、`--create-scene`、`--editor` 三种入口互斥；
- `--create-scene` 要求同时提供 `--asset`。

QA 参数沿用 RC0 冻结值：Camera 为 `full-body-front`、`face-front`、
`face-three-quarter`、`back-detail`、`lighting-sweep`；Light 为
`neutral-material`、`stylized-key`、`specular-rim`、`rear-emissive`；Effect 为
`toon`、`shadow`、`hair-kk`、`rim`、`specular`、`emissive`、`outline`、
`face-sdf`、`overlay`、`bloom`；Effect State 为 `enabled`、`disabled`、
`isolation`。Isolation 取值以程序 Usage 和 QA 文档为准，未知值在 CLI 阶段拒绝。

契约测试位于 `tests/CommandLineTests.cpp`，覆盖默认值、有效解析、缺值、未知参数、
非法数字、溢出、范围错误和组合冲突。真实进程负向调用用于确认退出码 `2`。
