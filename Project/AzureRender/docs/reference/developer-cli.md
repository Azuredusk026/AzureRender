# 开发命令参考

> 文档类型：命令参考
> 适用范围：Windows x64 本机开发

## 配置入口

`tools/azure.py` 提供统一开发入口。
构建目录与配置来自 `CMakePresets.json`。
命令从工程目录执行。
Windows 默认选择 `msvc-release`。

```powershell
python tools/azure.py doctor --json
python tools/azure.py describe --json
python tools/azure.py build --preset msvc-debug --json
python tools/azure.py test --preset msvc-debug --json
python tools/azure.py provenance --preset msvc-debug --json
```

`doctor` 检查当前工具路径并报告配置。
MSVC 构建由 `tools/msvc_env.bat` 提供环境。
完整构建消费现有来源记录工具。
指定目标时按 CMake 目标执行局部构建。

完整来源记录由完整构建生成。
`provenance` 检查记录与当前输入是否一致。
构建和发布持有对应配置的文件锁。
异常退出留下的锁由开发者核对进程后清理。

## 命令集合

| 命令 | 输入与作用 |
| --- | --- |
| `doctor` | 报告 Python、CMake 与构建配置 |
| `build` | 配置并构建，可指定 `--target` |
| `run` | 运行指定宿主，透传 `--` 后参数 |
| `test` | 执行 CTest，透传筛选与测试参数 |
| `describe` | 发现命令、配置或本机宿主操作 |
| `query` | 查询宿主注册的只读状态 |
| `edit` | 执行携带基础版本的编辑请求 |
| `shot`、`capture` | 安排宿主截图 |
| `validate` | 执行版本化验证脚本 |
| `package` | 执行现有完整发布门禁 |
| `provenance` | 验证构建来源与产物 |

运行目标为 `AzureRender`、`AzurePlayer` 或 `AzureMetaGen`。
构建还支持 `AzureRenderShaders`。
目标与配置不合法时返回非零状态。
`--json` 保持最终结果为结构化输出。

```powershell
python tools/azure.py run --preset msvc-debug --target AzureRender -- --editor-project assets_public/scene_inspector/project.azureproject
python tools/azure.py test --preset msvc-debug -- --tests-regex AzureEngine.Observation
```

## 本机控制

宿主通过显式选项启用控制通道。
令牌由指定环境变量提供。
端点文件只保存版本、地址与端口。
控制通道绑定 `127.0.0.1`。

```powershell
$env:AZURE_CONTROL_TOKEN = '<本机会话令牌>'
python tools/azure.py run --preset msvc-debug -- --editor-project assets_public/scene_inspector/project.azureproject --validation-token-env AZURE_CONTROL_TOKEN --validation-endpoint build/control-endpoint.json
```

在另一终端使用相同令牌环境。
每个编辑请求声明 `requestId` 与 `commandId`。
`baseVersion` 含文档 ID、修订和内容指纹。
`parameters` 遵守宿主发现的操作描述。

```powershell
python tools/azure.py describe --endpoint build/control-endpoint.json --json
python tools/azure.py query --endpoint build/control-endpoint.json --name selection.id --json
python tools/azure.py edit --endpoint build/control-endpoint.json --request build/edit-request.json --json
python tools/azure.py shot --endpoint build/control-endpoint.json --label editor-review --json
```

截图通过正式渲染读回路径执行。
`engine.screenshots` 表示已完成的截图数。
安排截图后可查询该计数确认完成。
默认文件位于资源定位器选择的 `captures/`。

## 验证脚本

脚本包含 `schemaVersion: 1` 与 `steps` 数组。
步骤使用观察、条件等待、生产编辑或输入。
每步报告结果、诊断与实际耗时。
脚本失败使宿主返回非零状态。

```powershell
python tools/azure.py validate --preset msvc-debug --script build/validation.json --report build/validation-report.json -- --editor-project assets_public/scene_inspector/project.azureproject --smoke-frames 32
```

脚本的期限与大小见[观察与内容来源](../runtime/observations-generation.md)。
运行报告保存正式模式及测试选项。
截图与实体键鼠复核分别记录实际范围。
