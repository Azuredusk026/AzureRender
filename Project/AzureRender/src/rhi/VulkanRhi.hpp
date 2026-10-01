#pragma once

#include "rhi/GpuAllocator.hpp"
#include "rhi/Rhi.hpp"

#include <functional>

namespace azurerender::rhi {

// Production backend. Owns nothing beyond what its create calls return;
// callers destroy through the matching destroy methods.
class VulkanRhi final : public IRhi {
public:
    VulkanRhi(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkQueue graphicsQueue,
        VkCommandPool commandPool,
        GpuAllocator& allocator)
        : device_(device),
          physicalDevice_(physicalDevice),
          graphicsQueue_(graphicsQueue),
          commandPool_(commandPool),
          allocator_(&allocator) {}

    GpuAllocator& allocator() override { return *allocator_; }

    void copyBuffer(
        const GpuBuffer& source,
        const GpuBuffer& destination,
        VkDeviceSize size) override;
    void transitionImageLayout(
        const GpuImage& image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        std::uint32_t mipLevels) override;
    void copyBufferToImage(
        const GpuBuffer& source,
        const GpuImage& destination,
        std::uint32_t width,
        std::uint32_t height) override;
    void clearImage(const GpuImage& image) override;
    void generateMipmaps(
        const GpuImage& image,
        VkFormat format,
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t mipLevels) override;

    VkImageView createImageView(
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect,
        std::uint32_t mipLevels,
        std::uint32_t baseMipLevel = 0) override;
    void destroyImageView(VkImageView view) override;
    VkSampler createSampler(const SamplerDesc& desc) override;
    void destroySampler(VkSampler sampler) override;
    void executeOneShot(
        const std::function<void(ICommandRecorder&)>& record) override;

    VkShaderModule createShaderModule(
        const std::vector<char>& code) override;
    void destroyShaderModule(VkShaderModule shader) override;
    VkPipelineLayout createPipelineLayout(
        VkDescriptorSetLayout setLayout,
        const PushConstantRangeDesc* pushConstants) override;
    void destroyPipelineLayout(VkPipelineLayout layout) override;
    VkPipeline createComputePipeline(const ComputePipelineDesc& desc) override;
    VkPipeline createGraphicsPipeline(
        const GraphicsPipelineDesc& desc) override;
    void destroyPipeline(VkPipeline pipeline) override;

    VkDescriptorSetLayout createDescriptorSetLayout(
        const std::vector<DescriptorBindingDesc>& bindings) override;
    void destroyDescriptorSetLayout(VkDescriptorSetLayout layout) override;
    VkDescriptorPool createDescriptorPool(
        const DescriptorPoolDesc& desc) override;
    void destroyDescriptorPool(VkDescriptorPool pool) override;
    std::vector<VkDescriptorSet> allocateDescriptorSets(
        VkDescriptorPool pool,
        VkDescriptorSetLayout layout,
        std::uint32_t count) override;
    void writeDescriptorImage(const DescriptorImageWrite& write) override;
    void writeDescriptorImageArray(
        const DescriptorImageArrayWrite& write) override;
    void writeDescriptorBuffer(const DescriptorBufferWrite& write) override;

    VkRenderPass createRenderPass(const RenderPassDesc& desc) override;
    void destroyRenderPass(VkRenderPass renderPass) override;
    VkFramebuffer createFramebuffer(const FramebufferDesc& desc) override;
    void destroyFramebuffer(VkFramebuffer framebuffer) override;

private:
    // Records a one-shot command buffer, submits, and waits for idle.
    void runOneShot(
        const char* label,
        const std::function<void(VkCommandBuffer)>& record);

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    GpuAllocator* allocator_ = nullptr;
};

// Records onto a live VkCommandBuffer. Created per frame by the engine.
class VulkanCommandRecorder final : public ICommandRecorder {
public:
    explicit VulkanCommandRecorder(VkCommandBuffer commandBuffer, bool exclusiveRecording = false)
        : commandBuffer_(commandBuffer), exclusiveRecording_(exclusiveRecording) {}
    void invalidatePipelineBindings() noexcept {
        boundGraphicsPipeline_ = VK_NULL_HANDLE;
        boundComputePipeline_ = VK_NULL_HANDLE;
        boundGraphicsSet_ = VK_NULL_HANDLE;
        boundComputeSet_ = VK_NULL_HANDLE;
        boundGraphicsLayout_ = VK_NULL_HANDLE;
        boundComputeLayout_ = VK_NULL_HANDLE;
    }

    void beginRenderPass(const RenderPassBeginDesc& desc) override;
    void endRenderPass() override;
    void setViewport(
        float width, float height, float x = 0.0F, float y = 0.0F) override;
    void setScissor(VkExtent2D extent, VkOffset2D offset = {}) override;
    void bindComputePipeline(VkPipeline pipeline) override;
    void bindComputeDescriptorSet(VkPipelineLayout layout, VkDescriptorSet set) override;
    void bindPipeline(VkPipeline pipeline) override;
    void bindDescriptorSet(
        VkPipelineLayout layout,
        VkDescriptorSet set) override;
    void pushConstants(
        VkPipelineLayout layout,
        VkShaderStageFlags stages,
        std::uint32_t offset,
        const void* data,
        std::uint32_t size) override;
    void bindVertexBuffer(VkBuffer buffer, VkDeviceSize offset) override;
    void bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset) override;
    void draw(std::uint32_t vertexCount) override;
    void drawIndexed(
        std::uint32_t indexCount,
        std::uint32_t firstIndex,
        std::uint32_t instanceCount = 1,
        std::uint32_t firstInstance = 0) override;
    void drawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset,
                             std::uint32_t drawCount, std::uint32_t stride) override;
    void dispatch(
        std::uint32_t groupCountX,
        std::uint32_t groupCountY,
        std::uint32_t groupCountZ) override;
    void imageBarrier(const ImageBarrierDesc& barrier) override;
    void bufferBarrier(const BufferBarrierDesc& barrier) override;
    void clearColorImage(
        VkImage image,
        VkImageLayout layout,
        const VkClearColorValue& color) override;
    void copyImageToBuffer(
        VkImage image,
        VkBuffer buffer,
        VkExtent2D extent) override;
    void writeTimestamp(
        VkQueryPool pool,
        std::uint32_t query,
        VkPipelineStageFlagBits stage) override;

private:
    VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
    bool exclusiveRecording_ = false;
    VkPipeline boundGraphicsPipeline_ = VK_NULL_HANDLE;
    VkPipeline boundComputePipeline_ = VK_NULL_HANDLE;
    VkDescriptorSet boundGraphicsSet_ = VK_NULL_HANDLE;
    VkDescriptorSet boundComputeSet_ = VK_NULL_HANDLE;
    VkPipelineLayout boundGraphicsLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout boundComputeLayout_ = VK_NULL_HANDLE;
};

}  // namespace azurerender::rhi
