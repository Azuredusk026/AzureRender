#include "render/ScreenBlackoutGpu.hpp"

#include "render/ScreenEffectTexture.hpp"
#include "resources/BinaryFile.hpp"

#include <array>
#include <stdexcept>

namespace azurerender {

ScreenBlackoutGpu::ScreenBlackoutGpu(rhi::IRhi& backend, VkRenderPass target, std::uint32_t frames,
                                     const std::filesystem::path& shaders, const std::filesystem::path& ramp,
                                     const std::filesystem::path& timeline, bool precedingRampRow)
    : backend_(backend), textures_(backend.allocator(), backend), rampPath_(ramp), timelinePath_(timeline),
      precedingRampRow_(precedingRampRow), opaque_(frames) {
    VkShaderModule vertex{}, fragment{};
    try {
        const auto rampSampler = loadScreenEffectTexture(backend_, textures_, rampPath_, {32, 32}, VK_FILTER_NEAREST);
        const auto timeSampler = loadScreenEffectTexture(backend_, textures_, timelinePath_, {64, 1}, VK_FILTER_LINEAR);
        textures_.seal();
        constexpr auto sampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        constexpr auto stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        descriptors_ = backend_.createDescriptorSetLayout({{0, sampler, 1, stage},
                                                            {1, sampler, 1, stage},
                                                            {2, sampler, 1, stage}});
        pool_ = backend_.createDescriptorPool({{{sampler, frames * 3}}, frames});
        sets_ = backend_.allocateDescriptorSets(pool_, descriptors_, frames);
        for (const auto set : sets_) {
            backend_.writeDescriptorImage({set, 1, sampler, rampSampler.view, rampSampler.sampler});
            backend_.writeDescriptorImage({set, 2, sampler, timeSampler.view, timeSampler.sampler});
        }
        constexpr rhi::PushConstantRangeDesc range{stage, 12 * sizeof(float)};
        layout_ = backend_.createPipelineLayout(descriptors_, &range);
        vertex = backend_.createShaderModule(readBinaryFile((shaders / "inner_outline.vert.spv").u8string()));
        fragment = backend_.createShaderModule(readBinaryFile((shaders / "opening_blackout.frag.spv").u8string()));
        rhi::GraphicsPipelineDesc description;
        description.vertexShader = vertex;
        description.fragmentShader = fragment;
        description.layout = layout_;
        description.renderPass = target;
        description.colorAttachmentCount = 1;
        description.depthTest = description.depthWrite = false;
        description.cullMode = VK_CULL_MODE_NONE;
        description.alphaBlend = true;
        pipeline_ = backend_.createGraphicsPipeline(description);
    } catch (...) {
        if (vertex) backend_.destroyShaderModule(vertex);
        if (fragment) backend_.destroyShaderModule(fragment);
        release();
        throw;
    }
    backend_.destroyShaderModule(vertex);
    backend_.destroyShaderModule(fragment);
}

ScreenBlackoutGpu::~ScreenBlackoutGpu() { release(); }

void ScreenBlackoutGpu::release() noexcept {
    if (pipeline_) backend_.destroyPipeline(pipeline_);
    if (layout_) backend_.destroyPipelineLayout(layout_);
    if (pool_) backend_.destroyDescriptorPool(pool_);
    if (descriptors_) backend_.destroyDescriptorSetLayout(descriptors_);
    for (const auto& source : opaque_)
        if (source.view) backend_.destroyImageView(source.view);
}

bool ScreenBlackoutGpu::matches(const std::filesystem::path& ramp, const std::filesystem::path& timeline) const noexcept {
    return rampPath_ == ramp && timelinePath_ == timeline;
}

void ScreenBlackoutGpu::bindOpaqueColor(std::uint32_t frame, VkImage image, VkFormat format, VkSampler sampler) {
    auto& source = opaque_.at(frame);
    if (source.image == image) return;
    const auto view = backend_.createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    backend_.writeDescriptorImage({sets_.at(frame), 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, view, sampler});
    if (source.view) backend_.destroyImageView(source.view);
    source = {image, view};
}

void ScreenBlackoutGpu::record(rhi::ICommandRecorder& commands, std::uint32_t frame, VkExtent2D extent, float seconds,
                               const ScreenBlackoutParameters& parameters) const {
    const std::array<float, 12> values{
        seconds, parameters.ratio, parameters.blind ? 1.F : 0.F, parameters.unstable,
        parameters.length, parameters.dotScale, parameters.speed, precedingRampRow_ ? 1.F : 0.F,
        static_cast<float>(extent.width), static_cast<float>(extent.height), parameters.blindTop, parameters.blindBottom,
    };
    commands.bindPipeline(pipeline_);
    commands.bindDescriptorSet(layout_, sets_.at(frame));
    commands.pushConstants(layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, values.data(), sizeof(values));
    commands.draw(3);
}

} // namespace azurerender
