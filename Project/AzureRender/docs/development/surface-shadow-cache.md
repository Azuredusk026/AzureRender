# 同帧表面阴影缓存候选

状态：候选隔离，生产采用受检入口。
本页说明精简证据中的候选接口与资源契约。
候选归属于 RenderCore 的表面照明缓存与材质提交。
资源、生产、消费及完整着色回退共用现有渲染生命周期。
普通表面和非积分风格化表面可消费同一机制。

## 载荷与身份

`SurfaceRadianceCache::Payload::ShadowVisibility` 使用两个附件。
第一个附件为 `VK_FORMAT_R32_SFLOAT`。
第二个附件为 `VK_FORMAT_R32G32B32A32_UINT`。
每个像素占二十字节，各帧槽分别持有资源。

身份包含设备深度的原始位值及全局实例编号。
其余两字记录源索引位置与全局材质编号。
图元编号在当前绘制的首索引上累加。
非有限深度或索引溢出生成无效身份。

不透明快照记录完整精度的阴影可见量。
最终绘制按同像素、同身份读取该值。
消费端通过指数位拒绝 NaN 与无穷值。
有效可见量还须位于零至一之间。

缓存读取按单个像素判断。
相邻像素可以分别命中或回退。
接收面导数在统一入口准备。
阴影过滤采集基础层，沿用原有采样与累计规则。

## 配置与资源边界

候选复现使用归档源文件及原基线。
归档中的 CMake 配置使用以下选项：

```powershell
cmake -S . -B build/ninja-msvc-release -DAZURE_ENABLE_SURFACE_SHADOW_REUSE=ON -DAZURE_ENABLE_SURFACE_RADIANCE_REUSE=OFF
```

表面照明缓存每次选择一种载荷。
场景阴影载荷使用单采样附件。
设备须支持图元身份及所需附件格式。
启用诊断时还须支持片元存储原子操作。
资源能力与纹理访问模式由既有配置选择器核对。

积分头发、多样点及雾专用程序使用完整着色。
透明、渐隐及对象主阴影采用原接收计算。
可选程序缺失时加载对应完整程序。
非法程序的创建错误由现有加载流程报告。

资源预算上限为 512 MiB。
`attachmentCount()` 返回当前载荷的附件数。
`attachmentFormat()` 与 `attachmentClearValue()` 使用相同索引。
调用方按有效附件数注册帧图资源及描述符。

身份附件以无效编号清空。
生产和消费通过帧图记录颜色写入及片元读取关系。
帧槽复用前沿用既有完成栅栏。
分配失败时释放部分资源，并使用完整着色。

## 计数与观察

诊断开关为 `AZURE_ENABLE_SURFACE_SHADOW_DIAGNOSTICS`。
显式开启时，使用三十二像素间隔的稀疏网格计数。
计数缓冲随帧槽持有，在完成后回收。
诊断开启与关闭分别持有实际计时。
该网格用于观察覆盖范围，逐像素画面另行比较。

`surfaceShadowReusePrepared` 表示标量载荷已准备。
`surfaceShadowVariantModules` 记录实际创建的可选程序数量。
`surfaceRadianceBytes` 记录表面照明附件的逻辑容量。
`surfaceRadianceStatistics` 保存生产、请求及像素命中数。

该载荷使用 `pixelMatches` 表示实际接受数量。
完整四像素组计数继续属于照明载荷。
归档与成本结果见[候选验收](../acceptance/nr/surface-shadow-reuse.md)。
生产配置和正式验收状态由开发总计划维护。
任务见[同帧表面阴影复用](../plans/2026-11-09-surface-shadow-reuse.md)。
