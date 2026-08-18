#include "SampleSceneRenderer.hpp"

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
    if (!loaded_ || context.commandBuffer == VK_NULL_HANDLE
        || context.sceneFramebuffer == VK_NULL_HANDLE) {
        throw std::logic_error("Sample renderer record outside valid frame");
    }
    if (context.shadowRenderPass != VK_NULL_HANDLE
        && context.shadowFramebuffer != VK_NULL_HANDLE) {
        VkClearValue shadowClear{};
        shadowClear.depthStencil = {1.0F, 0};
        VkRenderPassBeginInfo shadowPass{
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        shadowPass.renderPass = context.shadowRenderPass;
        shadowPass.framebuffer = context.shadowFramebuffer;
        shadowPass.renderArea.extent = {
            context.shadowMapSize, context.shadowMapSize};
        shadowPass.clearValueCount = 1;
        shadowPass.pClearValues = &shadowClear;
        vkCmdBeginRenderPass(
            context.commandBuffer, &shadowPass, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(context.commandBuffer);
    }
    std::array<VkClearValue, 3> clears{};
    clears[0].color = {{0.025F, 0.045F, 0.065F, 1.0F}};
    clears[1].depthStencil = {1.0F, 0};
    clears[2].color = {{0.5F, 0.5F, 1.0F, 0.0F}};
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = context.sceneRenderPass;
    pass.framebuffer = context.sceneFramebuffer;
    pass.renderArea.extent = context.renderExtent;
    pass.clearValueCount = static_cast<std::uint32_t>(clears.size());
    pass.pClearValues = clears.data();
    vkCmdBeginRenderPass(
        context.commandBuffer, &pass, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(context.commandBuffer);
}

void SampleSceneRenderer::onUnload(const RenderContext&) {
    loaded_ = false;
}

}  // namespace azurerender
