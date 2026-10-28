#include "render/AerialFog.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <filesystem>
using namespace azurerender;
namespace {
using Row=std::array<float,4>;
struct Input {std::array<Row,12> rows{};};
struct Reference {FogTransport aerial,height,combined;};
Input input(const ExponentialAerialFog& p,std::array<float,3> start,std::array<float,3> end,
    const ExponentialHeightFog& height,std::array<float,3> heightStart,Row volume){
    Input value;const auto packed=p.pack();std::copy(packed.rows.begin(),packed.rows.end(),value.rows.begin());
    for(unsigned i=0;i<3;++i){value.rows[6][i]=start[i];value.rows[7][i]=end[i];value.rows[9][i]=height.scatteringColor[i];value.rows[11][i]=heightStart[i];}
    value.rows[8]={height.density,height.heightFalloff,height.referenceHeight,0};value.rows[10]=volume;return value;
}
Reference reference(const ExponentialAerialFog& p,std::array<float,3> start,std::array<float,3> end,
    const ExponentialHeightFog& height,std::array<float,3> heightStart,Row volume){
    Reference r;r.aerial=evaluateAerialFog(p,start,end);r.height=evaluateHeightFog(height,heightStart,end);
    r.combined=composeFogTransport({{volume[0],volume[1],volume[2]},{volume[3],volume[3],volume[3]}},composeFogTransport(r.height,r.aerial));return r;
}
std::array<Row,6> rows(const Reference& r){std::array<Row,6> out{};unsigned n=0;for(const auto* fog:{&r.aerial,&r.height,&r.combined}){for(unsigned i=0;i<3;++i){out[n][i]=fog->transmittance[i];out[n+1][i]=fog->scattering[i];}n+=2;}return out;}
}
int main(int argc,char** argv)try{
 if(argc<2||argc>3)throw std::invalid_argument("Expected compute shader and optional native fixture");
 std::vector<Input> inputs;std::vector<Reference> expected;unsigned nativeCount=0;float nativeCpuError=0;
 ExponentialHeightFog height{.03F,.12F,2,{.2F,.4F,.6F}};const Row volume{.01F,.02F,.03F,.75F};
 const std::array<std::array<float,3>,6> ends{{{0,0,0},{1,0,0},{0,2,0},{0,-2,0},{0,0,50},{10,20,30}}};
 for(unsigned mode=0;mode<5;++mode){
  ExponentialAerialFog p;p.density=mode?.15F:0;p.extinction={.3F,.8F,1.2F};p.heightFalloff=.2F;p.referenceHeight=2;
  p.rayleighColor={.1F,.2F,.3F};p.mieColor={.4F,.3F,.2F};p.ambientColor={.05F,.1F,.2F};
  if(mode==2)p.minimumHeightDelta=.01F;if(mode==3){p.anisotropy=.7F;p.startDistance=1;}
  if(mode==4){p.radianceScale=255;p.ambientColor={2,.5F,.1F};}
  for(auto end:ends){inputs.push_back(input(p,{0,0,0},end,height,{0,0,0},volume));expected.push_back(reference(p,{0,0,0},end,height,{0,0,0},volume));}
 }
 for(float y:{-1e30F,1e30F}){ExponentialAerialFog p;p.density=1;p.heightFalloff=1;p.extinction={1,2,.5F};p.ambientColor={.1F,.2F,.3F};inputs.push_back(input(p,{0,y,0},{0,y,10},{},{0,y,0},{0,0,0,1}));expected.push_back(reference(p,{0,y,0},{0,y,10},{},{0,y,0},{0,0,0,1}));}
 for(bool reverse:{false,true}){ExponentialAerialFog p;p.density=.25F;p.heightFalloff=2;p.extinction={1,2,.5F};p.ambientColor={.1F,.2F,.3F};const std::array<float,3> start{0,reverse?0:1e30F,0},end{0,reverse?1e30F:0,0};const ExponentialHeightFog h{.25F,2,0,{.1F,.2F,.3F}};inputs.push_back(input(p,start,end,h,start,{0,0,0,1}));expected.push_back(reference(p,start,end,h,start,{0,0,0,1}));}
 if(argc==3){
  std::ifstream file(std::filesystem::u8path(argv[2]));std::string magic;file>>magic;if(magic!="AZAERIAL1")throw std::invalid_argument("Invalid native aerial fixture");
  std::array<Row,6> packed{};for(auto& row:packed)for(auto& v:row)if(!(file>>v))throw std::invalid_argument("Truncated aerial parameters");
  ExponentialAerialFog p;p.density=packed[0][3];p.heightFalloff=packed[1][3];p.referenceHeight=packed[2][3];p.startDistance=packed[3][3];
  for(unsigned i=0;i<3;++i){p.extinction[i]=packed[0][i];p.rayleighColor[i]=packed[1][i];p.mieColor[i]=packed[2][i];p.ambientColor[i]=packed[3][i];p.sunDirection[i]=packed[4][i];}
  p.anisotropy=packed[4][3];p.radianceScale=packed[5][0];p.phaseDenominatorFloor=packed[5][1];
  if(packed[5][3]!=0&&packed[5][3]!=1)throw std::invalid_argument("Invalid aerial integral clamp flag");
  if(packed[5][3]==1)p.minimumHeightDelta=packed[5][2];p.validate();
  std::array<float,32> values{};
  while(file>>values[0]){
   for(unsigned i=1;i<32;++i)if(!(file>>values[i]))throw std::invalid_argument("Truncated native aerial sample");
   if(++nativeCount>64)throw std::invalid_argument("Native aerial sample budget exceeded");
   const std::array<float,3> start{values[0],values[1],values[2]},end{values[3],values[4],values[5]},hs{values[6],values[7],values[8]};
   const ExponentialHeightFog h{values[9],values[10],values[11],{values[12],values[13],values[14]}};
   const Row vol{values[15],values[16],values[17],values[18]};Reference native;
   for(unsigned i=0;i<3;++i){native.aerial.transmittance[i]=values[19+i];native.aerial.scattering[i]=values[22+i];native.height.transmittance[i]=values[25];native.height.scattering[i]=h.scatteringColor[i]*(1-values[25]);native.combined.transmittance[i]=values[26+i];native.combined.scattering[i]=values[29+i];}
   const auto cpu=rows(reference(p,start,end,h,hs,vol)),oracle=rows(native);
   for(unsigned i=0;i<6;++i)for(unsigned c=0;c<3;++c){if(!std::isfinite(oracle[i][c]))throw std::invalid_argument("Nonfinite native aerial reference");nativeCpuError=std::max(nativeCpuError,std::abs(cpu[i][c]-oracle[i][c]));}
   inputs.push_back(input(p,start,end,h,hs,vol));expected.push_back(native);
  }
  if(!file.eof()||nativeCount==0)throw std::invalid_argument("Invalid native aerial payload");
  if(nativeCpuError>5e-6F)throw std::runtime_error("CPU aerial model differs from original fragments");
 }
 test::VulkanComputeFixture host;auto& backend=*host.backend;rhi::GpuBuffer source{},destination{};
 VkDescriptorSetLayout descriptors{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
 auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);if(descriptors)backend.destroyDescriptorSetLayout(descriptors);host.allocator.destroyBuffer(source);host.allocator.destroyBuffer(destination);});
 source=host.allocator.createBuffer(inputs.size()*sizeof(Input),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
 destination=host.allocator.createBuffer(inputs.size()*sizeof(std::array<Row,6>),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
 std::memcpy(source.mapped,inputs.data(),std::size_t(source.size));host.allocator.flush(source,0,source.size);
 descriptors=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
 rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};pool=backend.createDescriptorPool(desc);auto set=backend.allocateDescriptorSets(pool,descriptors,1).front();
 backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,source.buffer,source.size});backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,destination.buffer,destination.size});
 layout=backend.createPipelineLayout(descriptors,nullptr);shader=backend.createShaderModule(readBinaryFile(argv[1]));pipeline=backend.createComputePipeline({shader,layout});
 host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(static_cast<unsigned>(inputs.size()),1,1);commands.bufferBarrier({destination.buffer,0,destination.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});});
 host.allocator.invalidate(destination,0,destination.size);const auto* actual=static_cast<const std::array<Row,6>*>(destination.mapped);float maximum=0;
 for(std::size_t n=0;n<inputs.size();++n){const auto r=rows(expected[n]);for(unsigned i=0;i<6;++i)for(unsigned c=0;c<3;++c){if(!std::isfinite(actual[n][i][c]))throw std::runtime_error("Nonfinite analytic fog output");maximum=std::max(maximum,std::abs(actual[n][i][c]-r[i][c]));}}
 if(maximum>3e-5F)throw std::runtime_error("GPU analytic fog differs: "+std::to_string(maximum));
 cleanup.reset();if(host.errors.load()||host.allocator.statistics().liveImages||host.allocator.statistics().liveBuffers)throw std::runtime_error("Analytic fog validation or lifetime failure");
 std::cout<<std::setprecision(9)<<"Analytic fog samples: "<<inputs.size()<<" native: "<<nativeCount<<" CPU native error: "<<nativeCpuError<<" GPU maximum error: "<<maximum<<" allocations: 0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
