#include "rhi/NullRhi.hpp"

#include <algorithm>
#include <cstdio>
#include <type_traits>

namespace azurerender::rhi {

namespace {

template <typename Handle>
Handle fakeHandle(const std::uint64_t value) {
    if constexpr (std::is_pointer_v<Handle>) {
        return reinterpret_cast<Handle>(static_cast<std::uintptr_t>(value));
    } else {
        return static_cast<Handle>(value);
    }
}

template <typename Handle>
std::string hexHandle(const Handle handle) {
    char buffer[24];
    if constexpr (std::is_pointer_v<Handle>) {
        std::snprintf(buffer, sizeof(buffer), "%p", static_cast<const void*>(handle));
    } else {
        std::snprintf(
            buffer,
            sizeof(buffer),
            "0x%llx",
            static_cast<unsigned long long>(handle));
    }
    return buffer;
}

}  // namespace

// ---------------------------------------------------------------------------
// NullGpuAllocator
// ---------------------------------------------------------------------------

GpuBuffer NullGpuAllocator::createBuffer(
    const VkDeviceSize size,
    const VkBufferUsageFlags usage,
    const bool hostVisible) {
    (void)usage;
    (void)hostVisible;
    GpuBuffer result;
    result.buffer = fakeHandle<VkBuffer>(nextHandle_++);
    result.size = size;
    hostBlocks_.emplace_back(static_cast<std::size_t>(size));
    result.mapped = hostBlocks_.back().data();
    ++statistics_.bufferAllocations;
    ++statistics_.liveBuffers;
    statistics_.bufferBytes += size;
    statistics_.liveBufferBytes+=size;
    return result;
}

void NullGpuAllocator::destroyBuffer(GpuBuffer& buffer) noexcept {
    if (buffer.buffer != VK_NULL_HANDLE && statistics_.liveBuffers > 0) {
        --statistics_.liveBuffers;
        statistics_.liveBufferBytes-=buffer.size;
    }
    buffer = GpuBuffer{};
}

GpuImage NullGpuAllocator::createImage(
    const VkImageCreateInfo& createInfo,
    const bool hostVisible) {
    (void)hostVisible;
    GpuImage result;
    result.image = fakeHandle<VkImage>(nextHandle_++);
    result.width = createInfo.extent.width;
    result.height = createInfo.extent.height;
    result.mipLevels = createInfo.mipLevels;
    ++statistics_.imageAllocations;
    ++statistics_.liveImages;
    statistics_.imageBytes += static_cast<VkDeviceSize>(createInfo.extent.width)
        * createInfo.extent.height * 4;
    statistics_.liveImageBytes+=static_cast<VkDeviceSize>(result.width)*result.height*4;
    return result;
}

void NullGpuAllocator::destroyImage(GpuImage& image) noexcept {
    if (image.image != VK_NULL_HANDLE && statistics_.liveImages > 0) {
        --statistics_.liveImages;
        statistics_.liveImageBytes-=static_cast<VkDeviceSize>(image.width)*image.height*4;
    }
    image = GpuImage{};
}

GpuImage NullGpuAllocator::createImage2D(
    const std::uint32_t width,
    const std::uint32_t height,
    const VkFormat format,
    const VkImageUsageFlags usage,
    const std::uint32_t mipLevels,
    const bool hostVisible) {
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = std::max(mipLevels, 1U);
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    return createImage(imageInfo, hostVisible);
}

void NullGpuAllocator::flush(
    const GpuBuffer& buffer,
    const VkDeviceSize offset,
    const VkDeviceSize size) {
    (void)buffer;
    (void)offset;
    (void)size;
}

// ---------------------------------------------------------------------------
// NullRhi
// ---------------------------------------------------------------------------

template <typename Handle>
Handle NullRhi::mint(const char* name) {
    const Handle handle = fakeHandle<Handle>(nextHandle_++);
    calls.push_back({name, hexHandle(handle)});
    return handle;
}

void NullRhi::copyBuffer(
    const GpuBuffer& source,
    const GpuBuffer& destination,
    const VkDeviceSize size) {
    calls.push_back(
        {"copyBuffer",
         hexHandle(source.buffer) + " -> " + hexHandle(destination.buffer)
             + " (" + std::to_string(size) + " B)"});
}

void NullRhi::transitionImageLayout(
    const GpuImage& image,
    const VkImageLayout oldLayout,
    const VkImageLayout newLayout,
    const std::uint32_t mipLevels) {
    (void)mipLevels;
    calls.push_back(
        {"transitionImageLayout",
         hexHandle(image.image) + " " + std::to_string(oldLayout) + " -> "
             + std::to_string(newLayout)});
}

void NullRhi::copyBufferToImage(
    const GpuBuffer& source,
    const GpuImage& destination,
    const std::uint32_t width,
    const std::uint32_t height) {
    (void)source;
    calls.push_back(
        {"copyBufferToImage",
         hexHandle(destination.image) + " " + std::to_string(width) + "x"
             + std::to_string(height)});
}

void NullRhi::clearImage(const GpuImage& image) {
    calls.push_back({"clearImage", hexHandle(image.image)});
}

void NullRhi::generateMipmaps(
    const GpuImage& image,
    const VkFormat format,
    const std::uint32_t width,
    const std::uint32_t height,
    const std::uint32_t mipLevels) {
    (void)format;
    calls.push_back(
        {"generateMipmaps",
         hexHandle(image.image) + " " + std::to_string(width) + "x"
             + std::to_string(height) + " mips "
             + std::to_string(mipLevels)});
}

VkImageView NullRhi::createImageView(
    const VkImage image,
    const VkFormat format,
    const VkImageAspectFlags aspect,
    const std::uint32_t mipLevels,
    const std::uint32_t baseMipLevel) {
    (void)image;
    (void)format;
    (void)aspect;
    (void)mipLevels;
    (void)baseMipLevel;
    return mint<VkImageView>("createImageView");
}

void NullRhi::destroyImageView(const VkImageView view) {
    calls.push_back({"destroyImageView", hexHandle(view)});
}

VkSampler NullRhi::createSampler(const SamplerDesc& desc) {
    (void)desc;
    return mint<VkSampler>("createSampler");
}

void NullRhi::destroySampler(const VkSampler sampler) {
    calls.push_back({"destroySampler", hexHandle(sampler)});
}

void NullRhi::executeOneShot(
    const std::function<void(ICommandRecorder&)>& record) {
    NullCommandRecorder recorder;
    record(recorder);
    calls.push_back({"executeOneShot", std::to_string(recorder.calls.size())});
    calls.insert(calls.end(), recorder.calls.begin(), recorder.calls.end());
}

VkShaderModule NullRhi::createShaderModule(const std::vector<char>& code) {
    (void)code;
    return mint<VkShaderModule>("createShaderModule");
}

void NullRhi::destroyShaderModule(const VkShaderModule shader) {
    calls.push_back({"destroyShaderModule", hexHandle(shader)});
}

VkPipelineLayout NullRhi::createPipelineLayout(
    const VkDescriptorSetLayout setLayout,
    const PushConstantRangeDesc* pushConstants) {
    (void)setLayout;
    (void)pushConstants;
    return mint<VkPipelineLayout>("createPipelineLayout");
}

void NullRhi::destroyPipelineLayout(const VkPipelineLayout layout) {
    calls.push_back({"destroyPipelineLayout", hexHandle(layout)});
}

VkPipeline NullRhi::createComputePipeline(const ComputePipelineDesc& desc) {
    if (!desc.shader || !desc.layout) throw std::invalid_argument("Compute pipeline requires shader and layout");
    return mint<VkPipeline>("createComputePipeline");
}
void NullCommandRecorder::bindComputePipeline(VkPipeline pipeline) {
    calls.push_back({"bindComputePipeline", hexHandle(pipeline)});
}
void NullCommandRecorder::bindComputeDescriptorSet(VkPipelineLayout layout, VkDescriptorSet set) {
    (void)layout;
    calls.push_back({"bindComputeDescriptorSet", hexHandle(set)});
}

VkPipeline NullRhi::createGraphicsPipeline(const GraphicsPipelineDesc& desc) {
    const VkPipeline pipeline = mint<VkPipeline>("createGraphicsPipeline");
    calls.back().detail +=
        " cull=" + std::to_string(desc.cullMode)
        + " front=" + std::to_string(desc.frontFace)
        + " blend=" + std::to_string(desc.alphaBlend)
        + " depth=" + std::to_string(desc.depthTest)
        + "/" + std::to_string(desc.depthWrite);
    return pipeline;
}

void NullRhi::destroyPipeline(const VkPipeline pipeline) {
    calls.push_back({"destroyPipeline", hexHandle(pipeline)});
}

VkDescriptorSetLayout NullRhi::createDescriptorSetLayout(
    const std::vector<DescriptorBindingDesc>& bindings) {
    (void)bindings;
    return mint<VkDescriptorSetLayout>(
        "createDescriptorSetLayout");
}

void NullRhi::destroyDescriptorSetLayout(const VkDescriptorSetLayout layout) {
    calls.push_back({"destroyDescriptorSetLayout", hexHandle(layout)});
}

VkDescriptorPool NullRhi::createDescriptorPool(const DescriptorPoolDesc& desc) {
    (void)desc;
    return mint<VkDescriptorPool>("createDescriptorPool");
}

void NullRhi::destroyDescriptorPool(const VkDescriptorPool pool) {
    calls.push_back({"destroyDescriptorPool", hexHandle(pool)});
}

std::vector<VkDescriptorSet> NullRhi::allocateDescriptorSets(
    const VkDescriptorPool pool,
    const VkDescriptorSetLayout layout,
    const std::uint32_t count) {
    (void)pool;
    (void)layout;
    std::vector<VkDescriptorSet> sets(count);
    for (auto& set : sets) {
        set = mint<VkDescriptorSet>("allocateDescriptorSet");
    }
    return sets;
}

void NullRhi::writeDescriptorImage(const DescriptorImageWrite& write) {
    calls.push_back(
        {"writeDescriptorImage",
         "set=" + hexHandle(write.set) + " binding="
             + std::to_string(write.binding)});
}

void NullRhi::writeDescriptorImageArray(
    const DescriptorImageArrayWrite& write) {
    calls.push_back(
        {"writeDescriptorImageArray",
         "set=" + hexHandle(write.set) + " binding="
             + std::to_string(write.binding) + " count="
             + std::to_string(write.elements.size())});
}

void NullRhi::writeDescriptorBuffer(const DescriptorBufferWrite& write) {
    calls.push_back(
        {"writeDescriptorBuffer",
         "set=" + hexHandle(write.set) + " binding="
             + std::to_string(write.binding)});
}

VkRenderPass NullRhi::createRenderPass(const RenderPassDesc& desc) {
    (void)desc;
    return mint<VkRenderPass>("createRenderPass");
}

void NullRhi::destroyRenderPass(const VkRenderPass renderPass) {
    calls.push_back({"destroyRenderPass", hexHandle(renderPass)});
}

VkFramebuffer NullRhi::createFramebuffer(const FramebufferDesc& desc) {
    (void)desc;
    return mint<VkFramebuffer>("createFramebuffer");
}

void NullRhi::destroyFramebuffer(const VkFramebuffer framebuffer) {
    calls.push_back({"destroyFramebuffer", hexHandle(framebuffer)});
}

// ---------------------------------------------------------------------------
// NullCommandRecorder
// ---------------------------------------------------------------------------

void NullCommandRecorder::beginRenderPass(const RenderPassBeginDesc& desc) {
    calls.push_back(
        {"beginRenderPass",
         hexHandle(desc.renderPass) + " "
             + std::to_string(desc.extent.width) + "x"
             + std::to_string(desc.extent.height)});
}

void NullCommandRecorder::endRenderPass() {
    calls.push_back({"endRenderPass", ""});
}

void NullCommandRecorder::setViewport(
    const float width,
    const float height,
    const float x,
    const float y) {
    calls.push_back(
        {"setViewport",
         std::to_string(width) + "x" + std::to_string(height) + "@"
             + std::to_string(x) + "," + std::to_string(y)});
}

void NullCommandRecorder::setScissor(
    const VkExtent2D extent,
    const VkOffset2D offset) {
    calls.push_back(
        {"setScissor",
         std::to_string(extent.width) + "x" + std::to_string(extent.height)
             + "@" + std::to_string(offset.x) + ","
             + std::to_string(offset.y)});
}

void NullCommandRecorder::bindPipeline(const VkPipeline pipeline) {
    calls.push_back({"bindPipeline", hexHandle(pipeline)});
}

void NullCommandRecorder::bindDescriptorSet(
    const VkPipelineLayout layout,
    const VkDescriptorSet set) {
    (void)layout;
    calls.push_back({"bindDescriptorSet", hexHandle(set)});
}

void NullCommandRecorder::pushConstants(
    const VkPipelineLayout layout,
    const VkShaderStageFlags stages,
    const std::uint32_t offset,
    const void* data,
    const std::uint32_t size) {
    (void)layout;
    (void)data;
    calls.push_back(
        {"pushConstants",
         "offset=" + std::to_string(offset) + " size="
             + std::to_string(size) + " stages=" + std::to_string(stages)});
}

void NullCommandRecorder::bindVertexBuffer(
    const VkBuffer buffer,
    const VkDeviceSize offset) {
    calls.push_back(
        {"bindVertexBuffer",
         hexHandle(buffer) + " +" + std::to_string(offset)});
}

void NullCommandRecorder::bindIndexBuffer(
    const VkBuffer buffer,
    const VkDeviceSize offset) {
    calls.push_back(
        {"bindIndexBuffer",
         hexHandle(buffer) + " +" + std::to_string(offset)});
}

void NullCommandRecorder::draw(const std::uint32_t vertexCount) {
    calls.push_back({"draw", std::to_string(vertexCount)});
}

void NullCommandRecorder::drawIndexed(
    const std::uint32_t indexCount,
    const std::uint32_t firstIndex,
    const std::uint32_t instanceCount,
    const std::uint32_t firstInstance) {
    calls.push_back(
        {"drawIndexed",
         std::to_string(indexCount) + " from "
             + std::to_string(firstIndex) + " x"
             + std::to_string(instanceCount) + " @"
             + std::to_string(firstInstance)});
}

void NullCommandRecorder::imageBarrier(const ImageBarrierDesc& barrier) {
    validateImageBarrier(barrier);
    calls.push_back(
        {"imageBarrier",
         hexHandle(barrier.image) + " " + std::to_string(barrier.oldLayout)
             + " -> " + std::to_string(barrier.newLayout)
             + " srcStage=" + std::to_string(barrier.srcStageMask)
             + " dstStage=" + std::to_string(barrier.dstStageMask)
             + " srcAccess=" + std::to_string(barrier.srcAccessMask)
             + " dstAccess=" + std::to_string(barrier.dstAccessMask)
             + " aspect=" + std::to_string(barrier.aspectMask)
             + " baseMip=" + std::to_string(barrier.baseMipLevel)
             + " mipLevels=" + std::to_string(barrier.mipLevels)});
}

void NullCommandRecorder::drawIndexedIndirect(
    VkBuffer buffer, VkDeviceSize offset, std::uint32_t drawCount,
    std::uint32_t stride) {
    if (drawCount == 0) return;
    if (buffer == VK_NULL_HANDLE || offset % 4 != 0
        || stride < sizeof(VkDrawIndexedIndirectCommand) || stride % 4 != 0)
        throw std::invalid_argument("Invalid indexed indirect draw parameters");
    calls.push_back({"drawIndexedIndirect", std::to_string(drawCount)
        + " @" + std::to_string(offset) + " stride " + std::to_string(stride)});
}

void NullCommandRecorder::dispatch(
    const std::uint32_t groupCountX,
    const std::uint32_t groupCountY,
    const std::uint32_t groupCountZ) {
    if (!groupCountX || !groupCountY || !groupCountZ) throw std::invalid_argument("Empty compute dispatch");
    calls.push_back({"dispatch", std::to_string(groupCountX) + "x"
        + std::to_string(groupCountY) + "x" + std::to_string(groupCountZ)});
}

void NullCommandRecorder::bufferBarrier(const BufferBarrierDesc& barrier) {
    calls.push_back(
        {"bufferBarrier",
         hexHandle(barrier.buffer) + " offset="
             + std::to_string(barrier.offset) + " size="
             + std::to_string(barrier.size) + " srcAccess="
             + std::to_string(barrier.srcAccessMask) + " dstAccess="
             + std::to_string(barrier.dstAccessMask)
             + " srcStage=" + std::to_string(barrier.srcStageMask)
             + " dstStage=" + std::to_string(barrier.dstStageMask)});
}

void NullCommandRecorder::clearColorImage(
    const VkImage image,
    const VkImageLayout layout,
    const VkClearColorValue& color) {
    (void)layout;
    (void)color;
    calls.push_back({"clearColorImage", hexHandle(image)});
}

void NullCommandRecorder::copyImageToBuffer(
    const VkImage image,
    const VkBuffer buffer,
    const VkExtent2D extent) {
    (void)extent;
    calls.push_back(
        {"copyImageToBuffer",
         hexHandle(image) + " -> " + hexHandle(buffer)});
}

void NullCommandRecorder::writeTimestamp(
    const VkQueryPool pool,
    const std::uint32_t query,
    const VkPipelineStageFlagBits stage) {
    (void)pool;
    calls.push_back(
        {"writeTimestamp",
         "query=" + std::to_string(query)
             + " stage=" + std::to_string(stage)});
}

}  // namespace azurerender::rhi
