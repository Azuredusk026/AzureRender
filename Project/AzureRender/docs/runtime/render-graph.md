# 渲染图运行时说明

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-09-26
> 适用范围：R2 渲染图基础编译器
> 源码入口：`src/render/RenderGraph.hpp`、`src/render/RenderGraph.cpp`
> 关联测试：`tests/RenderGraphTests.cpp`

## 职责与使用场景

`RenderGraph` 保存帧内资源和 pass 的读写声明，并生成满足写入依赖的执行顺序。公共帧已登记 shadow、scene、post-process 和 editor-ui 四类 Pass；Vulkan 录制仍由现有后端执行，图编译负责统一验证顺序和资源声明。

## 数据与所有权

资源和 pass 由图对象按整数 ID 持有。调用方只保存 ID，不持有图内部对象。编译结果是 pass ID 的连续列表，下一次编译会替换上一份结果。

## 生命周期与时序

每帧创建或清空图，注册资源和 pass，声明读写关系，调用 `compile`，再按 `executionOrder` 录制命令。瞬态资源池已提供 GPU 帧完成后的复用边界；跨帧历史资源继续由场景资源生命周期管理。

## 接口契约

`addResource` 与 `addPass` 返回稳定的图内 ID。`read` 与 `write` 校验 ID，非法 ID 抛出 `std::out_of_range`。`compile` 成功时返回 `true` 并清空错误字符串；失败时返回 `false` 并保留诊断文本。

## 线程与同步

图对象只在帧编译线程修改。编译完成后，录制线程只读取执行顺序和声明。GPU 屏障生成尚未接入，当前由既有 RHI 和 render pass 契约负责同步。

## 序列化与兼容

渲染图不进入资产和场景序列化格式。资源名称用于诊断，不作为持久化标识。

## 平台行为

图编译器使用标准 C++，Windows 可用。Android 适配保持 Deferred。

## 使用示例

```cpp
RenderGraph graph;
const auto color = graph.addResource("scene-color");
const auto pass = graph.addPass("scene");
graph.write(pass, color);
std::string error;
if (!graph.compile(error)) throw std::runtime_error(error);
```

## 诊断与排错

编译失败时先记录错误字符串和所有 pass 的名称、读写资源。ID 越界表示调用方保存了错误的图内句柄。GPU 屏障错误仍按 `docs/runtime/rhi-synchronization.md` 排查。

## 验收与证据

`AzureRender.RenderGraph` 验证写入者先于读取者，且非法 ID 被拒绝。完整 R2 验收将在资源状态、瞬态资源和三个场景 pass 接入后记录。

## 参考来源

Piccolo RenderScene 的 pass 与资源组织方式用于概念校对；本实现保留 Azure Engine 自有 ID、测试和错误接口。
