# 渲染图运行时说明

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-10-01
> 适用范围：公共帧与场景 Renderer 的资源使用声明和顺序编译
> 源码入口：`src/render/RenderGraph.hpp`、`src/render/RenderGraph.cpp`
> 关联测试：`tests/RenderGraphTests.cpp`

## 职责与使用场景

`RenderGraph` 保存帧内资源和 Pass 的读写声明，并生成满足依赖的执行顺序与 RHI 屏障。Character 注册蒙皮、阴影和主场景 Pass；Blackhole 注册阴影清理、光线追踪、Bloom、历史累积和场景合成 Pass。宿主再加入后处理、HUD、编辑器界面、捕获和 Present Pass。Pass 回调仍负责调用 RHI 记录具体绘制、计算和复制命令。

## 数据与所有权

资源和 Pass 声明由图对象按整数 ID 持有。调用方通过导入描述提供初始图像或缓冲区状态，图对象不接管 Vulkan 资源所有权。编译结果包含 Pass 执行顺序、图像屏障和缓冲区屏障，下一次编译会替换上一份结果。

## 生命周期与时序

每帧导入公共附件和场景缓冲区，注册 Pass，声明读写关系，调用 `compile`，再由 `execute` 按依赖顺序记录屏障并运行 Pass 回调。宿主的瞬态资源池管理捕获回读缓冲区，并在帧提交完成后复用；Blackhole 历史图像由场景 Renderer 管理其跨帧生命周期。

## 接口契约

`addResource` 与 `addPass` 返回稳定的图内 ID。`read` 与 `write` 校验 ID，非法 ID 抛出 `std::out_of_range`。`compile` 成功时返回 `true` 并清空错误字符串；失败时返回 `false` 并保留诊断文本。

## 线程与同步

图对象只在帧编译线程修改。编译完成后，录制线程按执行顺序应用 RHI 图像与缓冲区屏障，再运行对应 Pass 回调。图像屏障包含旧布局、新布局、阶段和访问掩码；缓冲区屏障包含阶段和访问掩码。

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

`AzureRender.RenderGraph` 验证写入者先于读取者、图像与缓冲区状态转换按执行顺序生成、同一 Pass 的冲突状态声明被拒绝，且非法 ID 被拒绝。Character、Blackhole 与公共后处理的 Pass 声明都由运行时图编译。

## 参考来源

Piccolo RenderScene 的 pass 与资源组织方式用于概念校对；本实现保留 Azure Engine 自有 ID、测试和错误接口。
