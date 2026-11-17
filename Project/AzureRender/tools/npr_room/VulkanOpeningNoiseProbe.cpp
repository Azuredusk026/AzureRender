#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>

int main(int argc, char** argv) try {
    if (argc != 4 || std::filesystem::exists(argv[3]))
        throw std::invalid_argument("Expected vertex and fragment shaders and unused capture path");
    using namespace azurerender;
    test::VulkanComputeFixture host(true);
    auto& backend = *host.backend;
    rhi::GpuBuffer output{};
    rhi::GpuImage color{};
    VkImageView view{};
    VkRenderPass pass{};
    VkFramebuffer framebuffer{};
    VkDescriptorSetLayout descriptors{};
    VkPipelineLayout layout{};
    VkShaderModule vertex{}, fragment{};
    VkPipeline pipeline{};
    auto cleanup = std::unique_ptr<void, std::function<void(void*)>>(&host, [&](void*) {
        if (pipeline) backend.destroyPipeline(pipeline);
        if (vertex) backend.destroyShaderModule(vertex);
        if (fragment) backend.destroyShaderModule(fragment);
        if (layout) backend.destroyPipelineLayout(layout);
        if (descriptors) backend.destroyDescriptorSetLayout(descriptors);
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
    descriptors = backend.createDescriptorSetLayout({});
    constexpr rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_FRAGMENT_BIT, 4 * sizeof(float)};
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
    graphics.alphaBlend = true;
    pipeline = backend.createGraphicsPipeline(graphics);
    const std::array<std::array<float, 2>, 3> samples{{{0, std::sin(9.302F)}, {.05F, std::sin(9.302F)}, {.05F, std::sin(9.802F)}}};
    std::ofstream capture(argv[3], std::ios::binary);
    for (const auto& sample : samples) {
        const std::array<float, 4> parameters{sample[0], sample[1], float(extent.width), float(extent.height)};
        host.submit([&](rhi::ICommandRecorder& commands, VkCommandBuffer native) {
            constexpr VkClearValue background{{{.25F, .5F, .75F, 1}}};
            commands.beginRenderPass({pass, framebuffer, extent, {background}});
            commands.setViewport(float(extent.width), float(extent.height));
            commands.setScissor(extent);
            commands.bindPipeline(pipeline);
            commands.pushConstants(layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, parameters.data(), sizeof(parameters));
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
    std::cout << "Monochrome noise sampled on " << host.properties.deviceName << '\n';
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
