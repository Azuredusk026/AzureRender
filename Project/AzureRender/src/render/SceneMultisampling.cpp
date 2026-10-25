#include "SceneMultisampling.hpp"
#include "resources/BinaryFile.hpp"
#include <algorithm>
#include <stdexcept>
namespace azurerender {
SceneMultisampling::SceneMultisampling(rhi::IRhi& rhi,rhi::IGpuAllocator& allocator,VkExtent2D extent,VkFormat depth,VkFormat secondary,
    SceneSecondaryResolve policy,std::uint32_t slots,std::vector<SceneMultisampleDestination> destinations,const std::string& shaders)
    :rhi_(rhi),allocator_(allocator),extent_(extent),secondaryResolve_(policy),destinations_(std::move(destinations)){
    const std::array<VkFormat,3> formats{VK_FORMAT_R16G16B16A16_SFLOAT,depth,secondary};
    const auto pixels=std::uint64_t(extent.width)*extent.height;
    const auto bytesPerPixel=std::uint64_t(8+4+4)*4*slots;
    if(!extent.width||!extent.height||!slots||slots>2||pixels>kMaximumBytes/bytesPerPixel
        ||destinations_.empty()||destinations_.size()>UINT32_MAX
        ||depth!=VK_FORMAT_D32_SFLOAT||(secondary!=VK_FORMAT_R8G8B8A8_UNORM&&secondary!=VK_FORMAT_R32_SFLOAT)
        ||(policy!=SceneSecondaryResolve::Average&&policy!=SceneSecondaryResolve::Minimum)
        ||(policy==SceneSecondaryResolve::Minimum&&secondary!=VK_FORMAT_R32_SFLOAT))
        throw std::invalid_argument("Scene multisampling requires bounded four-sample targets and supported resolve formats");
    for(const auto& destination:destinations_)for(unsigned i=0;i<3;++i){
        if(!destination.images[i]||!destination.views[i])throw std::invalid_argument("Missing multisample destination image or view");
        for(unsigned j=0;j<i;++j)if(destination.images[i]==destination.images[j]||destination.views[i]==destination.views[j])
            throw std::invalid_argument("Scene destination attachments must be distinct");
    }
    for(unsigned i=0;i<3;++i){const auto usage=VK_IMAGE_USAGE_SAMPLED_BIT|(i==1?VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        if(!(rhi_.imageSampleCounts(formats[i],usage)&kSamples)||!rhi_.supportsSampledImage(formats[i],false))
            throw std::runtime_error("Scene format lacks four-sample attachment and sampled-image support");}
    bytes_=pixels*bytesPerPixel;
    VkShaderModule vertex{},fragment{};
    try{
        rhi::RenderPassDesc pass;pass.depthAttachment=1;pass.externalReadDependency=true;
        for(unsigned i=0;i<3;++i)pass.attachments.push_back({formats[i],VK_IMAGE_LAYOUT_UNDEFINED,
            i==1?VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,i==1,false,kSamples});
        sourcePass_=rhi_.createRenderPass(pass);
        for(auto& attachment:pass.attachments)attachment.samples=VK_SAMPLE_COUNT_1_BIT;
        resolvePass_=rhi_.createRenderPass(pass);
        const auto priorBytes=allocator_.statistics().liveImageBytes;
        sources_.resize(slots);
        for(auto& source:sources_){
            for(unsigned i=0;i<3;++i){VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};image.imageType=VK_IMAGE_TYPE_2D;
                image.extent={extent.width,extent.height,1};image.format=formats[i];image.mipLevels=image.arrayLayers=1;image.samples=kSamples;
                image.tiling=VK_IMAGE_TILING_OPTIMAL;image.usage=VK_IMAGE_USAGE_SAMPLED_BIT|(i==1?VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
                source.images[i]=allocator_.createImage(image,false);source.views[i]=rhi_.createImageView(source.images[i].image,formats[i],i==1?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT,1);}
            source.framebuffer=rhi_.createFramebuffer({sourcePass_,{source.views.begin(),source.views.end()},extent.width,extent.height});
        }
        allocatedBytes_=allocator_.statistics().liveImageBytes-priorBytes;
        if(allocatedBytes_>kMaximumBytes)throw std::runtime_error("Scene multisample allocation exceeds view budget");
        outputs_.reserve(destinations_.size());
        for(const auto& destination:destinations_)outputs_.push_back(rhi_.createFramebuffer({resolvePass_,{destination.views.begin(),destination.views.end()},extent.width,extent.height}));
        rhi::SamplerDesc sampling;sampling.filter=VK_FILTER_NEAREST;sampling.maxLod=0;sampling.mipmapLinear=false;
        sampler_=rhi_.createSampler(sampling);
        descriptors_=rhi_.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT},
            {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT},{2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT}});
        pool_=rhi_.createDescriptorPool({{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,slots*3}},slots});
        sets_=rhi_.allocateDescriptorSets(pool_,descriptors_,slots);
        for(unsigned slot=0;slot<slots;++slot)for(unsigned i=0;i<3;++i)
            rhi_.writeDescriptorImage({sets_[slot],i,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,sources_[slot].views[i],sampler_,
                i==1?VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        rhi::PushConstantRangeDesc push{VK_SHADER_STAGE_FRAGMENT_BIT,sizeof(std::uint32_t)};
        layout_=rhi_.createPipelineLayout(descriptors_,&push);
        vertex=rhi_.createShaderModule(readBinaryFile(shaders+"/blackhole.vert.spv"));fragment=rhi_.createShaderModule(readBinaryFile(shaders+"/scene_multisample_resolve.frag.spv"));
        rhi::GraphicsPipelineDesc p;p.vertexShader=vertex;p.fragmentShader=fragment;p.layout=layout_;p.renderPass=resolvePass_;
        p.depthTest=p.depthWrite=true;p.depthCompare=VK_COMPARE_OP_ALWAYS;p.cullMode=VK_CULL_MODE_NONE;p.colorAttachmentCount=2;
        pipeline_=rhi_.createGraphicsPipeline(p);rhi_.destroyShaderModule(fragment);fragment={};rhi_.destroyShaderModule(vertex);vertex={};
    }catch(...){if(fragment)rhi_.destroyShaderModule(fragment);if(vertex)rhi_.destroyShaderModule(vertex);reset();throw;}
}
SceneMultisampling::~SceneMultisampling(){reset();}
void SceneMultisampling::reset() noexcept{
    if(pipeline_)rhi_.destroyPipeline(pipeline_);if(layout_)rhi_.destroyPipelineLayout(layout_);
    if(pool_)rhi_.destroyDescriptorPool(pool_);if(descriptors_)rhi_.destroyDescriptorSetLayout(descriptors_);if(sampler_)rhi_.destroySampler(sampler_);
    for(auto framebuffer:outputs_)if(framebuffer)rhi_.destroyFramebuffer(framebuffer);
    for(auto& source:sources_){if(source.framebuffer)rhi_.destroyFramebuffer(source.framebuffer);for(auto view:source.views)if(view)rhi_.destroyImageView(view);
        for(auto& image:source.images)if(image.image)allocator_.destroyImage(image);}
    if(resolvePass_)rhi_.destroyRenderPass(resolvePass_);if(sourcePass_)rhi_.destroyRenderPass(sourcePass_);
}
std::array<RenderGraph::ResourceId,3> SceneMultisampling::importSources(RenderGraph& graph,std::uint32_t slot,const std::string& name) const{
    const auto& source=sources_.at(slot);std::array<RenderGraph::ResourceId,3> result{};
    for(unsigned i=0;i<3;++i){rhi::ImageBarrierDesc initial;initial.image=source.images[i].image;initial.aspectMask=i==1?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT;
        result[i]=graph.importImage(name+"-"+std::to_string(i),initial);}return result;
}
void SceneMultisampling::recordResolveDraw(rhi::ICommandRecorder& commands,std::uint32_t slot) const{
    commands.setViewport(float(extent_.width),float(extent_.height));commands.setScissor(extent_);commands.bindPipeline(pipeline_);
    commands.bindDescriptorSet(layout_,sets_.at(slot));const auto policy=static_cast<std::uint32_t>(secondaryResolve_);
    commands.pushConstants(layout_,VK_SHADER_STAGE_FRAGMENT_BIT,0,&policy,sizeof(policy));commands.draw(3);
}
void SceneMultisampling::recordResolve(rhi::ICommandRecorder& commands,std::uint32_t slot,std::uint32_t destination) const{
    (void)sets_.at(slot);const auto framebuffer=outputs_.at(destination);
    rhi::RenderPassBeginDesc begin;begin.renderPass=resolvePass_;begin.framebuffer=framebuffer;begin.extent=extent_;
    begin.clearValues.resize(3);begin.clearValues[1].depthStencil={1,0};
    commands.beginRenderPass(begin);recordResolveDraw(commands,slot);commands.endRenderPass();
}
void SceneMultisampling::schedule(RenderGraph& graph,std::uint32_t slot,std::uint32_t destination,
    std::array<RenderGraph::ResourceId,3> inputs,std::array<RenderGraph::ResourceId,3> outputs,const std::string& name) const{
    const auto& source=sources_.at(slot);const auto& target=destinations_.at(destination);
    const std::array<RenderGraph::ResourceId,6> ids{inputs[0],inputs[1],inputs[2],outputs[0],outputs[1],outputs[2]};
    for(unsigned i=0;i<6;++i){if(ids[i]>=graph.resources().size())throw std::invalid_argument("Invalid multisample graph resource");
        for(unsigned j=0;j<i;++j)if(ids[i]==ids[j])throw std::invalid_argument("Aliased multisample graph resources");
        const auto expected=i<3?source.images[i].image:target.images[i-3];
        const VkImageAspectFlags aspect=i%3==1?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT;
        if(graph.resources()[ids[i]].initial.image!=expected||graph.resources()[ids[i]].initial.aspectMask!=aspect)
            throw std::invalid_argument("Multisample graph resources must belong to this view");}
    for(auto input:inputs)if(std::none_of(graph.passes().begin(),graph.passes().end(),[&](const auto& pass){return std::find(pass.writes.begin(),pass.writes.end(),input)!=pass.writes.end();}))
        throw std::logic_error("Multisample resolve requires every scene attachment producer");
    rhi::RenderPassBeginDesc begin;begin.renderPass=resolvePass_;begin.framebuffer=outputs_.at(destination);begin.extent=extent_;begin.clearValues.resize(3);begin.clearValues[1].depthStencil={1,0};
    const auto pass=graph.addGraphicsPass(name,begin,[this,slot](rhi::ICommandRecorder& commands){
        recordResolveDraw(commands,slot);});
    for(auto input:inputs)graph.use(pass,input,RenderGraphUsage::Sampled,false);
    for(unsigned i=0;i<3;++i)graph.attachment(pass,outputs[i],i==1?RenderGraphUsage::DepthAttachment:RenderGraphUsage::ColorAttachment,
        i==1?VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
}
