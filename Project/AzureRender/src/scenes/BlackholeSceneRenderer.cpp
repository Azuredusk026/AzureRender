#include "scenes/BlackholeSceneRenderer.hpp"

#include "render/RenderMath.hpp"
#include "resources/BinaryFile.hpp"
#include "render/ComputePass.hpp"
#include "render/RenderSettings.hpp"
#include "render/EnvironmentAsset.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"

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
    allocator_ = context.allocator;
    if (allocator_ == nullptr) {
        throw std::runtime_error(
            "RenderContext must carry the engine GPU allocator");
    }
    rhi_ = context.rhi;
    if (rhi_ == nullptr) {
        throw std::runtime_error("RenderContext must carry the engine RHI");
    }
    shaderDirectory_ = context.shaderDirectory;
    environmentSource_ = context.environment;
    renderSettings_ = context.renderSettings;
    computeBloomEnabled_ = context.rgba16fStorageImage;
    RuntimeDiagnostics::instance().info(
        "render",
        computeBloomEnabled_
            ? "Blackhole bloom: four-level Vulkan compute path"
            : "Blackhole bloom: fragment fallback path");

    constexpr VkShaderStageFlags kFragment = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayout_ = rhi_->createDescriptorSetLayout({
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, kFragment},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
    });
    rhi::DescriptorPoolDesc tracePool{};
    tracePool.sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
         static_cast<std::uint32_t>(kMaxFramesInFlight)},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         static_cast<std::uint32_t>(kMaxFramesInFlight)},
    };
    tracePool.maxSets = static_cast<std::uint32_t>(kMaxFramesInFlight);
    descriptorPool_ = rhi_->createDescriptorPool(tracePool);
    descriptorSets_ = rhi_->allocateDescriptorSets(
        descriptorPool_,
        descriptorSetLayout_,
        static_cast<std::uint32_t>(kMaxFramesInFlight));

    createUniformBuffers();
    createEnvironmentTexture();
    createTraceResources(context);
    if (computeBloomEnabled_) {
        createBloomResources(context);
        createBloomDescriptors();
        createBloomPipelines();
    }

    // TAA descriptors are immutable for every frame/ping combination. This
    // avoids updating descriptors that may still be referenced in flight.
    {
        std::vector<rhi::DescriptorBindingDesc> taaBindings = {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, kFragment},
            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
            {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        };
        if (computeBloomEnabled_) {
            taaBindings.push_back(
                {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment});
        }
        taaDescriptorSetLayout_ =
            rhi_->createDescriptorSetLayout(taaBindings);

        rhi::DescriptorPoolDesc taaPool{};
        taaPool.sizes = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             static_cast<std::uint32_t>(taaDescriptorSets_.size())},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             static_cast<std::uint32_t>(
                 taaDescriptorSets_.size()
                 * (computeBloomEnabled_ ? 3 : 2))},
        };
        taaPool.maxSets =
            static_cast<std::uint32_t>(taaDescriptorSets_.size());
        taaDescriptorPool_ = rhi_->createDescriptorPool(taaPool);
        const auto allocated = rhi_->allocateDescriptorSets(
            taaDescriptorPool_,
            taaDescriptorSetLayout_,
            static_cast<std::uint32_t>(taaDescriptorSets_.size()));
        std::copy(allocated.begin(), allocated.end(), taaDescriptorSets_.begin());
    }

    {
        compositeDescriptorSetLayout_ = rhi_->createDescriptorSetLayout({
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        });

        rhi::DescriptorPoolDesc compositePool{};
        compositePool.sizes = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2}};
        compositePool.maxSets = 2;
        compositeDescriptorPool_ = rhi_->createDescriptorPool(compositePool);
        const auto allocated = rhi_->allocateDescriptorSets(
            compositeDescriptorPool_, compositeDescriptorSetLayout_, 2);
        std::copy(
            allocated.begin(), allocated.end(),
            compositeDescriptorSets_.begin());
    }

    // TAA per-frame uniform buffers.
    taaUniformBuffers_.resize(kMaxFramesInFlight);
    for (std::size_t frame = 0; frame < kMaxFramesInFlight; ++frame) {
        taaUniformBuffers_[frame] = allocator_->createBuffer(
            sizeof(TaaUniform),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            true);
    }
    updateTaaUniform();

    transitionInitialLayouts();
    createGraphicsPipeline(context);
    createTaaPipeline(context);
    createCompositePipeline(context);

    // Bind one per-frame uniform buffer per per-frame descriptor set.
    for (std::size_t frame = 0; frame < kMaxFramesInFlight; ++frame) {
        rhi::DescriptorBufferWrite uniformWrite{};
        uniformWrite.set = descriptorSets_[frame];
        uniformWrite.binding = 0;
        uniformWrite.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniformWrite.buffer = uniformBuffers_[frame].buffer;
        uniformWrite.range = sizeof(BlackholeUniform);
        rhi_->writeDescriptorBuffer(uniformWrite);

        rhi::DescriptorImageWrite environmentWrite{};
        environmentWrite.set = descriptorSets_[frame];
        environmentWrite.binding = 1;
        environmentWrite.view = environment_.view;
        environmentWrite.sampler = environment_.sampler;
        environmentWrite.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        rhi_->writeDescriptorImage(environmentWrite);
    }
    updateTemporalDescriptorSets();
    invalidateHistory();
}

void BlackholeSceneRenderer::transitionInitialLayouts() {
    for (const rhi::GpuImage* image : {
             &traceImage_,
             &historyImages_[0],
             &historyImages_[1]}) {
        rhi_->transitionImageLayout(
            *image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1);
        rhi_->clearImage(*image);
        rhi_->transitionImageLayout(
            *image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            1);
    }
}

void BlackholeSceneRenderer::onSwapchainRecreate(
    const RenderContext& context) {
    renderSettings_ = context.renderSettings;
    if (pipeline_ != VK_NULL_HANDLE) {
        rhi_->destroyPipeline(pipeline_);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyPipelineLayout(pipelineLayout_);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (taaPipeline_ != VK_NULL_HANDLE) {
        rhi_->destroyPipeline(taaPipeline_);
        taaPipeline_ = VK_NULL_HANDLE;
    }
    if (taaPipelineLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyPipelineLayout(taaPipelineLayout_);
        taaPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (compositePipeline_ != VK_NULL_HANDLE) {
        rhi_->destroyPipeline(compositePipeline_);
        compositePipeline_ = VK_NULL_HANDLE;
    }
    if (compositePipelineLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyPipelineLayout(compositePipelineLayout_);
        compositePipelineLayout_ = VK_NULL_HANDLE;
    }
    destroySizeDependentResources();
    createTraceResources(context);
    if (computeBloomEnabled_) {
        createBloomResources(context);
        createBloomDescriptors();
    }
    transitionInitialLayouts();
    updateTemporalDescriptorSets();
    createGraphicsPipeline(context);
    createTaaPipeline(context);
    createCompositePipeline(context);
    invalidateHistory();
    RuntimeDiagnostics::instance().print(
        "blackhole", "Blackhole history reset after recreate: "
            + std::to_string(historyResetCount_));
}

void BlackholeSceneRenderer::updateFrame(const SceneFrameData& frame) {
    const auto previousPosition=cameraPosition_,previousTarget=cameraTarget_;
    currentFrame_ = frame.currentFrame;
    if (frame.renderSettings != nullptr) {
        frameRenderSettings_ = *frame.renderSettings;
        renderSettings_ = &frameRenderSettings_;
    }
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
        // Reference-close composition: the hole sits on the right while the
        // near disk sweeps diagonally across the frame.
        cameraPosition_ = {0.0F, 2.2F, 10.5F};
        cameraTarget_ = {-3.4F, -0.3F, 0.0F};
        break;
    case BlackholeCamera::OverShoulder:
        cameraPosition_ = {-12.0F, 8.0F, 23.0F};
        cameraTarget_ = {-7.0F, 4.5F, 0.0F};
        break;
    }
    if(frame.cameraOverride){
        const std::array<float,3> position{frame.cameraPosition[0],frame.cameraPosition[1],frame.cameraPosition[2]};
        const std::array<float,3> target{frame.cameraTarget[0],frame.cameraTarget[1],frame.cameraTarget[2]};
        if(previousPosition!=position || previousTarget!=target)invalidateHistory();
        cameraPosition_=position;cameraTarget_=target;
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

void BlackholeSceneRenderer::registerPasses(
    RenderGraph& graph, const SceneGraphResources& resources, const RenderContext& context) {
    const auto importColor = [&graph](
        const std::string& name,
        const VkImage image) {
        rhi::ImageBarrierDesc initial{};
        initial.image = image;
        initial.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        initial.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        return graph.importImage(name, initial);
    };
    const auto raw = importColor("blackhole-trace", traceImage_.image);
    const std::size_t previousIndex = historyWriteIndex_ ^ 1U;
    const auto previous = importColor(
        "blackhole-history-read", historyImages_[previousIndex].image);
    const auto history = importColor(
        "blackhole-history-write", historyImages_[historyWriteIndex_].image);
    const auto shadow = graph.addPass("blackhole-shadow-clear", [this, &context] { recordShadowClear(context); });
    graph.attachment(shadow, resources.shadow, RenderGraphUsage::DepthAttachment,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    const auto trace = graph.addPass("blackhole-trace", [this, &context] { recordTrace(context); });
    graph.attachment(
        trace, raw, RenderGraphUsage::ColorAttachment,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    graph.dependsOn(trace, shadow);

    std::uint32_t bloomOutput = 0;
    if (computeBloomEnabled_) {
        std::vector<RenderGraph::ResourceId> bloomResources;
        bloomResources.reserve(kBloomLevelCount);
        for (std::size_t level = 0; level < kBloomLevelCount; ++level) {
            bloomResources.push_back(importColor(
                "blackhole-bloom-level-" + std::to_string(level),
                bloomLevels_[level].image));
        }
        const auto compositeBloom = importColor(
            "blackhole-bloom-composite", bloomCompositeImage_.image);

        for (std::size_t level = 0; level < kBloomLevelCount; ++level) {
            const auto downsample = graph.addPass(
                "blackhole-bloom-downsample-" + std::to_string(level),
                [this, &context, level] {
                    rhi::ICommandRecorder& commands = *context.commands;
                    const BloomPushConstants parameters{
                        1.2F,
                        level == 0 ? 1U : 0U,
                    };
                    commands.pushConstants(
                        bloomDownsamplePipelineLayout_,
                        VK_SHADER_STAGE_COMPUTE_BIT,
                        0,
                        &parameters,
                        sizeof(parameters));
                    const VkExtent2D extent = bloomLevelExtents_[level];
                    ComputePass dispatch(
                        {extent.width, extent.height, 8, 8, 1, true});
                    dispatch.record(
                        commands,
                        bloomDownsamplePipeline_,
                        bloomDownsamplePipelineLayout_,
                        bloomDownsampleSets_[level]);
                });
            graph.use(
                downsample,
                level == 0 ? raw : bloomResources[level - 1],
                RenderGraphUsage::ComputeSampled,
                false);
            graph.use(
                downsample,
                bloomResources[level],
                RenderGraphUsage::Storage,
                true);
        }

        const auto combineBloom = graph.addPass(
            "blackhole-bloom-combine",
            [this, &context] {
                const VkExtent2D extent = bloomLevelExtents_[0];
                ComputePass dispatch(
                    {extent.width, extent.height, 8, 8, 1, true});
                dispatch.record(
                    *context.commands,
                    bloomCompositePipeline_,
                    bloomCompositePipelineLayout_,
                    bloomCompositeSet_);
            });
        for (const RenderGraph::ResourceId resource : bloomResources) {
            graph.use(
                combineBloom,
                resource,
                RenderGraphUsage::ComputeSampled,
                false);
        }
        graph.use(
            combineBloom,
            compositeBloom,
            RenderGraphUsage::Storage,
            true);
        bloomOutput = compositeBloom;
    }

    const auto temporal = graph.addPass("blackhole-temporal", [this, &context] { recordTemporal(context); });
    graph.use(temporal, raw, RenderGraphUsage::Sampled, false);
    graph.use(temporal, previous, RenderGraphUsage::Sampled, false);
    if (computeBloomEnabled_) {
        graph.use(temporal, bloomOutput, RenderGraphUsage::Sampled, false);
    }
    graph.attachment(
        temporal, history, RenderGraphUsage::ColorAttachment,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const auto composite = graph.addPass("blackhole-composite", [this, &context] { recordComposite(context); });
    graph.use(composite, history, RenderGraphUsage::Sampled, false);
    graph.attachment(composite, resources.color, RenderGraphUsage::ColorAttachment,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    graph.attachment(composite, resources.depth, RenderGraphUsage::DepthAttachment,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    graph.attachment(composite, resources.normal, RenderGraphUsage::ColorAttachment,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void BlackholeSceneRenderer::recordScene(const RenderContext& context) {
    recordShadowClear(context);
    recordTrace(context);
    if (computeBloomEnabled_) {
        recordBloom(context);
    }
    recordTemporal(context);
    recordComposite(context);
}

void BlackholeSceneRenderer::recordShadowClear(const RenderContext& context) {
    // Clear the engine shadow map with an empty depth pass so the image ends
    // in DEPTH_STENCIL_READ_ONLY_OPTIMAL (the post-process Shadow Map
    // diagnostic samples it) even though this renderer casts no shadows.
    if (context.shadowRenderPass != VK_NULL_HANDLE
        && context.shadowFramebuffer != VK_NULL_HANDLE) {
        VkClearValue shadowClear{};
        shadowClear.depthStencil = {1.0F, 0};
        rhi::RenderPassBeginDesc shadowPass{};
        shadowPass.renderPass = context.shadowRenderPass;
        shadowPass.framebuffer = context.shadowFramebuffer;
        shadowPass.extent = {context.shadowMapSize, context.shadowMapSize};
        shadowPass.clearValues = {shadowClear};
        context.commands->beginRenderPass(shadowPass);
        context.commands->endRenderPass();
    }

}

void BlackholeSceneRenderer::recordTrace(const RenderContext& context) {
    if (context.gpuTimingEnabled && context.timestampQueryPool != VK_NULL_HANDLE) {
        context.commands->writeTimestamp(
            context.timestampQueryPool,
            1,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    }

    rhi::ICommandRecorder& commands = *context.commands;

    // 1. Trace the current jittered sample into a private raw HDR image.
    VkClearValue traceClear{};
    traceClear.color.float32[3] = 1.0F;
    rhi::RenderPassBeginDesc tracePass{};
    tracePass.renderPass = traceRenderPass_;
    tracePass.framebuffer = traceFramebuffer_;
    tracePass.extent = context.renderExtent;
    tracePass.clearValues = {traceClear};
    commands.beginRenderPass(tracePass);
    commands.setViewport(
        static_cast<float>(context.renderExtent.width),
        static_cast<float>(context.renderExtent.height));
    commands.setScissor(context.renderExtent);
    commands.bindPipeline(pipeline_);
    const VkDescriptorSet traceSet =
        descriptorSets_[context.currentFrame % kMaxFramesInFlight];
    commands.bindDescriptorSet(pipelineLayout_, traceSet);
    commands.draw(3);
    if (context.submissionCounters != nullptr) {
        ++context.submissionCounters->drawCalls;
        ++context.submissionCounters->pipelineBinds;
        ++context.submissionCounters->descriptorSetBinds;
    }
    commands.endRenderPass();

}

void BlackholeSceneRenderer::recordTemporal(const RenderContext& context) {
    rhi::ICommandRecorder& commands = *context.commands;
    VkClearValue traceClear{};
    traceClear.color.float32[3] = 1.0F;
    // 2. Accumulate raw trace + previous history and apply compact HDR bloom.
    rhi::RenderPassBeginDesc historyPass{};
    historyPass.renderPass = traceRenderPass_;
    historyPass.framebuffer = historyFramebuffers_[historyWriteIndex_];
    historyPass.extent = context.renderExtent;
    historyPass.clearValues = {traceClear};
    commands.beginRenderPass(historyPass);
    commands.setViewport(
        static_cast<float>(context.renderExtent.width),
        static_cast<float>(context.renderExtent.height));
    commands.setScissor(context.renderExtent);
    commands.bindPipeline(taaPipeline_);
    const std::size_t taaSetIndex =
        (context.currentFrame % kMaxFramesInFlight) * 2 + historyWriteIndex_;
    const VkDescriptorSet taaSet = taaDescriptorSets_[taaSetIndex];
    commands.bindDescriptorSet(taaPipelineLayout_, taaSet);
    commands.draw(3);
    if (context.submissionCounters != nullptr) {
        ++context.submissionCounters->drawCalls;
        ++context.submissionCounters->pipelineBinds;
        ++context.submissionCounters->descriptorSetBinds;
    }
    commands.endRenderPass();

}

void BlackholeSceneRenderer::recordComposite(const RenderContext& context) {
    rhi::ICommandRecorder& commands = *context.commands;
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
    rhi::RenderPassBeginDesc compositePass{};
    compositePass.renderPass = context.sceneRenderPass;
    compositePass.framebuffer = context.sceneFramebuffer;
    compositePass.extent = context.renderExtent;
    compositePass.clearValues.assign(
        clearValues.begin(), clearValues.end());
    commands.beginRenderPass(compositePass);
    commands.setViewport(
        static_cast<float>(context.renderExtent.width),
        static_cast<float>(context.renderExtent.height));
    commands.setScissor(context.renderExtent);
    commands.bindPipeline(compositePipeline_);
    const VkDescriptorSet compositeSet =
        compositeDescriptorSets_[historyWriteIndex_];
    commands.bindDescriptorSet(compositePipelineLayout_, compositeSet);
    commands.draw(3);
    if (context.submissionCounters != nullptr) {
        ++context.submissionCounters->drawCalls;
        ++context.submissionCounters->pipelineBinds;
        ++context.submissionCounters->descriptorSetBinds;
    }
    commands.endRenderPass();
    historyValid_ = true;
    historyWriteIndex_ ^= 1U;

    if (context.gpuTimingEnabled && context.timestampQueryPool != VK_NULL_HANDLE) {
        commands.writeTimestamp(
            context.timestampQueryPool,
            2,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
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
        << "    \"bloom\": {\"mode\": \""
        << (computeBloomEnabled_ ? "four-level-compute" : "single-pass")
        << "\", "
        << "\"threshold\": 1.2, \"intensity\": 0.30, \"compute\": "
        << (computeBloomEnabled_ ? "true" : "false") << "},\n"
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
    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        uniformBuffers_[index] = allocator_->createBuffer(
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            true);
    }
}

void BlackholeSceneRenderer::createEnvironmentTexture() {
    EnvironmentImage image;
    if (!environmentSource_.path.empty()) {
        image = loadEnvironmentImage(environmentSource_);
        RuntimeDiagnostics::instance().print(
            "asset",
            "Blackhole environment: " + image.description + " ("
                + std::to_string(image.width) + "x"
                + std::to_string(image.height) + ")");
    } else {
        image.width = 2;
        image.height = 1;
        image.description = "procedural fallback";
        image.rgba16f.resize(8);
        for (std::size_t pixel = 0; pixel < 2; ++pixel) {
            image.rgba16f[pixel * 4 + 0] = environmentFloatToHalf(0.002F);
            image.rgba16f[pixel * 4 + 1] = environmentFloatToHalf(0.004F);
            image.rgba16f[pixel * 4 + 2] = environmentFloatToHalf(0.010F);
            image.rgba16f[pixel * 4 + 3] = environmentFloatToHalf(1.0F);
        }
    }
    const VkDeviceSize size = static_cast<VkDeviceSize>(image.rgba16f.size())
        * sizeof(std::uint16_t);
    rhi::GpuBuffer staging = allocator_->createBuffer(
        size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);
    std::memcpy(
        staging.mapped, image.rgba16f.data(), static_cast<std::size_t>(size));
    environment_.image = allocator_->createImage2D(
        image.width, image.height,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    rhi_->transitionImageLayout(
        environment_.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1);
    rhi_->copyBufferToImage(
        staging, environment_.image, image.width, image.height);
    rhi_->transitionImageLayout(
        environment_.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        1);
    allocator_->destroyBuffer(staging);
    environment_.view = rhi_->createImageView(
        environment_.image.image,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        1);
    rhi::SamplerDesc samplerDesc{};
    samplerDesc.addressU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerDesc.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.addressW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerDesc.mipmapLinear = false;
    environment_.sampler = rhi_->createSampler(samplerDesc);
}

VkPipeline BlackholeSceneRenderer::createFullscreenPipeline(
    const std::string& fragmentShader,
    const VkPipelineLayout layout,
    const VkRenderPass renderPass,
    const std::uint32_t colorAttachmentCount) {
    const auto vertexCode =
        azurerender::readBinaryFile(shaderDirectory_ + "/blackhole.vert.spv");
    const auto fragmentCode =
        azurerender::readBinaryFile(shaderDirectory_ + "/" + fragmentShader + ".spv");
    const VkShaderModule vertexModule = rhi_->createShaderModule(vertexCode);
    const VkShaderModule fragmentModule = rhi_->createShaderModule(fragmentCode);
    rhi::GraphicsPipelineDesc desc{};
    desc.vertexShader = vertexModule;
    desc.fragmentShader = fragmentModule;
    desc.cullMode = VK_CULL_MODE_NONE;
    desc.depthTest = false;
    desc.depthWrite = false;
    desc.colorAttachmentCount = colorAttachmentCount;
    desc.renderPass = renderPass;
    desc.layout = layout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    try {
        pipeline = rhi_->createGraphicsPipeline(desc);
    } catch (...) {
        rhi_->destroyShaderModule(fragmentModule);
        rhi_->destroyShaderModule(vertexModule);
        throw;
    }
    rhi_->destroyShaderModule(fragmentModule);
    rhi_->destroyShaderModule(vertexModule);
    return pipeline;
}

void BlackholeSceneRenderer::createGraphicsPipeline(
    const RenderContext& context) {
    (void)context;
    pipelineLayout_ = rhi_->createPipelineLayout(descriptorSetLayout_, nullptr);
    pipeline_ = createFullscreenPipeline(
        "blackhole.frag",
        pipelineLayout_,
        traceRenderPass_);
}

void BlackholeSceneRenderer::createTraceResources(
    const RenderContext& context) {
    // All private images stay shader-readable between passes. The explicit
    // dependencies cover history sampling -> color write -> later sampling.
    rhi::RenderPassDesc passDesc{};
    passDesc.attachments = {{
        context.sceneColorFormat,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        true,
        true,
        false,
    }};
    passDesc.externalReadDependency = true;
    traceRenderPass_ = rhi_->createRenderPass(passDesc);

    const VkExtent2D extent = context.renderExtent;
    const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    traceImage_ = allocator_->createImage2D(
        extent.width, extent.height, context.sceneColorFormat, usage);
    traceImageView_ = rhi_->createImageView(
        traceImage_.image,
        context.sceneColorFormat,
        VK_IMAGE_ASPECT_COLOR_BIT,
        1);
    rhi::FramebufferDesc framebufferDesc{};
    framebufferDesc.renderPass = traceRenderPass_;
    framebufferDesc.attachments = {traceImageView_};
    framebufferDesc.width = extent.width;
    framebufferDesc.height = extent.height;
    traceFramebuffer_ = rhi_->createFramebuffer(framebufferDesc);

    for (std::size_t index = 0; index < historyImages_.size(); ++index) {
        historyImages_[index] = allocator_->createImage2D(
            extent.width, extent.height, context.sceneColorFormat, usage);
        historyImageViews_[index] = rhi_->createImageView(
            historyImages_[index].image,
            context.sceneColorFormat,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1);
        framebufferDesc.attachments = {historyImageViews_[index]};
        historyFramebuffers_[index] = rhi_->createFramebuffer(framebufferDesc);
    }
    rhi::SamplerDesc samplerDesc{};
    samplerDesc.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.mipmapLinear = false;
    traceSampler_ = rhi_->createSampler(samplerDesc);
    historyWriteIndex_ = 0;
}

void BlackholeSceneRenderer::createBloomResources(
    const RenderContext& context) {
    const VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkExtent2D extent{
        std::max(context.renderExtent.width / 2, 1U),
        std::max(context.renderExtent.height / 2, 1U),
    };
    bloomLevels_.resize(kBloomLevelCount);
    bloomLevelViews_.resize(kBloomLevelCount, VK_NULL_HANDLE);
    bloomLevelExtents_.resize(kBloomLevelCount);
    for (std::size_t level = 0; level < kBloomLevelCount; ++level) {
        bloomLevelExtents_[level] = extent;
        bloomLevels_[level] = allocator_->createImage2D(
            extent.width,
            extent.height,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            usage);
        bloomLevelViews_[level] = rhi_->createImageView(
            bloomLevels_[level].image,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1);
        rhi_->transitionImageLayout(
            bloomLevels_[level],
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1);
        rhi_->clearImage(bloomLevels_[level]);
        rhi_->transitionImageLayout(
            bloomLevels_[level],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            1);
        extent = {
            std::max(extent.width / 2, 1U),
            std::max(extent.height / 2, 1U),
        };
    }

    bloomCompositeImage_ = allocator_->createImage2D(
        bloomLevelExtents_[0].width,
        bloomLevelExtents_[0].height,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        usage);
    bloomCompositeView_ = rhi_->createImageView(
        bloomCompositeImage_.image,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        1);
    rhi_->transitionImageLayout(
        bloomCompositeImage_,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1);
    rhi_->clearImage(bloomCompositeImage_);
    rhi_->transitionImageLayout(
        bloomCompositeImage_,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        1);
}

void BlackholeSceneRenderer::createBloomDescriptors() {
    constexpr VkShaderStageFlags kCompute = VK_SHADER_STAGE_COMPUTE_BIT;
    if (bloomDownsampleSetLayout_ == VK_NULL_HANDLE) {
        bloomDownsampleSetLayout_ = rhi_->createDescriptorSetLayout({
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kCompute},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, kCompute},
        });
        rhi::DescriptorPoolDesc downsamplePool{};
        downsamplePool.sizes = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             static_cast<std::uint32_t>(kBloomLevelCount)},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             static_cast<std::uint32_t>(kBloomLevelCount)},
        };
        downsamplePool.maxSets = static_cast<std::uint32_t>(kBloomLevelCount);
        bloomDownsamplePool_ = rhi_->createDescriptorPool(downsamplePool);
        bloomDownsampleSets_ = rhi_->allocateDescriptorSets(
            bloomDownsamplePool_,
            bloomDownsampleSetLayout_,
            static_cast<std::uint32_t>(kBloomLevelCount));

        bloomCompositeSetLayout_ = rhi_->createDescriptorSetLayout({
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kCompute},
            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kCompute},
            {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kCompute},
            {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kCompute},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, kCompute},
        });
        rhi::DescriptorPoolDesc compositePool{};
        compositePool.sizes = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        };
        compositePool.maxSets = 1;
        bloomCompositePool_ = rhi_->createDescriptorPool(compositePool);
        bloomCompositeSet_ = rhi_->allocateDescriptorSets(
            bloomCompositePool_, bloomCompositeSetLayout_, 1)[0];
    }

    for (std::size_t level = 0; level < kBloomLevelCount; ++level) {
        rhi::DescriptorImageWrite source{};
        source.set = bloomDownsampleSets_[level];
        source.binding = 0;
        source.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        source.view = level == 0 ? traceImageView_ : bloomLevelViews_[level - 1];
        source.sampler = traceSampler_;
        source.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        rhi_->writeDescriptorImage(source);

        rhi::DescriptorImageWrite destination{};
        destination.set = bloomDownsampleSets_[level];
        destination.binding = 1;
        destination.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        destination.view = bloomLevelViews_[level];
        destination.layout = VK_IMAGE_LAYOUT_GENERAL;
        rhi_->writeDescriptorImage(destination);
    }
    for (std::size_t level = 0; level < kBloomLevelCount; ++level) {
        rhi::DescriptorImageWrite input{};
        input.set = bloomCompositeSet_;
        input.binding = static_cast<std::uint32_t>(level);
        input.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        input.view = bloomLevelViews_[level];
        input.sampler = traceSampler_;
        input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        rhi_->writeDescriptorImage(input);
    }
    rhi::DescriptorImageWrite output{};
    output.set = bloomCompositeSet_;
    output.binding = 4;
    output.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    output.view = bloomCompositeView_;
    output.layout = VK_IMAGE_LAYOUT_GENERAL;
    rhi_->writeDescriptorImage(output);
}

void BlackholeSceneRenderer::createBloomPipelines() {
    if (bloomDownsamplePipeline_ != VK_NULL_HANDLE) {
        return;
    }
    const auto downsampleCode = azurerender::readBinaryFile(
        shaderDirectory_ + "/bloom_downsample.comp.spv");
    const VkShaderModule downsampleShader =
        rhi_->createShaderModule(downsampleCode);
    const rhi::PushConstantRangeDesc pushRange{
        VK_SHADER_STAGE_COMPUTE_BIT,
        sizeof(BloomPushConstants),
    };
    bloomDownsamplePipelineLayout_ = rhi_->createPipelineLayout(
        bloomDownsampleSetLayout_, &pushRange);
    bloomDownsamplePipeline_ = rhi_->createComputePipeline(
        {downsampleShader, bloomDownsamplePipelineLayout_});
    rhi_->destroyShaderModule(downsampleShader);

    const auto compositeCode = azurerender::readBinaryFile(
        shaderDirectory_ + "/bloom_composite.comp.spv");
    const VkShaderModule compositeShader = rhi_->createShaderModule(compositeCode);
    bloomCompositePipelineLayout_ = rhi_->createPipelineLayout(
        bloomCompositeSetLayout_, nullptr);
    bloomCompositePipeline_ = rhi_->createComputePipeline(
        {compositeShader, bloomCompositePipelineLayout_});
    rhi_->destroyShaderModule(compositeShader);
}

void BlackholeSceneRenderer::recordBloom(const RenderContext& context) {
    rhi::ICommandRecorder& commands = *context.commands;
    for (std::size_t level = 0; level < kBloomLevelCount; ++level) {
        commands.bindComputePipeline(bloomDownsamplePipeline_);
        commands.bindComputeDescriptorSet(
            bloomDownsamplePipelineLayout_, bloomDownsampleSets_[level]);
        const BloomPushConstants parameters{
            1.2F,
            level == 0 ? 1U : 0U,
        };
        commands.pushConstants(
            bloomDownsamplePipelineLayout_,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            &parameters,
            sizeof(parameters));
        const VkExtent2D extent = bloomLevelExtents_[level];
        ComputePass dispatch({extent.width, extent.height, 8, 8, 1, true});
        dispatch.record(
            commands,
            bloomDownsamplePipeline_,
            bloomDownsamplePipelineLayout_,
            bloomDownsampleSets_[level]);
    }
    commands.bindComputePipeline(bloomCompositePipeline_);
    commands.bindComputeDescriptorSet(
        bloomCompositePipelineLayout_, bloomCompositeSet_);
    const VkExtent2D extent = bloomLevelExtents_[0];
    ComputePass dispatch({extent.width, extent.height, 8, 8, 1, true});
    dispatch.record(
        commands,
        bloomCompositePipeline_,
        bloomCompositePipelineLayout_,
        bloomCompositeSet_);
}

void BlackholeSceneRenderer::updateTemporalDescriptorSets() {
    for (std::size_t frame = 0; frame < kMaxFramesInFlight; ++frame) {
        for (std::size_t writeIndex = 0; writeIndex < 2; ++writeIndex) {
            const std::size_t setIndex = frame * 2 + writeIndex;
            rhi::DescriptorBufferWrite uniformWrite{};
            uniformWrite.set = taaDescriptorSets_[setIndex];
            uniformWrite.binding = 0;
            uniformWrite.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uniformWrite.buffer = taaUniformBuffers_[frame].buffer;
            uniformWrite.range = sizeof(TaaUniform);
            rhi_->writeDescriptorBuffer(uniformWrite);

            rhi::DescriptorImageWrite traceWrite{};
            traceWrite.set = taaDescriptorSets_[setIndex];
            traceWrite.binding = 1;
            traceWrite.view = traceImageView_;
            traceWrite.sampler = traceSampler_;
            traceWrite.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            rhi_->writeDescriptorImage(traceWrite);

            rhi::DescriptorImageWrite previousWrite{};
            previousWrite.set = taaDescriptorSets_[setIndex];
            previousWrite.binding = 2;
            previousWrite.view = historyImageViews_[writeIndex ^ 1U];
            previousWrite.sampler = traceSampler_;
            previousWrite.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            rhi_->writeDescriptorImage(previousWrite);

            if (computeBloomEnabled_) {
                rhi::DescriptorImageWrite bloomWrite{};
                bloomWrite.set = taaDescriptorSets_[setIndex];
                bloomWrite.binding = 3;
                bloomWrite.view = bloomCompositeView_;
                bloomWrite.sampler = traceSampler_;
                bloomWrite.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                rhi_->writeDescriptorImage(bloomWrite);
            }
        }
    }
    for (std::size_t index = 0; index < compositeDescriptorSets_.size();
         ++index) {
        rhi::DescriptorImageWrite compositeWrite{};
        compositeWrite.set = compositeDescriptorSets_[index];
        compositeWrite.binding = 0;
        compositeWrite.view = historyImageViews_[index];
        compositeWrite.sampler = traceSampler_;
        compositeWrite.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        rhi_->writeDescriptorImage(compositeWrite);
    }
}

void BlackholeSceneRenderer::createTaaPipeline(const RenderContext& context) {
    (void)context;
    taaPipelineLayout_ =
        rhi_->createPipelineLayout(taaDescriptorSetLayout_, nullptr);
    taaPipeline_ = createFullscreenPipeline(
        computeBloomEnabled_
            ? "blackhole_taa_compute.frag"
            : "blackhole_taa.frag",
        taaPipelineLayout_,
        traceRenderPass_);
}

void BlackholeSceneRenderer::createCompositePipeline(
    const RenderContext& context) {
    compositePipelineLayout_ =
        rhi_->createPipelineLayout(compositeDescriptorSetLayout_, nullptr);
    compositePipeline_ = createFullscreenPipeline(
        "blackhole_composite.frag",
        compositePipelineLayout_,
        context.sceneRenderPass,
        2);
}

void BlackholeSceneRenderer::destroySizeDependentResources() {
    if (bloomCompositeView_ != VK_NULL_HANDLE) {
        rhi_->destroyImageView(bloomCompositeView_);
        bloomCompositeView_ = VK_NULL_HANDLE;
    }
    allocator_->destroyImage(bloomCompositeImage_);
    for (std::size_t index = 0; index < bloomLevelViews_.size(); ++index) {
        if (bloomLevelViews_[index] != VK_NULL_HANDLE) {
            rhi_->destroyImageView(bloomLevelViews_[index]);
            bloomLevelViews_[index] = VK_NULL_HANDLE;
        }
        allocator_->destroyImage(bloomLevels_[index]);
    }
    bloomLevels_.clear();
    bloomLevelViews_.clear();
    bloomLevelExtents_.clear();

    if (traceSampler_ != VK_NULL_HANDLE) {
        rhi_->destroySampler(traceSampler_);
        traceSampler_ = VK_NULL_HANDLE;
    }
    if (traceFramebuffer_ != VK_NULL_HANDLE) {
        rhi_->destroyFramebuffer(traceFramebuffer_);
        traceFramebuffer_ = VK_NULL_HANDLE;
    }
    if (traceImageView_ != VK_NULL_HANDLE) {
        rhi_->destroyImageView(traceImageView_);
        traceImageView_ = VK_NULL_HANDLE;
    }
    allocator_->destroyImage(traceImage_);
    for (std::size_t index = 0; index < historyImages_.size(); ++index) {
        if (historyFramebuffers_[index] != VK_NULL_HANDLE) {
            rhi_->destroyFramebuffer(historyFramebuffers_[index]);
            historyFramebuffers_[index] = VK_NULL_HANDLE;
        }
        if (historyImageViews_[index] != VK_NULL_HANDLE) {
            rhi_->destroyImageView(historyImageViews_[index]);
            historyImageViews_[index] = VK_NULL_HANDLE;
        }
        allocator_->destroyImage(historyImages_[index]);
    }
    if (traceRenderPass_ != VK_NULL_HANDLE) {
        rhi_->destroyRenderPass(traceRenderPass_);
        traceRenderPass_ = VK_NULL_HANDLE;
    }
}

void BlackholeSceneRenderer::destroyResources() {
    if (rhi_ == nullptr) {
        return;
    }
    if (compositePipeline_ != VK_NULL_HANDLE) {
        rhi_->destroyPipeline(compositePipeline_);
        compositePipeline_ = VK_NULL_HANDLE;
    }
    if (compositePipelineLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyPipelineLayout(compositePipelineLayout_);
        compositePipelineLayout_ = VK_NULL_HANDLE;
    }
    if (compositeDescriptorPool_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorPool(compositeDescriptorPool_);
        compositeDescriptorPool_ = VK_NULL_HANDLE;
    }
    if (compositeDescriptorSetLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorSetLayout(compositeDescriptorSetLayout_);
        compositeDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
    if (taaPipeline_ != VK_NULL_HANDLE) {
        rhi_->destroyPipeline(taaPipeline_);
        taaPipeline_ = VK_NULL_HANDLE;
    }
    if (taaPipelineLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyPipelineLayout(taaPipelineLayout_);
        taaPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (taaDescriptorPool_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorPool(taaDescriptorPool_);
        taaDescriptorPool_ = VK_NULL_HANDLE;
    }
    if (taaDescriptorSetLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorSetLayout(taaDescriptorSetLayout_);
        taaDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
    for (auto& buffer : taaUniformBuffers_) {
        allocator_->destroyBuffer(buffer);
    }
    taaUniformBuffers_.clear();

    if (bloomCompositePipeline_ != VK_NULL_HANDLE) {
        rhi_->destroyPipeline(bloomCompositePipeline_);
        bloomCompositePipeline_ = VK_NULL_HANDLE;
    }
    if (bloomCompositePipelineLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyPipelineLayout(bloomCompositePipelineLayout_);
        bloomCompositePipelineLayout_ = VK_NULL_HANDLE;
    }
    if (bloomDownsamplePipeline_ != VK_NULL_HANDLE) {
        rhi_->destroyPipeline(bloomDownsamplePipeline_);
        bloomDownsamplePipeline_ = VK_NULL_HANDLE;
    }
    if (bloomDownsamplePipelineLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyPipelineLayout(bloomDownsamplePipelineLayout_);
        bloomDownsamplePipelineLayout_ = VK_NULL_HANDLE;
    }
    if (bloomCompositePool_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorPool(bloomCompositePool_);
        bloomCompositePool_ = VK_NULL_HANDLE;
    }
    if (bloomDownsamplePool_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorPool(bloomDownsamplePool_);
        bloomDownsamplePool_ = VK_NULL_HANDLE;
    }
    if (bloomCompositeSetLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorSetLayout(bloomCompositeSetLayout_);
        bloomCompositeSetLayout_ = VK_NULL_HANDLE;
    }
    if (bloomDownsampleSetLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorSetLayout(bloomDownsampleSetLayout_);
        bloomDownsampleSetLayout_ = VK_NULL_HANDLE;
    }
    bloomDownsampleSets_.clear();

    destroySizeDependentResources();

    if (pipeline_ != VK_NULL_HANDLE) {
        rhi_->destroyPipeline(pipeline_);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyPipelineLayout(pipelineLayout_);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (descriptorPool_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorPool(descriptorPool_);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorSetLayout(descriptorSetLayout_);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }
    for (auto& buffer : uniformBuffers_) {
        allocator_->destroyBuffer(buffer);
    }
    uniformBuffers_.clear();
    if (environment_.sampler != VK_NULL_HANDLE) {
        rhi_->destroySampler(environment_.sampler);
        environment_.sampler = VK_NULL_HANDLE;
    }
    if (environment_.view != VK_NULL_HANDLE) {
        rhi_->destroyImageView(environment_.view);
        environment_.view = VK_NULL_HANDLE;
    }
    allocator_->destroyImage(environment_.image);
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
        uniformBuffers_[currentFrame_].mapped,
        &uniform,
        sizeof(uniform));
}

void BlackholeSceneRenderer::updateTaaUniform() {
    if (taaUniformBuffers_.empty()) {
        return;
    }
    TaaUniform uniform{};
    uniform.blendWeight = historyValid_ ? blendWeight_ : 1.0F;
    uniform.bloomThreshold = 1.2F;
    uniform.bloomIntensity = 0.30F;
    uniform.renderWidth = static_cast<float>(renderWidth_);
    std::memcpy(
        taaUniformBuffers_[currentFrame_].mapped,
        &uniform,
        sizeof(uniform));
}

}  // namespace azurerender
