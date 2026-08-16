#include "scenes/BlackholeSceneRenderer.hpp"

#include "app/AzureRenderInternal.hpp"
#include "render/RenderSettings.hpp"
#include "render/VulkanHelpers.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace azurerender::internal;

namespace azurerender {

SceneRendererCapabilities BlackholeSceneRenderer::capabilities() const {
    SceneRendererCapabilities caps;
    caps.requiresSceneDepth = false;
    caps.requiresSceneNormal = false;
    caps.diagnosticViewNames = {
        "Beauty",
        "Photon Ring",
        "Gravitational Lens",
    };
    return caps;
}

void BlackholeSceneRenderer::onLoad(const RenderContext& context) {
    device_ = context.device;
    physicalDevice_ = context.physicalDevice;
    graphicsQueue_ = context.graphicsQueue;
    commandPool_ = context.commandPool;
    shaderDirectory_ = context.shaderDirectory;
    renderSettings_ = context.renderSettings;

    VkDescriptorSetLayoutBinding uniformBinding{};
    uniformBinding.binding = 0;
    uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBinding.descriptorCount = 1;
    uniformBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uniformBinding;
    vkCheck(
        vkCreateDescriptorSetLayout(
            device_, &layoutInfo, nullptr, &descriptorSetLayout_),
        "vkCreateDescriptorSetLayout(blackhole)");

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<std::uint32_t>(kMaxFramesInFlight);
    VkDescriptorPoolCreateInfo poolInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<std::uint32_t>(kMaxFramesInFlight);
    vkCheck(
        vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_),
        "vkCreateDescriptorPool(blackhole)");

    const std::vector<VkDescriptorSetLayout> layouts(
        kMaxFramesInFlight, descriptorSetLayout_);
    VkDescriptorSetAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocateInfo.descriptorPool = descriptorPool_;
    allocateInfo.descriptorSetCount =
        static_cast<std::uint32_t>(layouts.size());
    allocateInfo.pSetLayouts = layouts.data();
    descriptorSets_.resize(kMaxFramesInFlight);
    vkCheck(
        vkAllocateDescriptorSets(device_, &allocateInfo, descriptorSets_.data()),
        "vkAllocateDescriptorSets(blackhole)");

    createUniformBuffers();
    createGraphicsPipeline(context);

    // Bind one per-frame uniform buffer per per-frame descriptor set.
    for (std::size_t frame = 0; frame < kMaxFramesInFlight; ++frame) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers_[frame];
        bufferInfo.range = sizeof(BlackholeUniform);
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = descriptorSets_[frame];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &bufferInfo;
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    }
}

void BlackholeSceneRenderer::onSwapchainRecreate(
    const RenderContext& context) {
    renderSettings_ = context.renderSettings;
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    createGraphicsPipeline(context);
}

void BlackholeSceneRenderer::updateFrame(const SceneFrameData& frame) {
    currentFrame_ = frame.currentFrame;
    cameraPosition_ = {
        frame.cameraPosition[0],
        frame.cameraPosition[1],
        frame.cameraPosition[2],
    };
    cameraTarget_ = {
        frame.cameraTarget[0],
        frame.cameraTarget[1],
        frame.cameraTarget[2],
    };
    rotationAngle_ = frame.rotationAngle;
    aspect_ = static_cast<float>(frame.swapchainWidth)
        / static_cast<float>(std::max(frame.swapchainHeight, 1U));
    updateUniformBuffer();
}

void BlackholeSceneRenderer::recordScene(const RenderContext& context) {
    // Clear the engine shadow map with an empty depth pass so the image ends
    // in DEPTH_STENCIL_READ_ONLY_OPTIMAL (the post-process Shadow Map
    // diagnostic samples it) even though this renderer casts no shadows.
    if (context.shadowRenderPass != VK_NULL_HANDLE
        && context.shadowFramebuffer != VK_NULL_HANDLE) {
        VkClearValue shadowClear{};
        shadowClear.depthStencil = {1.0F, 0};
        VkRenderPassBeginInfo shadowPassInfo{
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        shadowPassInfo.renderPass = context.shadowRenderPass;
        shadowPassInfo.framebuffer = context.shadowFramebuffer;
        shadowPassInfo.renderArea.extent = {
            context.shadowMapSize,
            context.shadowMapSize,
        };
        shadowPassInfo.clearValueCount = 1;
        shadowPassInfo.pClearValues = &shadowClear;
        vkCmdBeginRenderPass(
            context.commandBuffer,
            &shadowPassInfo,
            VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(context.commandBuffer);
    }

    if (context.gpuTimingEnabled && context.timestampQueryPool != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp(
            context.commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            context.timestampQueryPool,
            1);
    }

    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color.float32[0] = 0.0F;
    clearValues[0].color.float32[1] = 0.0F;
    clearValues[0].color.float32[2] = 0.0F;
    clearValues[0].color.float32[3] = 1.0F;
    clearValues[1].depthStencil = {1.0F, 0};
    clearValues[2].color.float32[0] = 0.5F;
    clearValues[2].color.float32[1] = 0.5F;
    clearValues[2].color.float32[2] = 1.0F;
    clearValues[2].color.float32[3] = 0.0F;
    VkRenderPassBeginInfo renderPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassInfo.renderPass = context.sceneRenderPass;
    renderPassInfo.framebuffer = context.sceneFramebuffer;
    renderPassInfo.renderArea.extent = context.renderExtent;
    renderPassInfo.clearValueCount =
        static_cast<std::uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(
        context.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{};
    viewport.width = static_cast<float>(context.renderExtent.width);
    viewport.height = static_cast<float>(context.renderExtent.height);
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(context.commandBuffer, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.extent = context.renderExtent;
    vkCmdSetScissor(context.commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(
        context.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_);
    vkCmdBindDescriptorSets(
        context.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout_,
        0,
        1,
        &descriptorSets_[context.currentFrame % kMaxFramesInFlight],
        0,
        nullptr);
    vkCmdDraw(context.commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(context.commandBuffer);

    if (context.gpuTimingEnabled && context.timestampQueryPool != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp(
            context.commandBuffer,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            context.timestampQueryPool,
            2);
    }
}

void BlackholeSceneRenderer::onUnload(const RenderContext& context) {
    (void)context;
    destroyResources();
}

void BlackholeSceneRenderer::appendHudText(std::ostringstream& text) const {
    const float rs = 1.0F;
    text << "BLACKHOLE: SCHWARZSCHILD RS=" << std::fixed
         << std::setprecision(1) << rs << "  PHOTON SPHERE 1.5RS  "
         << "ESCAPE " << 40.0F << "RS\n"
         << "CAM  : [" << cameraPosition_[0] << ", "
         << cameraPosition_[1] << ", " << cameraPosition_[2] << "]  "
         << "ANGLE " << std::fixed << std::setprecision(1)
         << rotationAngle_ * 180.0F / 3.14159265358979323846F << " DEG\n";
}

// ---------------------------------------------------------------------------
// Resource creation
// ---------------------------------------------------------------------------

void BlackholeSceneRenderer::createUniformBuffers() {
    const VkDeviceSize size = sizeof(BlackholeUniform);
    uniformBuffers_.resize(kMaxFramesInFlight);
    uniformBufferMemories_.resize(kMaxFramesInFlight);
    uniformBufferMapped_.resize(kMaxFramesInFlight);
    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        vk::createBuffer(
            device_,
            physicalDevice_,
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffers_[index],
            uniformBufferMemories_[index]);
        vkCheck(
            vkMapMemory(
                device_,
                uniformBufferMemories_[index],
                0,
                size,
                0,
                &uniformBufferMapped_[index]),
            "vkMapMemory(blackhole uniform)");
    }
}

void BlackholeSceneRenderer::createGraphicsPipeline(
    const RenderContext& context) {
    const auto vertexCode =
        vk::readBinaryFile(shaderDirectory_ + "/blackhole.vert.spv");
    const auto fragmentCode =
        vk::readBinaryFile(shaderDirectory_ + "/blackhole.frag.spv");
    const VkShaderModule vertexModule =
        vk::createShaderModule(device_, vertexCode);
    const VkShaderModule fragmentModule =
        vk::createShaderModule(device_, fragmentCode);

    try {
        VkPipelineShaderStageCreateInfo vertexStage{
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.module = vertexModule;
        vertexStage.pName = "main";
        VkPipelineShaderStageCreateInfo fragmentStage{
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.module = fragmentModule;
        fragmentStage.pName = "main";
        const std::array shaderStages = {vertexStage, fragmentStage};

        VkPipelineVertexInputStateCreateInfo vertexInput{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewportState{
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rasterizer{
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.lineWidth = 1.0F;
        VkPipelineMultisampleStateCreateInfo multisampling{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depthStencil{
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        // The scene render pass has two color attachments (Scene Color +
        // World Normal); the tracer writes only the first.
        std::array<VkPipelineColorBlendAttachmentState, 2>
            colorAttachments{};
        colorAttachments[0].blendEnable = VK_FALSE;
        colorAttachments[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorAttachments[1].blendEnable = VK_FALSE;
        colorAttachments[1].colorWriteMask = 0;
        VkPipelineColorBlendStateCreateInfo colorBlending{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.attachmentCount =
            static_cast<std::uint32_t>(colorAttachments.size());
        colorBlending.pAttachments = colorAttachments.data();
        const std::array dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo dynamicState{
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicState.dynamicStateCount =
            static_cast<std::uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineLayoutCreateInfo layoutInfo{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &descriptorSetLayout_;
        vkCheck(
            vkCreatePipelineLayout(
                device_, &layoutInfo, nullptr, &pipelineLayout_),
            "vkCreatePipelineLayout(blackhole)");

        VkGraphicsPipelineCreateInfo pipelineInfo{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = static_cast<std::uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout_;
        pipelineInfo.renderPass = context.sceneRenderPass;
        pipelineInfo.subpass = 0;
        vkCheck(
            vkCreateGraphicsPipelines(
                device_,
                VK_NULL_HANDLE,
                1,
                &pipelineInfo,
                nullptr,
                &pipeline_),
            "vkCreateGraphicsPipelines(blackhole)");
    } catch (...) {
        vkDestroyShaderModule(device_, fragmentModule, nullptr);
        vkDestroyShaderModule(device_, vertexModule, nullptr);
        throw;
    }
    vkDestroyShaderModule(device_, fragmentModule, nullptr);
    vkDestroyShaderModule(device_, vertexModule, nullptr);
}

void BlackholeSceneRenderer::destroyResources() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }
    for (std::size_t index = 0; index < uniformBuffers_.size(); ++index) {
        if (uniformBufferMapped_[index] != nullptr) {
            vkUnmapMemory(device_, uniformBufferMemories_[index]);
        }
        vkDestroyBuffer(device_, uniformBuffers_[index], nullptr);
        vkFreeMemory(device_, uniformBufferMemories_[index], nullptr);
    }
    uniformBuffers_.clear();
    uniformBufferMemories_.clear();
    uniformBufferMapped_.clear();
}

// ---------------------------------------------------------------------------
// Frame data
// ---------------------------------------------------------------------------

void BlackholeSceneRenderer::updateUniformBuffer() {
    const float fov = 0.9F;

    // Orbit the camera around the hole while keeping the target at origin.
    const float cosine = std::cos(rotationAngle_);
    const float sine = std::sin(rotationAngle_);
    std::array<float, 3> orbitPosition{
        cameraPosition_[0] * cosine - cameraPosition_[2] * sine,
        cameraPosition_[1],
        cameraPosition_[0] * sine + cameraPosition_[2] * cosine,
    };
    const std::array<float, 3> forward = normalize(subtract(
        {cameraTarget_[0], cameraTarget_[1], cameraTarget_[2]},
        orbitPosition));
    const std::array<float, 3> worldUp{0.0F, 1.0F, 0.0F};
    const std::array<float, 3> right = normalize(cross(forward, worldUp));
    const std::array<float, 3> up = cross(right, forward);

    BlackholeUniform uniform{};
    uniform.cameraPosition = {
        orbitPosition[0], orbitPosition[1], orbitPosition[2], 1.0F,
    };
    uniform.cameraRight = {right[0], right[1], right[2], 0.0F};
    uniform.cameraUp = {up[0], up[1], up[2], 0.0F};
    uniform.cameraForward = {forward[0], forward[1], forward[2], 0.0F};
    uniform.physics = {1.0F, 40.0F, 900.0F, 1.0F};
    uniform.cameraFov = {fov, aspect_, 0.0F, 0.0F};
    uniform.diskParameters = {3.0F, 12.0F, 1.0F, 1.0F};
    std::memcpy(
        uniformBufferMapped_[currentFrame_],
        &uniform,
        sizeof(uniform));
}

}  // namespace azurerender
