# 编辑器资源加载基线验收

本记录对应 NR0-C0 的初次资源同步。
公共资源状态检查已通过独立 CPU 探针。
Debug 与 Release 的构建及会话回归通过。

两配置的编辑宿主和可玩回归也已通过。
可玩项目在 89.156 秒内完成实际启动。
完整场景复刻继续由 NR12 联合验收。

## 根因证据

可玩项目启动日志记录两次完整渲染器装载。
首次装载耗时 82.257 秒。
首次帧同步随后请求资源刷新。
该请求报告的文件变化数量为零。

第二次装载的 CPU 准备耗时约 55.584 秒。
材质处理耗时约 23.147 秒。
该批次未在 180 秒内完成自动通道就绪。
卸载日志记录缓冲与图像资源均为零。

证据见[标准输出](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/playable-import/editor/stdout.log)、[诊断日志](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/playable-import/editor/stderr.log)与[启动条件](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/playable-import/editor/launch.json)。

资源签名比较位于宿主的每帧编辑同步。
首次比较时，已加载场景具有非空签名。
未记录的加载基线会触发显式资源刷新。
文件时间戳检查独立返回真实变化数量。

显式刷新支持场景引用和材质绑定变化。
这些变化可以发生于文件时间戳保持一致时。
因此，文件变化数量不能决定显式刷新语义。

## 公共契约

[资源状态](../../../src/editor/EditorRenderResourceState.hpp)由编辑宿主拥有。
状态仅在宿主主线程访问。
输入为完整的场景文档快照。
该机制同时支持角色场景与无网格场景。

签名包含渲染器类型及资源身份和路径。
节点身份、资源绑定和已解析材质也参与签名。
材质包含槽位、路径与内容指纹。
字段以长度编码，集合记录其元素数量。

加载操作先计算提交快照的签名。
渲染器加载成功后，状态发布该签名。
异常继续传递，既有成功基线保持有效。
加载期间产生的新编辑保留刷新需求。

| 输入或状态 | 同步行为 |
| --- | --- |
| 渲染器尚未成功加载 | 请求加载 |
| 成功加载后的相同快照 | 保持当前资源 |
| 资源路径或节点资源绑定变化 | 请求重载 |
| 材质路径或内容指纹变化 | 请求重载 |
| 渲染器类型变化 | 请求重载 |
| 节点变换或可见性变化 | 使用逐帧场景更新 |
| 显式资源刷新 | 执行完整刷新 |
| 文件时间戳变化 | 保留现有文件检查流程 |

宿主在[渲染器创建](../../../src/app/AzureRenderApp.cpp)中发布成功状态。
[逐帧同步](../../../src/app/AzureRenderGameplay.cpp)查询同一公共状态。
[会话命令](../../../src/editor/EditorSession.cpp)持有显式刷新语义。

## CPU 验证

检查使用 MSVC 独立编译并运行实际公共头文件。
探针使用本机主开发配置的依赖头文件。
此项验证未启动 Ninja 或 GPU。
[结构化收据](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/editor-resource-state-cpu-checks.json)保存输入与程序摘要。

敏感性检查使用独立的临时头文件。
其中仅删除成功加载后的基线发布语句。
该候选编译成功，运行退出码为 1。
诊断明确指出首次同步未保持空闲。

生产头文件使用同一组检查。
其编译与运行退出码均为 0。
源码及检查文件的 SHA-256 在执行中保持一致。

| 证据 | 路径 |
| --- | --- |
| 回归检查 | [公共检查](../../../tests/EditorRenderResourceStateChecks.hpp) |
| 集成入口 | [编辑会话测试](../../../tests/EditorSessionTests.cpp) |
| 独立执行器 | [验证脚本](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/verify_editor_resource_state.py) |
| 独立入口 | [探针源码](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/editor_resource_state_probe.cpp) |
| 预期失败探针 | [缺失基线候选](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/editor-resource-missing-baseline.exe) |
| 通过探针 | [生产候选](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/editor-resource-production.exe) |

在引擎目录执行以下命令，可复现 CPU 检查。

```powershell
python build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/verify_editor_resource_state.py
```

## 构建与会话测试

本批使用 MSVC 14.44 的 x64 环境。
Debug 与 Release 依次执行构建和会话测试。
完整命令见[构建脚本](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/resource-sync-build.cmd)。
以下命令均从引擎目录执行。

```powershell
cmake --build build/ninja-msvc-debug --target AzureRender AzurePlayer AzureRenderEditorSessionTests -j 3
ctest --test-dir build/ninja-msvc-debug -R "^AzureRender.EditorSession$" --output-on-failure
cmake --build build/ninja-msvc-release --target AzureRender AzurePlayer AzureRenderEditorSessionTests -j 3
ctest --test-dir build/ninja-msvc-release -R "^AzureRender.EditorSession$" --output-on-failure
```

| 配置与操作 | 结果 | 实测耗时 | 日志 |
| --- | --- | --- | --- |
| Debug 构建 | 退出码 0，三个目标链接成功 | 未单独记录 | [构建日志](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/resource-sync-debug-build.log) |
| Debug 会话测试 | 退出码 0，1/1 通过 | 用例 0.74 秒，总计 0.77 秒 | [测试日志](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/resource-sync-debug-test.log) |
| Release 构建 | 退出码 0，三个目标链接成功 | 未单独记录 | [构建日志](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/resource-sync-release-build.log) |
| Release 会话测试 | 退出码 0，1/1 通过 | 用例 0.18 秒，总计 0.22 秒 | [测试日志](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/resource-sync-release-test.log) |

四份日志的 SHA-256 如下。

| 日志 | SHA-256 |
| --- | --- |
| `resource-sync-debug-build.log` | `33fabdee8c7eaf79a714eef59c35698332d623062f8682198b9febfa886d81ea` |
| `resource-sync-debug-test.log` | `98021e296001002dd240a4ef60985065c57588851a6af0537f16c61367776c19` |
| `resource-sync-release-build.log` | `1d7ad531a48c1129d940c0b8eb4061b138834a6c12f305e882b0582afa44548b` |
| `resource-sync-release-test.log` | `403d87745195a432336f0700c756d4c3a8537d02e739f057b05fcb397e5c587f` |

本批 Release 编辑宿主已用于可玩追加重试。
[启动收据](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/playable-import-v2/editor/launch.json)记录相同的 180 秒就绪预算。

## 编辑宿主与可玩回归

主任务串行执行两配置的实际 GPU 回归。
范围为 `AzureEngine.EditorHost` 和 `AzureEngine.EditorPlayable`。
Debug 与 Release 均为 2/2 通过。
全部用例的失败数量为零。

| 配置 | 编辑宿主 | 可玩预览 | 总耗时 | 日志 |
| --- | --- | --- | --- | --- |
| Debug | 11.24 秒 | 97.82 秒 | 109.09 秒 | [GPU 回归日志](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/resource-sync-debug-gpu-tests.log) |
| Release | 5.46 秒 | 14.33 秒 | 19.83 秒 | [GPU 回归日志](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/resource-sync-release-gpu-tests.log) |

Debug 日志的 SHA-256 为：

`2845a05427b6f522d82d8e74b3cebca631275ecc83fe8ef18b07b481491eea1b`

Release 日志的 SHA-256 为：

`391c390a4aa6b2c9ae04d77327e601f8bab8a29af1bd913ed6cd7422f2361fc2`

## 原生项目实测

可玩项目的实测启动耗时为 89.156 秒。
预算保持 180 秒，自动通道已返回就绪。
就绪响应位于场景导入请求之前。
首次冗余刷新与零文件变化刷新均为零次。

导入触发一次真实资源刷新，变化数量为 26。
渲染器完整重载记录也恰好一次。
该重载耗时 75.103 秒。
自动任务完成，生产保存操作成功。

本批进程渲染 46 帧。
卸载后缓冲与图像资源均为零。
脚本与表现错误列表为空。
[时间线收据](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/playable-import-v2/reload-timeline-verification.json)保存事件顺序和来源摘要。

### 碰撞节点保存语义

原始字段保持检查记录为失败。
唯一差异节点为 `native-railing-collision`。
该节点的稀疏输入具有空变换数据。
原始刚体形状和碰撞网格引用保持一致。

生产加载器将其解释为无父节点、无渲染资源。
其变换为单位变换，渲染可见性为关闭。
编辑保存会显式编码这些字段及图元索引零。
本次原始字段失败继续单独保留。

语义验证使用隔离工程和真实碰撞网格。
检查执行实际公共加载、安装、保存和重开。
比较范围含节点、变换、渲染与刚体全部字段。
保存输出精确复现观测到的默认字段。

公共操作包含 `EditorContext::openProject` 和 `Level::load`。
运行安装使用 `RuntimeLifecycle::loadScene` 和 `installComponents`。
序列化使用 `EditorContext::levelDocument` 和 `EditorContext::save`。
各次加载后的运行及物理语义完全相等。

该探针编译和执行退出码均为零。
执行耗时 0.390 秒，全程仅使用 CPU。
[语义收据](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/playable-import-v2/collision-canonicalization-verification.json)绑定原始输入、输出和程序摘要。
[公共接口探针](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/verify_collision_canonicalization.cpp)保留实际调用路径。

### 保存后几何

新增补包包含 67 个实体和 90 个绘制单元。
保存后检查覆盖 26 份资源和 15216 个实例顶点。
最大世界坐标误差为 2.463 微米。
全部绘制满足 100 微米门槛。

旧补包复验覆盖 808 个绘制单元。
其全部 507605 个实例顶点通过同一门槛。
旧远景锚点的最大误差为 67.621 微米。
锚点和父节点的原始字段保持一致。

[可玩世界坐标收据](../../../build/runs/20261021-172156-native-render-evidence-623c52/rear-corridor-extension/playable-world-verification.json)独立记录三种状态。
几何检查及语义保持检查通过。
原始字段保持检查仍为失败。
该收据绑定验收时的场景摘要。

## 回归范围

CPU 检查覆盖首次同步、资源引用与材质变化。
失败加载检查异常传播及待重试状态。
第二种场景使用无网格的 Blackhole 渲染器。
逐帧变换、可见性及签名字段边界也有检查。

完整构建覆盖 Debug 与 Release 的编辑宿主。
现有 `AzureRender.EditorSession` 两配置均通过。
显式刷新与场景导入保留独立操作回归。
Player 构建覆盖编辑模块的条件编译边界。

实际启动满足 180 秒就绪预算。
首次成功装载后未触发冗余完整重载。
可玩追加已完成生产保存和全顶点检查。
后续画面、裁剪和场景完整性按各自门禁验收。
