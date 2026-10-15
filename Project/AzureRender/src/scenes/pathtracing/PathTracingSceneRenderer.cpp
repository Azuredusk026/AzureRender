#include "PathTracingSceneRenderer.hpp"
#include "assets/MaterialData.hpp"
#include "resources/BinaryFile.hpp"
#include "render/LinearMath.hpp"
#include <cstring>
#include <algorithm>
namespace azurerender {
using namespace internal;
void PathTracingSceneRenderer::upload(rhi::GpuBuffer& buffer,const void* data,std::size_t size,VkBufferUsageFlags usage){
 const auto capacity=std::max<std::size_t>(size,16);
 if(capacity>maxBuffer_)throw std::runtime_error("Path tracing buffer exceeds device storage range");
 if(buffer.size<capacity){allocator_->destroyBuffer(buffer);buffer=allocator_->createBuffer(capacity,usage,true);}
 if(size)std::memcpy(buffer.mapped,data,size);else std::memset(buffer.mapped,0,capacity);
 allocator_->flush(buffer,0,capacity);
}
void PathTracingSceneRenderer::allocateHistory(const RenderContext& c){
 width_=c.renderExtent.width;height_=c.renderExtent.height;
 const auto size=static_cast<VkDeviceSize>(width_)*height_*16;
 if(!size||size>maxBuffer_)throw std::runtime_error("Path tracing viewport exceeds device storage range");
 allocator_->destroyBuffer(history_);history_=allocator_->createBuffer(size,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,false);
 for(auto set:sets_)rhi_->writeDescriptorBuffer({set,5,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,history_.buffer,history_.size});
 samples_=0;++resets_;
}
void PathTracingSceneRenderer::onLoad(const RenderContext& c){
 rhi_=c.rhi;allocator_=c.allocator;scene_=c.scene;
 if(!rhi_||!allocator_||!c.renderSettings)throw std::invalid_argument("Path tracing requires host services");
 settings_=*c.renderSettings;
 VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties(c.physicalDevice,&properties);
 maxBuffer_=properties.limits.maxStorageBufferRange;
 if(properties.limits.maxPerStageDescriptorStorageBuffers<6)throw std::runtime_error("Path tracing requires six storage buffer descriptors");
 unsigned queueCount=0;vkGetPhysicalDeviceQueueFamilyProperties(c.physicalDevice,&queueCount,nullptr);
 std::vector<VkQueueFamilyProperties> queues(queueCount);vkGetPhysicalDeviceQueueFamilyProperties(c.physicalDevice,&queueCount,queues.data());
 if(c.graphicsQueueFamily>=queueCount||!(queues[c.graphicsQueueFamily].queueFlags&VK_QUEUE_COMPUTE_BIT))throw std::runtime_error("Path tracing requires a compute capable graphics queue");
 try{
 EnvironmentImage environment;
 if(!c.environment.path.empty())environment=loadEnvironmentImage(c.environment);
 else{environment.width=environment.height=1;environment.rgba16f={environmentFloatToHalf(.1F),environmentFloatToHalf(.15F),environmentFloatToHalf(.25F),environmentFloatToHalf(1)};}
 rhi::GpuBuffer staging;
 try{
  upload(staging,environment.rgba16f.data(),environment.rgba16f.size()*2,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  environment_=allocator_->createImage2D(environment.width,environment.height,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
  rhi_->transitionImageLayout(environment_,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);
  rhi_->copyBufferToImage(staging,environment_,environment.width,environment.height);
  rhi_->transitionImageLayout(environment_,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1);
 }catch(...){allocator_->destroyBuffer(staging);throw;}
 allocator_->destroyBuffer(staging);
 environmentView_=rhi_->createImageView(environment_.image,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
 rhi::SamplerDesc sampler;sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;environmentSampler_=rhi_->createSampler(sampler);
 std::vector<tracing::Material> materials;std::vector<unsigned> pixels;
 const auto distribution=tracing::environmentDistribution(environment.width,environment.height,environment.rgba16f);
 pixels.resize(distribution.size());std::memcpy(pixels.data(),distribution.data(),distribution.size()*sizeof(float));
 for(const auto& resource:c.scene.resources){
  tracing::Mesh mesh;
  if(c.preparedMeshes&&c.preparedMeshes->count(resource.path))mesh.asset=*c.preparedMeshes->at(resource.path);else mesh.asset=loadGltfAsset(resource.path);
  applyMaterialBindings(mesh.asset,resource.materials);mesh.materialBase=static_cast<unsigned>(materials.size());
  for(const auto& m:mesh.asset.materials){
   tracing::Material material;
   material.albedo=tracing::appendTexture(pixels,m.baseColorPixels,m.baseColorWidth,m.baseColorHeight,true);
   material.surface=tracing::appendTexture(pixels,m.metallicRoughnessPixels,m.metallicRoughnessWidth,m.metallicRoughnessHeight,false);
   material.emission=tracing::appendTexture(pixels,m.specularEmissivePixels,m.specularEmissiveWidth,m.specularEmissiveHeight,true);
   material.normal=tracing::appendTexture(pixels,m.normalPixels,m.normalWidth,m.normalHeight,false);
   material.properties={m.emissiveStrength,static_cast<float>(m.alphaMode),m.alphaCutoff,m.doubleSided?1.F:0.F};materials.push_back(material);
  }
  meshes_.emplace(resource.id,std::move(mesh));
 }
 geometry_=tracing::buildGeometry(scene_,meshes_,{});fingerprint_=tracing::sceneFingerprint(scene_,{});
 upload(materials_,materials.data(),materials.size()*sizeof(tracing::Material),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
 upload(pixels_,pixels.data(),pixels.size()*sizeof(unsigned),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
 std::vector<rhi::DescriptorBindingDesc> bindings{{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT|VK_SHADER_STAGE_FRAGMENT_BIT}};
 for(unsigned i=1;i<=6;++i)bindings.push_back({i,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT|(i==5?VK_SHADER_STAGE_FRAGMENT_BIT:0U)});
 bindings.push_back({7,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT});
 setLayout_=rhi_->createDescriptorSetLayout(bindings);
 pool_=rhi_->createDescriptorPool({{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,c.maxFramesInFlight},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,c.maxFramesInFlight*6},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,c.maxFramesInFlight}},c.maxFramesInFlight});
 sets_=rhi_->allocateDescriptorSets(pool_,setLayout_,c.maxFramesInFlight);slots_.resize(c.maxFramesInFlight);
 layout_=rhi_->createPipelineLayout(setLayout_,nullptr);
 const auto module=[&](const char* file){return rhi_->createShaderModule(readBinaryFile(c.shaderDirectory+"/"+file));};
 auto cs=module("pathtracing.comp.spv");try{compute_=rhi_->createComputePipeline({cs,layout_});}catch(...){rhi_->destroyShaderModule(cs);throw;}rhi_->destroyShaderModule(cs);
 auto vs=module("blackhole.vert.spv"),fs=module("pathtracing.frag.spv");
 rhi::GraphicsPipelineDesc desc{};desc.vertexShader=vs;desc.fragmentShader=fs;desc.cullMode=VK_CULL_MODE_NONE;desc.depthTest=false;desc.depthWrite=false;desc.colorAttachmentCount=2;desc.renderPass=c.sceneRenderPass;desc.layout=layout_;
 try{display_=rhi_->createGraphicsPipeline(desc);}catch(...){rhi_->destroyShaderModule(vs);rhi_->destroyShaderModule(fs);throw;}
 rhi_->destroyShaderModule(vs);rhi_->destroyShaderModule(fs);
 allocateHistory(c);
 for(auto set:sets_){rhi_->writeDescriptorImage({set,7,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,environmentView_,environmentSampler_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});rhi_->writeDescriptorBuffer({set,3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,materials_.buffer,materials_.size});rhi_->writeDescriptorBuffer({set,4,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,pixels_.buffer,pixels_.size});}
 }catch(...){onUnload(c);throw;}
}
void PathTracingSceneRenderer::updateFrame(const SceneFrameData& frame){
 current_=frame.currentFrame;if(frame.renderSettings)settings_=*frame.renderSettings;
 const auto& scene=frame.sceneSnapshot?*frame.sceneSnapshot:scene_;
 state_.pickables.clear();poses_.clear();poses_.reserve(scene.nodes.size());
 const auto worlds=scene::resolveNodeWorldTransforms(scene);
 for(std::size_t i=0;i<scene.nodes.size();++i){const auto& node=scene.nodes[i];const auto found=meshes_.find(node.resourceId);if(!node.visible||found==meshes_.end())continue;
  const auto& asset=found->second.asset;auto pose=bindAnimationPose(asset);std::array<float,2> morph{};
  const auto animation=std::find_if(frame.animations.begin(),frame.animations.end(),[&](const auto& a){return a.node==node.id;});
  if(animation!=frame.animations.end()){pose=sampleAnimationPose(asset,animation->clip,static_cast<float>(animation->time),animation->loop);morph=animation->morph;if(animation->blend<1)pose=blendAnimationPoses(asset,sampleAnimationPose(asset,animation->previousClip,static_cast<float>(animation->previousTime),animation->previousLoop),pose,animation->blend);}
  poses_.push_back(std::move(pose));state_.pickables.push_back({node.id,&asset,worlds[i],asset.hasSkin?&poses_.back():nullptr,morph});
 }
 state_.asset=state_.pickables.empty()?nullptr:state_.pickables.front().asset;
 state_.modelMatrix=state_.pickables.empty()?nullptr:state_.pickables.front().model.data();
 state_.primitiveCount=state_.asset?state_.asset->primitives.size():0;
 const auto hash=tracing::sceneFingerprint(scene,frame.animations);
 if(hash!=fingerprint_){geometry_=tracing::buildGeometry(scene,meshes_,frame.animations);fingerprint_=hash;++generation_;samples_=0;++resets_;}
 Uniform next{};const auto& camera=frame.camera.view;
 const auto forward=normalize(subtract(camera.target,camera.position)),right=normalize(cross(forward,camera.up)),up=cross(right,forward);
 const bool ortho=camera.projection==CameraProjectionKind::Orthographic;
 const float scale=ortho?camera.orthoHeight*.5F:std::tan(camera.fov*.5F);
 for(unsigned i=0;i<3;++i){next.origin[i]=camera.position[i];next.forward[i]=forward[i];next.right[i]=right[i]*scale*static_cast<float>(width_)/static_cast<float>(height_);next.up[i]=up[i]*scale;next.skyColor[i]=settings_.sky.color[i];}
 next.origin[3]=ortho?1.F:0.F;next.forward[3]=camera.nearPlane;next.up[3]=camera.farPlane;
 next.sky={static_cast<float>(settings_.sky.mode),settings_.sky.intensity,settings_.sky.rotationDegrees*.01745329252F,0};
 next.counts={static_cast<unsigned>(geometry_.triangles.size()),static_cast<unsigned>(geometry_.nodes.size()),static_cast<unsigned>(geometry_.lights.size()),settings_.pathTraceBounces};
 next.image={width_,height_,0,settings_.pathTraceSamples};
 auto previous=uniform_;previous.image[2]=0;
 if(std::memcmp(&previous,&next,sizeof(next))!=0){samples_=0;++resets_;}
 uniform_=next;uniform_.image[2]=samples_;
 auto& slot=slots_.at(current_);upload(slot.uniform,&uniform_,sizeof(uniform_),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
 rhi_->writeDescriptorBuffer({sets_[current_],0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,slot.uniform.buffer,sizeof(uniform_)});
 if(slot.generation!=generation_){
  upload(slot.triangles,geometry_.triangles.data(),geometry_.triangles.size()*sizeof(tracing::Triangle),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  upload(slot.nodes,geometry_.nodes.data(),geometry_.nodes.size()*sizeof(tracing::BvhNode),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  upload(slot.lights,geometry_.lights.data(),geometry_.lights.size()*sizeof(tracing::Light),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  rhi_->writeDescriptorBuffer({sets_[current_],1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,slot.triangles.buffer,slot.triangles.size});
  rhi_->writeDescriptorBuffer({sets_[current_],2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,slot.nodes.buffer,slot.nodes.size});
  rhi_->writeDescriptorBuffer({sets_[current_],6,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,slot.lights.buffer,slot.lights.size});slot.generation=generation_;
 }
}
void PathTracingSceneRenderer::recordScene(const RenderContext& c){
 auto& cmd=*c.commands;
 if(samples_<settings_.pathTraceSamples){
  cmd.bufferBarrier({history_.buffer,0,VK_WHOLE_SIZE,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT});
  cmd.bindComputePipeline(compute_);cmd.bindComputeDescriptorSet(layout_,sets_.at(current_));cmd.dispatch((width_+7)/8,(height_+7)/8,1);
  cmd.bufferBarrier({history_.buffer,0,VK_WHOLE_SIZE,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});++samples_;
 }
 state_.accumulatedSamples=samples_;state_.targetSamples=settings_.pathTraceSamples;
 if(c.shadowRenderPass&&c.shadowFramebuffer){VkClearValue clear{};clear.depthStencil={1,0};cmd.beginRenderPass({c.shadowRenderPass,c.shadowFramebuffer,{c.shadowMapSize,c.shadowMapSize},{clear}});cmd.endRenderPass();}
 if(c.gpuTimingEnabled)cmd.writeTimestamp(c.timestampQueryPool,1,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
 VkClearValue color{},depth{},normal{};depth.depthStencil={1,0};normal.color={{.5F,.5F,1,0}};
 cmd.beginRenderPass({c.sceneRenderPass,c.sceneFramebuffer,c.renderExtent,{color,depth,normal}});
 cmd.setViewport(static_cast<float>(width_),static_cast<float>(height_));cmd.setScissor(c.renderExtent);cmd.bindPipeline(display_);cmd.bindDescriptorSet(layout_,sets_.at(current_));cmd.draw(3);cmd.endRenderPass();
 if(c.gpuTimingEnabled)cmd.writeTimestamp(c.timestampQueryPool,2,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}
void PathTracingSceneRenderer::onSwapchainRecreate(const RenderContext& c){allocateHistory(c);}
void PathTracingSceneRenderer::appendHudText(std::ostringstream& out)const{out<<"Path tracing: "<<samples_<<" / "<<settings_.pathTraceSamples<<" samples, "<<settings_.pathTraceBounces<<" bounces\n";}
void PathTracingSceneRenderer::appendCaptureManifestFields(std::ostream& out)const{out<<"  \"pathTracingSamples\": "<<samples_<<",\n  \"pathTracingResets\": "<<resets_<<",\n";}
void PathTracingSceneRenderer::onUnload(const RenderContext&){
 if(!rhi_||!allocator_)return;
 rhi_->destroyPipeline(display_);rhi_->destroyPipeline(compute_);rhi_->destroyPipelineLayout(layout_);rhi_->destroyDescriptorPool(pool_);rhi_->destroyDescriptorSetLayout(setLayout_);
 display_=compute_=VK_NULL_HANDLE;layout_=VK_NULL_HANDLE;pool_=VK_NULL_HANDLE;setLayout_=VK_NULL_HANDLE;
 for(auto& s:slots_){allocator_->destroyBuffer(s.uniform);allocator_->destroyBuffer(s.triangles);allocator_->destroyBuffer(s.nodes);allocator_->destroyBuffer(s.lights);}slots_.clear();sets_.clear();
 rhi_->destroySampler(environmentSampler_);rhi_->destroyImageView(environmentView_);environmentSampler_=VK_NULL_HANDLE;environmentView_=VK_NULL_HANDLE;allocator_->destroyImage(environment_);
 allocator_->destroyBuffer(history_);allocator_->destroyBuffer(materials_);allocator_->destroyBuffer(pixels_);meshes_.clear();
}
}
