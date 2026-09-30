#pragma once

#include "rhi/IGpuAllocator.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

// Narrow RHI boundary for the renderer. The interface deliberately mirrors
// Vulkan types (formats, layouts, compare ops) instead of inventing a
// parallel enum universe: the goals are a single call funnel, explicit
// ownership, and a recording mock for pass tests — not API neutrality.
//
// Two implementations exist: VulkanRhi forwards to the device, NullRhi
// records the call sequence for unit tests without a GPU.

namespace azurerender::rhi {

struct ComputePipelineDesc {
    VkShaderModule shader = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
};

struct SamplerDesc {
    VkFilter filter = VK_FILTER_LINEAR;
    VkSamplerAddressMode addressU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    float maxLod = 0.0F;
    bool mipmapLinear = true;
};

struct DescriptorBindingDesc {
    std::uint32_t binding = 0;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    std::uint32_t count = 1;
    VkShaderStageFlags stages = 0;
};

struct DescriptorPoolSizeDesc {
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    std::uint32_t count = 1;
};

struct DescriptorPoolDesc {
    std::vector<DescriptorPoolSizeDesc> sizes;
    std::uint32_t maxSets = 1;
};

struct DescriptorImageWrite {
    VkDescriptorSet set = VK_NULL_HANDLE;
    std::uint32_t binding = 0;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
};

struct DescriptorImageArrayWrite {
    VkDescriptorSet set = VK_NULL_HANDLE;
    std::uint32_t binding = 0;
    std::vector<VkDescriptorImageInfo> elements;
};

struct DescriptorBufferWrite {
    VkDescriptorSet set = VK_NULL_HANDLE;
    std::uint32_t binding = 0;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize range = 0;
};

struct PushConstantRangeDesc {
    VkShaderStageFlags stages = 0;
    std::uint32_t size = 0;
};

struct VertexAttributeDesc {
    std::uint32_t location = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    std::uint32_t offset = 0;
};

struct GraphicsPipelineDesc {
    VkShaderModule vertexShader = VK_NULL_HANDLE;
    VkShaderModule fragmentShader = VK_NULL_HANDLE;

    // Empty vertexStride means a fullscreen pipeline with no vertex input.
    std::uint32_t vertexStride = 0;
    std::vector<VertexAttributeDesc> vertexAttributes;

    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    bool depthBias = false;
    float depthBiasConstant = 0.0F;
    float depthBiasSlope = 0.0F;

    bool depthTest = true;
    bool depthWrite = true;

    bool alphaBlend = false;
    // 0 for depth-only passes.
    std::uint32_t colorAttachmentCount = 0;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
};

struct RenderPassAttachmentDesc {
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool clear = true;
    bool store = true;
    bool isDepth = false;
};

// A single-subpass render pass; every pass in the project fits this shape.
struct RenderPassDesc {
    std::vector<RenderPassAttachmentDesc> attachments;
    // Index into attachments, or -1 when the pass has no depth target.
    std::int32_t depthAttachment = -1;
    // External fragment-read -> attachment-write dependency, used when the
    // pass output is sampled later in the frame.
    bool externalReadDependency = false;
};

struct FramebufferDesc {
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkImageView> attachments;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct RenderPassBeginDesc {
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkExtent2D extent{};
    std::vector<VkClearValue> clearValues;
};

struct ImageBarrierDesc {
    VkImage image = VK_NULL_HANDLE;
    VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::uint32_t mipLevels = 1;
    VkPipelineStageFlags srcStageMask = 0;
    VkPipelineStageFlags dstStageMask = 0;
    VkAccessFlags srcAccessMask = 0;
    VkAccessFlags dstAccessMask = 0;
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    std::uint32_t baseMipLevel = 0;
};

struct BufferBarrierDesc {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
    VkDeviceSize size = VK_WHOLE_SIZE;
    VkPipelineStageFlags srcStageMask = 0;
    VkPipelineStageFlags dstStageMask = 0;
    VkAccessFlags srcAccessMask = 0;
    VkAccessFlags dstAccessMask = 0;
};

inline void validateImageBarrier(const ImageBarrierDesc& barrier) {
    if ((barrier.srcStageMask == 0) != (barrier.dstStageMask == 0))
        throw std::invalid_argument("Image barrier requires both pipeline stages");
    if (barrier.mipLevels == 0 || barrier.aspectMask == 0)
        throw std::invalid_argument("Image barrier requires a nonempty subresource range");
    if (barrier.srcStageMask == 0
        && (barrier.srcAccessMask != 0 || barrier.dstAccessMask != 0))
        throw std::invalid_argument("Explicit image access requires pipeline stages");
}

// Per-frame command recording. The engine hands a recorder to scene
// renderers for the duration of one command buffer; renderers never see the
// raw VkCommandBuffer.
class ICommandRecorder {
public:
    virtual ~ICommandRecorder() = default;

    virtual void beginRenderPass(const RenderPassBeginDesc& desc) = 0;
    virtual void endRenderPass() = 0;
    virtual void setViewport(
        float width, float height, float x = 0.0F, float y = 0.0F) = 0;
    virtual void setScissor(
        VkExtent2D extent, VkOffset2D offset = {}) = 0;

    virtual void bindComputePipeline(VkPipeline pipeline) = 0;
    virtual void bindComputeDescriptorSet(VkPipelineLayout layout, VkDescriptorSet set) = 0;
    virtual void bindPipeline(VkPipeline pipeline) = 0;
    virtual void bindDescriptorSet(
        VkPipelineLayout layout,
        VkDescriptorSet set) = 0;
    virtual void pushConstants(
        VkPipelineLayout layout,
        VkShaderStageFlags stages,
        std::uint32_t offset,
        const void* data,
        std::uint32_t size) = 0;

    virtual void bindVertexBuffer(VkBuffer buffer, VkDeviceSize offset) = 0;
    virtual void bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset) = 0;
    virtual void draw(std::uint32_t vertexCount) = 0;
    virtual void drawIndexed(
        std::uint32_t indexCount,
        std::uint32_t firstIndex,
        std::uint32_t instanceCount = 1,
        std::uint32_t firstInstance = 0) = 0;
    virtual void dispatch(
        std::uint32_t groupCountX,
        std::uint32_t groupCountY,
        std::uint32_t groupCountZ) = 0;

    virtual void imageBarrier(const ImageBarrierDesc& barrier) = 0;
    virtual void bufferBarrier(const BufferBarrierDesc& barrier) = 0;
    virtual void clearColorImage(
        VkImage image,
        VkImageLayout layout,
        const VkClearColorValue& color) = 0;
    virtual void copyImageToBuffer(
        VkImage image,
        VkBuffer buffer,
        VkExtent2D extent) = 0;
    virtual void writeTimestamp(
        VkQueryPool pool,
        std::uint32_t query,
        VkPipelineStageFlagBits stage) = 0;
};

// Device-level resource operations. Scene renderers create all GPU objects
// through this interface so a recording backend can host them in tests.
class IRhi {
public:
    virtual ~IRhi() = default;

    virtual IGpuAllocator& allocator() = 0;

    // One-shot upload helpers. Each submits and waits internally; call only
    // during load, never per frame.
    virtual void copyBuffer(
        const GpuBuffer& source,
        const GpuBuffer& destination,
        VkDeviceSize size) = 0;
    virtual void transitionImageLayout(
        const GpuImage& image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        std::uint32_t mipLevels) = 0;
    virtual void copyBufferToImage(
        const GpuBuffer& source,
        const GpuImage& destination,
        std::uint32_t width,
        std::uint32_t height) = 0;
    virtual void clearImage(const GpuImage& image) = 0;
    virtual void generateMipmaps(
        const GpuImage& image,
        VkFormat format,
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t mipLevels) = 0;

    virtual VkImageView createImageView(
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect,
        std::uint32_t mipLevels,
        std::uint32_t baseMipLevel = 0) = 0;
    virtual void destroyImageView(VkImageView view) = 0;
    virtual VkSampler createSampler(const SamplerDesc& desc) = 0;
    virtual void destroySampler(VkSampler sampler) = 0;
    virtual void executeOneShot(
        const std::function<void(ICommandRecorder&)>& record) = 0;

    virtual VkShaderModule createShaderModule(
        const std::vector<char>& code) = 0;
    virtual void destroyShaderModule(VkShaderModule shader) = 0;
    virtual VkPipelineLayout createPipelineLayout(
        VkDescriptorSetLayout setLayout,
        const PushConstantRangeDesc* pushConstants) = 0;
    virtual void destroyPipelineLayout(VkPipelineLayout layout) = 0;
    virtual VkPipeline createComputePipeline(const ComputePipelineDesc& desc) = 0;
    virtual VkPipeline createGraphicsPipeline(
        const GraphicsPipelineDesc& desc) = 0;
    virtual void destroyPipeline(VkPipeline pipeline) = 0;

    virtual VkDescriptorSetLayout createDescriptorSetLayout(
        const std::vector<DescriptorBindingDesc>& bindings) = 0;
    virtual void destroyDescriptorSetLayout(VkDescriptorSetLayout layout) = 0;
    virtual VkDescriptorPool createDescriptorPool(
        const DescriptorPoolDesc& desc) = 0;
    virtual void destroyDescriptorPool(VkDescriptorPool pool) = 0;
    virtual std::vector<VkDescriptorSet> allocateDescriptorSets(
        VkDescriptorPool pool,
        VkDescriptorSetLayout layout,
        std::uint32_t count) = 0;
    virtual void writeDescriptorImage(const DescriptorImageWrite& write) = 0;
    virtual void writeDescriptorImageArray(
        const DescriptorImageArrayWrite& write) = 0;
    virtual void writeDescriptorBuffer(
        const DescriptorBufferWrite& write) = 0;

    virtual VkRenderPass createRenderPass(const RenderPassDesc& desc) = 0;
    virtual void destroyRenderPass(VkRenderPass renderPass) = 0;
    virtual VkFramebuffer createFramebuffer(const FramebufferDesc& desc) = 0;
    virtual void destroyFramebuffer(VkFramebuffer framebuffer) = 0;
};

}  // namespace azurerender::rhi
