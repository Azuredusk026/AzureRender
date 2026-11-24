# U16 资产缓存并发发布实施计划

> 文档类型：缺陷修复子计划。
> 状态：机制、并发规划与完整回归通过。
> 执行方式：当前会话原生执行。

## 目标与归属

资产数据库负责缓存文件的完整发布。
同一项目的多个读取和规划进程共享缓存。
每次写入拥有独立临时文件和清理责任。
正式文件仅向读取者暴露完整内容。

实现使用 C++17 和现有文件系统接口。
Windows 发布使用本机的原子替换接口。
其他平台按已有支持范围记录验证状态。
公共资产接口和缓存格式保持兼容。

依据见[界面驻留调查](../acceptance/p5/closure/ui-residency.md)。
主计划见[体验收口计划](2026-10-10-p5-closure-and-user-experience.md)。
问题归属为 `AssetDatabase`，标识为 UX-009。

## 文件与契约

| 文件 | 职责 |
| --- | --- |
| `src/runtime/AssetDatabase.cpp` | 缓存写入、原子发布与失败清理 |
| `tests/AssetLevelTests.cpp` | 临时文件隔离、修复与失败恢复 |
| `docs/acceptance/u16/cache-publication.md` | 原始失败、修复证据与覆盖范围 |
| `docs/plans/authoring-usability-manifest.json` | 缺陷真实状态与验收入口 |

调用方继续使用 `refresh` 和 `readSource`。
缓存内容由源内容及其指纹确定。
发布失败保留异常诊断和数据库事务语义。
元数据生成及项目内容编辑各有独立所有权。

## 全局约束

先收口冻结批次，再修改生产源码。
性能预算、落点范围和长跑时长保持有效。
用户文件和其他未提交增量保持原有状态。
每个独立修复通过对应检查后单独提交。

## 重点失败输入

| 条件 | 验证要求 |
| --- | --- |
| 同一指纹的多个写入进程 | 读取完整缓存且全部规划成功 |
| 既有同名临时路径属于其他操作 | 发布成功且外部内容保持原字节 |
| 缓存内容损坏 | 从合法源内容修复并重新读取 |
| 目标被目录阻挡 | 诊断失败，清理自有临时文件 |
| 源内容在数据库快照后改变 | 拒绝过期发布并保留既有缓存 |

## 修复与验收步骤

- [x] 在资产契约测试中加入临时路径隔离用例。
- [x] 运行现有实现，保存明确失败日志。
- [x] 在缓存所属模块实施最小发布修复。
- [x] 验证临时文件隔离、损坏修复和失败清理。
- [x] 重跑原始三批并行规划及串行对照。
- [x] 核对两配置构建及受影响生产入口。
- [x] 执行两配置回归，保存失败和完整结果。
- [x] 同步机制说明、缺陷清单与变更记录。
- [x] 检查暂存范围并提交独立修复。

并发验证使用独立项目副本。
Windows 读取另验证发布者的删除访问句柄。
独占暂存目录位于缓存根目录。
暂存路径保持在现有文件路径支持范围内。
三个进程分别规划原始平台路线阶段。
验收要求全部进程成功，读取内容正确。
通过结果仅覆盖实际执行的调度和平台。

## 验收命令

以下命令从工程目录执行。
构建使用已配置的 MSVC 环境。
并行规划脚本沿用原始复现流程。

```powershell
ctest --test-dir build/ninja-msvc-release -R AzureEngine.AssetLevel --output-on-failure -j 1
ctest --test-dir build/ninja-msvc-debug -R AzureEngine.AssetLevel --output-on-failure -j 1
ctest --test-dir build/ninja-msvc-release -LE gpu --output-on-failure -j 1
ctest --test-dir build/ninja-msvc-debug -LE gpu --output-on-failure -j 1
```

预期退出码为零，对应行为断言全部通过。
性能和人工门禁使用各自候选的来源记录。
资产格式实施依赖当前阶段全部必需项通过。
