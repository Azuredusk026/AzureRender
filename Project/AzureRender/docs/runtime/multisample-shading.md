# 多样点渲染硬件契约

RHI 提供二维多样点图像的能力查询与通道声明。
图像通过既有 `IGpuAllocator::createImage` 创建。
调用者在 `VkImageCreateInfo::samples` 声明样本数。
单样点图像继续使用所属分配入口。

## 通道与管线

`RenderPassAttachmentDesc::samples` 默认值为一。
同一子通道的有效附件使用相同样本数。
合法样本数为一至六十四的二次幂。
无附件通道由所属管线声明样本数。

`GraphicsPipelineDesc::rasterizationSamples` 默认值为一。
管线与已登记的附件通道须具有相同样本数。
`minSampleShading` 是零至一的有限比例。
零选择管线默认着色策略，一请求完整逐样点着色。

正的着色比例依赖已启用的逐样点能力。
片元着色器的样点输入依赖相同设备能力。
配置错误在创建 GPU 管线前拒绝。
单样点通道继续消费默认声明。

## 能力与所有权

`IRhi::imageSampleCounts` 按格式和用途返回样本位掩码。
Vulkan 查询采用二维、最优平铺的图像条件。
`supportsSampleShading` 表示后端实际启用的能力。
调用者在分配及选择着色器前消费这些查询。

Vulkan 后端记录自身创建的通道样本数。
登记、读取及销毁通过互斥保护。
外部提供的通道继续消费所属创建者的约束。
创建者持有 GPU 对象并按原有生命周期销毁。

记录后端可配置样本位及逐样点能力。
默认能力为单样点。
管线记录包含实际样本数与着色比例。
图像估算统计计入每个样本并在释放时归零。

## 接入范围

离屏 GPU 探针核对标准样点插值与透明纹理裁剪。
实际驱动参数、线性颜色解析及资源释放共同检查。
标准位置依据 [Vulkan 光栅化规范](https://docs.vulkan.org/spec/latest/chapters/primsrast.html)。
完整接入按[四样点着色计划](../plans/2026-10-24-multisample-shading.md)执行。

主辅视图装配、材质逐样点着色及描边按计划接入。
正式画质仍消费设置、资源与联合质量门禁。
RHI 能力验证范围见[专项验收](../acceptance/nr/multisample-rhi.md)。

## 场景目标与解析

`SceneMultisampling` 持有所属视图的四样点源。
源附件顺序为 HDR、深度及第二颜色附件。
源槽数量为一或二，目标图像及视图由调用方持有。
替换或销毁前须退役录制及 GPU 工作。

HDR 使用 `R16G16B16A16_SFLOAT`。
深度使用 `D32_SFLOAT`，解析取四样点最近值。
第二颜色支持 `R8G8B8A8_UNORM` 的平均法线。
`R32_SFLOAT` 可声明平均或最近线性深度。

解析在单样点目标上执行一个全屏图形通道。
解析颜色保持线性 HDR，随后进入曝光及调色。
光栅深度通过 `gl_FragDepth` 写入最近表面。
资源图消费三个源附件的实际生产依赖。

每源槽每像素估算六十四字节。
两槽在 1080p 下估算 265,420,800 字节。
单视图源预算为 512 MiB，分配前检查面积及格式。
分配器实际字节另行记录，并校验相同预算。

`byteSize` 返回源图像的格式估算。
`allocatedByteSize` 返回所属分配器的实际统计差值。
调用线程须独占该视图的创建及销毁过程。
记录后端按自身估算统计，Vulkan 返回实际分配。

源附件、解析附件及样本数在创建时固定。
缺少生产者、错误资源或附件别名在图修改前拒绝。
失败创建须释放全部所属资源。
主辅视图装配与角色质量范围见[场景目标验收](../acceptance/nr/multisample-scene-target.md)。

## 视图质量与版本

渲染设置版本十九提供 `antiAliasing=4`。
命令行使用 `--anti-aliasing multisample`。
控制台使用 `render.antiAliasing=4`。
默认模式为一，正式画质晋升消费联合验收。

完整关卡设置保存版本与整数画质。
模式四要求来源版本至少十九。
模式三要求来源版本至少十八。
部分覆盖可继承目标对象已有的有效画质。

渲染器 API 版本五声明 `supportsMultisampleShading`。
`RenderContext::rasterizationSamples` 表示实际附件样本数。
默认值为一，能力通过后值为四。
管线与着色器选择消费该实际值。

主视图与辅助视图共用公共目标和解析流程。
每个视图独立检查能力、标准位置、格式及预算。
能力不足时采用模式一的空间输出并记录原因。
四样点模式消费独立几何覆盖，其历史分配为零。

辅助视图从自己的质量设置建立实际样本状态。
视口重建等待资源退役，再建立新尺寸的样点目标。
连续在途帧使用不同源槽。
单样点输出继续使用所属视图的完成序号。

诊断记录 `rasterizationSamples` 与 `multisampleBytes`。
`multisampleAllocatedBytes` 记录分配器统计。
`multisampleFallback` 记录空间回退原因。
主辅视图记录 `multisampleSourceSlots`。
`sceneAttachmentFormats` 按颜色、深度及法线记录格式。

已接入的渲染器按自身能力决定可用画质。
角色逐样点材质及描边按 MS3、MS4 接入。
真实角色的动态细节须通过 MS5 的原有门禁。
