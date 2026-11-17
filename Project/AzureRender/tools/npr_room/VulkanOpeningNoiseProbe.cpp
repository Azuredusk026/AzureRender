#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace {
using namespace azurerender;

struct Texture {
    rhi::GpuImage image{};
    VkImageView view{};
    VkSampler sampler{};

    void upload(test::VulkanComputeFixture& host, VkFormat format, VkExtent2D extent,
                const void* bytes, std::size_t size, unsigned pixelBytes, rhi::SamplerDesc sampling) {
        auto& backend = *host.backend;
        auto staging = host.allocator.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);
        const auto release = std::unique_ptr<void, std::function<void(void*)>>(
            &host, [&](void*) { host.allocator.destroyBuffer(staging); });
        std::memcpy(staging.mapped, bytes, size);
        host.allocator.flush(staging, 0, size);
        image = host.allocator.createImage2D(extent.width, extent.height, format,
                                            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        backend.transitionImageLayout(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
        backend.copyBufferToImage(staging, image, extent.width, extent.height, 1, pixelBytes);
        backend.transitionImageLayout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
        view = backend.createImageView(image.image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        sampler = backend.createSampler(sampling);
    }

    void destroy(test::VulkanComputeFixture& host) {
        if (sampler) host.backend->destroySampler(sampler);
        if (view) host.backend->destroyImageView(view);
        host.allocator.destroyImage(image);
    }
};
}

int main(int argc, char** argv) try {
    if (argc < 4 || argc > 6 || std::filesystem::exists(argv[3]))
        throw std::invalid_argument("Expected vertex and fragment shaders, unused capture path and optional source inputs");
    const bool chromatic = argc == 6 && std::string_view(argv[5]) == "chromatic";
    if (argc == 6 && !chromatic) throw std::invalid_argument("Expected chromatic source inputs");
    const bool blackout = argc == 5;
    const bool textured = blackout || chromatic;
    using namespace azurerender;
    test::VulkanComputeFixture host(true);
    auto& backend = *host.backend;
    rhi::GpuBuffer output{};
    rhi::GpuImage color{};
    VkImageView view{};
    VkRenderPass pass{};
    VkFramebuffer framebuffer{};
    VkDescriptorSetLayout descriptors{};
    VkDescriptorPool pool{};
    VkDescriptorSet set{};
    std::array<Texture, 3> textures;
    VkPipelineLayout layout{};
    VkShaderModule vertex{}, fragment{};
    VkPipeline pipeline{};
    auto cleanup = std::unique_ptr<void, std::function<void(void*)>>(&host, [&](void*) {
        if (pipeline) backend.destroyPipeline(pipeline);
        if (vertex) backend.destroyShaderModule(vertex);
        if (fragment) backend.destroyShaderModule(fragment);
        if (layout) backend.destroyPipelineLayout(layout);
        if (pool) backend.destroyDescriptorPool(pool);
        if (descriptors) backend.destroyDescriptorSetLayout(descriptors);
        for (auto& texture : textures) texture.destroy(host);
        if (framebuffer) backend.destroyFramebuffer(framebuffer);
        if (pass) backend.destroyRenderPass(pass);
        if (view) backend.destroyImageView(view);
        host.allocator.destroyImage(color);
        host.allocator.destroyBuffer(output);
    });
    constexpr VkExtent2D extent{320, 180};
    constexpr auto format = VK_FORMAT_R32G32B32A32_SFLOAT;
    output = host.allocator.createBuffer(extent.width * extent.height * 4 * sizeof(float), VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    color = host.allocator.createImage2D(extent.width, extent.height, format,
                                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    view = backend.createImageView(color.image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    rhi::RenderPassDesc description;
    description.attachments = {{format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true, true, false}};
    pass = backend.createRenderPass(description);
    framebuffer = backend.createFramebuffer({pass, {view}, extent.width, extent.height});
    std::vector<rhi::DescriptorBindingDesc> bindings;
    std::array<float, 20> material{};
    if (textured) {
        const std::filesystem::path source(argv[4]);
        const auto constants = readBinaryFile((source / (blackout ? "blackout-material.bin" : "chromatic-material.bin")).string());
        if (constants.size() != (blackout ? sizeof(material) : 12 * sizeof(float)))
            throw std::invalid_argument("Unexpected source material constants");
        std::memcpy(material.data(), constants.data(), constants.size());
        constexpr std::array<float, 4> opaque{.7F, .45F, .2F, 1};
        rhi::SamplerDesc linear;
        auto clamp = linear;
        clamp.addressU = clamp.addressV = clamp.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        auto point = linear;
        point.filter = VK_FILTER_NEAREST;
        const auto upload = [&](unsigned binding, const char* name, VkExtent2D size, rhi::SamplerDesc sampling) {
            const auto bytes = readBinaryFile((source / name).string());
            if (bytes.size() != size.width * size.height * 4)
                throw std::invalid_argument("Unexpected source effect raster");
            textures[binding].upload(host, VK_FORMAT_R8G8B8A8_SRGB, size, bytes.data(), bytes.size(), 4, sampling);
        };
        if (blackout) {
            textures[0].upload(host, format, {1, 1}, opaque.data(), sizeof(opaque), sizeof(opaque), clamp);
            upload(1, "blackout-ramp.rgba", {32, 32}, point);
            upload(2, "blackout-timeline.rgba", {64, 1}, linear);
        } else {
            const auto screen = readBinaryFile((source / "chromatic-source.rgba32f").string());
            if (screen.size() != output.size) throw std::invalid_argument("Unexpected source screen dimensions");
            upload(0, "chromatic-noise.rgba", {256, 256}, linear);
            upload(1, "chromatic-timeline.rgba", {64, 1}, linear);
            textures[2].upload(host, format, extent, screen.data(), screen.size(), 4 * sizeof(float), clamp);
        }
        for (unsigned binding = 0; binding < textures.size(); ++binding)
            bindings.push_back({binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
    }
    descriptors = backend.createDescriptorSetLayout(bindings);
    if (textured) {
        pool = backend.createDescriptorPool({{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3}}, 1});
        set = backend.allocateDescriptorSets(pool, descriptors, 1).at(0);
        for (unsigned binding = 0; binding < textures.size(); ++binding) {
            const auto& texture = textures[binding];
            backend.writeDescriptorImage({set, binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texture.view, texture.sampler});
        }
    }
    const auto pushSize = static_cast<unsigned>((blackout ? 12 : chromatic ? 8 : 4) * sizeof(float));
    const rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_FRAGMENT_BIT, pushSize};
    layout = backend.createPipelineLayout(descriptors, &range);
    vertex = backend.createShaderModule(readBinaryFile(argv[1]));
    fragment = backend.createShaderModule(readBinaryFile(argv[2]));
    rhi::GraphicsPipelineDesc graphics;
    graphics.vertexShader = vertex;
    graphics.fragmentShader = fragment;
    graphics.layout = layout;
    graphics.renderPass = pass;
    graphics.colorAttachmentCount = 1;
    graphics.depthTest = graphics.depthWrite = false;
    graphics.cullMode = VK_CULL_MODE_NONE;
    graphics.alphaBlend = !chromatic;
    pipeline = backend.createGraphicsPipeline(graphics);
    std::vector<std::array<float, 3>> samples{{9.302F, 0, 0}, {9.302F, .05F, 0}, {9.802F, .05F, 0}};
    if (blackout) samples = {{9.302F, 0, 0}, {9.302F, .5F, 0}, {9.802F, .5F, 0}, {9.302F, 1, 0}, {9.302F, .5F, 1}};
    if (chromatic) samples = {{9.302F, 0, 0}, {9.302F, .2F, 0}, {9.802F, .2F, 0}, {9.302F, .5F, 0}};
    const float precedingRampRow = host.properties.vendorID == 0x10DE ? 1.F : 0.F;
    std::ofstream capture(argv[3], std::ios::binary);
    for (const auto& sample : samples) {
        std::array<float, 12> parameters{sample[1], std::sin(sample[0]), float(extent.width), float(extent.height)};
        if (blackout)
            parameters = {sample[0], sample[1], sample[2], material[16], material[12], material[14],
                          material[15], precedingRampRow, float(extent.width), float(extent.height), material[17], material[18]};
        if (chromatic)
            parameters = {sample[0], sample[1], material[9], material[10], material[11], float(extent.width), float(extent.height)};
        host.submit([&](rhi::ICommandRecorder& commands, VkCommandBuffer native) {
            constexpr VkClearValue background{{{.25F, .5F, .75F, 1}}};
            commands.beginRenderPass({pass, framebuffer, extent, {background}});
            commands.setViewport(float(extent.width), float(extent.height));
            commands.setScissor(extent);
            commands.bindPipeline(pipeline);
            if (textured) commands.bindDescriptorSet(layout, set);
            commands.pushConstants(layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, parameters.data(), pushSize);
            commands.draw(3);
            commands.endRenderPass();
            commands.imageBarrier({color.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT});
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {extent.width, extent.height, 1};
            vkCmdCopyImageToBuffer(native, color.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, output.buffer, 1, &copy);
            commands.bufferBarrier({output.buffer, 0, output.size, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT});
        });
        host.allocator.invalidate(output, 0, output.size);
        capture.write(static_cast<const char*>(output.mapped), output.size);
        if (!capture) throw std::runtime_error("Could not record Vulkan output");
    }
    cleanup.reset();
    if (host.errors.load() || host.allocator.statistics().liveBuffers || host.allocator.statistics().liveImages)
        throw std::runtime_error("Vulkan validation or allocation retirement failed");
    std::cout << (blackout ? "Blackout" : chromatic ? "Chromatic" : "Monochrome noise")
              << " sampled on " << host.properties.deviceName << '\n';
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
