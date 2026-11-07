#include "render/ScreenObjectVisibility.hpp"
#include "resources/BinaryFile.hpp"
#include <algorithm>
#include <cstring>
namespace azurerender {
namespace {
using Float4=std::array<float,4>;
struct alignas(16) PackedRecord {std::array<float,16> projection{};Float4 lightBias{},normalFilter{},rect{};};
struct alignas(16) PackedFrame {
    std::array<float,16> clipToWorld{};Float4 reconstruction{};std::array<std::uint32_t,4> control{};
    std::array<PackedRecord,8> records{};
};
static_assert(sizeof(PackedRecord)==112&&sizeof(PackedFrame)==992,"Visibility uniform layout mismatch");
void require(bool value,const char* message){if(!value)throw std::invalid_argument(message);}
template<class T>bool finite(const T& values){return std::all_of(values.begin(),values.end(),[](float v){return std::isfinite(v);});}
bool invertible(const std::array<float,16>& matrix){
    double a[4][4];for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)a[r][c]=matrix[c*4+r];
    for(unsigned c=0;c<4;++c){
        unsigned pivot=c;for(unsigned r=c+1;r<4;++r)if(std::abs(a[r][c])>std::abs(a[pivot][c]))pivot=r;
        if(std::abs(a[pivot][c])<1e-30)return false;
        for(unsigned j=c;j<4;++j)std::swap(a[c][j],a[pivot][j]);
        for(unsigned r=c+1;r<4;++r){const double ratio=a[r][c]/a[c][c];for(unsigned j=c+1;j<4;++j)a[r][j]-=ratio*a[c][j];}
    }return true;
}
std::uint64_t budget(VkExtent2D extent,unsigned frames){
    require(extent.width&&extent.height&&extent.width<=8192&&extent.height<=8192&&frames&&frames<=8,"Invalid visibility extent or slot count");
    const auto bytes=(std::uint64_t(extent.width)*extent.height*2+sizeof(PackedFrame))*frames;
    require(bytes<=256ULL*1024*1024,"Visibility resources exceed 256 MiB");return bytes;
}
bool depthFormat(VkFormat f){return f==VK_FORMAT_R32_SFLOAT||f==VK_FORMAT_D32_SFLOAT||f==VK_FORMAT_D16_UNORM;}
bool sampledLayout(VkImageLayout layout,VkFormat format){
    return layout==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL||((format==VK_FORMAT_D16_UNORM||format==VK_FORMAT_D32_SFLOAT)&&layout==VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
}
PackedFrame pack(VkExtent2D extent,const ScreenObjectVisibilityInputs& inputs,const ScreenObjectVisibilityView& view,const std::vector<ScreenObjectVisibilityRecord>& records){
    require(records.size()<=8,"Visibility supports at most eight object records");
    require(finite(view.clipToWorld)&&invertible(view.clipToWorld),"Visibility inverse projection must be finite and invertible");
    require(finite(view.pixelOffset)&&std::abs(view.pixelOffset[0])<=1&&std::abs(view.pixelOffset[1])<=1,"Visibility pixel offset is outside -1..1");
    require(view.phaseOrigin==ScreenObjectVisibilityPhaseOrigin::TopLeft||view.phaseOrigin==ScreenObjectVisibilityPhaseOrigin::BottomLeft,"Unknown visibility phase origin");
    require(inputs.objectDepth==ScreenObjectDepth::Forward||inputs.objectDepth==ScreenObjectDepth::Reversed,"Unknown object depth convention");
    PackedFrame result;result.clipToWorld=view.clipToWorld;result.reconstruction={1.F/extent.width,1.F/extent.height,view.pixelOffset[0],view.pixelOffset[1]};
    result.control={static_cast<unsigned>(records.size()),view.flipNdcY?1U:0U,inputs.objectDepth==ScreenObjectDepth::Forward?1U:0U,view.phaseOrigin==ScreenObjectVisibilityPhaseOrigin::BottomLeft?1U:0U};
    for(unsigned i=0;i<records.size();++i){
        const auto& record=records[i];
        require(finite(record.worldToUnitShadow)&&record.worldToUnitShadow[3]==0&&record.worldToUnitShadow[7]==0&&record.worldToUnitShadow[11]==0&&record.worldToUnitShadow[15]==1,"Visibility object projection must be finite and affine");
        require(finite(record.lightDirection),"Visibility light direction must be finite");
        float length=0;for(float x:record.lightDirection)length+=x*x;
        require(length>=.99F&&length<=1.01F,"Visibility light direction must have unit length");
        require(std::isfinite(record.lightBias)&&record.lightBias>=0&&record.lightBias<=10000&&std::isfinite(record.normalBias)&&record.normalBias>=0&&record.normalBias<=10000,"Visibility bias must be finite within 0..10000");
        require(finite(record.filterRadiusUv)&&record.filterRadiusUv[0]>=0&&record.filterRadiusUv[1]>=0&&record.filterRadiusUv[0]<=1&&record.filterRadiusUv[1]<=1,"Visibility filter radius must be within 0..1 UV");
        require(std::isfinite(record.minimumReceiverDepth)&&record.minimumReceiverDepth>0&&record.minimumReceiverDepth<=1,"Visibility minimum depth must be within (0,1]");
        const auto& rect=record.atlasRect;
        require(finite(rect)&&rect[0]>=0&&rect[1]>=0&&rect[2]>0&&rect[3]>0&&rect[0]+rect[2]<=1&&rect[1]+rect[3]<=1,"Visibility atlas rectangle must lie within unit UV");
        auto& target=result.records[i];target.projection=record.worldToUnitShadow;
        target.lightBias={record.lightDirection[0],record.lightDirection[1],record.lightDirection[2],record.lightBias};
        target.normalFilter={record.normalBias,record.filterRadiusUv[0],record.filterRadiusUv[1],record.minimumReceiverDepth};target.rect=rect;
    }return result;
}
}
struct ScreenObjectVisibilityGpu::Impl {
    struct Slot {rhi::GpuImage image{};VkImageView view{};VkFramebuffer framebuffer{};rhi::GpuBuffer parameters{};VkDescriptorSet set{};bool initialized=false;};
    rhi::IRhi& backend;VkExtent2D extent{};std::uint64_t bytes=0;std::vector<Slot> slots;
    VkSampler sampler{};VkDescriptorSetLayout descriptors{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkRenderPass pass{};VkPipeline pipeline{};
    Impl(rhi::IRhi& b,VkExtent2D e):backend(b),extent(e){}
    ~Impl(){
        if(pipeline)backend.destroyPipeline(pipeline);if(layout)backend.destroyPipelineLayout(layout);
        if(pool)backend.destroyDescriptorPool(pool);if(descriptors)backend.destroyDescriptorSetLayout(descriptors);
        for(auto& slot:slots){if(slot.framebuffer)backend.destroyFramebuffer(slot.framebuffer);if(slot.view)backend.destroyImageView(slot.view);backend.allocator().destroyImage(slot.image);backend.allocator().destroyBuffer(slot.parameters);}
        if(pass)backend.destroyRenderPass(pass);if(sampler)backend.destroySampler(sampler);
    }
    void initialize(unsigned frames,const ScreenObjectVisibilityShaders& shaders){
        bytes=budget(extent,frames);slots.resize(frames);
        rhi::SamplerDesc sampled;sampled.filter=VK_FILTER_NEAREST;sampled.mipmapLinear=false;
        sampled.addressU=sampled.addressV=sampled.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sampler=backend.createSampler(sampled);
        descriptors=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT},
            {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT},{2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT},
            {3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT},{4,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT}});
        rhi::DescriptorPoolDesc descriptorPool;descriptorPool.maxSets=frames;descriptorPool.sizes={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,frames},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,frames*4}};
        pool=backend.createDescriptorPool(descriptorPool);const auto sets=backend.allocateDescriptorSets(pool,descriptors,frames);
        layout=backend.createPipelineLayout(descriptors,nullptr);
        rhi::RenderPassDesc render;render.attachments={{VK_FORMAT_R8G8_UNORM,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,false,true,false}};
        pass=backend.createRenderPass(render);
        VkShaderModule vertex{},fragment{};
        try{
            vertex=backend.createShaderModule(shaders.vertex);fragment=backend.createShaderModule(shaders.fragment);
            rhi::GraphicsPipelineDesc graphics;graphics.vertexShader=vertex;graphics.fragmentShader=fragment;graphics.layout=layout;graphics.renderPass=pass;
            graphics.cullMode=VK_CULL_MODE_NONE;graphics.depthTest=graphics.depthWrite=false;graphics.colorAttachmentCount=1;
            pipeline=backend.createGraphicsPipeline(graphics);
        }catch(...){if(vertex)backend.destroyShaderModule(vertex);if(fragment)backend.destroyShaderModule(fragment);throw;}
        backend.destroyShaderModule(vertex);backend.destroyShaderModule(fragment);
        for(unsigned i=0;i<frames;++i){
            auto& slot=slots[i];slot.set=sets[i];
            slot.image=backend.allocator().createImage2D(extent.width,extent.height,VK_FORMAT_R8G8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            slot.view=backend.createImageView(slot.image.image,VK_FORMAT_R8G8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT,1);slot.framebuffer=backend.createFramebuffer({pass,{slot.view},extent.width,extent.height});
            slot.parameters=backend.allocator().createBuffer(sizeof(PackedFrame),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
            backend.writeDescriptorBuffer({slot.set,0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,slot.parameters.buffer,slot.parameters.size});
        }
    }
};
bool ScreenObjectVisibilityGpu::supported(rhi::IRhi& backend,VkExtent2D extent,unsigned frames){
    try{(void)budget(extent,frames);}catch(const std::invalid_argument&){return false;}
    return backend.supportsSampledImage(VK_FORMAT_R8G8_UNORM,false)&&(backend.imageSampleCounts(VK_FORMAT_R8G8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT)&VK_SAMPLE_COUNT_1_BIT);
}
ScreenObjectVisibilityShaders ScreenObjectVisibilityShaders::load(const std::string& directory){return {readBinaryFile(directory+"/screen_object_visibility.vert.spv"),readBinaryFile(directory+"/screen_object_visibility.frag.spv")};}
ScreenObjectVisibilityGpu::ScreenObjectVisibilityGpu(rhi::IRhi& backend,VkExtent2D extent,unsigned frames,const ScreenObjectVisibilityShaders& shaders){
    (void)budget(extent,frames);if(!supported(backend,extent,frames))throw std::runtime_error("Visibility color attachment is unavailable");
    impl_=std::make_unique<Impl>(backend,extent);impl_->initialize(frames,shaders);
}
ScreenObjectVisibilityGpu::~ScreenObjectVisibilityGpu()=default;
std::uint64_t ScreenObjectVisibilityGpu::byteBudget() const noexcept{return impl_->bytes;}
ScreenObjectVisibilityOutput ScreenObjectVisibilityGpu::output(unsigned index)const{const auto& slot=impl_->slots.at(index);return {slot.image.image,slot.view,impl_->sampler,impl_->extent};}
void ScreenObjectVisibilityGpu::record(rhi::ICommandRecorder& commands,unsigned index,const ScreenObjectVisibilityInputs& inputs,const ScreenObjectVisibilityView& view,const std::vector<ScreenObjectVisibilityRecord>& records){
    auto& state=*impl_;auto& slot=state.slots.at(index);auto& backend=state.backend;
    require(inputs.depth&&inputs.normals&&inputs.objectIds&&inputs.atlas&&inputs.extent.width==state.extent.width&&inputs.extent.height==state.extent.height,"Visibility requires complete matching input views");
    require(depthFormat(inputs.depthFormat)&&(inputs.atlasFormat==VK_FORMAT_R16_UNORM||inputs.atlasFormat==VK_FORMAT_D16_UNORM),"Unsupported visibility depth format");
    require(inputs.normalFormat==VK_FORMAT_A2B10G10R10_UNORM_PACK32||inputs.normalFormat==VK_FORMAT_R16G16_UNORM,"Unsupported visibility octahedral normal format");
    require(inputs.objectIdFormat==VK_FORMAT_R8_UINT||inputs.objectIdFormat==VK_FORMAT_R16_UINT||inputs.objectIdFormat==VK_FORMAT_R32_UINT,"Visibility object IDs require unsigned integer format");
    require(sampledLayout(inputs.depthLayout,inputs.depthFormat)&&sampledLayout(inputs.atlasLayout,inputs.atlasFormat),"Visibility depth inputs require sampled read-only layouts");
    for(auto format:{inputs.depthFormat,inputs.normalFormat,inputs.objectIdFormat,inputs.atlasFormat})require(backend.supportsSampledImage(format,false),"Visibility sampled input format is unavailable");
    for(const auto& owned:state.slots)for(auto borrowed:{inputs.depth,inputs.normals,inputs.objectIds,inputs.atlas})require(borrowed!=owned.view,"Visibility inputs alias owned outputs");
    const auto frame=pack(state.extent,inputs,view,records);
    std::memcpy(slot.parameters.mapped,&frame,sizeof(frame));backend.allocator().flush(slot.parameters,0,sizeof(frame));
    const VkImageView views[]={inputs.depth,inputs.normals,inputs.objectIds,inputs.atlas};
    const VkImageLayout layouts[]={inputs.depthLayout,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,inputs.atlasLayout};
    for(unsigned i=0;i<4;++i)backend.writeDescriptorImage({slot.set,i+1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,views[i],state.sampler,layouts[i]});
    commands.bufferBarrier({slot.parameters.buffer,0,slot.parameters.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_UNIFORM_READ_BIT});
    commands.imageBarrier({slot.image.image,slot.initialized?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,1,
        VkPipelineStageFlags(slot.initialized?VK_PIPELINE_STAGE_ALL_COMMANDS_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        slot.initialized?VK_ACCESS_SHADER_READ_BIT:0U,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT});
    commands.beginRenderPass({state.pass,slot.framebuffer,state.extent,{}});commands.bindPipeline(state.pipeline);commands.bindDescriptorSet(state.layout,slot.set);
    commands.setViewport(float(state.extent.width),float(state.extent.height));commands.setScissor(state.extent);commands.draw(3);commands.endRenderPass();
    commands.imageBarrier({slot.image.image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    slot.initialized=true;
}
}
