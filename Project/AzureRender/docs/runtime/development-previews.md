# 开发期重载与独立预览

> 文档类型：设计与操作说明
> 状态：已实现，阶段验收通过
> 更新日期：2026-10-06
> 适用范围：Windows 编辑器与独立预览工具

## 模块与生命周期

RenderCore 定义重载接口和独立视图服务。
AzureDevtools 提供监听、快照和后台编译。
Platform 拥有受控子进程与取消机制。
编辑器通过生产编辑操作调用这些服务。

编辑会话拥有开发操作的注册与模式。
宿主挂接服务回调，关闭时完成解绑。
解绑后的操作返回服务不可用诊断。
同一会话可以挂接新的宿主服务。

Player 链接运行与渲染模块。
开发编译服务由 AzureRender 宿主装配。
视图创建、调度与销毁要求其创建线程。
关闭先取消编译，再等待 GPU 并释放资源。

## 着色器重载

构建目录生成版本 1 的 `shader-reload.json`。
描述记录源码、程序、宏、编译器和候选目录。
源码快照包含依赖文件内容。
调用直接启动编译器，参数保留独立边界。

从工程目录启动开发服务。

```powershell
./build/ninja-msvc-debug/AzureRender.exe --editor-project assets_public/exploration/project.azureproject --shader-reload build/ninja-msvc-debug/shader-reload.json
```

Console 显示编译状态与诊断。
Rebuild shaders 请求完整候选。
状态包含 Idle、Compiling、Ready、Error 和 Cancelled。
重复请求在当前编译或候选内合并。

候选包含完整的 SPIR-V 程序目录。
宿主先加载候选渲染器与预览服务。
GPU 空闲后在帧边界替换场景渲染器。
编译或加载失败保留有效代次。

资源替换范围为注册的场景渲染器。
主视口合成、HUD 与编辑器 UI 按构建加载。
源码描述仍涵盖构建登记的全部程序。
上述宿主 Pipeline 的更新使用完整重建。

源文本预算为 16 MiB，文件数至多 4096。
程序数至多 128，单次编译期限为 60000 ms。
子进程输出预算为 65536 字节。
取消和超时终止所属进程组。

旧候选目录被占用时，状态记录清理诊断。
后续重建重试目录清理。
待清理目录最多保留四项。
达到预算时需释放文件占用，再请求重建。

## 独立视图契约

`RenderViewService` 管理带代次的视图句柄。
每个视图拥有相机、尺寸、设置与历史。
HDR、深度、法线、阴影和输出各自拥有资源。
帧图执行期间保留视图所有权。

宿主每帧推进一次模拟并生成不可变快照。
主视口和辅助视图共同消费该快照。
渲染图登记和执行期间保持渲染器生命周期。

`create()` 创建视图，`request()` 请求一帧。
重复请求在同帧内合并。
静态结果完成后暂停渲染。
`resize()` 等待旧提交完成，再构建完整资源。

待调尺寸视图暂停图像采样。
旧提交完成后，后续请求使用新的尺寸。

`release()` 立即使句柄失效。
GPU 完成后销毁在途资源。
已缓存图像的每次采样也登记使用提交。
句柄在服务重建后保持失效。

`developer.maxViews` 默认是 3，可选 1 至 3。
在途退役视图计入容量。
尺寸范围为 1 至 4096，两个轴分别校验。
相机要求有限数值和有效观察方向。

预览输出消费曝光、色调映射、饱和度和对比度。
输出同时消费颜色偏移及线性或 sRGB 传递。
场景渲染器拥有自身的光照与时间算法。
主视口后处理效果具有独立的合成生命周期。

## 资产、相机与截图

Content Browser 的 Grid 按可见范围请求模型。
每帧最多请求两个模型缩略图。
缓存键包含资产 ID、来源指纹与预览设置。
相同键复用已完成的静态结果。

模型缩略图消费渲染器的包围盒适配。
独立模型按包围盒居中并缩放。
场景视图使用其场景世界坐标。

Capture 面板的 Camera preview 请求相机视图。
面板隐藏时停止请求新帧。
Capture Camera 保存独立视图的 PNG。
标签使用字母、数字、连字符或下划线。

生产操作包含 `preview.create`、`preview.request`。
检查与调尺寸使用 `preview.inspect`、`preview.resize`。
释放使用 `preview.release` 或 `preview.clear`。
图像回读通过 `preview.capture` 完成。

`preview.release` 释放 `preview.create` 创建的视图。
相机由 `preview.camera` 的停用操作释放。
缩略图由缓存失效或 `preview.clear` 统一释放。
共享缓存的句柄由所属服务管理生命周期。

`preview.asset` 接收数据库 ID 或场景资源别名。
`preview.camera` 接收启用状态和可选相机参数。
`developer.describe` 返回状态、代次和视图报告。
重建与取消分别有独立生产操作。

资产指纹变化使缩略图换代。
文档版本、Play/Stop 和场景切换使预览失效。
窗口交换链重建同时清理关联缓存。
后续请求根据现行资源与文档建立视图。

## 独立工具工作流

在可写目录创建 `views.json`。
配置版本为 1，视图数组最多包含三个条目。
以下示例请求一个相机与独立调色结果。

```json
{
  "schemaVersion": 1,
  "views": [{
    "renderer": "blackhole",
    "extent": [192, 128],
    "position": [-12, 8, 23],
    "target": [-7, 4.5, 0],
    "transfer": "srgb",
    "grade": {"exposureEv": -1, "saturation": 0.8}
  }]
}
```

从工程目录运行工具。
将描述路径替换为创建文件的位置。
报告与图像写入报告所在目录。

```powershell
./build/ninja-msvc-debug/AzureRender.exe --asset assets_public/test_model.gltf --preview-views views.json --runtime-report build/preview/runtime.json --smoke-frames 24 --fixed-frame-step
```

运行结果包含 `developer.views`。
`preview-images/0.png` 对应该次视图。
相机与调色的实际像素差异有 GPU 夹具验证。
正常退出应报告零缓冲与零图像。

## C++ 热编译评估

参考实现以可选 Live++ 同步代理进入帧边界。
接口包含 Startup、BeginFrame、RequestReload 和 Shutdown。
代理拥有 Broker 连接及当前模块登记。
完整源码版本与路径见[来源评估](../research/2026-10-06-gknextengine.md)。

C++ 补丁要求一致的工具链、符号与模块 ABI。
对象布局变化需要重建对象或迁移状态。
帧任务与 GPU 回调要求明确的执行屏障。
代理分发还需独立确认其 SDK 许可与运行条件。

本阶段适配着色器重载和服务边界。
C++ 代码更新采用构建与进程重启。
Live++ 保留为经过接口评估的后续候选。
其成本包括工具接入、状态迁移和额外生命周期验收。

## 验证入口

契约测试覆盖候选失败、取消和文件占用。
视图测试覆盖容量、失效、调尺寸与失败释放。
真实编辑器夹具使用探索和检视两个项目。
独立工具覆盖三渲染器、相机与调色图像。

```powershell
ctest --test-dir build/ninja-msvc-debug -R 'AzureRender.(HotReload|RenderView|PreviewHost)|AzureEditor.(Thumbnail|DevtoolsHost)|AzurePlatform.ProcessRunner' --output-on-failure
```

阶段状态以[开发总计划](../plans/azure-engine-plan.md)为准。
完整回归、设备与性能通过后登记阶段验收。
