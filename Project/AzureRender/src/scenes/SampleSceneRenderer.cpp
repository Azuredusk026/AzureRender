#include "SampleSceneRenderer.hpp"

#include "rhi/Rhi.hpp"

#include <array>
#include <stdexcept>

namespace azurerender {

SceneRendererCapabilities SampleSceneRenderer::capabilities() const {
    SceneRendererCapabilities capabilities;
    capabilities.diagnosticViewNames = {"Beauty"};
    return capabilities;
}

void SampleSceneRenderer::onLoad(const RenderContext& context) {
    if (loaded_) {
        throw std::logic_error("Sample renderer is already loaded");
    }
    if (context.device == VK_NULL_HANDLE
        || context.sceneRenderPass == VK_NULL_HANDLE) {
        throw std::invalid_argument("Sample renderer requires a valid host context");
    }
    loaded_ = true;
}

void SampleSceneRenderer::onSwapchainRecreate(const RenderContext& context) {
    if (!loaded_ || context.sceneRenderPass == VK_NULL_HANDLE) {
        throw std::logic_error("Sample renderer recreate outside loaded lifecycle");
    }
}

void SampleSceneRenderer::updateFrame(const SceneFrameData&) {
    if (!loaded_) {
        throw std::logic_error("Sample renderer update before load");
    }
}

void SampleSceneRenderer::recordScene(const RenderContext& context) {
    if (!loaded_ || context.commands == nullptr
        || context.sceneFramebuffer == VK_NULL_HANDLE) {
        throw std::logic_error("Sample renderer record outside valid frame");
    }
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
    rhi::RenderPassBeginDesc pass{};
    pass.renderPass = context.sceneRenderPass;
    pass.framebuffer = context.sceneFramebuffer;
    pass.extent = context.renderExtent;
    VkClearValue clearColor{};
    clearColor.color = {{0.025F, 0.045F, 0.065F, 1.0F}};
    VkClearValue clearDepth{};
    clearDepth.depthStencil = {1.0F, 0};
    VkClearValue clearNormal{};
    clearNormal.color = {{0.5F, 0.5F, 1.0F, 0.0F}};
    pass.clearValues = {clearColor, clearDepth, clearNormal};
    context.commands->beginRenderPass(pass);
    context.commands->endRenderPass();
}

void SampleSceneRenderer::onUnload(const RenderContext&) {
    loaded_ = false;
}

}  // namespace azurerender
