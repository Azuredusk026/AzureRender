#include "render/ScreenEffectGpu.hpp"

#include "render/ScreenEffectTexture.hpp"
#include "resources/BinaryFile.hpp"

#include <algorithm>

namespace azurerender {

ScreenEffectGpu::ScreenEffectGpu(rhi::IRhi& backend, VkRenderPass target, VkFormat format, VkExtent2D extent,
                                std::uint32_t frames, VkSampler screenSampler, const std::filesystem::path& shader,
                                std::initializer_list<ScreenEffectTexture> textures)
    : backend_(backend), textures_(backend.allocator(), backend), inputs_(frames) {
    VkShaderModule vertex{}, fragment{};
    try {
        std::vector<SampledTextureBinding> boundTextures;
        for (const auto& texture : textures) {
            paths_.push_back(texture.path);
            boundTextures.push_back(loadScreenEffectTexture(backend_, textures_, texture.path, texture.extent,
                texture.filter, texture.format, texture.clampUv));
        }
        textures_.seal();
        constexpr auto sampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        constexpr auto stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        const auto screenBinding = static_cast<std::uint32_t>(boundTextures.size());
        std::vector<rhi::DescriptorBindingDesc> bindings;
        for (std::uint32_t binding = 0; binding <= screenBinding; ++binding)
            bindings.push_back({binding, sampler, 1, stage});
        descriptors_ = backend_.createDescriptorSetLayout(bindings);
        pool_ = backend_.createDescriptorPool({{{sampler, frames * (screenBinding + 1)}}, frames});
        const auto sets = backend_.allocateDescriptorSets(pool_, descriptors_, frames);
        for (std::uint32_t i = 0; i < frames; ++i) {
            auto& input = inputs_[i];
            input.image = backend_.allocator().createImage2D(extent.width, extent.height, format,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
            input.view = backend_.createImageView(input.image.image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
            input.set = sets[i];
            for (std::uint32_t binding = 0; binding < screenBinding; ++binding) {
                const auto& texture = boundTextures[binding];
                backend_.writeDescriptorImage({input.set, binding, sampler, texture.view, texture.sampler});
            }
            backend_.writeDescriptorImage({input.set, screenBinding, sampler, input.view, screenSampler});
        }
        constexpr rhi::PushConstantRangeDesc range{stage, 8 * sizeof(float)};
        layout_ = backend_.createPipelineLayout(descriptors_, &range);
        vertex = backend_.createShaderModule(readBinaryFile((shader.parent_path() / "inner_outline.vert.spv").u8string()));
        fragment = backend_.createShaderModule(readBinaryFile(shader.u8string()));
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

ScreenEffectGpu::~ScreenEffectGpu() { release(); }

void ScreenEffectGpu::release() noexcept {
    if (pipeline_) backend_.destroyPipeline(pipeline_);
    if (layout_) backend_.destroyPipelineLayout(layout_);
    if (pool_) backend_.destroyDescriptorPool(pool_);
    if (descriptors_) backend_.destroyDescriptorSetLayout(descriptors_);
    for (auto& input : inputs_) {
        if (input.view) backend_.destroyImageView(input.view);
        backend_.allocator().destroyImage(input.image);
    }
}

bool ScreenEffectGpu::matches(std::initializer_list<ScreenEffectTexture> textures) const noexcept {
    return std::equal(paths_.begin(), paths_.end(), textures.begin(), textures.end(),
                      [](const auto& path, const auto& texture) { return path == texture.path; });
}

VkImage ScreenEffectGpu::inputImage(std::uint32_t frame) const {
    return inputs_.at(frame).image.image;
}

void ScreenEffectGpu::record(rhi::ICommandRecorder& commands, std::uint32_t frame,
                             const std::array<float, 8>& parameters) const {
    commands.bindPipeline(pipeline_);
    commands.bindDescriptorSet(layout_, inputs_.at(frame).set);
    commands.pushConstants(layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, parameters.data(), sizeof(parameters));
    commands.draw(3);
}

} // namespace azurerender
