#pragma once

#include "render/RenderGraph.hpp"
#include <array>

namespace azurerender {

// Each command slot owns its image and descriptor set. Reuse and destruction
// require completion of that slot's submitted commands.
class GeneratedHeightFogGpu final {
public:
    GeneratedHeightFogGpu(rhi::IRhi&, std::array<unsigned,3> extent,
        const std::vector<rhi::GpuBuffer>& frameParameters, const std::vector<char>& program);
    ~GeneratedHeightFogGpu();
    GeneratedHeightFogGpu(const GeneratedHeightFogGpu&) = delete;
    GeneratedHeightFogGpu& operator=(const GeneratedHeightFogGpu&) = delete;

    [[nodiscard]] VkImage image(unsigned slot) const;
    void writeTexture(VkDescriptorSet set, unsigned binding, unsigned slot) const;
    [[nodiscard]] RenderGraph::ResourceId addPass(RenderGraph&, unsigned slot,
        RenderGraph::ResourceId frameParameters) const;

private:
    struct Slot {
        rhi::GpuImage image;
        VkImageView view = VK_NULL_HANDLE;
        VkDescriptorSet descriptor = VK_NULL_HANDLE;
    };

    void release() noexcept;

    rhi::IRhi& backend_;
    std::array<unsigned,3> extent_;
    std::vector<Slot> slots_;
    VkSampler sampler_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptors_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};

}
