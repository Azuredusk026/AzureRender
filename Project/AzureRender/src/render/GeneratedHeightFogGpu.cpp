#include "render/GeneratedHeightFogGpu.hpp"
#include "render/ViewFogGpu.hpp"
#include <stdexcept>

namespace azurerender {
namespace {
constexpr auto format=VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr unsigned groupSize=4;
}

GeneratedHeightFogGpu::GeneratedHeightFogGpu(rhi::IRhi& backend, std::array<unsigned,3> extent,
    const std::vector<rhi::GpuBuffer>& frameParameters, const std::vector<char>& program)
    : backend_(backend), extent_(extent)
{
    if(extent[0]==0||extent[0]>64||extent[1]==0||extent[1]>64||extent[2]<2||extent[2]>128
        ||frameParameters.empty()||frameParameters.size()>8)
        throw std::invalid_argument("Generated height fog requires bounded dimensions and frame slots");
    for(const auto& parameters:frameParameters)
        if(!parameters.buffer||parameters.size<sizeof(PackedViewFogFrame))
            throw std::invalid_argument("Generated height fog requires complete per-frame parameters");
    if(!backend.supportsSampledImage(format,true)||!backend.supportsStorageImage(format))
        throw std::runtime_error("Device does not support linear sampling and storage of generated height fog");

    slots_.resize(frameParameters.size());
    VkShaderModule shader=VK_NULL_HANDLE;
    try {
        descriptors_=backend.createDescriptorSetLayout({
            {0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        const auto count=static_cast<unsigned>(slots_.size());
        rhi::DescriptorPoolDesc poolDescription;
        poolDescription.maxSets=count;
        poolDescription.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,count},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,count}};
        pool_=backend.createDescriptorPool(poolDescription);
        const auto sets=backend.allocateDescriptorSets(pool_,descriptors_,count);
        layout_=backend.createPipelineLayout(descriptors_,nullptr);
        shader=backend.createShaderModule(program);
        pipeline_=backend.createComputePipeline({shader,layout_});
        backend.destroyShaderModule(shader);
        shader=VK_NULL_HANDLE;

        rhi::SamplerDesc sampling;
        sampling.addressU=sampling.addressV=sampling.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampling.mipmapLinear=false;
        sampler_=backend.createSampler(sampling);
        VkImageCreateInfo description{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        description.imageType=VK_IMAGE_TYPE_3D;
        description.extent={extent_[0],extent_[1],extent_[2]};
        description.mipLevels=description.arrayLayers=1;
        description.format=format;
        description.tiling=VK_IMAGE_TILING_OPTIMAL;
        description.usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
        description.samples=VK_SAMPLE_COUNT_1_BIT;
        for(unsigned index=0;index<count;++index){
            auto& slot=slots_[index];
            slot.image=backend.allocator().createImage(description,false);
            slot.view=backend.createImageView(slot.image.image,format,VK_IMAGE_ASPECT_COLOR_BIT,1,0,VK_IMAGE_VIEW_TYPE_3D);
            slot.descriptor=sets[index];
            const auto& parameters=frameParameters[index];
            backend.writeDescriptorBuffer({slot.descriptor,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                parameters.buffer,sizeof(PackedViewFogFrame)});
            backend.writeDescriptorImage({slot.descriptor,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                slot.view,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});
        }
    } catch(...) {
        if(shader)backend.destroyShaderModule(shader);
        release();
        throw;
    }
}

GeneratedHeightFogGpu::~GeneratedHeightFogGpu(){release();}

void GeneratedHeightFogGpu::release() noexcept {
    if(pipeline_)backend_.destroyPipeline(pipeline_);
    if(layout_)backend_.destroyPipelineLayout(layout_);
    if(pool_)backend_.destroyDescriptorPool(pool_);
    if(descriptors_)backend_.destroyDescriptorSetLayout(descriptors_);
    if(sampler_)backend_.destroySampler(sampler_);
    for(auto& slot:slots_){
        if(slot.view)backend_.destroyImageView(slot.view);
        backend_.allocator().destroyImage(slot.image);
    }
}

VkImage GeneratedHeightFogGpu::image(unsigned slot) const {
    return slots_.at(slot).image.image;
}

void GeneratedHeightFogGpu::writeTexture(VkDescriptorSet set,unsigned binding,unsigned slot) const {
    backend_.writeDescriptorImage({set,binding,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        slots_.at(slot).view,sampler_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
}

RenderGraph::ResourceId GeneratedHeightFogGpu::addPass(RenderGraph& graph,unsigned slot,
    RenderGraph::ResourceId frameParameters) const
{
    rhi::ImageBarrierDesc initial;
    // The producer overwrites every voxel after the slot's preceding submission retires.
    initial.image=image(slot);
    const auto output=graph.importImage("character-generated-height-fog",initial);
    const auto descriptor=slots_.at(slot).descriptor;
    const auto pass=graph.addCommandPass("character-generate-height-fog",[this,descriptor](rhi::ICommandRecorder& commands){
        commands.bindComputePipeline(pipeline_);
        commands.bindComputeDescriptorSet(layout_,descriptor);
        commands.dispatch((extent_[0]+groupSize-1)/groupSize,(extent_[1]+groupSize-1)/groupSize,
                          (extent_[2]+groupSize-1)/groupSize);
    });
    graph.use(pass,frameParameters,RenderGraphUsage::Storage,false);
    graph.use(pass,output,RenderGraphUsage::Storage,true);
    return output;
}

}
