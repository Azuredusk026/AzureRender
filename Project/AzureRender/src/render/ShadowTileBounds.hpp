#pragma once
#include "rhi/Rhi.hpp"
#include "rhi/IGpuAllocator.hpp"
#include <memory>

namespace azurerender {
// Conservative depth bounds. The owning renderer retires frame fences before destruction.
class ShadowTileBounds final {
    rhi::IRhi& backend_;
    VkDescriptorSetLayout setLayout_{};
    VkDescriptorPool pool_{};
    VkDescriptorSet set_{};
    VkPipelineLayout layout_{};
    VkPipeline pipeline_{};
    rhi::GpuBuffer buffer_{};
    unsigned tiles_{};
public:
    explicit ShadowTileBounds(rhi::IRhi& backend):backend_(backend){}
    ShadowTileBounds(const ShadowTileBounds&)=delete;
    ~ShadowTileBounds(){release();}
    void initialize(const std::vector<char>& code,VkImageView depth,VkSampler sampler,unsigned extent){
        if(tiles_||!extent||extent%8)throw std::invalid_argument("Shadow bounds require an empty owner and a multiple-of-8 atlas");
        try{
            tiles_=extent/8;
            buffer_=backend_.allocator().createBuffer(VkDeviceSize(tiles_)*tiles_*2*sizeof(float),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
            setLayout_=backend_.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
            rhi::DescriptorPoolDesc pool;pool.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};pool.maxSets=1;
            pool_=backend_.createDescriptorPool(pool);set_=backend_.allocateDescriptorSets(pool_,setLayout_,1).front();
            backend_.writeDescriptorImage({set_,0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,depth,sampler,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL});
            backend_.writeDescriptorBuffer({set_,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,buffer_.buffer,buffer_.size});
            layout_=backend_.createPipelineLayout(setLayout_,nullptr);
            auto shader=backend_.createShaderModule(code);
            try{pipeline_=backend_.createComputePipeline({shader,layout_});}catch(...){backend_.destroyShaderModule(shader);throw;}
            backend_.destroyShaderModule(shader);
        }catch(...){release();throw;}
    }
    void record(rhi::ICommandRecorder& commands)const{
        if(!pipeline_)throw std::logic_error("Shadow bounds are not initialized");
        commands.bindComputePipeline(pipeline_);commands.bindComputeDescriptorSet(layout_,set_);commands.dispatch(tiles_,tiles_,1);
    }
    const rhi::GpuBuffer& buffer()const{return buffer_;}
private:
    void release(){
        if(pipeline_)backend_.destroyPipeline(pipeline_);if(layout_)backend_.destroyPipelineLayout(layout_);
        if(pool_)backend_.destroyDescriptorPool(pool_);if(setLayout_)backend_.destroyDescriptorSetLayout(setLayout_);
        backend_.allocator().destroyBuffer(buffer_);pipeline_={};layout_={};pool_={};setLayout_={};set_={};tiles_=0;
    }
};
}
