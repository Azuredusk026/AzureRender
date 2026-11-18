#pragma once

#include "render/SampledTextureCache.hpp"

#include <array>
#include <filesystem>
#include <initializer_list>
#include <vector>

namespace azurerender {

struct ScreenEffectTexture {
    std::filesystem::path path;
    VkExtent2D extent;
    VkFilter filter;
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    bool clampUv = false;
};

class ScreenEffectGpu final {
public:
    ScreenEffectGpu(rhi::IRhi& backend, VkRenderPass target, VkFormat format, VkExtent2D extent,
                    std::uint32_t frames, VkSampler screenSampler, const std::filesystem::path& shader,
                    std::initializer_list<ScreenEffectTexture> textures, bool blendExternalInput = false);
    ~ScreenEffectGpu();
    ScreenEffectGpu(const ScreenEffectGpu&) = delete;
    ScreenEffectGpu& operator=(const ScreenEffectGpu&) = delete;

    bool matches(std::initializer_list<ScreenEffectTexture> textures) const noexcept;
    VkImage inputImage(std::uint32_t frame) const;
    void bindInput(std::uint32_t frame, VkImageView image);
    void record(rhi::ICommandRecorder& commands, std::uint32_t frame, const std::array<float, 12>& parameters) const;

private:
    void release() noexcept;

    rhi::IRhi& backend_;
    SampledTextureCache textures_;
    std::vector<std::filesystem::path> paths_;
    VkSampler screenSampler_;
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
