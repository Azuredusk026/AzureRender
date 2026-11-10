#include "render/ReferenceDepth.hpp"
#include "render/ReferenceFmaShader.hpp"
#include "resources/BinaryFile.hpp"
namespace azurerender {
namespace {
void require(bool value,const char* message){if(!value)throw std::invalid_argument(message);}
std::uint64_t budget(VkExtent2D extent,unsigned frames){
    require(extent.width&&extent.height&&extent.width<=8192&&extent.height<=8192&&frames&&frames<=8,"Invalid reference depth extent or slot count");
    const auto bytes=std::uint64_t(extent.width)*extent.height*8*frames;
    require(bytes<=256ULL*1024*1024,"Reference depth resources exceed 256 MiB");return bytes;
}
void validate(const ReferenceDepthConfig& config){
    require((config.schemaVersion==1&&!config.positionFrame)||(config.schemaVersion==2&&config.positionFrame),"Reference depth version2 requires a position frame; version1 uses relative inputs");
    if(config.positionFrame)validateReferencePositionFrame(*config.positionFrame);
    require(config.convention==ReferenceDepthConvention::Forward||config.convention==ReferenceDepthConvention::Reversed,"Unknown reference depth convention");
}
struct alignas(16) Parameters {ReferenceClipProjectionParameters projection;std::array<unsigned,4> control{};std::array<float,4> coverage{},frameOffset{};};
static_assert(sizeof(Parameters)==128);
constexpr unsigned maskedLimit=128;
void validate(const ReferenceDepthSurface& surface,bool maskedAvailable,bool ellipseAvailable){
    require(surface.schemaVersion==1||surface.schemaVersion==2,"Unsupported reference depth surface version");
    require(surface.cullMode==VK_CULL_MODE_NONE||surface.cullMode==VK_CULL_MODE_FRONT_BIT||surface.cullMode==VK_CULL_MODE_BACK_BIT,"Unknown reference depth cull mode");
    require(surface.frontFace==VK_FRONT_FACE_CLOCKWISE||surface.frontFace==VK_FRONT_FACE_COUNTER_CLOCKWISE,"Unknown reference depth front face");
    if(!surface.alpha)return;
    const auto& alpha=*surface.alpha;
    require(alpha.lodPolicy==ReferenceDepthAlphaLodPolicy::Device||alpha.lodPolicy==ReferenceDepthAlphaLodPolicy::EllipseMajorAxisMirroredCoarseY,"Unknown reference depth alpha LOD policy");
    if(alpha.lodPolicy==ReferenceDepthAlphaLodPolicy::EllipseMajorAxisMirroredCoarseY)
        require(surface.schemaVersion==2&&ellipseAvailable,"Ellipse coverage requires surface version2 and qualified ellipse shaders");
    require(maskedAvailable,"Reference depth coverage requires masked shaders");
    require(alpha.uv&&alpha.image&&alpha.sampler,"Reference depth coverage requires UVs, image and sampler");
    require(alpha.layout==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,"Reference depth coverage requires sampled color layout");
    require(alpha.channel<4&&std::isfinite(alpha.cutoff)&&alpha.cutoff>=0&&alpha.cutoff<=1
        &&std::isfinite(alpha.lodBias)&&std::abs(alpha.lodBias)<=16,"Invalid reference depth coverage channel, cutoff or LOD bias");
}
constexpr VkImageUsageFlags colorUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
constexpr VkImageUsageFlags depthUsage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
}
struct ReferenceDepthGpu::Impl {
    struct Slot {rhi::GpuImage color{},depth{};VkImageView colorView{},depthView{};VkFramebuffer framebuffer{};std::vector<VkDescriptorSet> sets;bool initialized=false;};
    rhi::IRhi& backend;VkExtent2D extent;ReferenceDepthConfig config;std::uint64_t bytes;std::vector<Slot> slots;
    VkDescriptorSetLayout descriptors{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkRenderPass pass{};
    std::array<VkPipeline,18> pipelines{};std::array<VkShaderModule,2> vertices{};std::array<VkShaderModule,3> fragments{};bool maskedAvailable=false,ellipseAvailable=false;
    Impl(rhi::IRhi& b,VkExtent2D e,const ReferenceDepthConfig& c,unsigned count):backend(b),extent(e),config(c),bytes(budget(e,count)){}
    ~Impl(){
        for(auto pipeline:pipelines)if(pipeline)backend.destroyPipeline(pipeline);
        for(auto module:vertices)if(module)backend.destroyShaderModule(module);
        for(auto module:fragments)if(module)backend.destroyShaderModule(module);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(descriptors)backend.destroyDescriptorSetLayout(descriptors);
        for(auto& slot:slots){
            if(slot.framebuffer)backend.destroyFramebuffer(slot.framebuffer);
            if(slot.colorView)backend.destroyImageView(slot.colorView);if(slot.depthView)backend.destroyImageView(slot.depthView);
            backend.allocator().destroyImage(slot.color);backend.allocator().destroyImage(slot.depth);
        }
        if(pass)backend.destroyRenderPass(pass);
    }
    VkPipeline pipeline(const ReferenceDepthSurface& surface){
        const unsigned masked=surface.alpha?1:0,cull=surface.cullMode==VK_CULL_MODE_FRONT_BIT?1:surface.cullMode==VK_CULL_MODE_BACK_BIT?2:0;
        const unsigned kind=masked&&surface.alpha->lodPolicy==ReferenceDepthAlphaLodPolicy::EllipseMajorAxisMirroredCoarseY?2:masked;
        auto& cached=pipelines[kind*6+(surface.frontFace==VK_FRONT_FACE_COUNTER_CLOCKWISE?3:0)+cull];
        if(cached)return cached;
        rhi::GraphicsPipelineDesc graphics;graphics.vertexShader=vertices[masked];graphics.fragmentShader=fragments[kind];
        if(masked){graphics.vertexBindings={{0,16},{1,8}};graphics.vertexAttributes={{0,VK_FORMAT_R32G32B32A32_SFLOAT,0,0},{1,VK_FORMAT_R32G32_SFLOAT,0,1}};}
        else{graphics.vertexStride=16;graphics.vertexAttributes={{0,VK_FORMAT_R32G32B32A32_SFLOAT,0}};}
        graphics.cullMode=surface.cullMode;graphics.frontFace=surface.frontFace;graphics.colorAttachmentCount=1;graphics.renderPass=pass;graphics.layout=layout;
        graphics.depthCompare=config.convention==ReferenceDepthConvention::Reversed?VK_COMPARE_OP_GREATER:VK_COMPARE_OP_LESS;
        cached=backend.createGraphicsPipeline(graphics);return cached;
    }
    void initialize(unsigned count,const ReferenceDepthShaders& shaders){
        // Prepare and validate the marked arithmetic before allocating resources.
        const auto& vertex=config.positionFrame?shaders.positionFrame.vertex:shaders.vertex;
        const auto& maskedVertex=config.positionFrame?shaders.positionFrame.maskedVertex:shaders.maskedVertex;
        const auto prepared=prepareReferenceFmaShader(vertex);
        require(prepared.fusedResults==3,"Reference depth projection requires three marked Fma results");
        require(shaders.maskedVertex.empty()==shaders.maskedFragment.empty(),"Reference depth masked shaders must form a complete pair");
        maskedAvailable=!shaders.maskedVertex.empty();ReferenceFmaShader maskedPrepared;
        ellipseAvailable=!shaders.ellipseFragment.empty();
        if(ellipseAvailable){require(maskedAvailable,"Ellipse coverage requires the complete masked shader pair");
            validateMaterialUvGradientPolicy(MaterialUvGradientPolicy::MirroredCoarseY,shaders.ellipseSupport);}
        if(maskedAvailable){maskedPrepared=prepareReferenceFmaShader(maskedVertex);require(maskedPrepared.fusedResults==3,"Masked reference projection requires three marked Fma results");}
        descriptors=backend.createDescriptorSetLayout(maskedAvailable?std::vector<rhi::DescriptorBindingDesc>{{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT}}:std::vector<rhi::DescriptorBindingDesc>{});
        const rhi::PushConstantRangeDesc push{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,sizeof(Parameters)};
        layout=backend.createPipelineLayout(descriptors,&push);
        rhi::RenderPassDesc render;render.depthAttachment=1;
        render.attachments={{VK_FORMAT_R32_SFLOAT,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,true,true,false},
            {VK_FORMAT_D32_SFLOAT,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,true,true,true}};
        pass=backend.createRenderPass(render);
        vertices[0]=backend.createShaderModule(prepared.code);fragments[0]=backend.createShaderModule(shaders.fragment);
        if(ellipseAvailable)fragments[2]=backend.createShaderModule(shaders.ellipseFragment);
        if(maskedAvailable){vertices[1]=backend.createShaderModule(maskedPrepared.code);fragments[1]=backend.createShaderModule(shaders.maskedFragment);
            rhi::DescriptorPoolDesc descriptorPool;descriptorPool.maxSets=count*maskedLimit;descriptorPool.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,count*maskedLimit}};pool=backend.createDescriptorPool(descriptorPool);}
        (void)pipeline({});
        slots.resize(count);
        for(auto& slot:slots){
            if(maskedAvailable)slot.sets=backend.allocateDescriptorSets(pool,descriptors,maskedLimit);
            slot.color=backend.allocator().createImage2D(extent.width,extent.height,VK_FORMAT_R32_SFLOAT,colorUsage);
            slot.depth=backend.allocator().createImage2D(extent.width,extent.height,VK_FORMAT_D32_SFLOAT,depthUsage);
            slot.colorView=backend.createImageView(slot.color.image,VK_FORMAT_R32_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
            slot.depthView=backend.createImageView(slot.depth.image,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT,1);
            slot.framebuffer=backend.createFramebuffer({pass,{slot.colorView,slot.depthView},extent.width,extent.height});
        }
    }
};
bool ReferenceDepthGpu::supported(rhi::IRhi& backend,VkExtent2D extent,unsigned frames){
    try{(void)budget(extent,frames);}catch(const std::invalid_argument&){return false;}
    return backend.supportsSampledImage(VK_FORMAT_R32_SFLOAT,false)&&backend.supportsSampledImage(VK_FORMAT_D32_SFLOAT,false)
        &&(backend.imageSampleCounts(VK_FORMAT_R32_SFLOAT,colorUsage)&VK_SAMPLE_COUNT_1_BIT)
        &&(backend.imageSampleCounts(VK_FORMAT_D32_SFLOAT,depthUsage)&VK_SAMPLE_COUNT_1_BIT);
}
ReferenceDepthShaders ReferenceDepthShaders::load(const std::string& directory){return {readBinaryFile(directory+"/reference_depth.vert.spv"),readBinaryFile(directory+"/reference_depth.frag.spv")};}
ReferenceDepthPositionFrameShaders ReferenceDepthPositionFrameShaders::load(const std::string& directory){return {readBinaryFile(directory+"/reference_depth_frame.vert.spv"),readBinaryFile(directory+"/reference_depth_masked_frame.vert.spv")};}
ReferenceDepthShaders ReferenceDepthShaders::loadMasked(const std::string& directory){auto result=load(directory);result.maskedVertex=readBinaryFile(directory+"/reference_depth_masked.vert.spv");result.maskedFragment=readBinaryFile(directory+"/reference_depth_masked.frag.spv");return result;}
ReferenceDepthShaders ReferenceDepthShaders::loadEllipseMasked(const std::string& directory,MaterialUvGradientSupport support){
    validateMaterialUvGradientPolicy(MaterialUvGradientPolicy::MirroredCoarseY,support);
    auto result=loadMasked(directory);result.ellipseFragment=readBinaryFile(directory+"/reference_depth_ellipse.frag.spv");result.ellipseSupport=support;return result;
}
ReferenceDepthGpu::ReferenceDepthGpu(rhi::IRhi& backend,VkExtent2D extent,unsigned frames,const ReferenceDepthConfig& config,const ReferenceDepthShaders& shaders){
    validate(config);require(supported(backend,extent,frames),"Reference depth formats, extent or slot budget unavailable");
    auto candidate=std::make_unique<Impl>(backend,extent,config,frames);candidate->initialize(frames,shaders);impl_=std::move(candidate);
}
ReferenceDepthGpu::~ReferenceDepthGpu()=default;
std::uint64_t ReferenceDepthGpu::byteBudget() const noexcept{return impl_->bytes;}
ReferenceDepthOutput ReferenceDepthGpu::output(unsigned index) const {
    const auto& slot=impl_->slots.at(index);return {slot.color.image,slot.depth.image,slot.colorView,slot.depthView,impl_->extent};
}
void ReferenceDepthGpu::record(rhi::ICommandRecorder& commands,unsigned index,const ReferenceClipProjectionProfile& profile,const std::vector<ReferenceDepthGeometry>& geometry){
    validateReferenceClipProjection(profile);auto& state=*impl_;auto& slot=state.slots.at(index);
    require(geometry.size()<=65536,"Reference depth draw count exceeds 65536");
    unsigned maskedCount=0;
    for(const auto& draw:geometry){require(draw.positions&&draw.indices&&draw.vertexCount&&draw.vertexCount<=16777216
        &&draw.indexCount&&draw.indexCount<=67108864&&draw.indexCount%3==0,"Invalid reference depth geometry buffers or counts");
        validate(draw.surface,state.maskedAvailable,state.ellipseAvailable);maskedCount+=draw.surface.alpha?1U:0U;}
    require(maskedCount<=maskedLimit,"Reference depth coverage exceeds 128 draws per slot");
    for(const auto& draw:geometry)(void)state.pipeline(draw.surface);
    unsigned maskedIndex=0;
    for(const auto& draw:geometry)if(draw.surface.alpha){const auto& alpha=*draw.surface.alpha;
        state.backend.writeDescriptorImage({slot.sets[maskedIndex++],0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,alpha.image,alpha.sampler,alpha.layout});}
    Parameters parameters;parameters.projection=profile.parameters;parameters.control[0]=state.config.flipNdcY?1U:0U;
    if(state.config.positionFrame){parameters.control[2]=referencePositionFrameCode(*state.config.positionFrame);parameters.frameOffset=state.config.positionFrame->parameters.offset;}
    commands.imageBarrier({slot.color.image,slot.initialized?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,1,
        VkPipelineStageFlags(slot.initialized?VK_PIPELINE_STAGE_ALL_COMMANDS_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        slot.initialized?VK_ACCESS_SHADER_READ_BIT:0U,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT});
    rhi::ImageBarrierDesc depth;depth.image=slot.depth.image;depth.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
    depth.oldLayout=slot.initialized?VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED;depth.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.srcStageMask=slot.initialized?VK_PIPELINE_STAGE_ALL_COMMANDS_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    depth.dstStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depth.srcAccessMask=slot.initialized?VK_ACCESS_SHADER_READ_BIT:0;depth.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    commands.imageBarrier(depth);
    const float clear=state.config.convention==ReferenceDepthConvention::Reversed?0.F:1.F;
    rhi::RenderPassBeginDesc begin;begin.renderPass=state.pass;begin.framebuffer=slot.framebuffer;begin.extent=state.extent;begin.clearValues.resize(2);
    begin.clearValues[0].color.float32[0]=clear;begin.clearValues[1].depthStencil={clear,0};
    commands.beginRenderPass(begin);commands.setViewport(float(state.extent.width),float(state.extent.height));commands.setScissor(state.extent);maskedIndex=0;
    for(const auto& draw:geometry){
        commands.bindPipeline(state.pipeline(draw.surface));
        if(draw.surface.alpha){const auto& alpha=*draw.surface.alpha;parameters.control[1]=alpha.channel;parameters.coverage={alpha.cutoff,alpha.lodBias,0,0};
            commands.bindDescriptorSet(state.layout,slot.sets[maskedIndex++]);commands.bindVertexStream(1,alpha.uv,0);}
        commands.pushConstants(state.layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,&parameters,sizeof(parameters));
        commands.bindVertexBuffer(draw.positions,0);commands.bindIndexBuffer(draw.indices,0);commands.drawIndexed(draw.indexCount,0);
    }
    commands.endRenderPass();
    commands.imageBarrier({slot.color.image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    depth.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;depth.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depth.srcStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;depth.dstStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    depth.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;depth.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;commands.imageBarrier(depth);slot.initialized=true;
}
}
