#include "render/SurfaceRadianceCache.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <cstring>
#include <iostream>
using namespace azurerender;
namespace {
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
constexpr VkExtent2D extent{32,24};
struct Probe{float depth=.25F,value=1000.03125F;std::uint32_t firstIndex=123,material=19;};
struct Resources{
 test::VulkanComputeFixture& host;rhi::IRhi& rhi;
 std::unique_ptr<SurfaceRadianceCache> source,target;
 rhi::GpuImage depth{};VkImageView depthView{};
 VkRenderPass pass{};std::array<VkFramebuffer,3> frames{};
 VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkPipelineLayout layout{};
 VkShaderModule vertex{},producer{},consumer{};VkPipeline produce{},consume{};
 rhi::GpuBuffer readValues{},readIds{};VkDescriptorSet set{};
 explicit Resources(test::VulkanComputeFixture& h):host(h),rhi(*h.backend){}
 ~Resources(){
  for(auto frame:frames)if(frame)rhi.destroyFramebuffer(frame);
  if(produce)rhi.destroyPipeline(produce);if(consume)rhi.destroyPipeline(consume);
  if(vertex)rhi.destroyShaderModule(vertex);if(producer)rhi.destroyShaderModule(producer);if(consumer)rhi.destroyShaderModule(consumer);
  if(pool)rhi.destroyDescriptorPool(pool);if(layout)rhi.destroyPipelineLayout(layout);if(setLayout)rhi.destroyDescriptorSetLayout(setLayout);
  if(pass)rhi.destroyRenderPass(pass);
  if(depthView)rhi.destroyImageView(depthView);host.allocator.destroyImage(depth);
  host.allocator.destroyBuffer(readValues);host.allocator.destroyBuffer(readIds);
 }
 void initialize(const std::string& shaders){
  source=std::make_unique<SurfaceRadianceCache>(rhi,extent,2);target=std::make_unique<SurfaceRadianceCache>(rhi,extent,1);
  depth=host.allocator.createImage2D(extent.width,extent.height,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  depthView=rhi.createImageView(depth.image,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT,1);
  rhi::RenderPassDesc desc;
  for(unsigned kind=0;kind<4;++kind)desc.attachments.push_back({SurfaceRadianceCache::format(kind),VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false});
  desc.attachments.push_back({VK_FORMAT_D32_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,true,true,true});desc.depthAttachment=4;desc.externalReadDependency=true;
  pass=rhi.createRenderPass(desc);
  for(unsigned i=0;i<3;++i){const auto& cache=i==2?*target:*source;const auto slot=i==2?0:i;
   frames[i]=rhi.createFramebuffer({pass,{cache.texture(slot,0).view,cache.texture(slot,1).view,cache.texture(slot,2).view,cache.texture(slot,3).view,depthView},extent.width,extent.height});}
  std::vector<rhi::DescriptorBindingDesc> bindings;for(unsigned kind=0;kind<4;++kind)bindings.push_back({38+kind,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT});
  setLayout=rhi.createDescriptorSetLayout(bindings);
  rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,sizeof(Probe)};layout=rhi.createPipelineLayout(setLayout,&range);
  rhi::DescriptorPoolDesc poolDesc;poolDesc.maxSets=1;poolDesc.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,4}};pool=rhi.createDescriptorPool(poolDesc);
  set=rhi.allocateDescriptorSets(pool,setLayout,1).at(0);
  vertex=rhi.createShaderModule(readBinaryFile(shaders+"/surface_radiance_probe.vert.spv"));producer=rhi.createShaderModule(readBinaryFile(shaders+"/surface_radiance_probe.frag.spv"));
  consumer=rhi.createShaderModule(readBinaryFile(shaders+"/surface_radiance_consumer.frag.spv"));
  rhi::GraphicsPipelineDesc pipeline;pipeline.vertexShader=vertex;pipeline.fragmentShader=producer;pipeline.layout=layout;pipeline.renderPass=pass;
  pipeline.colorAttachmentCount=4;pipeline.cullMode=VK_CULL_MODE_NONE;pipeline.depthCompare=VK_COMPARE_OP_LESS;
  produce=rhi.createGraphicsPipeline(pipeline);pipeline.fragmentShader=consumer;consume=rhi.createGraphicsPipeline(pipeline);
  readValues=host.allocator.createBuffer(extent.width*extent.height*16,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
  readIds=host.allocator.createBuffer(extent.width*extent.height*16,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
 }
 void draw(unsigned frame,Probe probe,unsigned instance,unsigned vertices,bool sampled=false){
  host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
   VkClearValue clearDepth{};clearDepth.depthStencil={1,0};
   commands.beginRenderPass({pass,frames.at(frame),extent,{SurfaceRadianceCache::clearValue(0),SurfaceRadianceCache::clearValue(1),SurfaceRadianceCache::clearValue(2),SurfaceRadianceCache::clearValue(3),clearDepth}});
   commands.setViewport(float(extent.width),float(extent.height));commands.setScissor(extent);commands.bindPipeline(sampled?consume:produce);
   if(sampled)commands.bindDescriptorSet(layout,set);
   commands.pushConstants(layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,&probe,sizeof(probe));
   if(vertices)vkCmdDraw(raw,vertices,1,0,instance);commands.endRenderPass();
  });
 }
 void bind(unsigned slot){for(unsigned kind=0;kind<4;++kind)rhi.writeDescriptorImage({set,38+kind,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
  source->texture(slot,kind).view,source->sampler(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});}
 void read(const SurfaceRadianceCache& cache,unsigned slot,float expected,const SurfaceRadianceIdentity* identity=nullptr){
  host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){for(unsigned kind=0;kind<(identity?2U:1U);++kind){
   const auto image=kind?cache.texture(slot,3).image.image:cache.texture(slot,0).image.image;
   const auto buffer=kind?readIds:readValues;
   commands.imageBarrier({image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
   commands.copyImageToBuffer(image,buffer.buffer,extent);
   commands.bufferBarrier({buffer.buffer,0,buffer.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
   commands.imageBarrier({image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT});
  }});
  host.allocator.invalidate(readValues,0,readValues.size);if(identity)host.allocator.invalidate(readIds,0,readIds.size);
  for(unsigned i=0;i<extent.width*extent.height;++i){require(static_cast<const float*>(readValues.mapped)[4*i]==expected,"GPU radiance precision/composition mismatch");
   if(identity)require(std::memcmp(static_cast<const std::uint32_t*>(readIds.mapped)+4*i,identity->words.data(),16)==0,"GPU surface identity mismatch");}
 }
};
}
int main(int argc,char** argv)try{
 require(argc==2,"Expected shader directory");test::VulkanComputeFixture host(true,false,false,true,true);
 if(!SurfaceRadianceCache::supported(*host.backend,extent,2))return 77;
 {Resources r(host);r.initialize(argv[1]);Probe p;
  r.draw(0,p,7,6);auto identity=surfaceRadianceIdentity(.25F,7,123,0,19);r.read(*r.source,0,1000.03125F,&identity);
  r.draw(1,p,7,0);r.read(*r.source,1,0);r.bind(0);
  p.value=.75F;r.draw(2,p,7,3,true);r.read(*r.target,0,1000.53125F);
  r.draw(2,p,8,3,true);r.read(*r.target,0,.75F);
  p.depth=.5F;r.draw(2,p,7,3,true);r.read(*r.target,0,.75F);p.depth=.25F;
  p.firstIndex+=3;r.draw(2,p,7,3,true);r.read(*r.target,0,.75F);p.firstIndex-=3;
  ++p.material;r.draw(2,p,7,3,true);r.read(*r.target,0,.75F);--p.material;
  r.bind(1);r.draw(2,p,7,3,true);r.read(*r.target,0,.75F);
  p.value=.375F;r.draw(0,p,8,3);r.bind(0);p.value=.75F;r.draw(2,p,7,3,true);r.read(*r.target,0,.75F);
  p.value=-2.F;r.draw(0,p,7,3);r.bind(0);p.value=.75F;r.draw(2,p,7,3,true);r.read(*r.target,0,.75F);
 }
 require(host.allocator.statistics().liveBuffers==0&&host.allocator.statistics().liveImages==0,"GPU resources must retire");require(host.errors==0,"Vulkan validation must remain clean");
 std::cout<<"Radiance cache GPU identity, coplanar winner, sampled hits, misses and frame slots passed on "<<host.properties.deviceName<<'\n';
}catch(const test::VulkanFeatureUnavailable& e){std::cout<<e.what()<<"; experimental GPU path unavailable\n";return 77;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
