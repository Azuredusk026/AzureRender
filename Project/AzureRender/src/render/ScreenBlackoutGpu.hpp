#pragma once

#include "render/SampledTextureCache.hpp"

#include <filesystem>
#include <vector>

namespace azurerender {

struct ScreenBlackoutParameters {
    float ratio = 0;
    bool blind = false;
    float unstable = 0;
    float length = .2F;
    float dotScale = 15.F;
    float speed = 5.F;
    float blindTop = 15.3F;
    float blindBottom = 6.3F;
};

class ScreenBlackoutGpu final {
public:
    ScreenBlackoutGpu(rhi::IRhi& backend, VkRenderPass target, std::uint32_t frames,
                      const std::filesystem::path& shaders, const std::filesystem::path& ramp,
                      const std::filesystem::path& timeline, bool precedingRampRow);
    ~ScreenBlackoutGpu();
    ScreenBlackoutGpu(const ScreenBlackoutGpu&) = delete;
    ScreenBlackoutGpu& operator=(const ScreenBlackoutGpu&) = delete;

    bool matches(const std::filesystem::path& ramp, const std::filesystem::path& timeline) const noexcept;
    void bindOpaqueColor(std::uint32_t frame, VkImage image, VkFormat format, VkSampler sampler);
    void record(rhi::ICommandRecorder& commands, std::uint32_t frame, VkExtent2D extent, float seconds,
                const ScreenBlackoutParameters& parameters) const;

private:
    void release() noexcept;

    rhi::IRhi& backend_;
    SampledTextureCache textures_;
    std::filesystem::path rampPath_, timelinePath_;
    bool precedingRampRow_;
    VkDescriptorSetLayout descriptors_{};
    VkDescriptorPool pool_{};
    VkPipelineLayout layout_{};
    VkPipeline pipeline_{};
    std::vector<VkDescriptorSet> sets_;
    struct OpaqueColorView { VkImage image{}; VkImageView view{}; };
    std::vector<OpaqueColorView> opaque_;
};

} // namespace azurerender
