#pragma once

#include "render/SampledTextureCache.hpp"

#include <filesystem>
#include <vector>

namespace azurerender {

struct ScreenChromaticParameters {
    float ratio = 0, unstable = 0, filamentScale = 5.F, rage = .075F;
    bool encodedDisplay = false;
};

class ScreenChromaticGpu final {
public:
    ScreenChromaticGpu(rhi::IRhi& backend, VkRenderPass target, VkFormat format, VkExtent2D extent,
                       std::uint32_t frames, VkSampler screenSampler, const std::filesystem::path& shaders,
                       const std::filesystem::path& noise, const std::filesystem::path& timeline);
    ~ScreenChromaticGpu();
    ScreenChromaticGpu(const ScreenChromaticGpu&) = delete;
    ScreenChromaticGpu& operator=(const ScreenChromaticGpu&) = delete;

    bool matches(const std::filesystem::path& noise, const std::filesystem::path& timeline) const noexcept;
    VkImage inputImage(std::uint32_t frame) const;
    void record(rhi::ICommandRecorder& commands, std::uint32_t frame, float seconds,
                const ScreenChromaticParameters& parameters) const;

private:
    void release() noexcept;

    rhi::IRhi& backend_;
    SampledTextureCache textures_;
    std::filesystem::path noisePath_, timelinePath_;
    VkExtent2D extent_;
    VkDescriptorSetLayout descriptors_{};
    VkDescriptorPool pool_{};
    VkPipelineLayout layout_{};
    VkPipeline pipeline_{};
    struct Input {
        rhi::GpuImage image{};
        VkImageView view{};
        VkDescriptorSet set{};
    };
    std::vector<Input> inputs_;
};

} // namespace azurerender
