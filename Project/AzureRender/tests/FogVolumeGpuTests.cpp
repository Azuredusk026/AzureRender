#include "render/FogVolumeGpu.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include "support/FogViewProjectionFixture.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <Imath/half.h>

using namespace azurerender;
int main(int argc,char** argv)try{
 if(argc<2||argc>5)throw std::invalid_argument("Expected test shader, fog volume and optional sampler or projection oracle");
 const bool projectionMode=argc==5&&std::string(argv[3])=="--projection";
 if(argc==5&&!projectionMode)throw std::invalid_argument("Unknown fog verification mode");
 FogVolumeData data;
 if(argc>=3)data=FogVolumeData::load(std::filesystem::u8path(argv[2]));
 else {
  data.dimensions={2,2,2};
  for(unsigned z=0;z<2;++z)for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x)
   for(auto bits:{std::uint16_t(x?0x3c00:0),std::uint16_t(y?0x4000:0),std::uint16_t(z?0x4200:0),std::uint16_t(x?0x3800:0x3c00)})data.rgba16f.push_back(bits);
 }
 std::vector<std::array<float,4>> positions={{0,0,0,0},{1,1,1,0},{.25F,.25F,.25F,0},{.75F,.25F,.25F,0},
  {.5F,.5F,.5F,0},{.375F,.625F,.5F,0},{-1,2,3,0}};
 FogViewProjection projection;std::vector<std::array<float,4>> projectedOracle;
 test::FogProjectionFixture projectionFixture;
 if(projectionMode){projectionFixture=test::readFogProjectionFixture(std::filesystem::u8path(argv[4]));projection=projectionFixture.profile;
  positions.push_back({0,0,0,1});positions.push_back({float(projection.extent[0]/2),float(projection.extent[1]/2),projection.minimumViewDepth+projection.nearFadeWidth*.5F,1});}
 const auto ordinaryCount=positions.size();std::vector<std::array<float,4>> oracle;
 if(projectionMode)for(const auto& sample:projectionFixture.samples){positions.push_back({float(sample.pixel[0]),float(sample.pixel[1]),sample.viewDepth,1});oracle.push_back(sample.rgba);projectedOracle.push_back(sample.projected);}
 if(argc==4){std::ifstream file(std::filesystem::u8path(argv[3]));if(!file)throw std::invalid_argument("Cannot open native sampler oracle");
  std::array<float,4> position{},rgba{};while(file>>position[0]>>position[1]>>position[2]>>rgba[0]>>rgba[1]>>rgba[2]>>rgba[3]){
   if(oracle.size()>=64)throw std::invalid_argument("Native fog oracle exceeds its sample budget");positions.push_back(position);oracle.push_back(rgba);}
  if(!file.eof()||oracle.empty())throw std::invalid_argument("Native sampler oracle is empty or malformed");}
 test::VulkanComputeFixture host;auto& backend=*host.backend;rhi::GpuBuffer input{},output{},parameters{};
 VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
 auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
  if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);if(layout)backend.destroyPipelineLayout(layout);
  if(pool)backend.destroyDescriptorPool(pool);if(setLayout)backend.destroyDescriptorSetLayout(setLayout);host.allocator.destroyBuffer(input);host.allocator.destroyBuffer(output);host.allocator.destroyBuffer(parameters);
 });
 float maximum=0,fullPrecisionDifference=0;
 {
  FogVolumeGpu volume(backend,data);input=host.allocator.createBuffer(positions.size()*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
  output=host.allocator.createBuffer(positions.size()*64,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
  const auto packed=projection.pack();parameters=host.allocator.createBuffer(sizeof(packed),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);std::memcpy(parameters.mapped,&packed,sizeof(packed));host.allocator.flush(parameters,0,parameters.size);
  std::memcpy(input.mapped,positions.data(),std::size_t(input.size));host.allocator.flush(input,0,input.size);
  setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},
   {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
  rhi::DescriptorPoolDesc poolDesc;poolDesc.maxSets=1;poolDesc.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3}};
  pool=backend.createDescriptorPool(poolDesc);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();volume.writeTexture(set,0);
  backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,input.size});backend.writeDescriptorBuffer({set,2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
  backend.writeDescriptorBuffer({set,3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,parameters.buffer,parameters.size});
  layout=backend.createPipelineLayout(setLayout,nullptr);shader=backend.createShaderModule(readBinaryFile(argv[1]));pipeline=backend.createComputePipeline({shader,layout});
  host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(static_cast<unsigned>(positions.size()),1,1);
   commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});});
  host.allocator.invalidate(output,0,output.size);const auto* values=static_cast<const float*>(output.mapped);
  for(std::size_t i=0;i<positions.size();++i){
   const auto& p=positions[i];auto coordinates=p[3]>.5F?projection.coordinates({static_cast<std::uint32_t>(p[0]),static_cast<std::uint32_t>(p[1])},p[2]):std::array<float,4>{p[0],p[1],p[2],1};
   if(projectionMode&&i>=ordinaryCount)coordinates=projectedOracle[i-ordinaryCount];
   for(unsigned c=0;c<4;++c)if(std::abs(values[i*16+12+c]-coordinates[c])>2e-6F)throw std::runtime_error("Fog GPU projected coordinates differ");
   auto expected=data.sample({coordinates[0],coordinates[1],coordinates[2]});
   for(unsigned c=0;c<4;++c)fullPrecisionDifference=std::max(fullPrecisionDifference,std::abs(values[i*16+c]-(c==3?expected.transmittance[0]:expected.scattering[c])));
   // The captured D3D11 sampler returns half-precision filtered values. Preserve
   // the 1e-4 comparison budget and compare its actual format precision.
   const auto filtered=[](float value){return float(Imath::half(value));};
   for(float& value:expected.scattering)value=filtered(value);for(float& value:expected.transmittance)value=filtered(value);
   if(i>=ordinaryCount){const auto& source=oracle[i-ordinaryCount];expected.scattering={source[0],source[1],source[2]};expected.transmittance={source[3],source[3],source[3]};}
   for(float& value:expected.scattering)value*=coordinates[3];for(float& value:expected.transmittance)value=1+(value-1)*coordinates[3];
   const auto alpha=applyFogTransport(expected,{1,2,3}),additive=applyFogTransport(expected,{1,2,3},0);
   for(unsigned c=0;c<4;++c){const float reference=c==3?expected.transmittance[0]:expected.scattering[c];const float actual=values[i*16+c];if(!std::isfinite(actual))throw std::runtime_error("Nonfinite fog volume sample");maximum=std::max(maximum,std::abs(actual-reference));}
   for(unsigned c=0;c<3;++c)for(unsigned mode=0;mode<2;++mode){const auto actual=values[i*16+4+mode*4+c];if(!std::isfinite(actual))throw std::runtime_error("Nonfinite fog surface response");maximum=std::max(maximum,std::abs(actual-(mode?additive[c]:alpha[c])));}
  }
 }
 if(maximum>(argc>=3?1e-4F:1e-5F))throw std::runtime_error("Fog GPU sampling or composition differs: "+std::to_string(maximum));
 cleanup.reset();if(host.errors.load()||host.allocator.statistics().liveImages||host.allocator.statistics().liveBuffers)throw std::runtime_error("Fog GPU validation or resource lifetime failure");
 std::cout<<std::setprecision(9)<<"Fog GPU samples: "<<positions.size()<<" native oracle samples: "<<oracle.size()<<" maximum error: "<<maximum<<" float reference difference: "<<fullPrecisionDifference<<" allocations: 0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
