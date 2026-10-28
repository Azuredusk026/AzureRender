#include "MotionVectorPass.hpp"
#include "assets/GltfLoader.hpp"
#include "resources/BinaryFile.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace azurerender {
MotionVectorPass::MotionVectorPass(rhi::IRhi& backend,rhi::IGpuAllocator& allocator,MotionVectorTarget& target,
    const std::string& shaders,std::uint32_t materials,std::uint32_t frames,bool culling,bool depthTest)
    :rhi_(backend),allocator_(allocator),target_(target),materials_(materials){
    if(!materials||!frames||frames>2||materials>UINT32_MAX/frames/6)throw std::invalid_argument("Invalid motion descriptor capacity");
    previous_.resize(frames);
    try{
        std::vector<rhi::DescriptorBindingDesc> bindings;
        for(unsigned i=0;i<5;++i)bindings.push_back({i,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT});
        bindings.push_back({5,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT});
        bindings.push_back({6,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT});
        bindings.push_back({7,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_VERTEX_BIT});
        bindings.push_back({8,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT});
        descriptors_=rhi_.createDescriptorSetLayout(bindings);
        rhi::DescriptorPoolDesc pool;pool.maxSets=materials*frames;
        pool.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,pool.maxSets*6},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,pool.maxSets*3}};
        pool_=rhi_.createDescriptorPool(pool);sets_=rhi_.allocateDescriptorSets(pool_,descriptors_,pool.maxSets);
        rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,sizeof(MotionVectorPush)};
        layout_=rhi_.createPipelineLayout(descriptors_,&range);
        const auto vs=rhi_.createShaderModule(readBinaryFile(shaders+"/motion_vector.vert.spv"));
        VkShaderModule fs=VK_NULL_HANDLE,shell=VK_NULL_HANDLE;
        try{
            fs=rhi_.createShaderModule(readBinaryFile(shaders+"/motion_vector.frag.spv"));
            shell=rhi_.createShaderModule(readBinaryFile(shaders+"/motion_outline.vert.spv"));
            rhi::GraphicsPipelineDesc p;p.vertexShader=vs;p.fragmentShader=fs;p.layout=layout_;p.renderPass=target_.renderPass();
            p.vertexStride=sizeof(AssetVertex);p.depthTest=depthTest;p.depthWrite=false;p.depthCompare=VK_COMPARE_OP_LESS_OR_EQUAL;
            p.colorAttachmentCount=2;p.vertexAttributes={
                {0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(AssetVertex,position)},
                {1,VK_FORMAT_R32G32B32_SFLOAT,offsetof(AssetVertex,normal)},
                {2,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(AssetVertex,tangent)},
                {3,VK_FORMAT_R32G32_SFLOAT,offsetof(AssetVertex,texcoord)},
                {4,VK_FORMAT_R32G32B32A32_UINT,offsetof(AssetVertex,joints)},
                {5,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(AssetVertex,weights)},
                {6,VK_FORMAT_R32G32B32_SFLOAT,offsetof(AssetVertex,morph0)},
                {7,VK_FORMAT_R32G32B32_SFLOAT,offsetof(AssetVertex,morph1)},
                {8,VK_FORMAT_R32_SFLOAT,offsetof(AssetVertex,browMask)}};
            for(unsigned i=0;i<8;++i){p.vertexShader=i&4?shell:vs;
                p.frontFace=i&1?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
                p.cullMode=i&4?VK_CULL_MODE_FRONT_BIT:(!culling||(i&2)?VK_CULL_MODE_NONE:VK_CULL_MODE_BACK_BIT);
                pipelines_[i]=rhi_.createGraphicsPipeline(p);}
        }catch(...){if(shell)rhi_.destroyShaderModule(shell);if(fs)rhi_.destroyShaderModule(fs);rhi_.destroyShaderModule(vs);throw;}
        rhi_.destroyShaderModule(shell);rhi_.destroyShaderModule(fs);rhi_.destroyShaderModule(vs);
    }catch(...){reset();throw;}
}
MotionVectorPass::~MotionVectorPass(){reset();}
void MotionVectorPass::reset() noexcept{
    for(auto p:pipelines_)if(p)rhi_.destroyPipeline(p);pipelines_={};
    if(layout_)rhi_.destroyPipelineLayout(layout_);layout_=VK_NULL_HANDLE;
    if(pool_)rhi_.destroyDescriptorPool(pool_);pool_=VK_NULL_HANDLE;sets_.clear();
    if(descriptors_)rhi_.destroyDescriptorSetLayout(descriptors_);descriptors_=VK_NULL_HANDLE;
    for(auto& b:previous_)if(b.buffer)allocator_.destroyBuffer(b);previous_.clear();
}
void MotionVectorPass::prepare(std::uint32_t frame,const PackedMotionPoseFrame& packed,const MotionVectorInputs& inputs){
    if(frame>=previous_.size()||packed.words.size()<PackedMotionPoseFrame::kHeaderWords
        ||packed.words.size()>128ULL*1024*1024/4||inputs.colors.size()!=materials_||inputs.masks.size()!=materials_)
        throw std::invalid_argument("Invalid motion frame inputs");
    for(auto b:{inputs.instances,inputs.joints,inputs.weights,inputs.targets,inputs.outline})
        if(!b||!b->buffer||!b->size)throw std::invalid_argument("Missing motion storage input");
    const auto missing=[](const auto& value){return !value.view||!value.sampler;};
    if(missing(inputs.coverage)||std::any_of(inputs.colors.begin(),inputs.colors.end(),missing)||std::any_of(inputs.masks.begin(),inputs.masks.end(),missing))
        throw std::invalid_argument("Motion descriptor images must have complete borrowed bindings");
    auto& buffer=previous_[frame];const auto bytes=packed.words.size()*4;
    if(buffer.size<bytes){auto next=allocator_.createBuffer(bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        if(buffer.buffer)allocator_.destroyBuffer(buffer);buffer=next;}
    std::memcpy(buffer.mapped,packed.words.data(),bytes);allocator_.flush(buffer,0,bytes);
    const std::array<const rhi::GpuBuffer*,5> buffers{inputs.instances,inputs.joints,inputs.weights,inputs.targets,&buffer};
    for(unsigned material=0;material<materials_;++material){const auto set=sets_.at(frame*materials_+material);
        for(unsigned binding=0;binding<5;++binding)rhi_.writeDescriptorBuffer({set,binding,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,buffers[binding]->buffer,buffers[binding]->size});
        rhi_.writeDescriptorBuffer({set,6,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,inputs.outline->buffer,inputs.outline->size});
        for(auto pair:{std::pair{5U,inputs.colors[material]},std::pair{7U,inputs.masks[material]},std::pair{8U,inputs.coverage}})
            rhi_.writeDescriptorImage({set,pair.first,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,pair.second.view,pair.second.sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    }
}
void MotionVectorPass::record(rhi::ICommandRecorder& commands,std::uint32_t frame,std::uint32_t image,VkExtent2D extent,const std::vector<MotionVectorDraw>& draws) const{
    recordToFramebuffer(commands,frame,target_.framebuffer(image),extent,draws);
}
void MotionVectorPass::recordToFramebuffer(rhi::ICommandRecorder& commands,std::uint32_t frame,VkFramebuffer framebuffer,VkExtent2D extent,const std::vector<MotionVectorDraw>& draws) const{
    if(frame>=previous_.size()||!framebuffer||extent.width!=target_.extent().width||extent.height!=target_.extent().height)
        throw std::invalid_argument("Motion recording requires a matching slot, extent and borrowed framebuffer");
    for(auto image:{target_.image().image,target_.surfaceDepthImage().image})commands.imageBarrier({image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,1,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT});
    rhi::RenderPassBeginDesc pass;pass.renderPass=target_.renderPass();pass.framebuffer=framebuffer;pass.extent=extent;pass.clearValues.resize(3);
    pass.clearValues[1].color.float32[0]=1;commands.beginRenderPass(pass);commands.setViewport(float(extent.width),float(extent.height));commands.setScissor(extent);
    for(const auto& draw:draws){commands.bindPipeline(pipelines_.at((draw.shell?4:0)|(draw.doubleSided?2:0)|(draw.mirrored?1:0)));
        commands.bindDescriptorSet(layout_,sets_.at(frame*materials_+draw.push.material));
        commands.bindVertexBuffer(draw.vertices,0);commands.bindIndexBuffer(draw.indices,0);
        commands.pushConstants(layout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,&draw.push,sizeof(draw.push));
        commands.drawIndexed(draw.indexCount,draw.firstIndex,1,draw.instance);}
    commands.endRenderPass();
}
}
