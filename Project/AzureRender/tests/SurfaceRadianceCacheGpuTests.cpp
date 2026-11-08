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
 rhi::GpuImage copySource{};VkImageView copySourceView{};rhi::GpuBuffer copyUpload{},copyRead{};
 VkRenderPass copyPass{};VkFramebuffer copyFrame{};VkPipeline copyPipeline{};VkShaderModule copyVertex{},copyFragment{};
 bool diagnostics=false,selection=false;VkPipeline validate{},commit{};VkShaderModule validation{},commitModule{};
 explicit Resources(test::VulkanComputeFixture& h,bool statistics=false,bool frameSelection=false):host(h),rhi(*h.backend),diagnostics(statistics),selection(frameSelection){}
 ~Resources(){
  if(copyFrame)rhi.destroyFramebuffer(copyFrame);if(copyPipeline)rhi.destroyPipeline(copyPipeline);
  if(copyVertex)rhi.destroyShaderModule(copyVertex);if(copyFragment)rhi.destroyShaderModule(copyFragment);
  if(copyPass)rhi.destroyRenderPass(copyPass);if(copySourceView)rhi.destroyImageView(copySourceView);
  host.allocator.destroyImage(copySource);host.allocator.destroyBuffer(copyUpload);host.allocator.destroyBuffer(copyRead);
  for(auto frame:frames)if(frame)rhi.destroyFramebuffer(frame);
  if(validate)rhi.destroyPipeline(validate);if(validation)rhi.destroyShaderModule(validation);
  if(commit)rhi.destroyPipeline(commit);if(commitModule)rhi.destroyShaderModule(commitModule);
  if(produce)rhi.destroyPipeline(produce);if(consume)rhi.destroyPipeline(consume);
  if(vertex)rhi.destroyShaderModule(vertex);if(producer)rhi.destroyShaderModule(producer);if(consumer)rhi.destroyShaderModule(consumer);
  if(pool)rhi.destroyDescriptorPool(pool);if(layout)rhi.destroyPipelineLayout(layout);if(setLayout)rhi.destroyDescriptorSetLayout(setLayout);
  if(pass)rhi.destroyRenderPass(pass);
  if(depthView)rhi.destroyImageView(depthView);host.allocator.destroyImage(depth);
  host.allocator.destroyBuffer(readValues);host.allocator.destroyBuffer(readIds);
 }
 void initialize(const std::string& shaders,bool withConsumer=true){
  source=std::make_unique<SurfaceRadianceCache>(rhi,extent,2,SurfaceRadianceCache::kMaximumBytes,diagnostics,selection);target=std::make_unique<SurfaceRadianceCache>(rhi,extent,1);
  depth=host.allocator.createImage2D(extent.width,extent.height,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  depthView=rhi.createImageView(depth.image,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT,1);
  rhi::RenderPassDesc desc;
  for(unsigned kind=0;kind<4;++kind)desc.attachments.push_back({SurfaceRadianceCache::format(kind),VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false});
  desc.attachments.push_back({VK_FORMAT_D32_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,true,true,true});desc.depthAttachment=4;desc.externalReadDependency=true;
  pass=rhi.createRenderPass(desc);
  for(unsigned i=0;i<3;++i){const auto& cache=i==2?*target:*source;const auto slot=i==2?0:i;
   frames[i]=rhi.createFramebuffer({pass,{cache.texture(slot,0).view,cache.texture(slot,1).view,cache.texture(slot,2).view,cache.texture(slot,3).view,depthView},extent.width,extent.height});}
  std::vector<rhi::DescriptorBindingDesc> bindings;for(unsigned kind=0;kind<4;++kind)bindings.push_back({38+kind,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT});
  if(diagnostics)bindings.push_back({42,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT});
  if(selection){bindings.push_back({43,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT|VK_SHADER_STAGE_COMPUTE_BIT});bindings.push_back({44,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT});}
  setLayout=rhi.createDescriptorSetLayout(bindings);
  rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,sizeof(Probe)};layout=rhi.createPipelineLayout(setLayout,&range);
  rhi::DescriptorPoolDesc poolDesc;poolDesc.maxSets=1;poolDesc.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,selection?5U:4U}};if(diagnostics||selection)poolDesc.sizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,(diagnostics?1U:0U)+(selection?1U:0U)});pool=rhi.createDescriptorPool(poolDesc);
  set=rhi.allocateDescriptorSets(pool,setLayout,1).at(0);
  if(diagnostics){const auto& counter=source->statisticsBuffer(0);rhi.writeDescriptorBuffer({set,42,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,counter.buffer,counter.size});}
  if(selection){const auto& flag=source->selectionBuffer(0);rhi.writeDescriptorBuffer({set,43,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,flag.buffer,4});}
  const std::string suffix=diagnostics?"_stats":"";
  vertex=rhi.createShaderModule(readBinaryFile(shaders+"/surface_radiance_probe.vert.spv"));producer=rhi.createShaderModule(readBinaryFile(shaders+"/surface_radiance_probe"+suffix+".frag.spv"));
  if(withConsumer)consumer=rhi.createShaderModule(readBinaryFile(shaders+"/surface_radiance_consumer"+suffix+".frag.spv"));
  rhi::GraphicsPipelineDesc pipeline;pipeline.vertexShader=vertex;pipeline.fragmentShader=producer;pipeline.layout=layout;pipeline.renderPass=pass;
  pipeline.colorAttachmentCount=4;pipeline.cullMode=VK_CULL_MODE_NONE;pipeline.depthCompare=VK_COMPARE_OP_LESS;
  produce=rhi.createGraphicsPipeline(pipeline);pipeline.fragmentShader=consumer;if(withConsumer)consume=rhi.createGraphicsPipeline(pipeline);
  if(selection){validation=rhi.createShaderModule(readBinaryFile(shaders+"/surface_radiance_validation_probe.frag.spv"));
   pipeline.fragmentShader=validation;pipeline.depthWrite=true;pipeline.colorWriteMasks.assign(4,0);validate=rhi.createGraphicsPipeline(pipeline);
   commitModule=rhi.createShaderModule(readBinaryFile(shaders+"/surface_radiance_selection_commit.comp.spv"));commit=rhi.createComputePipeline({commitModule,layout});}

  readValues=host.allocator.createBuffer(extent.width*extent.height*16,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
  readIds=host.allocator.createBuffer(extent.width*extent.height*16,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
 }
 void draw(unsigned frame,Probe probe,unsigned instance,unsigned vertices,bool sampled=false,bool validateOnly=false){
  host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
   if(diagnostics){const auto& counter=source->statisticsBuffer(0);commands.bufferBarrier({counter.buffer,0,counter.size,
    VK_PIPELINE_STAGE_HOST_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    VK_ACCESS_HOST_WRITE_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT});}
   if(selection){const auto& flag=source->selectionBuffer(0);commands.bufferBarrier({flag.buffer,0,4,
    VK_PIPELINE_STAGE_HOST_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    VK_ACCESS_HOST_WRITE_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT});}
   VkClearValue clearDepth{};clearDepth.depthStencil={1,0};
   commands.beginRenderPass({pass,frames.at(frame),extent,{SurfaceRadianceCache::clearValue(0),SurfaceRadianceCache::clearValue(1),SurfaceRadianceCache::clearValue(2),SurfaceRadianceCache::clearValue(3),clearDepth}});
   commands.setViewport(float(extent.width),float(extent.height));commands.setScissor(extent);commands.bindPipeline(validateOnly?validate:sampled?consume:produce);
   if(sampled||diagnostics||validateOnly)commands.bindDescriptorSet(layout,set);
   commands.pushConstants(layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,&probe,sizeof(probe));
   if(vertices)vkCmdDraw(raw,vertices,1,0,instance);commands.endRenderPass();
   if(diagnostics)source->recordStatisticsReady(commands,0);
   if(selection&&validateOnly){const auto& flag=source->selectionBuffer(0);commands.bufferBarrier({flag.buffer,0,4,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT});
    commands.bindComputePipeline(commit);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(1,1,1);source->recordSelectionReady(commands,0);}
  });
 }
 void verifyDepthCopy(const std::string& shaders){
  const auto bytes=extent.width*extent.height*4;
  copySource=host.allocator.createImage2D(extent.width,extent.height,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  copySourceView=rhi.createImageView(copySource.image,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT,1);
  copyUpload=host.allocator.createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);copyRead=host.allocator.createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
  rhi::RenderPassDesc desc;desc.depthAttachment=0;desc.attachments={{VK_FORMAT_D32_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,true,true,true}};
  copyPass=rhi.createRenderPass(desc);copyFrame=rhi.createFramebuffer({copyPass,{depthView},extent.width,extent.height});
  copyVertex=rhi.createShaderModule(readBinaryFile(shaders+"/surface_radiance_depth_copy.vert.spv"));copyFragment=rhi.createShaderModule(readBinaryFile(shaders+"/surface_radiance_depth_copy.frag.spv"));
  rhi::GraphicsPipelineDesc pipeline;pipeline.vertexShader=copyVertex;pipeline.fragmentShader=copyFragment;pipeline.layout=layout;pipeline.renderPass=copyPass;
  pipeline.cullMode=VK_CULL_MODE_NONE;pipeline.depthCompare=VK_COMPARE_OP_ALWAYS;copyPipeline=rhi.createGraphicsPipeline(pipeline);
  rhi.writeDescriptorImage({set,44,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,copySourceView,source->sampler(),VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL});
  const std::array<std::uint32_t,16> patterns{0x00800000,0x01000000,0x33000000,0x3e800000,0x3effffff,0x3f000000,0x3f000001,0x3f800000,0,1,0x007fffff,0x7f800000,0x7fc00001,0xbf800000,0x3f800001,0x80000000};
  for(unsigned flow=0;flow<2;++flow){
   auto* data=static_cast<std::uint32_t*>(copyUpload.mapped);for(unsigned i=0;i<bytes/4;++i)data[i]=patterns[i%(flow?16:8)];
   host.allocator.flush(copyUpload,0,bytes);source->beginSelectionFrame(0,true);
   host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
    commands.bufferBarrier({copyUpload.buffer,0,bytes,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
    commands.imageBarrier({copySource.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
    VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};region.imageExtent={extent.width,extent.height,1};
    vkCmdCopyBufferToImage(raw,copyUpload.buffer,copySource.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);
    commands.imageBarrier({copySource.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
    const auto& flag=source->selectionBuffer(0);commands.bufferBarrier({flag.buffer,0,4,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
    VkClearValue clear{};clear.depthStencil={1,0};commands.beginRenderPass({copyPass,copyFrame,extent,{clear}});commands.setViewport(float(extent.width),float(extent.height));commands.setScissor(extent);
    commands.bindPipeline(copyPipeline);commands.bindDescriptorSet(layout,set);commands.draw(3);commands.endRenderPass();
    commands.imageBarrier({depth.image,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
    vkCmdCopyImageToBuffer(raw,depth.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,copyRead.buffer,1,&region);
    commands.bufferBarrier({copyRead.buffer,0,bytes,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
    commands.bufferBarrier({flag.buffer,0,4,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
   });
   host.allocator.invalidate(copyRead,0,bytes);const auto* actual=static_cast<const std::uint32_t*>(copyRead.mapped);
   for(unsigned i=0;i<bytes/4;++i){const auto expected=i%(flow?16:8)<8?data[i]:0x3f800000U;require(actual[i]==expected,"Depth replay must preserve normal D32 bits and reject unsafe values");}
   const auto& flag=source->selectionBuffer(0);host.allocator.invalidate(flag,0,4);std::uint32_t value=0;std::memcpy(&value,flag.mapped,4);require(value==(flow?9U:0U),"Unsafe depth must request complete shading");
  }
 }
 void verifyDrawReduction(const std::string& shaders){
  SurfaceRadianceCache draws(rhi,extent,1,SurfaceRadianceCache::kMaximumBytes,false,true,8);
  const auto module=rhi.createShaderModule(readBinaryFile(shaders+"/surface_radiance_draw_commit.comp.spv"));const auto pipeline=rhi.createComputePipeline({module,layout});
  auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){rhi.destroyPipeline(pipeline);rhi.destroyShaderModule(module);});
  const auto& buffer=draws.selectionBuffer(0);rhi.writeDescriptorBuffer({set,43,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,buffer.buffer,buffer.size});
  for(unsigned flow=0;flow<2;++flow){
   draws.beginSelectionFrame(0,true,8);auto* words=static_cast<std::uint32_t*>(buffer.mapped);words[0]=flow?9U:0U;
   const std::array<std::uint32_t,8> flags{22,31,0,46,55,0,70,79};std::memcpy(words+6,flags.data(),32);host.allocator.flush(buffer,0,buffer.size);
   host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
    commands.bufferBarrier({buffer.buffer,0,buffer.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT});
    commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(1,1,1);draws.recordSelectionReady(commands,0);
   });
   draws.beginSelectionFrame(0);const auto stats=draws.selectionStatistics();
   require(stats.lastFastDraws==(flow?0U:3U)&&stats.lastFullDraws==(flow?6U:3U)&&stats.lastFallback==bool(flow)&&stats.lastGlobalReasons==(flow?8U:0U)&&stats.lastFastWitnesses==(flow?0U:33U)&&stats.lastFullWitnesses==(flow?72U:39U),"GPU reduction preserves mixed and global fallback counts");
  }
  const auto& original=source->selectionBuffer(0);rhi.writeDescriptorBuffer({set,43,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,original.buffer,original.size});
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
 require(argc==2,"Expected shader directory");
 {test::VulkanComputeFixture graphics(true,false,false,true,false,AZURE_TEST_SURFACE_RADIANCE_FRAME_SELECTION!=0);{
  Resources r(graphics);r.initialize(argv[1],false);
  for(const auto mode:{rhi::PrimaryColorBlend::Alpha,rhi::PrimaryColorBlend::Additive,rhi::PrimaryColorBlend::Multiply}){
   rhi::GraphicsPipelineDesc desc;desc.vertexShader=r.vertex;desc.fragmentShader=r.producer;desc.layout=r.layout;desc.renderPass=r.pass;
   desc.colorAttachmentCount=4;desc.colorWriteMasks.assign(4,0);desc.alphaBlend=true;desc.primaryColorBlend=mode;
   const auto pipeline=r.rhi.createGraphicsPipeline(desc),original=r.produce;r.produce=pipeline;
   try{r.draw(0,Probe{},7,3);r.read(*r.source,0,0);}catch(...){r.produce=original;r.rhi.destroyPipeline(pipeline);throw;}
   r.produce=original;r.rhi.destroyPipeline(pipeline);
  }
 }
#if AZURE_TEST_SURFACE_RADIANCE_FRAME_SELECTION
 {Resources r(graphics,false,true);r.initialize(argv[1],false);r.verifyDepthCopy(argv[1]);r.verifyDrawReduction(argv[1]);}
 std::cout<<"D32 precision, unsafe fallback and per-draw GPU retirement passed on "<<graphics.properties.deviceName<<'\n';
#endif
 require(graphics.errors==0,"Masked integer attachment blending must satisfy Vulkan");
 require(!graphics.allocator.statistics().liveBuffers&&!graphics.allocator.statistics().liveImages,"Masked blending must retire resources");
 std::cout<<"Masked integer attachment blending passed on "<<graphics.properties.deviceName<<'\n';}
 test::VulkanComputeFixture host(true,false,false,true,true,(AZURE_TEST_SURFACE_RADIANCE_DIAGNOSTICS||AZURE_TEST_SURFACE_RADIANCE_FRAME_SELECTION)!=0);
 if(!SurfaceRadianceCache::supported(*host.backend,extent,2))return 77;
 {Resources r(host);r.initialize(argv[1]);Probe p;
  for(const auto mode:{rhi::PrimaryColorBlend::Alpha,rhi::PrimaryColorBlend::Additive,rhi::PrimaryColorBlend::Multiply}){
   rhi::GraphicsPipelineDesc blend;blend.vertexShader=r.vertex;blend.fragmentShader=r.producer;blend.layout=r.layout;blend.renderPass=r.pass;
   blend.colorAttachmentCount=4;blend.colorWriteMasks={0xf,0,0,0};blend.alphaBlend=true;blend.primaryColorBlend=mode;
   const auto pipeline=r.rhi.createGraphicsPipeline(blend);r.rhi.destroyPipeline(pipeline);
  }

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
#if AZURE_TEST_SURFACE_RADIANCE_DIAGNOSTICS
 {Resources r(host,true);r.initialize(argv[1]);Probe p;r.bind(0);r.source->beginStatisticsFrame(0);
  r.draw(0,p,7,3);p.value=.75F;r.draw(2,p,7,3,true);r.source->beginStatisticsFrame(0);
  const auto full=r.source->statistics();const auto pixels=extent.width*extent.height;
  require(full.frames==1&&full.producerStores==pixels&&full.consumerVisits==pixels&&full.eligibleRequests==pixels
   &&full.pixelMatches==pixels&&full.quadMatches==pixels&&!full.overflow,"GPU full-hit statistics are exact");
  p.value=-2.F;r.draw(0,p,7,3);p.value=.75F;r.draw(2,p,7,3,true);r.source->beginStatisticsFrame(0);
  const auto mixed=r.source->statistics();require(mixed.frames==2&&mixed.pixelMatches-full.pixelMatches==pixels/2
   &&mixed.quadMatches==full.quadMatches&&mixed.consumerVisits-full.consumerVisits==pixels,"GPU partial-quad statistics distinguish pixel matches from acceptance");
  auto& counter=r.source->statisticsBuffer(0);std::array<std::uint32_t,6> saturated{UINT32_MAX,0,0,0,0,0};
  std::memcpy(counter.mapped,saturated.data(),24);host.allocator.flush(counter,0,24);p.value=1;r.draw(0,p,7,3);r.source->beginStatisticsFrame(0);
  require(r.source->statistics().overflow,"GPU counter overflow is observable");
 }
#endif
#if AZURE_TEST_SURFACE_RADIANCE_FRAME_SELECTION
 {Resources r(host,false,true);r.initialize(argv[1]);Probe p;r.bind(0);
  r.source->beginSelectionFrame(0,true);r.draw(0,p,7,3);r.draw(2,p,7,3,false,true);
  r.source->beginSelectionFrame(0,true);auto selected=r.source->selectionStatistics();
  require(selected.frames==1&&selected.fastFrames==1&&!selected.lastFallback,"Actual matching fragments preserve the fast predicate");
  p.value=-2;r.draw(0,p,7,3);r.draw(2,p,7,3,false,true);
  r.source->beginSelectionFrame(0,true);selected=r.source->selectionStatistics();
  require(selected.frames==2&&selected.fullFrames==1&&selected.lastFallback,"Any real identity mismatch requests complete shading");
  p.value=0;r.draw(0,p,7,0);r.draw(2,p,7,3,false,true);
  r.source->beginSelectionFrame(0);selected=r.source->selectionStatistics();
  require(selected.fullFrames==2&&selected.lastFallback,"Missing production is fail closed");
  r.source->beginSelectionFrame(0,true);p.value=1000.03125F;r.draw(0,p,7,6);r.draw(2,p,7,6,false,true);
  r.source->beginSelectionFrame(0);selected=r.source->selectionStatistics();
  require(selected.fastFrames==2&&selected.fullFrames==2&&!selected.lastFallback,"Depth-writing validation preserves the visible coplanar winner");

 }
#endif
 require(host.allocator.statistics().liveBuffers==0&&host.allocator.statistics().liveImages==0,"GPU resources must retire");require(host.errors==0,"Vulkan validation must remain clean");
 std::cout<<"Radiance cache GPU identity, coplanar winner, sampled hits, misses and frame slots passed on "<<host.properties.deviceName<<'\n';
}catch(const test::VulkanFeatureUnavailable& e){std::cout<<e.what()<<"; experimental GPU path unavailable\n";return 77;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
