#include "render/ScreenChromaticGpu.hpp"

#include "render/ScreenEffectTexture.hpp"
#include "resources/BinaryFile.hpp"

#include <array>

namespace azurerender {

ScreenChromaticGpu::ScreenChromaticGpu(rhi::IRhi& backend, VkRenderPass target, VkFormat format, VkExtent2D extent,
                                       std::uint32_t frames, VkSampler screenSampler, const std::filesystem::path& shaders,
                                       const std::filesystem::path& noise, const std::filesystem::path& timeline)
    : backend_(backend), textures_(backend.allocator(), backend), noisePath_(noise), timelinePath_(timeline),
      extent_(extent), inputs_(frames) {
    VkShaderModule vertex{}, fragment{};
    try {
        const auto noiseTexture = loadScreenEffectTexture(backend_, textures_, noise, {256, 256}, VK_FILTER_LINEAR);
        const auto timeTexture = loadScreenEffectTexture(backend_, textures_, timeline, {64, 1}, VK_FILTER_LINEAR);
        textures_.seal();
        constexpr auto sampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        constexpr auto stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        descriptors_ = backend_.createDescriptorSetLayout({{0, sampler, 1, stage}, {1, sampler, 1, stage},
                                                            {2, sampler, 1, stage}});
        pool_ = backend_.createDescriptorPool({{{sampler, frames * 3}}, frames});
        const auto sets = backend_.allocateDescriptorSets(pool_, descriptors_, frames);
        for (std::uint32_t i = 0; i < frames; ++i) {
            auto& input = inputs_[i];
            input.image = backend_.allocator().createImage2D(extent.width, extent.height, format,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
            input.view = backend_.createImageView(input.image.image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
            input.set = sets[i];
            backend_.writeDescriptorImage({input.set, 0, sampler, noiseTexture.view, noiseTexture.sampler});
            backend_.writeDescriptorImage({input.set, 1, sampler, timeTexture.view, timeTexture.sampler});
            backend_.writeDescriptorImage({input.set, 2, sampler, input.view, screenSampler});
        }
        constexpr rhi::PushConstantRangeDesc range{stage, 8 * sizeof(float)};
        layout_ = backend_.createPipelineLayout(descriptors_, &range);
        vertex = backend_.createShaderModule(readBinaryFile((shaders / "inner_outline.vert.spv").u8string()));
        fragment = backend_.createShaderModule(readBinaryFile((shaders / "opening_chromatic.frag.spv").u8string()));
        rhi::GraphicsPipelineDesc description;
        description.vertexShader = vertex;
        description.fragmentShader = fragment;
        description.layout = layout_;
        description.renderPass = target;
        description.colorAttachmentCount = 1;
        description.depthTest = description.depthWrite = false;
        description.cullMode = VK_CULL_MODE_NONE;
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

ScreenChromaticGpu::~ScreenChromaticGpu() { release(); }

void ScreenChromaticGpu::release() noexcept {
    if (pipeline_) backend_.destroyPipeline(pipeline_);
    if (layout_) backend_.destroyPipelineLayout(layout_);
    if (pool_) backend_.destroyDescriptorPool(pool_);
    if (descriptors_) backend_.destroyDescriptorSetLayout(descriptors_);
    for (auto& input : inputs_) {
        if (input.view) backend_.destroyImageView(input.view);
        backend_.allocator().destroyImage(input.image);
    }
}

bool ScreenChromaticGpu::matches(const std::filesystem::path& noise, const std::filesystem::path& timeline) const noexcept {
    return noisePath_ == noise && timelinePath_ == timeline;
}

VkImage ScreenChromaticGpu::inputImage(std::uint32_t frame) const {
    return inputs_.at(frame).image.image;
}

void ScreenChromaticGpu::record(rhi::ICommandRecorder& commands, std::uint32_t frame, float seconds,
                                 const ScreenChromaticParameters& parameters) const {
    const std::array<float, 8> values{seconds, parameters.ratio, parameters.unstable, parameters.filamentScale,
        parameters.rage, static_cast<float>(extent_.width), static_cast<float>(extent_.height),
        parameters.encodedDisplay ? 1.F : 0.F};
    commands.bindPipeline(pipeline_);
    commands.bindDescriptorSet(layout_, inputs_.at(frame).set);
    commands.pushConstants(layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, values.data(), sizeof(values));
    commands.draw(3);
}

} // namespace azurerender
