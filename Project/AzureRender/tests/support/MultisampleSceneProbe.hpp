#pragma once
#include "extensions/ISceneRenderer.hpp"
#include <vector>
namespace azurerender::test {
struct MultisampleSceneObservation {bool supported=true;VkSampleCountFlagBits loadedSamples=VK_SAMPLE_COUNT_1_BIT;std::vector<VkFramebuffer> framebuffers;};
class MultisampleSceneProbe final:public ISceneRenderer {
public:
    explicit MultisampleSceneProbe(std::shared_ptr<MultisampleSceneObservation> observation):observation_(std::move(observation)){}
    std::string_view name()const noexcept override{return "multisample-scene-probe";}
    SceneRendererCapabilities capabilities()const override{SceneRendererCapabilities result;result.diagnosticViewNames={"Beauty"};result.supportsMultisampleShading=observation_->supported;return result;}
    void onLoad(const RenderContext& context)override{observation_->loadedSamples=context.rasterizationSamples;}
    void onSwapchainRecreate(const RenderContext& context)override{onLoad(context);}
    void onUnload(const RenderContext&)override{}
    void updateFrame(const SceneFrameData&)override{}
    void recordScene(const RenderContext& context)override{
        observation_->framebuffers.push_back(context.sceneFramebuffer);
        rhi::RenderPassBeginDesc begin;begin.renderPass=context.sceneRenderPass;begin.framebuffer=context.sceneFramebuffer;begin.extent=context.renderExtent;
        begin.clearValues.resize(3);begin.clearValues[0].color={{.25F,.5F,2.F,1}};begin.clearValues[1].depthStencil={.4F,0};begin.clearValues[2].color={{.5F,.5F,1,0}};
        context.commands->beginRenderPass(begin);context.commands->endRenderPass();
    }
private:std::shared_ptr<MultisampleSceneObservation> observation_;
};
}
