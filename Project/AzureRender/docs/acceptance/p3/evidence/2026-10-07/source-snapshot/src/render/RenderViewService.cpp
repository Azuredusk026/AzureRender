#include "RenderViewService.hpp"
#include "RenderViewOutput.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <atomic>
namespace azurerender {
namespace {
std::atomic<std::uint64_t> viewGeneration{1};
void validateExtent(VkExtent2D extent){
    if(!extent.width || !extent.height || extent.width>4096 || extent.height>4096)
        throw std::invalid_argument("Render view extent must be within 1..4096");
}
void validateCamera(const std::array<float,3>& position,const std::array<float,3>& target){
    float length=0;
    for(std::size_t i=0;i<3;++i){
        if(!std::isfinite(position[i]) || !std::isfinite(target[i]))throw std::invalid_argument("Render view camera must be finite");
        const float delta=position[i]-target[i];length+=delta*delta;
    }
    if(length<1e-8F || !std::isfinite(length))throw std::invalid_argument("Render view camera requires a direction");
}
struct View {
    RenderViewDescriptor descriptor;
    RenderContext context;
    SceneFrameData frame;
    std::unique_ptr<ISceneRenderer> renderer;
    std::unique_ptr<RenderViewOutput> output;
    std::array<rhi::GpuImage,4> images{};
    std::array<VkImageView,4> views{};
    VkRenderPass scenePass=VK_NULL_HANDLE,shadowPass=VK_NULL_HANDLE;
    VkFramebuffer framebuffer=VK_NULL_HANDLE,shadowFramebuffer=VK_NULL_HANDLE;
    VkSampler sampler=VK_NULL_HANDLE;
    std::uint64_t lastUse=0,renderCount=0;
    bool requested=false,loaded=false;
    VkExtent2D pendingExtent{};
    ~View(){
        if(!context.rhi)return;
        if(renderer){try{renderer->onUnload(context);}catch(...){}}
        output.reset();
        auto& rhi=*context.rhi;
        if(framebuffer)rhi.destroyFramebuffer(framebuffer);
        if(shadowFramebuffer)rhi.destroyFramebuffer(shadowFramebuffer);
        if(sampler)rhi.destroySampler(sampler);
        if(scenePass)rhi.destroyRenderPass(scenePass);
        if(shadowPass)rhi.destroyRenderPass(shadowPass);
        for(const auto view:views)if(view)rhi.destroyImageView(view);
        for(auto& image:images)if(image.image)context.allocator->destroyImage(image);
    }
};
}
struct RenderViewService::State {
    struct Slot{std::uint32_t generation=1;std::shared_ptr<View> view;};
    RenderContext base;
    Factory factory;
    std::function<void()> waitIdle;
    std::vector<Slot> slots;
    std::vector<std::shared_ptr<View>> retired;
    std::uint64_t completed=0,lastScheduled=0;
    std::thread::id owner=std::this_thread::get_id();
    void check() const{if(owner!=std::this_thread::get_id())throw std::logic_error("Render views require their owner thread");}
    Slot& slot(RenderViewHandle handle){
        check();const auto index=static_cast<std::uint32_t>(handle);
        if(index>=slots.size() || slots[index].generation!=(handle>>32) || !slots[index].view)
            throw std::invalid_argument("Expired render view handle");
        return slots[index];
    }
    std::shared_ptr<View> make(const RenderViewDescriptor& descriptor){
        validateExtent(descriptor.extent);validateCamera(descriptor.cameraPosition,descriptor.cameraTarget);
        validateRenderSettings(descriptor.settings);
        auto view=std::make_shared<View>();view->descriptor=descriptor;view->context=base;
        auto& context=view->context;context.scene=descriptor.scene;context.renderSettings=&view->descriptor.settings;
        context.sceneWorldCoordinates=descriptor.sceneWorldCoordinates;
        context.gpuTimingEnabled=false;context.timestampQueryPool=VK_NULL_HANDLE;context.submissionCounters=nullptr;
        context.renderExtent=descriptor.extent;context.swapchainExtent=descriptor.extent;
        context.qaInstanceCount=1;context.preparedMeshes=nullptr;
        auto& rhi=*context.rhi;
        const auto shadowExtent=std::max(1u,context.shadowMapSize);
        const std::array<VkFormat,4> formats{context.sceneColorFormat,context.depthFormat,context.normalFormat,context.shadowFormat};
        for(std::size_t i=0;i<4;++i){
            const bool depth=i==1 || i==3;const auto width=i==3?shadowExtent:descriptor.extent.width;
            const auto height=i==3?shadowExtent:descriptor.extent.height;
            view->images[i]=context.allocator->createImage2D(width,height,formats[i],
                (depth?VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            view->views[i]=rhi.createImageView(view->images[i].image,formats[i],depth?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT,1);
        }
        rhi::RenderPassDesc pass;
        pass.attachments={{formats[0],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false},
            {formats[1],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,true,true,true},
            {formats[2],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false}};
        pass.depthAttachment=1;pass.externalReadDependency=true;
        view->scenePass=rhi.createRenderPass(pass);
        rhi::RenderPassDesc shadow;shadow.attachments={{formats[3],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,true,true,true}};
        shadow.depthAttachment=0;shadow.externalReadDependency=true;view->shadowPass=rhi.createRenderPass(shadow);
        view->framebuffer=rhi.createFramebuffer({view->scenePass,{view->views[0],view->views[1],view->views[2]},descriptor.extent.width,descriptor.extent.height});
        view->shadowFramebuffer=rhi.createFramebuffer({view->shadowPass,{view->views[3]},shadowExtent,shadowExtent});
        view->sampler=rhi.createSampler({VK_FILTER_LINEAR,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,0,false});
        context.sceneRenderPass=view->scenePass;context.sceneFramebuffer=view->framebuffer;
        context.shadowRenderPass=view->shadowPass;context.shadowFramebuffer=view->shadowFramebuffer;
        context.shadowImageView=view->views[3];context.shadowSampler=view->sampler;
        view->output=std::make_unique<RenderViewOutput>(context,view->views[0],view->sampler,descriptor.transfer);
        view->renderer=factory(descriptor.rendererId);
        if(!view->renderer)throw std::invalid_argument("Render view factory returned no renderer");
        validateSceneRendererCapabilities(view->renderer->capabilities());
        view->renderer->onLoad(context);view->loaded=true;
        return view;
    }
};
RenderViewService::RenderViewService(RenderContext base,Factory factory,std::function<void()> waitIdle,std::size_t capacity)
    :state_(std::make_unique<State>()){
    if(!base.rhi || !base.allocator || !factory || !waitIdle || capacity==0 || capacity>3)
        throw std::invalid_argument("Render view service requires allocation, lifecycle and 1..3 slots");
    state_->base=std::move(base);state_->factory=std::move(factory);state_->waitIdle=std::move(waitIdle);state_->slots.resize(capacity);
}
RenderViewService::~RenderViewService(){
    if(state_){try{state_->waitIdle();state_->retired.clear();for(auto& slot:state_->slots)slot.view.reset();}catch(...){std::terminate();}}
}
RenderViewHandle RenderViewService::create(const RenderViewDescriptor& descriptor){
    state_->check();
    const auto active=std::count_if(state_->slots.begin(),state_->slots.end(),[](const auto& slot){return static_cast<bool>(slot.view);});
    if(static_cast<std::size_t>(active)+state_->retired.size()>=state_->slots.size())throw std::runtime_error("Render view budget awaits GPU retirement");
    for(std::size_t i=0;i<state_->slots.size();++i){auto& slot=state_->slots[i];if(!slot.view){
        const auto generation=viewGeneration.fetch_add(1);
        if(generation>std::numeric_limits<std::uint32_t>::max())throw std::overflow_error("Render view generation exhausted");
        slot.generation=static_cast<std::uint32_t>(generation);
        slot.view=state_->make(descriptor);return (static_cast<std::uint64_t>(slot.generation)<<32)|i;
    }}throw std::runtime_error("Render view capacity exhausted");
}
void RenderViewService::request(RenderViewHandle handle){state_->slot(handle).view->requested=true;}
void RenderViewService::resize(RenderViewHandle handle,VkExtent2D extent){
    validateExtent(extent);auto& view=*state_->slot(handle).view;
    if(view.descriptor.extent.width==extent.width && view.descriptor.extent.height==extent.height)return;
    view.pendingExtent=extent;view.requested=true;
}
void RenderViewService::setCamera(RenderViewHandle handle,const std::array<float,3>& position,const std::array<float,3>& target){
    validateCamera(position,target);auto& view=*state_->slot(handle).view;
    view.descriptor.cameraPosition=position;view.descriptor.cameraTarget=target;view.requested=true;
}
void RenderViewService::schedule(RenderGraph& graph,const SceneFrameData& frame,const RenderContext& recording,std::uint64_t submission){
    state_->check();if(submission==0 || submission<=state_->completed || submission<=state_->lastScheduled)
        throw std::invalid_argument("Render view scheduling requires an increasing pending submission");
    state_->lastScheduled=submission;
    for(std::size_t index=0;index<state_->slots.size();++index){
        auto& slot=state_->slots[index];auto view=slot.view;if(!view || !view->requested)continue;
        if(view->pendingExtent.width){
            if(view->lastUse>state_->completed)continue;
            auto descriptor=view->descriptor;descriptor.extent=view->pendingExtent;
            auto replacement=state_->make(descriptor);replacement->requested=true;replacement->renderCount=view->renderCount;
            slot.view=replacement;view=std::move(replacement);
        }
        // Recording contexts are view-owned: reference-capturing renderer callbacks remain valid.
        view->context.commandBuffer=recording.commandBuffer;view->context.commands=recording.commands;
        view->context.currentFrame=recording.currentFrame;view->context.imageIndex=recording.currentFrame;
        view->frame=frame;view->frame.renderSettings=&view->descriptor.settings;
        view->frame.cameraOverride=true;
        view->frame.sceneSnapshot=std::make_shared<const scene::SceneDescription>(view->descriptor.scene);
        std::copy(view->descriptor.cameraPosition.begin(),view->descriptor.cameraPosition.end(),view->frame.cameraPosition);
        std::copy(view->descriptor.cameraTarget.begin(),view->descriptor.cameraTarget.end(),view->frame.cameraTarget);
        view->frame.swapchainWidth=view->descriptor.extent.width;view->frame.swapchainHeight=view->descriptor.extent.height;
        view->renderer->updateFrame(view->frame);
        auto import=[&](std::size_t i){rhi::ImageBarrierDesc initial{};initial.image=view->images[i].image;
            initial.aspectMask=(i==1 || i==3)?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT;
            return graph.importImage("view-"+std::to_string(index)+"-"+std::to_string(i),initial);};
        const SceneGraphResources resources{import(0),import(1),import(2),import(3)};
        const auto firstPass=graph.passes().size();
        view->renderer->registerPasses(graph,resources,view->context);
        view->output->schedule(graph,resources.color,"view-"+std::to_string(index));
        // The one-shot graph retains the owner through recording and worker joins.
        const auto lease=std::make_shared<std::shared_ptr<View>>(view);
        const auto owner=graph.addPass("view-owner-"+std::to_string(index),[lease]{lease->reset();});
        for(auto pass=firstPass;pass<owner;++pass)graph.dependsOn(owner,static_cast<RenderGraph::PassId>(pass));
        view->lastUse=std::max(view->lastUse,submission);view->requested=false;++view->renderCount;
    }
}
void RenderViewService::complete(std::uint64_t submission){
    state_->check();state_->completed=std::max(state_->completed,submission);
    auto& retired=state_->retired;retired.erase(std::remove_if(retired.begin(),retired.end(),[&](const auto& view){return view->lastUse<=state_->completed;}),retired.end());
}
VkImageView RenderViewService::sample(RenderViewHandle handle,std::uint64_t submission){
    auto& view=*state_->slot(handle).view;
    if(!view.renderCount || submission==0 || submission<=state_->completed)throw std::invalid_argument("Render view sampling requires a rendered output and pending submission");
    if(view.pendingExtent.width)return VK_NULL_HANDLE;
    view.lastUse=std::max(view.lastUse,submission);return view.output->view();
}
void RenderViewService::release(RenderViewHandle handle){
    auto& slot=state_->slot(handle);if(slot.generation==std::numeric_limits<std::uint32_t>::max())throw std::overflow_error("Render view generation exhausted");
    if(slot.view->lastUse>state_->completed)state_->retired.push_back(slot.view);
    slot.view.reset();++slot.generation;
}
nlohmann::json RenderViewService::describe(RenderViewHandle handle) const{
    const auto& view=*state_->slot(handle).view;return {{"schemaVersion",1},{"extent",{view.descriptor.extent.width,view.descriptor.extent.height}},
        {"cameraPosition",view.descriptor.cameraPosition},{"cameraTarget",view.descriptor.cameraTarget},
        {"renderCount",view.renderCount},{"colorImage",reinterpret_cast<std::uintptr_t>(view.images[0].image)},
        {"resolvedImage",reinterpret_cast<std::uintptr_t>(view.output->image().image)},
        {"outputFormat",view.descriptor.transfer==ViewColorTransfer::Linear?"rgba8-linear":"rgba8-srgb-encoded"},
        {"lastUseSubmission",view.lastUse},{"completedSubmission",state_->completed},{"requested",view.requested},
        {"renderer",view.descriptor.rendererId},{"worldCoordinates",view.descriptor.sceneWorldCoordinates},
        {"capacity",state_->slots.size()},{"retired",state_->retired.size()}};
}
void RenderViewService::clear(){
    state_->check();for(std::size_t i=0;i<state_->slots.size();++i)if(state_->slots[i].view)
        release((static_cast<std::uint64_t>(state_->slots[i].generation)<<32)|i);
}
std::vector<unsigned char> RenderViewService::readPixels(RenderViewHandle handle){
    auto& view=*state_->slot(handle).view;
    if(!view.renderCount || view.lastUse>state_->completed)throw std::logic_error("View readback requires GPU completion");
    const auto& image=view.output->image();const auto bytes=static_cast<VkDeviceSize>(image.width)*image.height*4;
    auto buffer=view.context.allocator->createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    try{
        view.context.rhi->executeOneShot([&](rhi::ICommandRecorder& commands){
            rhi::ImageBarrierDesc barrier{};barrier.image=image.image;
            barrier.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;barrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
            barrier.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
            commands.imageBarrier(barrier);commands.copyImageToBuffer(image.image,buffer.buffer,{image.width,image.height});
            barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;barrier.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;commands.imageBarrier(barrier);
        });
        view.context.allocator->invalidate(buffer,0,bytes);
        std::vector<unsigned char> pixels(static_cast<std::size_t>(bytes));std::memcpy(pixels.data(),buffer.mapped,pixels.size());
        view.context.allocator->destroyBuffer(buffer);return pixels;
    }catch(...){view.context.allocator->destroyBuffer(buffer);throw;}
}
}
