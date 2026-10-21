#include "SpatialAntiAliasing.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <filesystem>

namespace azurerender {
SpatialAntiAliasing::SpatialAntiAliasing(rhi::IRhi& backend,VkExtent2D extent,
    const std::string& shaders,VkRenderPass outputPass):backend_(backend){
    if(!extent.width||!extent.height||!outputPass)throw std::invalid_argument("Spatial AA requires a nonempty extent and output pass");
    if(!backend_.supportsSampledImage(kLinearFormat,true))throw std::runtime_error("Spatial AA requires linear RGBA16F sampling");
    VkShaderModule vertex=VK_NULL_HANDLE,fragment=VK_NULL_HANDLE;
    try{
        image_=backend_.allocator().createImage2D(extent.width,extent.height,kLinearFormat,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
        view_=backend_.createImageView(image_.image,kLinearFormat,VK_IMAGE_ASPECT_COLOR_BIT,1);
        rhi::RenderPassDesc pass;pass.attachments={{kLinearFormat,VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,false,true,false}};pass.externalReadDependency=true;
        pass_=backend_.createRenderPass(pass);
        framebuffer_=backend_.createFramebuffer({pass_,{view_},extent.width,extent.height});
        rhi::SamplerDesc sampler;sampler.filter=VK_FILTER_LINEAR;
        sampler.addressU=sampler.addressV=sampler.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.maxLod=0;sampler.mipmapLinear=false;sampler_=backend_.createSampler(sampler);
        setLayout_=backend_.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT}});
        pool_=backend_.createDescriptorPool({{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1}},1});
        set_=backend_.allocateDescriptorSets(pool_,setLayout_,1).at(0);
        backend_.writeDescriptorImage({set_,0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,view_,sampler_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        const rhi::PushConstantRangeDesc push{VK_SHADER_STAGE_FRAGMENT_BIT,16};layout_=backend_.createPipelineLayout(setLayout_,&push);
        vertex=backend_.createShaderModule(readBinaryFile((std::filesystem::u8path(shaders)/"blackhole.vert.spv").u8string()));
        fragment=backend_.createShaderModule(readBinaryFile((std::filesystem::u8path(shaders)/"spatial_antialiasing.frag.spv").u8string()));
        rhi::GraphicsPipelineDesc pipeline;pipeline.vertexShader=vertex;pipeline.fragmentShader=fragment;
        pipeline.cullMode=VK_CULL_MODE_NONE;pipeline.depthTest=pipeline.depthWrite=false;
        pipeline.colorAttachmentCount=1;pipeline.renderPass=outputPass;pipeline.layout=layout_;
        pipeline_=backend_.createGraphicsPipeline(pipeline);
        backend_.destroyShaderModule(vertex);vertex=VK_NULL_HANDLE;backend_.destroyShaderModule(fragment);fragment=VK_NULL_HANDLE;
    }catch(...){if(vertex)backend_.destroyShaderModule(vertex);if(fragment)backend_.destroyShaderModule(fragment);release();throw;}
}
SpatialAntiAliasing::~SpatialAntiAliasing(){release();}
void SpatialAntiAliasing::release() noexcept{
    if(pipeline_)backend_.destroyPipeline(pipeline_);
    if(layout_)backend_.destroyPipelineLayout(layout_);
    if(pool_)backend_.destroyDescriptorPool(pool_);
    if(setLayout_)backend_.destroyDescriptorSetLayout(setLayout_);
    if(framebuffer_)backend_.destroyFramebuffer(framebuffer_);
    if(pass_)backend_.destroyRenderPass(pass_);
    if(sampler_)backend_.destroySampler(sampler_);
    if(view_)backend_.destroyImageView(view_);
    backend_.allocator().destroyImage(image_);
}
void SpatialAntiAliasing::resolve(rhi::ICommandRecorder& commands,int mode,bool encodeSrgb,bool bypassEdge) const{
    if(mode<0||mode>2)throw std::invalid_argument("Spatial AA mode must be within 0..2");
    const std::array<float,4> parameters{float(mode),encodeSrgb?1.F:0.F,bypassEdge?1.F:0.F,0};
    commands.bindPipeline(pipeline_);commands.bindDescriptorSet(layout_,set_);
    commands.pushConstants(layout_,VK_SHADER_STAGE_FRAGMENT_BIT,0,parameters.data(),sizeof(parameters));commands.draw(3);
}
}
