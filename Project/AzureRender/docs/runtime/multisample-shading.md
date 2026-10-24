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

视图目标、材质逐样点着色及描边按后续阶段接入。
正式画质仍消费设置、资源与联合质量门禁。
RHI 能力验证范围见[专项验收](../acceptance/nr/multisample-rhi.md)。
