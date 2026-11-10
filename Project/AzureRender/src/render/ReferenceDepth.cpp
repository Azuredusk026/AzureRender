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
    require(config.schemaVersion==1,"Unsupported reference depth version");
    require(config.convention==ReferenceDepthConvention::Forward||config.convention==ReferenceDepthConvention::Reversed,"Unknown reference depth convention");
}
struct alignas(16) Parameters {ReferenceClipProjectionParameters projection;std::array<unsigned,4> control{};};
static_assert(sizeof(Parameters)==96);
constexpr VkImageUsageFlags colorUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
constexpr VkImageUsageFlags depthUsage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
}
struct ReferenceDepthGpu::Impl {
    struct Slot {rhi::GpuImage color{},depth{};VkImageView colorView{},depthView{};VkFramebuffer framebuffer{};bool initialized=false;};
    rhi::IRhi& backend;VkExtent2D extent;ReferenceDepthConfig config;std::uint64_t bytes;std::vector<Slot> slots;
    VkDescriptorSetLayout descriptors{};VkPipelineLayout layout{};VkRenderPass pass{};VkPipeline pipeline{};
    Impl(rhi::IRhi& b,VkExtent2D e,const ReferenceDepthConfig& c,unsigned count):backend(b),extent(e),config(c),bytes(budget(e,count)){}
    ~Impl(){
        if(pipeline)backend.destroyPipeline(pipeline);if(layout)backend.destroyPipelineLayout(layout);
        if(descriptors)backend.destroyDescriptorSetLayout(descriptors);
        for(auto& slot:slots){
            if(slot.framebuffer)backend.destroyFramebuffer(slot.framebuffer);
            if(slot.colorView)backend.destroyImageView(slot.colorView);if(slot.depthView)backend.destroyImageView(slot.depthView);
            backend.allocator().destroyImage(slot.color);backend.allocator().destroyImage(slot.depth);
        }
        if(pass)backend.destroyRenderPass(pass);
    }
    void initialize(unsigned count,const ReferenceDepthShaders& shaders){
        // Prepare and validate the marked arithmetic before allocating resources.
        const auto prepared=prepareReferenceFmaShader(shaders.vertex);
        require(prepared.fusedResults==3,"Reference depth projection requires three marked Fma results");
        descriptors=backend.createDescriptorSetLayout({});
        const rhi::PushConstantRangeDesc push{VK_SHADER_STAGE_VERTEX_BIT,sizeof(Parameters)};
        layout=backend.createPipelineLayout(descriptors,&push);
        rhi::RenderPassDesc render;render.depthAttachment=1;
        render.attachments={{VK_FORMAT_R32_SFLOAT,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,true,true,false},
            {VK_FORMAT_D32_SFLOAT,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,true,true,true}};
        pass=backend.createRenderPass(render);
        VkShaderModule vertex{},fragment{};
        try{
            vertex=backend.createShaderModule(prepared.code);fragment=backend.createShaderModule(shaders.fragment);
            rhi::GraphicsPipelineDesc graphics;graphics.vertexShader=vertex;graphics.fragmentShader=fragment;
            graphics.vertexStride=16;graphics.vertexAttributes={{0,VK_FORMAT_R32G32B32A32_SFLOAT,0}};
            graphics.cullMode=VK_CULL_MODE_NONE;graphics.colorAttachmentCount=1;graphics.renderPass=pass;graphics.layout=layout;
            graphics.depthCompare=config.convention==ReferenceDepthConvention::Reversed?VK_COMPARE_OP_GREATER:VK_COMPARE_OP_LESS;
            pipeline=backend.createGraphicsPipeline(graphics);
        }catch(...){if(vertex)backend.destroyShaderModule(vertex);if(fragment)backend.destroyShaderModule(fragment);throw;}
        backend.destroyShaderModule(vertex);backend.destroyShaderModule(fragment);
        slots.resize(count);
        for(auto& slot:slots){
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
    for(const auto& draw:geometry)require(draw.positions&&draw.indices&&draw.vertexCount&&draw.vertexCount<=16777216
        &&draw.indexCount&&draw.indexCount<=67108864&&draw.indexCount%3==0,"Invalid reference depth geometry buffers or counts");
    Parameters parameters;parameters.projection=profile.parameters;parameters.control[0]=state.config.flipNdcY?1U:0U;
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
    commands.beginRenderPass(begin);commands.bindPipeline(state.pipeline);commands.setViewport(float(state.extent.width),float(state.extent.height));commands.setScissor(state.extent);
    commands.pushConstants(state.layout,VK_SHADER_STAGE_VERTEX_BIT,0,&parameters,sizeof(parameters));
    for(const auto& draw:geometry){commands.bindVertexBuffer(draw.positions,0);commands.bindIndexBuffer(draw.indices,0);commands.drawIndexed(draw.indexCount,0);}
    commands.endRenderPass();
    commands.imageBarrier({slot.color.image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    depth.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;depth.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depth.srcStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;depth.dstStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    depth.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;depth.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;commands.imageBarrier(depth);slot.initialized=true;
}
}
