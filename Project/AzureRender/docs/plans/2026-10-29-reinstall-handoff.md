# 重装恢复交接

日期为 2026-10-29，分支为 `engine-dev`。
最近的渲染实现提交为 `53a6c11`。
当前工作区另有未提交的材质类别特化候选。
该候选保持待验，初始化异常尚未解决。

## 备份范围

请整体复制 `D:/Assigment/2609/FYP`。
复制时包含隐藏目录 `.git`。
阶段资产使用 NTFS 硬链接共享同源文件。
迁移方式应保留硬链接，避免展开重复数据。
`build/runs` 保存本机资产和原始截帧。
这些忽略文件需要随本地目录迁移。

GitHub 保存源码、文档及已归档截图。
克隆仓库不能恢复被忽略的模型、纹理和原始 RDC。
现行清理保留这些文件及运行证据。
编译中间产物和导入缓存可在重装后生成。

清理清单位于 `build/maintenance/reinstall-2026-10-29/cleanup.json`。
该目录同时保存 `workspace.patch` 和 Git 状态。
逻辑删除字节与实际空间释放分别记录。
硬链接文件的逻辑大小可能包含重复计数。

## 开发状态

NR0 为 Active，NR1 至 NR12 为 Pending。
完整 SSIM、连续截帧及持续 1080p60 保持待验。
当前抗锯齿成果见[轮廓显示验证](../acceptance/nr/temporal-display-edges.md)。
服饰与 KK 的公共动态夹具通过，薄透明继续验收。

类别特化候选涉及 `mesh.frag` 及角色渲染器。
双配置各通过十项受影响回归。
真实主入口八帧与冻结对照逐字节一致。
模式一平均耗时约为 22.97 毫秒。

四样点启动曾发生 `0xc0000005`。
原始转储指向 TinyGLTF 的 JSON 字符串解析。
带调试器的运行可以完成，但不能据此判定修复。
候选暂未提交，恢复后先定位该故障。

## 本机证据

当前任务目录为：

```text
Project/AzureRender/build/runs/20261029-071130-main-character-joint-budget-2cddc4/
```

`continuation.json` 记录排查上下文。
`original-crash.dmp` 保存首次异常现场。
`original-crash-analysis.json` 保存解析结果。
`profile` 保存真实 GPU 捕获及主入口截图。

`baseline-capture` 和 `class-capture` 保存八帧对照。
`asan-loader` 保存隔离诊断源码和日志。
完整加载诊断已完成至少七轮，后续主动停止。
尚无足够证据确认原始异常的根因。

## 恢复步骤

1. 将完整备份复制到 D 盘目标位置。
2. 阅读根目录 `AGENT.md` 和源码导航文档。
3. 检查 `git status`，保留未提交候选。
4. 核对清理清单及原始资产路径。
5. 安装 Visual Studio C++、Vulkan SDK 和 Python。
6. 根据项目构建说明重新配置 CMake。
7. 先验证加载故障，再继续材质及 NR0 至 NR12。

旧 CMake 缓存包含原设备的绝对路径。
重装后的构建目录应重新配置。
保留的依赖目录可用于核对版本与离线资源。
编译和缓存继续使用 D 盘。

GPU 程序串行执行，编译并行度使用二。
构建活动期间保持源码及着色程序稳定。
验收通过后同步文档、截图及独立提交。

## 归档与校验

维护目录提供 `make_backup.py`。
脚本创建保留硬链接的压缩归档，并拒绝覆盖现有文件。
输出位置应位于源目录之外，且具有足够空间。
归档前会检查目录连接及符号链接。

```powershell
python Project/AzureRender/build/maintenance/reinstall-2026-10-29/make_backup.py D:/Assigment/2609/FYP E:/FYP-backup.tar.gz
```

示例中的 E 盘应替换为实际备份盘。
当前仅验证了脚本的小样本硬链接行为。
完整归档尚未创建，需在目标备份盘执行。
备份后应验证归档可读，并抽查模型及原始 RDC。

用户粘贴的审计文本和两张原始截图已另行保存。
位置为维护目录的 `external-references`。
文档校验脚本同时保存在维护目录。
重装后可读取本页继续定位未提交候选。

## 新机环境恢复

环境包位于维护目录的 `environment`。
其中保存工具版本、技能副本和脱敏配置。
`skills-manifest.json` 提供逐文件 SHA256。
`toolchain-versions.json` 记录本机版本基线。

| 组件 | 本机版本 | 恢复用途 |
| --- | --- | --- |
| Python | 3.11.9 | 检索、资源工具及验证 |
| CMake / Ninja | 3.20.5 / 1.13.2 | 构建配置及调度 |
| MSVC | 14.44.35207 | x64 C++ 编译 |
| Vulkan SDK | 1.4.350.0 | 头文件、库及着色程序工具 |
| Git / GitHub CLI | 2.51.0 / 2.96.0 | 版本管理及远程操作 |
| Node / uv | 22.20.0 / 0.11.2 | 可选工具运行环境 |
| .NET SDK | 9.0.319 | 托管工具开发 |
| RenderDoc | 1.46 | 真实截帧及 GPU 分析 |

完整工具安装包尚未打入环境包。
首次恢复需要网络及安装权限。
安装 Visual Studio 2022 的桌面 C++ 工作负载。
包含 MSVC x64 工具和 Windows SDK。

将工具、下载、依赖和构建目录安排在 D 盘。
Vulkan SDK 优先使用表中的已验版本。
vcpkg 使用版本清单中的提交。
其内置基线以仓库 `vcpkg.json` 为准。

先在工程目录执行检查：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/reinstall/Restore-Environment.ps1
```

恢复技能和 Python 环境：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/reinstall/Restore-Environment.ps1 -RestoreSkills -PreparePython
```

脚本逐文件核对技能哈希。
已有同名技能保持原有内容。
Python 环境与下载缓存位于 D 盘 `build/reinstall-environment`。
依赖安装按保存的清单执行，失败时立即停止。

从 Visual Studio x64 开发者 PowerShell 配置：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/reinstall/Restore-Environment.ps1 -Configure -VcpkgRoot D:/Tools/vcpkg -VulkanSdk D:/Tools/VulkanSDK/1.4.350.0
cmake --build build/reinstall-msvc-debug -j 2
```

示例路径须与实际安装位置一致。
脚本使用独立构建目录，重新生成配置。
本机已通过脚本检查及技能完整性验证。
新机安装、Python 下载和构建仍待实际验收。

## 技能与工具恢复

技能包包含以下六项：

- `systematic-debugging`：故障定位。
- `test-driven-development`：实现与测试流程。
- `verification-before-completion`：提交前验证。
- `developer-documentation`：开发文档维护。
- `writing-plans`：实施计划维护。
- `openai-docs`：官方产品及配置资料。

技能恢复目标为用户目录 `.agents/skills`。
技能路径规则见[官方技能说明](https://learn.chatgpt.com/docs/build-skills)。
根目录 `AGENT.md` 持有项目协作规则。
环境包的 `global-AGENTS.md` 供新机合并参考。

Codex 模型偏好为 `gpt-6.1-sol`。
新机须登录相同 ChatGPT 账户并选择可用模型。
实际模型权限以账户和应用显示为准。
登录凭据、会话数据库和令牌由新机重新建立。

`codex-config-sanitized.json` 保存配置参考。
其中的旧用户名路径和临时管道属于设备现场。
应用负责生成本机运行时与通信配置。
请按参考内容逐项核对插件启用状态。

当前启用浏览器、计算机操作、可视化及应用工具插件。
文档、PDF、表格、演示和模板插件也已启用。
这些插件可通过新机 Codex 的插件入口恢复。
外部连接器须通过其授权入口重新登录。

| 工具或服务 | 恢复规则 |
| --- | --- |
| 应用 `node_repl` | 由新机 Codex 生成运行时、路径和通信管道 |
| `tableau` MCP | 依赖另一个项目的脚本，属于可选配置 |
| `unreal-mcp` | 本地地址为 `http://127.0.0.1:8000/mcp`，需对应服务 |
| RenderDoc | 使用保留的便携目录及 Python 接口 |
| Git、Python、PowerShell | 引擎开发的基础工具 |
| `tools/code_query.py` | 按模块进行有界检索和符号定位 |

Tableau 与 Unreal 服务源码属于其他项目。
AzureRender 的编译和渲染验证使用基础工具即可开展。
MCP 配置与授权流程见[官方 MCP 说明](https://learn.chatgpt.com/docs/extend/mcp?surface=cli)。
系统安装提权和账户授权由新机用户完成。

## 新机会话入口

将以下内容发送给新机 Codex：

```text
读取 AGENT.md 和 Project/AzureRender/docs/plans/2026-10-29-reinstall-handoff.md。
检查完整本地环境包、技能哈希、Git 状态和未提交候选。
按交接文档运行恢复脚本，自主补齐 D 盘开发依赖。
保留文档、截图、原始 RDC、模型和未提交修改。
先复现并解决 TinyGLTF 初始化访问异常，再验收材质类别特化。
随后按现有计划持续推进 NR0–NR12，逐项验证真实截帧。
使用有界检索和按需符号地图，限制构建并行度与磁盘占用。
阶段间直接推进；只有硬阻塞才停止并说明所需输入。
```
