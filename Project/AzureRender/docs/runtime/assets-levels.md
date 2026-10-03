# 资产、关卡与 Prefab

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-10-04
> 适用范围：Windows 项目资源与独立 Player
> 源码入口：`src/runtime/AssetDatabase.*`、`Level.*`、`Prefab.*`、`LevelSession.*`
> 关联测试：`AssetLevelTests.cpp`、`test_level_player.py`

## 职责与使用场景

AssetDatabase 为挂载目录中的文件建立 UUID 索引、依赖和导入缓存。Level 保存 JSON 场景与反射组件，Prefab 提供节点复用和实例覆盖。LevelSession 管理启动、切换与热重载。

## 数据与所有权

源文件的 `.azmeta` 侧车持有版本、UUID、导入设置与显式依赖。移动资源时同时移动侧车文件，引用使用 UUID 或项目虚拟路径。文件指纹由内容、导入设置、导入器版本和依赖指纹共同计算。

LevelSession 持有资源数据库与当前关卡，RuntimeLifecycle 持有 World。关卡替换先建立候选 World 和渲染资源，通过验证后提交。加载错误保留当前 World 与渲染器，并保存诊断。

## 生命周期与时序

资源扫描在主线程执行，500 ms 检查一次变化。依赖变化向引用资源传播，候选关卡在帧边界加载。成功热重载重新初始化当前关卡的运行状态。

渲染资源准备等待 GPU 完成信号，成功后释放当前资源。失败准备释放候选资源，当前渲染器继续服务。

## 接口契约

`refresh()` 返回变化的 UUID，失败扫描保留有效索引。`readSource()` 校验缓存，损坏缓存从当前有效源文件重建。依赖缺失、循环、重复 UUID 和越界路径产生错误。

`.azurelevel` 包含版本、关卡标识、渲染器类型、资源、节点和 Prefab 实例。组件对象以稳定类型名称为键，值使用反射存档包。节点父子关系、资源引用与组件数据均在加载时验证。

Prefab 实例包含资源标识、实例名称和覆盖对象。实例名称形成节点前缀，覆盖以源节点标识定位，使用 JSON Merge Patch 合并。源节点标识由 Prefab 持有，覆盖修改属性。

## 线程与同步

数据库、文件观察与关卡提交由主线程管理。运行时系统读取当前 World，录制线程读取不可变渲染快照。加载回调在 World 提交前运行。

## 序列化与兼容

资源侧车、关卡、Prefab 与目录资源包均为版本 1。缓存采用 `AZCACHE1` 标识，随后为小端 64 位指纹、负载长度和原始字节。缓存以 UUID 与指纹定位，有效版本在候选导入失败时保留。导入缓存用于稳定读取与失效判断，GPU 格式由 Render Core 解析。

目录资源包使用 `manifest.azurepack`，保留挂载目录结构、UUID 侧车、内容指纹和依赖。读取验证相对路径与内容指纹，glTF 外部引用沿原目录结构解析。FNV-1a 指纹用于缓存失效判断。

## 平台行为

Windows Player 支持 `.azurelevel` 启动场景，项目挂载沿用 Project 的路径边界。Android 为 Deferred。

## 使用示例

项目文件的 `startupScene` 可设为 `assets:/start.azurelevel`。通过 `AzurePlayer --project project.azureproject --runtime-report runtime.json` 启动并保存最终节点快照。

`AssetDatabase::writePack()` 要求空输出目录。`resolvePack()` 验证资源包条目并返回路径。

## 诊断与排错

UUID 重复时检查侧车复制流程。资源移动后检查源文件与侧车的位置。关卡重载错误从 `LevelSession::lastError()` 读取，修复后下一次成功加载提交新状态。

## 验收与证据

AssetLevel 测试覆盖移动、缓存修复、依赖失效、Prefab 覆盖、循环、资源包和切换事务。LevelPlayer 测试通过实际 Vulkan Player 验证 JSON 关卡与热重载。

## 参考来源

资产数据库、关卡与 Prefab 为项目实现。JSON 使用固定的 nlohmann/json，组件定义使用 G1 反射注册表。外部文件解析沿用 tinygltf 与现行 Render Core。
