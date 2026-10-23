#include "TemporalReconstruction.hpp"
#include "resources/BinaryFile.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace azurerender {
namespace {
constexpr auto hdrFormat=VK_FORMAT_R16G16B16A16_SFLOAT;
struct alignas(16) TemporalUniform {
    std::array<float,4> frame{};
    std::array<float,16> inverseCurrent{},previous{};
    std::array<float,4> limits{.001F,.02F,.9F,.01F};
    std::array<float,4> previousDepthProjection{};
};
static_assert(sizeof(TemporalUniform)==176);
bool temporalInverse(const std::array<float,16>& source,std::array<float,16>& result){
    double work[4][8]{};
    for(unsigned row=0;row<4;++row){for(unsigned col=0;col<4;++col)work[row][col]=source[col*4+row];work[row][row+4]=1;}
    for(unsigned column=0;column<4;++column){unsigned pivot=column;
        for(unsigned row=column+1;row<4;++row)if(std::abs(work[row][column])>std::abs(work[pivot][column]))pivot=row;
        if(std::abs(work[pivot][column])<1e-12)return false;
        for(unsigned entry=0;entry<8;++entry)std::swap(work[column][entry],work[pivot][entry]);
        double scale=work[column][column];for(auto& entry:work[column])entry/=scale;
        for(unsigned row=0;row<4;++row)if(row!=column){double factor=work[row][column];for(unsigned entry=0;entry<8;++entry)work[row][entry]-=factor*work[column][entry];}
    }
    std::array<float,16> inverse{};
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col){auto value=static_cast<float>(work[row][col+4]);if(!std::isfinite(value))return false;inverse[col*4+row]=value;}
    result=inverse;return true;
}
}
TemporalReconstruction::TemporalReconstruction(rhi::IRhi& rhi,rhi::IGpuAllocator& allocator,VkExtent2D extent,VkFormat depthFormat,
    std::vector<TemporalSceneSource> sources,const std::string& shaders):rhi_(rhi),allocator_(allocator),extent_(extent),sources_(std::move(sources)){
    const auto pixels=std::uint64_t(extent.width)*extent.height;
    if(!extent.width||!extent.height||pixels>kMaximumBytes/kBytesPerPixel||sources_.empty()||sources_.size()>UINT32_MAX/28
        ||std::any_of(sources_.begin(),sources_.end(),[](const auto& s){return !s.color||!s.colorView||!s.depth||!s.depthView;}))
        throw std::invalid_argument("Temporal reconstruction requires bounded extent and complete scene sources");
    if(!rhi_.supportsSampledImage(hdrFormat,true)||!rhi_.supportsSampledImage(VK_FORMAT_R32_SFLOAT,false))
        throw std::runtime_error("Temporal reconstruction requires sampled HDR and float depth formats");
    bytes_=pixels*kBytesPerPixel;
    VkShaderModule vertex=VK_NULL_HANDLE,copy=VK_NULL_HANDLE,resolve=VK_NULL_HANDLE;
    try{
        std::vector<VkImageView> depths;for(const auto& source:sources_)depths.push_back(source.depthView);
        motion_=std::make_unique<MotionVectorTarget>(rhi_,allocator_,extent,depthFormat,depths);
        for(unsigned i=0;i<5;++i){const auto format=i<3?hdrFormat:VK_FORMAT_R32_SFLOAT;
            images_[i]=allocator_.createImage2D(extent.width,extent.height,format,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            views_[i]=rhi_.createImageView(images_[i].image,format,VK_IMAGE_ASPECT_COLOR_BIT,1);}
        for(auto& buffer:uniforms_)buffer=allocator_.createBuffer(sizeof(TemporalUniform),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
        rhi::SamplerDesc sampler;sampler.filter=VK_FILTER_LINEAR;sampler.maxLod=0;sampler.mipmapLinear=false;
        sampler.addressU=sampler.addressV=sampler.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sampler_=rhi_.createSampler(sampler);
        rhi::RenderPassDesc description;description.externalReadDependency=true;
        description.attachments={{hdrFormat,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,false,true,false}};
        copyPass_=rhi_.createRenderPass(description);copyFramebuffer_=rhi_.createFramebuffer({copyPass_,{views_[0]},extent.width,extent.height});
        description.attachments.push_back(description.attachments.front());description.attachments.push_back({VK_FORMAT_R32_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,false,true,false});
        resolvePass_=rhi_.createRenderPass(description);
        for(const auto& source:sources_)for(unsigned next=0;next<2;++next)
            resolveFramebuffers_.push_back(rhi_.createFramebuffer({resolvePass_,{source.colorView,views_[1+next],views_[3+next]},extent.width,extent.height}));
        descriptors_[0]=rhi_.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT}});
        std::vector<rhi::DescriptorBindingDesc> bindings;
        for(unsigned i=0;i<6;++i)bindings.push_back({i,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT});
        bindings.push_back({6,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT});descriptors_[1]=rhi_.createDescriptorSetLayout(bindings);
        const auto count=static_cast<std::uint32_t>(sources_.size());
        pool_=rhi_.createDescriptorPool({{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,count*25},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,count*4}},count*5});
        copySets_=rhi_.allocateDescriptorSets(pool_,descriptors_[0],count);resolveSets_=rhi_.allocateDescriptorSets(pool_,descriptors_[1],count*4);
        for(unsigned source=0;source<count;++source){rhi_.writeDescriptorImage({copySets_[source],0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,sources_[source].colorView,sampler_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            for(unsigned frame=0;frame<2;++frame)for(unsigned previous=0;previous<2;++previous){const auto set=resolveSets_.at(source*4+frame*2+previous);
                const std::array<VkImageView,6> sampled{views_[0],views_[1+previous],views_[3+previous],sources_[source].depthView,motion_->view(),motion_->surfaceDepthView()};
                for(unsigned i=0;i<6;++i)rhi_.writeDescriptorImage({set,i,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,sampled[i],sampler_,i==3?VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
                rhi_.writeDescriptorBuffer({set,6,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,uniforms_[frame].buffer,uniforms_[frame].size});}
        }
        vertex=rhi_.createShaderModule(readBinaryFile(shaders+"/blackhole.vert.spv"));copy=rhi_.createShaderModule(readBinaryFile(shaders+"/temporal_copy.frag.spv"));resolve=rhi_.createShaderModule(readBinaryFile(shaders+"/temporal_resolve.frag.spv"));
        for(unsigned i=0;i<2;++i){layouts_[i]=rhi_.createPipelineLayout(descriptors_[i],nullptr);
            rhi::GraphicsPipelineDesc p;p.vertexShader=vertex;p.fragmentShader=i?resolve:copy;p.layout=layouts_[i];p.renderPass=i?resolvePass_:copyPass_;
            p.depthTest=p.depthWrite=false;p.cullMode=VK_CULL_MODE_NONE;p.colorAttachmentCount=i?3:1;pipelines_[i]=rhi_.createGraphicsPipeline(p);}
        rhi_.destroyShaderModule(resolve);resolve=VK_NULL_HANDLE;rhi_.destroyShaderModule(copy);copy=VK_NULL_HANDLE;rhi_.destroyShaderModule(vertex);vertex=VK_NULL_HANDLE;
        rhi_.executeOneShot([&](rhi::ICommandRecorder& commands){for(unsigned i=1;i<5;++i){rhi::ImageBarrierDesc b;b.image=images_[i].image;
            b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.srcStageMask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;b.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;commands.imageBarrier(b);}});
    }catch(...){if(resolve)rhi_.destroyShaderModule(resolve);if(copy)rhi_.destroyShaderModule(copy);if(vertex)rhi_.destroyShaderModule(vertex);reset();throw;}
}
TemporalReconstruction::~TemporalReconstruction(){reset();}
void TemporalReconstruction::reset() noexcept{
    for(auto pipeline:pipelines_)if(pipeline)rhi_.destroyPipeline(pipeline);pipelines_={};
    for(auto layout:layouts_)if(layout)rhi_.destroyPipelineLayout(layout);layouts_={};
    if(pool_)rhi_.destroyDescriptorPool(pool_);pool_=VK_NULL_HANDLE;
    for(auto layout:descriptors_)if(layout)rhi_.destroyDescriptorSetLayout(layout);descriptors_={};
    for(auto framebuffer:resolveFramebuffers_)if(framebuffer)rhi_.destroyFramebuffer(framebuffer);resolveFramebuffers_.clear();
    if(copyFramebuffer_)rhi_.destroyFramebuffer(copyFramebuffer_);copyFramebuffer_=VK_NULL_HANDLE;
    if(resolvePass_)rhi_.destroyRenderPass(resolvePass_);resolvePass_=VK_NULL_HANDLE;
    if(copyPass_)rhi_.destroyRenderPass(copyPass_);copyPass_=VK_NULL_HANDLE;
    if(sampler_)rhi_.destroySampler(sampler_);sampler_=VK_NULL_HANDLE;
    for(auto view:views_)if(view)rhi_.destroyImageView(view);views_={};
    for(auto& buffer:uniforms_)if(buffer.buffer)allocator_.destroyBuffer(buffer);uniforms_={};
    for(auto& image:images_)if(image.image)allocator_.destroyImage(image);images_={};motion_.reset();
}
std::array<RenderGraph::ResourceId,2> TemporalReconstruction::importMotion(RenderGraph& graph,const std::string& name) const{
    rhi::ImageBarrierDesc input;input.image=motion_->image().image;auto vectors=graph.importImage(name+"-motion",input);
    input.image=motion_->surfaceDepthImage().image;return {vectors,graph.importImage(name+"-surface-depth",input)};
}
void TemporalReconstruction::prepare(const TemporalPreparedFrame& frame,std::uint32_t slot,std::uint32_t source){
    if(slot>=2||source>=sources_.size()||frame.current.width!=extent_.width||frame.current.height!=extent_.height)
        throw std::invalid_argument("Temporal frame slot, source or extent mismatch");
    Pending next{slot,source,historyIndex_,1-historyIndex_,frame.previous.has_value()&&frame.previousSubmission==lastSubmission_&&lastSubmission_!=0};
    TemporalUniform uniform;uniform.frame={float(extent_.width),float(extent_.height),0,0};
    if(!temporalInverse(frame.current.viewProjection,uniform.inverseCurrent))next.valid=false;
    if(frame.previous)uniform.previous=frame.previous->viewProjection;
    const auto& projection=frame.previous?frame.previous->projection:frame.current.projection;
    uniform.previousDepthProjection={projection[10],projection[14],projection[11],projection[15]};
    uniform.frame[2]=next.valid?1.F:0.F;
    std::memcpy(uniforms_[slot].mapped,&uniform,sizeof(uniform));allocator_.flush(uniforms_[slot],0,sizeof(uniform));
    pending_=next;recorded_=false;
}
void TemporalReconstruction::schedule(RenderGraph& graph,RenderGraph::ResourceId color,RenderGraph::ResourceId depth,
    std::array<RenderGraph::ResourceId,2> motion,const std::string& name){
    if(!pending_)throw std::logic_error("Temporal resolve requires a prepared view frame");
    if(motion[0]>=graph.resources().size()||motion[1]>=graph.resources().size()||motion[0]==motion[1]
        ||graph.resources()[motion[0]].initial.image!=motion_->image().image
        ||graph.resources()[motion[1]].initial.image!=motion_->surfaceDepthImage().image)
        throw std::invalid_argument("Temporal graph motion resources must identify this view's distinct outputs");
    for(auto resource:motion){
        const bool produced=std::any_of(graph.passes().begin(),graph.passes().end(),[&](const auto& pass){
            return std::find(pass.writes.begin(),pass.writes.end(),resource)!=pass.writes.end();});
        if(!produced)throw std::logic_error("Temporal reconstruction requires both motion and color-surface depth producers");
    }
    const auto pending=*pending_;
    const auto import=[&](unsigned i){rhi::ImageBarrierDesc input;input.image=images_[i].image;
        if(i){input.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;input.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;input.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;}
        return graph.importImage(name+"-history-"+std::to_string(i),input);};
    std::array<RenderGraph::ResourceId,5> resources{};for(unsigned i=0;i<5;++i)resources[i]=import(i);
    const auto draw=[this](rhi::ICommandRecorder& commands,unsigned pipeline,VkFramebuffer framebuffer,VkDescriptorSet set){
        rhi::RenderPassBeginDesc begin;begin.renderPass=pipeline?resolvePass_:copyPass_;begin.framebuffer=framebuffer;begin.extent=extent_;
        commands.beginRenderPass(begin);commands.setViewport(float(extent_.width),float(extent_.height));commands.setScissor(extent_);
        commands.bindPipeline(pipelines_[pipeline]);commands.bindDescriptorSet(layouts_[pipeline],set);commands.draw(3);commands.endRenderPass();
    };
    const auto copy=graph.addCommandPass(name+"-temporal-copy",[this,pending,draw](rhi::ICommandRecorder& commands){draw(commands,0,copyFramebuffer_,copySets_.at(pending.source));});
    graph.use(copy,color,RenderGraphUsage::Sampled,false);graph.attachment(copy,resources[0],RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const auto resolve=graph.addCommandPass(name+"-temporal-resolve",[this,pending,draw](rhi::ICommandRecorder& commands){
        draw(commands,1,resolveFramebuffers_.at(pending.source*2+pending.next),resolveSets_.at(pending.source*4+pending.frame*2+pending.previous));recorded_=true;});
    for(auto resource:{resources[0],resources[1+pending.previous],resources[3+pending.previous],depth,motion[0],motion[1]})graph.use(resolve,resource,RenderGraphUsage::Sampled,false);
    rhi::BufferBarrierDesc initial;initial.buffer=uniforms_[pending.frame].buffer;initial.size=uniforms_[pending.frame].size;
    initial.dstStageMask=VK_PIPELINE_STAGE_HOST_BIT;initial.dstAccessMask=VK_ACCESS_HOST_WRITE_BIT;
    graph.use(resolve,graph.importBuffer(name+"-temporal-parameters",initial),RenderGraphUsage::FragmentUniform,false);
    for(auto resource:{color,resources[1+pending.next],resources[3+pending.next]})graph.attachment(resolve,resource,RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
bool TemporalReconstruction::commit(std::uint64_t submission) noexcept{
    if(!pending_||!recorded_||!submission||submission<=lastSubmission_)return false;
    historyIndex_=pending_->next;lastSubmission_=submission;pending_.reset();recorded_=false;return true;
}
}
