# 有界内容提案与模型接入

> 文档类型：运行说明
> 状态：实现，阶段验收通过
> 更新日期：2026-10-06

## 职责与装配

AzureAI 仅依赖 AzureFoundation。
它拥有请求、帧协议与异步进程生命周期。
编辑器领域适配器负责内容语义和候选差异。
Player 的装配保持独立于模型与编辑器。

内置进程传输的支持平台为 Windows。
其他平台返回可恢复的传输诊断。
公共模型接口可装配平台适用的传输实现。
平台集成按实际执行的测试范围记录。

模型服务通过 `ai.enabled` 显式启用。
每次请求启动独立进程并完成协议握手。
请求携带受控快照和有界历史。
配置、供应方路由与凭据由 Python 工具持有。

## 使用方式

先准备 Python 和版本为 1 的服务配置。
配置中的环境变量名称用于读取凭据。
启动参数选择配置或固定响应夹具。
在控制台面板填写指令并选择内容领域。

```powershell
$env:AZURE_MODEL_KEY = '<本机凭据>'
build/ninja-msvc-release/AzureRender.exe `
  --editor-project '<项目>/project.azureproject' `
  --set ai.enabled=true --ai-python '<Python 路径>' `
  --ai-config '<本机配置>/providers.json'
```

配置格式如下，路径与凭据按本机环境填写。
模式可选 `native`、`json` 或 `prompt`。
端点应提供 chat-completions 请求契约。

```json
{
  "schemaVersion": 1,
  "providers": [{
    "id": "local",
    "kind": "chat-completions",
    "endpoint": "http://127.0.0.1:8000/v1/chat/completions",
    "credentialEnv": "AZURE_MODEL_KEY",
    "model": "configured-model",
    "structuredMode": "json"
  }],
  "profiles": [{
    "id": "content",
    "provider": "local",
    "systemPrompt": "Produce content matching the supplied schema."
  }]
}
```

生成后检查差异与诊断，再选择应用或拒绝。
生成与预览保持制作文档和资产文件。
取消会结束当前请求并释放文档租约。
服务错误返回诊断，人工制作继续可用。

## 领域与生产操作

| 领域 | 候选范围 | 应用语义 |
| --- | --- | --- |
| `scene` | 注册的场景操作白名单 | U3 批量事务形成一次撤销 |
| `asset-parameters` | 注册生成器的文本参数 | 生成服务校验并保存来源 |

场景候选校验字段、数值、引用和操作冲突。
节点目标使用稳定标识并限制修改范围。
资产领域支持 JSON、关卡和 Prefab 文本。
资产写入使用生产生成器及 F5 来源清单。

资产生成属于声明过的文件副作用操作。
其应用语义由生成服务管理。
应用前复核依赖、输出和候选差异。
文件差异发生变化时提案返回过期状态。

菜单、控件和工具共用下列生产操作。

| 操作 | 用途 |
| --- | --- |
| `ai.generate` | 提交领域、目标、指令与请求标识 |
| `ai.describe` | 发现领域、模式和当前报告 |
| `ai.cancel` | 取消生成并释放请求 |
| `ai.reject` | 拒绝候选并保留制作内容 |
| `ai.apply` | 复核版本并应用合法候选 |

观察入口为 `ai.available` 和 `ai.state`。
`ai.proposal` 返回完整 JSON 报告。
报告含基础版本、差异、诊断和修复次数。
控制通道沿用本机令牌与生产操作约束。

## 状态与预算

生成状态经过 Generating 和 Validating。
合法候选进入 Ready，版本变化进入 Stale。
结束状态含 Applied、Rejected 和 Cancelled。
失败状态为 Error，关闭服务显示 Disabled。

| 预算 | 上限 |
| --- | --- |
| 单文档生成请求 | 1 个 |
| 候选操作 | 128 项 |
| 源文本 | 2 MiB |
| 请求帧 | 8 MiB |
| 历史 | 24 条、128 KiB |
| 总期限 | 300000 ms |
| 修复 | 1 次 |

设置可在合法范围内收紧预算。
修复与首次请求共用总期限。
历史字节预算至少为 2，容纳空数组。
文档操作和结果轮询由所属线程执行。

## 接入验证

协议采用 NDJSON 和 JSON-RPC 2.0。
握手声明 `protocolVersion=1`。
流事件携带请求标识与连续序号。
非法帧、模式、预算和版本得到拒绝。

方法支持 `initialize`、`llm.chat` 和 `run.cancel`。
目录方法为 `providers.list` 与 `profiles.list`。
会话通过 `session.create` 和 `session.close` 管理。
`shutdown` 结束服务并释放工作线程。

```powershell
python tools/test_ai_bridge.py
ctest --test-dir build/ninja-msvc-debug `
  -R '^Azure(AI|Editor.Proposal)' --output-on-failure
```

固定响应验证协议、领域规则与控制路径。
本机 HTTP 夹具验证路由、模式和错误脱敏。
真实模型的质量需使用配置和样本单独评估。
阶段证据见[G9 验收](../acceptance/g9/2026-10-06.md)。
