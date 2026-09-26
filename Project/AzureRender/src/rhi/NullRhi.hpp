#pragma once

#include "rhi/Rhi.hpp"

#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace azurerender::rhi {

// Records every call in order for pass-recording tests. The strings are the
// assertion surface: a test walks `calls` and checks the sequence.
struct RecordedCall {
    std::string name;
    std::string detail;
};

// Host-backed fake allocator. Buffers get a real heap block so `mapped`
// writes work; images carry no storage. Handles are minted from a counter.
class NullGpuAllocator final : public IGpuAllocator {
public:
    GpuBuffer createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        bool hostVisible) override;
    void destroyBuffer(GpuBuffer& buffer) noexcept override;
    GpuImage createImage(
        const VkImageCreateInfo& createInfo,
        bool hostVisible) override;
    void destroyImage(GpuImage& image) noexcept override;
    GpuImage createImage2D(
        std::uint32_t width,
        std::uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage,
        std::uint32_t mipLevels = 1,
        bool hostVisible = false) override;
    void flush(
        const GpuBuffer& buffer,
        VkDeviceSize offset,
        VkDeviceSize size) override;
    [[nodiscard]] const GpuAllocatorStatistics& statistics()
        const noexcept override {
        return statistics_;
    }

private:
    GpuAllocatorStatistics statistics_;
    // deque so block addresses stay stable while the vector grows.
    std::deque<std::vector<std::byte>> hostBlocks_;
    std::uint64_t nextHandle_ = 1;
};

// Recording backend. Creates distinct non-null fake handles and logs every
// operation; nothing touches a real device.
class NullRhi final : public IRhi {
public:
    std::vector<RecordedCall> calls;

    IGpuAllocator& allocator() override { return gpuAllocator_; }

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
        std::uint32_t mipLevels) override;
    void destroyImageView(VkImageView view) override;
    VkSampler createSampler(const SamplerDesc& desc) override;
    void destroySampler(VkSampler sampler) override;

    VkShaderModule createShaderModule(
        const std::vector<char>& code) override;
    void destroyShaderModule(VkShaderModule shader) override;
    VkPipelineLayout createPipelineLayout(
        VkDescriptorSetLayout setLayout,
        const PushConstantRangeDesc* pushConstants) override;
    void destroyPipelineLayout(VkPipelineLayout layout) override;
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
    template <typename Handle>
    Handle mint(const char* name);

    NullGpuAllocator gpuAllocator_;
    std::uint64_t nextHandle_ = 1;
};

// Records the command sequence; asserts nothing by itself.
class NullCommandRecorder final : public ICommandRecorder {
public:
    std::vector<RecordedCall> calls;

    void beginRenderPass(const RenderPassBeginDesc& desc) override;
    void endRenderPass() override;
    void setViewport(float width, float height) override;
    void setScissor(VkExtent2D extent) override;
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
};

}  // namespace azurerender::rhi
