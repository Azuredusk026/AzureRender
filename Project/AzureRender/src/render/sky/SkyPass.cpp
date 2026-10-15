#include "SkyPass.hpp"
#include "render/LinearMath.hpp"
#include "resources/BinaryFile.hpp"
#include <cstring>
namespace azurerender {
using namespace internal;
void SkyPass::load(const RenderContext& c){
 rhi_=c.rhi;allocator_=c.allocator;if(!rhi_||!allocator_)throw std::invalid_argument("Sky pass requires host RHI and allocator");
 if(c.renderSettings)settings_=c.renderSettings->sky;resize(c);
 try{
  EnvironmentImage source;if(!c.environment.path.empty())source=loadEnvironmentImage(c.environment);
  else{source.width=source.height=1;source.rgba16f={environmentFloatToHalf(.1F),environmentFloatToHalf(.15F),environmentFloatToHalf(.25F),environmentFloatToHalf(1)};}
  const auto size=source.rgba16f.size()*sizeof(std::uint16_t);auto staging=allocator_->createBuffer(size,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
  try{
   std::memcpy(staging.mapped,source.rgba16f.data(),size);allocator_->flush(staging,0,size);
   image_=allocator_->createImage2D(source.width,source.height,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
   rhi_->transitionImageLayout(image_,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);
   rhi_->copyBufferToImage(staging,image_,source.width,source.height);
   rhi_->transitionImageLayout(image_,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1);
  }catch(...){allocator_->destroyBuffer(staging);throw;}
  allocator_->destroyBuffer(staging);view_=rhi_->createImageView(image_.image,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
  rhi::SamplerDesc sampler;sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sampler_=rhi_->createSampler(sampler);
  setLayout_=rhi_->createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT},{1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT}});
  pool_=rhi_->createDescriptorPool({{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,c.maxFramesInFlight},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,c.maxFramesInFlight}},c.maxFramesInFlight});
  sets_=rhi_->allocateDescriptorSets(pool_,setLayout_,c.maxFramesInFlight);
  for(auto set:sets_){uniforms_.push_back(allocator_->createBuffer(sizeof(Uniform),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true));rhi_->writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,uniforms_.back().buffer,sizeof(Uniform)});rhi_->writeDescriptorImage({set,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,view_,sampler_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});}
  layout_=rhi_->createPipelineLayout(setLayout_,nullptr);
  auto vertex=rhi_->createShaderModule(readBinaryFile(c.shaderDirectory+"/blackhole.vert.spv"));VkShaderModule fragment=VK_NULL_HANDLE;
  try{fragment=rhi_->createShaderModule(readBinaryFile(c.shaderDirectory+"/sky.frag.spv"));rhi::GraphicsPipelineDesc desc;desc.vertexShader=vertex;desc.fragmentShader=fragment;desc.cullMode=VK_CULL_MODE_NONE;desc.depthTest=false;desc.depthWrite=false;desc.colorAttachmentCount=2;desc.layout=layout_;desc.renderPass=c.sceneRenderPass;pipeline_=rhi_->createGraphicsPipeline(desc);}
  catch(...){rhi_->destroyShaderModule(vertex);rhi_->destroyShaderModule(fragment);throw;}
  rhi_->destroyShaderModule(vertex);rhi_->destroyShaderModule(fragment);
 }catch(...){release();throw;}
}
void SkyPass::resize(const RenderContext& c){aspect_=static_cast<float>(c.renderExtent.width)/static_cast<float>(std::max(c.renderExtent.height,1U));}
void SkyPass::update(const SceneFrameData& f){
 frame_=f.currentFrame;if(f.renderSettings)settings_=f.renderSettings->sky;
 const auto& camera=f.camera.view;const auto forward=normalize(subtract(camera.target,camera.position));const auto right=normalize(cross(forward,camera.up));const auto up=cross(right,forward);
 const float scale=camera.projection==CameraProjectionKind::Orthographic?0.F:std::tan(camera.fov*.5F);
 Uniform uniform;for(unsigned i=0;i<3;++i){uniform.right[i]=right[i]*scale*aspect_;uniform.up[i]=up[i]*scale;uniform.forward[i]=forward[i];uniform.color[i]=settings_.color[i];}
 uniform.parameters={static_cast<float>(settings_.mode),settings_.intensity,settings_.rotationDegrees*.01745329252F,0};
 auto& buffer=uniforms_.at(frame_);std::memcpy(buffer.mapped,&uniform,sizeof(uniform));allocator_->flush(buffer,0,sizeof(uniform));
}
void SkyPass::draw(rhi::ICommandRecorder& cmd){cmd.bindPipeline(pipeline_);cmd.bindDescriptorSet(layout_,sets_.at(frame_));cmd.draw(3);}
void SkyPass::release(){
 if(!rhi_||!allocator_)return;
 rhi_->destroyPipeline(pipeline_);rhi_->destroyPipelineLayout(layout_);rhi_->destroyDescriptorPool(pool_);rhi_->destroyDescriptorSetLayout(setLayout_);
 pipeline_=VK_NULL_HANDLE;layout_=VK_NULL_HANDLE;pool_=VK_NULL_HANDLE;setLayout_=VK_NULL_HANDLE;
 for(auto& buffer:uniforms_)allocator_->destroyBuffer(buffer);uniforms_.clear();sets_.clear();
 rhi_->destroySampler(sampler_);rhi_->destroyImageView(view_);sampler_=VK_NULL_HANDLE;view_=VK_NULL_HANDLE;allocator_->destroyImage(image_);
}
}
