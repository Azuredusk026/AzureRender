#include "scenes/BlackholeSceneRenderer.hpp"

#include "app/AzureRenderInternal.hpp"
#include "render/RenderSettings.hpp"
#include "render/VulkanHelpers.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <ostream>
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
    createTraceResources(context);

    // TAA descriptors are immutable for every frame/ping combination. This
    // avoids updating descriptors that may still be referenced in flight.
    {
        const std::array<VkDescriptorSetLayoutBinding, 3> taaBindings = {{
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        }};
        VkDescriptorSetLayoutCreateInfo layoutInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = static_cast<std::uint32_t>(taaBindings.size());
        layoutInfo.pBindings = taaBindings.data();
        vkCheck(
            vkCreateDescriptorSetLayout(
                device_, &layoutInfo, nullptr, &taaDescriptorSetLayout_),
            "vkCreateDescriptorSetLayout(blackhole TAA)");

        const std::array<VkDescriptorPoolSize, 2> poolSizes = {{
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             static_cast<std::uint32_t>(taaDescriptorSets_.size())},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             static_cast<std::uint32_t>(taaDescriptorSets_.size() * 2)},
        }};
        VkDescriptorPoolCreateInfo poolInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets =
            static_cast<std::uint32_t>(taaDescriptorSets_.size());
        vkCheck(
            vkCreateDescriptorPool(device_, &poolInfo, nullptr, &taaDescriptorPool_),
            "vkCreateDescriptorPool(blackhole TAA)");
        const std::array<VkDescriptorSetLayout, kMaxFramesInFlight * 2>
            taaLayouts{taaDescriptorSetLayout_, taaDescriptorSetLayout_,
                       taaDescriptorSetLayout_, taaDescriptorSetLayout_};
        VkDescriptorSetAllocateInfo allocateInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocateInfo.descriptorPool = taaDescriptorPool_;
        allocateInfo.descriptorSetCount =
            static_cast<std::uint32_t>(taaLayouts.size());
        allocateInfo.pSetLayouts = taaLayouts.data();
        vkCheck(
            vkAllocateDescriptorSets(
                device_, &allocateInfo, taaDescriptorSets_.data()),
            "vkAllocateDescriptorSets(blackhole TAA)");
    }

    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo layoutInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;
        vkCheck(
            vkCreateDescriptorSetLayout(
                device_, &layoutInfo, nullptr, &compositeDescriptorSetLayout_),
            "vkCreateDescriptorSetLayout(blackhole composite)");

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 2;
        VkDescriptorPoolCreateInfo poolInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 2;
        vkCheck(
            vkCreateDescriptorPool(
                device_, &poolInfo, nullptr, &compositeDescriptorPool_),
            "vkCreateDescriptorPool(blackhole composite)");
        const std::array<VkDescriptorSetLayout, 2> layouts{
            compositeDescriptorSetLayout_, compositeDescriptorSetLayout_};
        VkDescriptorSetAllocateInfo allocateInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocateInfo.descriptorPool = compositeDescriptorPool_;
        allocateInfo.descriptorSetCount = 2;
        allocateInfo.pSetLayouts = layouts.data();
        vkCheck(
            vkAllocateDescriptorSets(
                device_, &allocateInfo, compositeDescriptorSets_.data()),
            "vkAllocateDescriptorSets(blackhole composite)");
    }

    // TAA per-frame uniform buffers.
    taaUniformBuffers_.resize(kMaxFramesInFlight);
    taaUniformBufferMemories_.resize(kMaxFramesInFlight);
    taaUniformBufferMapped_.resize(kMaxFramesInFlight);
    for (std::size_t frame = 0; frame < kMaxFramesInFlight; ++frame) {
        vk::createBuffer(
            device_,
            physicalDevice_,
            sizeof(TaaUniform),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            taaUniformBuffers_[frame],
            taaUniformBufferMemories_[frame]);
        vkCheck(
            vkMapMemory(
                device_,
                taaUniformBufferMemories_[frame],
                0,
                sizeof(TaaUniform),
                0,
                &taaUniformBufferMapped_[frame]),
            "vkMapMemory(blackhole TAA uniform)");
    }
    updateTaaUniform();

    transitionInitialLayouts();
    createGraphicsPipeline(context);
    createTaaPipeline(context);
    createCompositePipeline(context);

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
    updateTemporalDescriptorSets();
    invalidateHistory();
}

void BlackholeSceneRenderer::transitionInitialLayouts() {
    VkCommandBufferAllocateInfo allocInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool_;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkCheck(
        vkAllocateCommandBuffers(device_, &allocInfo, &cmd),
        "vkAllocateCommandBuffers(transition)");
    VkCommandBufferBeginInfo beginInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(
        vkBeginCommandBuffer(cmd, &beginInfo), "vkBeginCommandBuffer(transition)");
    const std::array<VkImage, 3> images{
        traceImage_, historyImages_[0], historyImages_[1]};
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    for (const VkImage image : images) {
        barrier.image = image;
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);
        const VkClearColorValue clear{{0.0F, 0.0F, 0.0F, 1.0F}};
        vkCmdClearColorImage(
            cmd,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clear,
            1,
            &barrier.subresourceRange);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    vkCheck(vkEndCommandBuffer(cmd), "vkEndCommandBuffer(transition)");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkCheck(
        vkQueueSubmit(graphicsQueue_, 1, &submit, VK_NULL_HANDLE),
        "vkQueueSubmit(transition)");
    vkCheck(vkQueueWaitIdle(graphicsQueue_), "vkQueueWaitIdle(transition)");
    vkFreeCommandBuffers(device_, commandPool_, 1, &cmd);
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
    if (taaPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, taaPipeline_, nullptr);
        taaPipeline_ = VK_NULL_HANDLE;
    }
    if (taaPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, taaPipelineLayout_, nullptr);
        taaPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (compositePipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, compositePipeline_, nullptr);
        compositePipeline_ = VK_NULL_HANDLE;
    }
    if (compositePipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, compositePipelineLayout_, nullptr);
        compositePipelineLayout_ = VK_NULL_HANDLE;
    }
    destroySizeDependentResources();
    createTraceResources(context);
    transitionInitialLayouts();
    updateTemporalDescriptorSets();
    createGraphicsPipeline(context);
    createTaaPipeline(context);
    createCompositePipeline(context);
    invalidateHistory();
}

void BlackholeSceneRenderer::updateFrame(const SceneFrameData& frame) {
    currentFrame_ = frame.currentFrame;
    if (frame.renderSettings != nullptr) {
        const BlackholeQuality nextQuality =
            frame.renderSettings->blackhole.quality;
        const BlackholeCamera nextCamera =
            frame.renderSettings->blackhole.camera;
        const BlackholeSettings previous{quality_, camera_};
        const BlackholeSettings next{nextQuality, nextCamera};
        if (frameSeen_ && blackholeHistoryNeedsReset(previous, next)) {
            invalidateHistory();
        }
        quality_ = nextQuality;
        camera_ = nextCamera;
    }
    const BlackholeQualityParameters qualityParameters =
        blackholeQualityParameters(quality_);
    maxTraceSteps_ = qualityParameters.maxTraceSteps;
    samplesPerPixel_ = qualityParameters.samplesPerPixel;
    nearStepScale_ = qualityParameters.nearStepScale;
    switch (camera_) {
    case BlackholeCamera::Front:
        cameraPosition_ = {0.0F, 4.0F, 24.0F};
        cameraTarget_ = {0.0F, 0.0F, 0.0F};
        break;
    case BlackholeCamera::OrbitLeft:
        cameraPosition_ = {-9.0F, 5.0F, 22.0F};
        cameraTarget_ = {0.0F, 0.0F, 0.0F};
        break;
    case BlackholeCamera::High:
        cameraPosition_ = {0.0F, 11.0F, 22.0F};
        cameraTarget_ = {0.0F, 0.0F, 0.0F};
        break;
    case BlackholeCamera::Close:
        cameraPosition_ = {0.0F, 2.0F, 15.0F};
        cameraTarget_ = {0.0F, 0.0F, 0.0F};
        break;
    case BlackholeCamera::OverShoulder:
        cameraPosition_ = {-12.0F, 8.0F, 23.0F};
        cameraTarget_ = {-7.0F, 4.5F, 0.0F};
        break;
    }
    // The black hole owns its own framing: the host camera/portfolio orbit
    // would place the eye too close or off-axis for the accretion disk to
    // be sampled. We only borrow the swapchain aspect ratio and size.
    const float rotationDelta = std::remainder(
        frame.rotationAngle - previousRotationAngle_,
        2.0F * 3.14159265358979323846F);
    if (frameSeen_ && std::abs(rotationDelta) > 0.12F) {
        invalidateHistory();
    }
    if (frame.captureActive && !captureActive_) {
        invalidateHistory();
    }
    rotationAngle_ = frame.rotationAngle;
    previousRotationAngle_ = frame.rotationAngle;
    captureActive_ = frame.captureActive;
    frameSeen_ = true;
    aspect_ = static_cast<float>(frame.swapchainWidth)
        / static_cast<float>(std::max(frame.swapchainHeight, 1U));
    simulationTime_ += std::max(frame.deltaSeconds, 0.0F);
    renderWidth_ = frame.swapchainWidth;
    renderHeight_ = frame.swapchainHeight;
    // Temporal blend weight: fast decay (~90ms) keeps the disk detail sharp
    // while smoothing the per-frame jitter noise.
    const float halfLife = 0.055F;
    blendWeight_ = 1.0F - std::pow(
        0.5F,
        std::clamp(frame.deltaSeconds / halfLife, 0.0F, 1.0F));
    blendWeight_ = std::max(
        blendWeight_, std::clamp(std::abs(rotationDelta) * 45.0F, 0.0F, 1.0F));
    updateUniformBuffer();
    updateTaaUniform();
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

    VkViewport viewport{};
    viewport.width = static_cast<float>(context.renderExtent.width);
    viewport.height = static_cast<float>(context.renderExtent.height);
    viewport.maxDepth = 1.0F;
    VkRect2D scissor{};
    scissor.extent = context.renderExtent;

    // 1. Trace the current jittered sample into a private raw HDR image.
    VkClearValue traceClear{};
    traceClear.color.float32[3] = 1.0F;
    VkRenderPassBeginInfo tracePassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    tracePassInfo.renderPass = traceRenderPass_;
    tracePassInfo.framebuffer = traceFramebuffer_;
    tracePassInfo.renderArea.extent = context.renderExtent;
    tracePassInfo.clearValueCount = 1;
    tracePassInfo.pClearValues = &traceClear;
    vkCmdBeginRenderPass(
        context.commandBuffer, &tracePassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(context.commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(context.commandBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(
        context.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    const VkDescriptorSet traceSet =
        descriptorSets_[context.currentFrame % kMaxFramesInFlight];
    vkCmdBindDescriptorSets(
        context.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout_,
        0,
        1,
        &traceSet,
        0,
        nullptr);
    vkCmdDraw(context.commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(context.commandBuffer);

    // 2. Accumulate raw trace + previous history and apply compact HDR bloom.
    VkRenderPassBeginInfo historyPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    historyPassInfo.renderPass = traceRenderPass_;
    historyPassInfo.framebuffer = historyFramebuffers_[historyWriteIndex_];
    historyPassInfo.renderArea.extent = context.renderExtent;
    historyPassInfo.clearValueCount = 1;
    historyPassInfo.pClearValues = &traceClear;
    vkCmdBeginRenderPass(
        context.commandBuffer, &historyPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(context.commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(context.commandBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(
        context.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, taaPipeline_);
    const std::size_t taaSetIndex =
        (context.currentFrame % kMaxFramesInFlight) * 2 + historyWriteIndex_;
    const VkDescriptorSet taaSet = taaDescriptorSets_[taaSetIndex];
    vkCmdBindDescriptorSets(
        context.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        taaPipelineLayout_,
        0,
        1,
        &taaSet,
        0,
        nullptr);
    vkCmdDraw(context.commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(context.commandBuffer);

    // 3. Copy the newly accumulated history into engine Scene Color.
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
    VkRenderPassBeginInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    passInfo.renderPass = context.sceneRenderPass;
    passInfo.framebuffer = context.sceneFramebuffer;
    passInfo.renderArea.extent = context.renderExtent;
    passInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
    passInfo.pClearValues = clearValues.data();
    vkCmdBeginRenderPass(
        context.commandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(context.commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(context.commandBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(
        context.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        compositePipeline_);
    const VkDescriptorSet compositeSet =
        compositeDescriptorSets_[historyWriteIndex_];
    vkCmdBindDescriptorSets(
        context.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        compositePipelineLayout_,
        0,
        1,
        &compositeSet,
        0,
        nullptr);
    vkCmdDraw(context.commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(context.commandBuffer);
    historyValid_ = true;
    historyWriteIndex_ ^= 1U;

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
         << rotationAngle_ * 180.0F / 3.14159265358979323846F << " DEG\n"
         << "TEMP : TAA ON  HISTORY "
         << (historyValid_ ? "VALID" : "RESET")
         << "  BLOOM 0.30  RESET " << historyResetCount_ << "\n"
         << "TRACE: " << samplesPerPixel_ << " SPP  MAX "
         << maxTraceSteps_ << " STEPS  "
         << blackholeQualityName(quality_) << "\n";
}

void BlackholeSceneRenderer::appendCaptureManifestFields(
    std::ostream& json) const {
    json
        << "  \"blackhole\": {\n"
        << "    \"rs\": 1.0,\n"
        << "    \"camera\": ["
        << cameraPosition_[0] << ", "
        << cameraPosition_[1] << ", "
        << cameraPosition_[2] << "],\n"
        << "    \"taa\": true,\n"
        << "    \"historyValid\": "
        << (historyValid_ ? "true" : "false") << ",\n"
        << "    \"historyResets\": " << historyResetCount_ << ",\n"
        << "    \"quality\": \"" << blackholeQualityName(quality_) << "\",\n"
        << "    \"cameraPreset\": \"" << blackholeCameraName(camera_) << "\",\n"
        << "    \"traceSamplesPerPixel\": " << samplesPerPixel_ << ",\n"
        << "    \"maxTraceSteps\": " << maxTraceSteps_ << ",\n"
        << "    \"nearStepScale\": " << nearStepScale_ << ",\n"
        << "    \"bloom\": {\"mode\": \"single-pass\", "
        << "\"threshold\": 1.2, \"intensity\": 0.30},\n"
        << "    \"lensingCorrection\": true,\n"
        << "    \"starfieldBlueShift\": true\n"
        << "  },\n";
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
    (void)context;
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
        VkPipelineColorBlendAttachmentState colorAttachment{};
        colorAttachment.blendEnable = VK_FALSE;
        colorAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlending{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorAttachment;
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
        pipelineInfo.renderPass = traceRenderPass_;
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

void BlackholeSceneRenderer::createTraceResources(
    const RenderContext& context) {
    // All private images stay shader-readable between passes. The explicit
    // dependencies cover history sampling -> color write -> later sampling.
    VkAttachmentDescription attachment{};
    attachment.format = context.sceneColorFormat;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    const std::array<VkSubpassDependency, 2> dependencies{{
        {
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
        },
        {
            0,
            VK_SUBPASS_EXTERNAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
        },
    }};
    VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    passInfo.attachmentCount = 1;
    passInfo.pAttachments = &attachment;
    passInfo.subpassCount = 1;
    passInfo.pSubpasses = &subpass;
    passInfo.dependencyCount =
        static_cast<std::uint32_t>(dependencies.size());
    passInfo.pDependencies = dependencies.data();
    vkCheck(
        vkCreateRenderPass(device_, &passInfo, nullptr, &traceRenderPass_),
        "vkCreateRenderPass(blackhole trace)");

    const VkExtent2D extent = context.renderExtent;
    const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    vk::createImage(
        device_, physicalDevice_, extent.width, extent.height,
        context.sceneColorFormat, usage, traceImage_, traceImageMemory_, 1);
    traceImageView_ = vk::createImageView(
        device_, traceImage_, context.sceneColorFormat,
        VK_IMAGE_ASPECT_COLOR_BIT, 1);
    VkFramebufferCreateInfo framebufferInfo{
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass = traceRenderPass_;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &traceImageView_;
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1;
    vkCheck(
        vkCreateFramebuffer(
            device_, &framebufferInfo, nullptr, &traceFramebuffer_),
        "vkCreateFramebuffer(blackhole raw trace)");

    for (std::size_t index = 0; index < historyImages_.size(); ++index) {
        vk::createImage(
            device_, physicalDevice_, extent.width, extent.height,
            context.sceneColorFormat, usage, historyImages_[index],
            historyImageMemories_[index], 1);
        historyImageViews_[index] = vk::createImageView(
            device_, historyImages_[index], context.sceneColorFormat,
            VK_IMAGE_ASPECT_COLOR_BIT, 1);
        VkFramebufferCreateInfo framebufferInfo{
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebufferInfo.renderPass = traceRenderPass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &historyImageViews_[index];
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;
        vkCheck(
            vkCreateFramebuffer(
                device_, &framebufferInfo, nullptr,
                &historyFramebuffers_[index]),
            "vkCreateFramebuffer(blackhole history)");
    }
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCheck(
        vkCreateSampler(device_, &samplerInfo, nullptr, &traceSampler_),
        "vkCreateSampler(blackhole trace)");
    historyWriteIndex_ = 0;
}

void BlackholeSceneRenderer::updateTemporalDescriptorSets() {
    for (std::size_t frame = 0; frame < kMaxFramesInFlight; ++frame) {
        for (std::size_t writeIndex = 0; writeIndex < 2; ++writeIndex) {
            const std::size_t setIndex = frame * 2 + writeIndex;
            VkDescriptorBufferInfo uniformInfo{};
            uniformInfo.buffer = taaUniformBuffers_[frame];
            uniformInfo.range = sizeof(TaaUniform);
            VkDescriptorImageInfo traceInfo{};
            traceInfo.sampler = traceSampler_;
            traceInfo.imageView = traceImageView_;
            traceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkDescriptorImageInfo previousInfo{};
            previousInfo.sampler = traceSampler_;
            previousInfo.imageView = historyImageViews_[writeIndex ^ 1U];
            previousInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            std::array<VkWriteDescriptorSet, 3> writes{};
            writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[0].dstSet = taaDescriptorSets_[setIndex];
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].pBufferInfo = &uniformInfo;
            writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[1].dstSet = taaDescriptorSets_[setIndex];
            writes[1].dstBinding = 1;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].pImageInfo = &traceInfo;
            writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[2].dstSet = taaDescriptorSets_[setIndex];
            writes[2].dstBinding = 2;
            writes[2].descriptorCount = 1;
            writes[2].descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[2].pImageInfo = &previousInfo;
            vkUpdateDescriptorSets(
                device_, static_cast<std::uint32_t>(writes.size()),
                writes.data(), 0, nullptr);
        }
    }

    for (std::size_t index = 0; index < 2; ++index) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = traceSampler_;
        imageInfo.imageView = historyImageViews_[index];
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = compositeDescriptorSets_[index];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    }
}

void BlackholeSceneRenderer::createTaaPipeline(const RenderContext& context) {
    (void)context;
    const auto vertexCode =
        vk::readBinaryFile(shaderDirectory_ + "/blackhole.vert.spv");
    const auto fragmentCode =
        vk::readBinaryFile(shaderDirectory_ + "/blackhole_taa.frag.spv");
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
        VkPipelineColorBlendAttachmentState colorAttachment{};
        colorAttachment.blendEnable = VK_FALSE;
        colorAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlending{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorAttachment;
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
        layoutInfo.pSetLayouts = &taaDescriptorSetLayout_;
        vkCheck(
            vkCreatePipelineLayout(
                device_, &layoutInfo, nullptr, &taaPipelineLayout_),
            "vkCreatePipelineLayout(blackhole TAA)");

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
        pipelineInfo.layout = taaPipelineLayout_;
        pipelineInfo.renderPass = traceRenderPass_;
        pipelineInfo.subpass = 0;
        vkCheck(
            vkCreateGraphicsPipelines(
                device_,
                VK_NULL_HANDLE,
                1,
                &pipelineInfo,
                nullptr,
                &taaPipeline_),
            "vkCreateGraphicsPipelines(blackhole TAA)");
    } catch (...) {
        vkDestroyShaderModule(device_, fragmentModule, nullptr);
        vkDestroyShaderModule(device_, vertexModule, nullptr);
        throw;
    }
    vkDestroyShaderModule(device_, fragmentModule, nullptr);
    vkDestroyShaderModule(device_, vertexModule, nullptr);
}

void BlackholeSceneRenderer::createCompositePipeline(
    const RenderContext& context) {
    const auto vertexCode =
        vk::readBinaryFile(shaderDirectory_ + "/blackhole.vert.spv");
    const auto fragmentCode =
        vk::readBinaryFile(shaderDirectory_ + "/blackhole_composite.frag.spv");
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
        const std::array stages{vertexStage, fragmentStage};
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
        std::array<VkPipelineColorBlendAttachmentState, 2> colorAttachments{};
        colorAttachments[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorAttachments[1].colorWriteMask = 0;
        VkPipelineColorBlendStateCreateInfo colorBlending{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.attachmentCount =
            static_cast<std::uint32_t>(colorAttachments.size());
        colorBlending.pAttachments = colorAttachments.data();
        const std::array dynamicStates{
            VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicState.dynamicStateCount =
            static_cast<std::uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();
        VkPipelineLayoutCreateInfo layoutInfo{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &compositeDescriptorSetLayout_;
        vkCheck(
            vkCreatePipelineLayout(
                device_, &layoutInfo, nullptr, &compositePipelineLayout_),
            "vkCreatePipelineLayout(blackhole composite)");
        VkGraphicsPipelineCreateInfo pipelineInfo{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = static_cast<std::uint32_t>(stages.size());
        pipelineInfo.pStages = stages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = compositePipelineLayout_;
        pipelineInfo.renderPass = context.sceneRenderPass;
        pipelineInfo.subpass = 0;
        vkCheck(
            vkCreateGraphicsPipelines(
                device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                &compositePipeline_),
            "vkCreateGraphicsPipelines(blackhole composite)");
    } catch (...) {
        vkDestroyShaderModule(device_, fragmentModule, nullptr);
        vkDestroyShaderModule(device_, vertexModule, nullptr);
        throw;
    }
    vkDestroyShaderModule(device_, fragmentModule, nullptr);
    vkDestroyShaderModule(device_, vertexModule, nullptr);
}

void BlackholeSceneRenderer::destroySizeDependentResources() {
    if (traceSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, traceSampler_, nullptr);
        traceSampler_ = VK_NULL_HANDLE;
    }
    if (traceFramebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, traceFramebuffer_, nullptr);
        traceFramebuffer_ = VK_NULL_HANDLE;
    }
    if (traceImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, traceImageView_, nullptr);
        traceImageView_ = VK_NULL_HANDLE;
    }
    if (traceImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, traceImage_, nullptr);
        vkFreeMemory(device_, traceImageMemory_, nullptr);
        traceImage_ = VK_NULL_HANDLE;
        traceImageMemory_ = VK_NULL_HANDLE;
    }
    for (std::size_t index = 0; index < historyImages_.size(); ++index) {
        if (historyFramebuffers_[index] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, historyFramebuffers_[index], nullptr);
            historyFramebuffers_[index] = VK_NULL_HANDLE;
        }
        if (historyImageViews_[index] != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, historyImageViews_[index], nullptr);
            historyImageViews_[index] = VK_NULL_HANDLE;
        }
        if (historyImages_[index] != VK_NULL_HANDLE) {
            vkDestroyImage(device_, historyImages_[index], nullptr);
            vkFreeMemory(device_, historyImageMemories_[index], nullptr);
            historyImages_[index] = VK_NULL_HANDLE;
            historyImageMemories_[index] = VK_NULL_HANDLE;
        }
    }
    if (traceRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, traceRenderPass_, nullptr);
        traceRenderPass_ = VK_NULL_HANDLE;
    }
}

void BlackholeSceneRenderer::destroyResources() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (compositePipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, compositePipeline_, nullptr);
        compositePipeline_ = VK_NULL_HANDLE;
    }
    if (compositePipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, compositePipelineLayout_, nullptr);
        compositePipelineLayout_ = VK_NULL_HANDLE;
    }
    if (compositeDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, compositeDescriptorPool_, nullptr);
        compositeDescriptorPool_ = VK_NULL_HANDLE;
    }
    if (compositeDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(
            device_, compositeDescriptorSetLayout_, nullptr);
        compositeDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
    if (taaPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, taaPipeline_, nullptr);
        taaPipeline_ = VK_NULL_HANDLE;
    }
    if (taaPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, taaPipelineLayout_, nullptr);
        taaPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (taaDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, taaDescriptorPool_, nullptr);
        taaDescriptorPool_ = VK_NULL_HANDLE;
    }
    if (taaDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, taaDescriptorSetLayout_, nullptr);
        taaDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
    for (std::size_t index = 0; index < taaUniformBuffers_.size(); ++index) {
        if (taaUniformBufferMapped_[index] != nullptr) {
            vkUnmapMemory(device_, taaUniformBufferMemories_[index]);
        }
        vkDestroyBuffer(device_, taaUniformBuffers_[index], nullptr);
        vkFreeMemory(device_, taaUniformBufferMemories_[index], nullptr);
    }
    taaUniformBuffers_.clear();
    taaUniformBufferMemories_.clear();
    taaUniformBufferMapped_.clear();

    destroySizeDependentResources();

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

void BlackholeSceneRenderer::invalidateHistory() {
    historyValid_ = false;
    historyWriteIndex_ = 0;
    ++historyResetCount_;
}

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
    uniform.physics = {
        1.0F, 80.0F, static_cast<float>(maxTraceSteps_), simulationTime_};
    uniform.cameraFov = {
        fov, aspect_, static_cast<float>(samplesPerPixel_),
        static_cast<float>(renderWidth_),
    };
    uniform.diskParameters = {2.1F, 12.0F, 1.0F, 2.4F};
    uniform.quality = {nearStepScale_, 0.0F, 0.0F, 0.0F};
    std::memcpy(
        uniformBufferMapped_[currentFrame_],
        &uniform,
        sizeof(uniform));
}

void BlackholeSceneRenderer::updateTaaUniform() {
    if (taaUniformBufferMapped_.empty()
        || taaUniformBufferMapped_[currentFrame_] == nullptr) {
        return;
    }
    TaaUniform uniform{};
    uniform.blendWeight = historyValid_ ? blendWeight_ : 1.0F;
    uniform.bloomThreshold = 1.2F;
    uniform.bloomIntensity = 0.30F;
    uniform.renderWidth = static_cast<float>(renderWidth_);
    std::memcpy(
        taaUniformBufferMapped_[currentFrame_],
        &uniform,
        sizeof(uniform));
}

}  // namespace azurerender
