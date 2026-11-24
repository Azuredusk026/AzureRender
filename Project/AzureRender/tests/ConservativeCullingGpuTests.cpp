#include "render/ConservativeCullShader.hpp"
#include "render/GpuCullingResources.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <cstring>
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class F> void rejects(F f){bool caught=false;try{f();}catch(const std::invalid_argument&){caught=true;}check(caught,"Expected conservative cull refusal");}
struct Case {GpuCullBounds bounds;bool expected;};
std::vector<Case> cases(float margin,bool zeroW){
    std::vector<Case> values;
    for(int i=0;i<4096;++i){float x=float(i%41-20)/4,y=float(i%37-18)/4,z=float(i%31-15)/4;const float extent=1.F/32;
        bool visible=std::abs(x)<=2+margin+extent&&std::abs(y)<=2+margin+extent&&std::abs(z)<=2+margin+extent;
        if(zeroW&&std::abs(x)<=extent)visible=true;
        values.push_back({{{x-extent,y-extent,z-extent,0},{x+extent,y+extent,z+extent,0}},visible});}
    if(margin>=0){const float edge=std::nextafter(2+margin,std::numeric_limits<float>::infinity());values.push_back({{{edge,0,0,0},{edge,0,0,0}},true});}
    return values;
}
}
int main(int argc,char** argv)try{
    if(argc!=3&&argc!=4)throw std::invalid_argument("Expected cull, surface cull and optional old cull shader paths");
    auto shader=readBinaryFile(argv[1]),surface=readBinaryFile(argv[2]),old=argc==4?readBinaryFile(argv[3]):shader;
    if(argc==3){std::vector<std::uint32_t> words(old.size()/4);std::memcpy(words.data(),old.data(),old.size());for(std::size_t at=5;at<words.size();){const auto count=words[at]>>16;if(!count||count>words.size()-at)throw std::invalid_argument("Broken fixture shader");if((words[at]&0xffffU)==71&&count==4&&words[at+2]==1&&words[at+3]==0)words[at+3]=7;at+=count;}std::memcpy(old.data(),words.data(),old.size());}
    validateConservativeCullShader(shader);validateConservativeCullShader(surface);rejects([&]{validateConservativeCullShader(old);});
    for(unsigned which=0;which<3;++which){auto bad=shader;if(which==0)bad.clear();if(which==1)bad[0]=0;if(which==2)bad.resize(bad.size()-1);rejects([&]{validateConservativeCullShader(bad);});}
    GpuCullParameters invalid;invalid.conservativeMargin=-.5F;rejects([&]{validateGpuCullParameters(invalid);});invalid={};invalid.conservativeMargin=std::numeric_limits<float>::infinity();rejects([&]{validateGpuCullParameters(invalid);});invalid={};invalid.referenceWPlane[0]=std::numeric_limits<float>::quiet_NaN();rejects([&]{validateGpuCullParameters(invalid);});
    test::VulkanComputeFixture host(true);std::size_t checked=0;
    {GpuCullingResources bad(*host.backend);rejects([&]{bad.initialize(old,16,false,true);});check(host.allocator.statistics().liveBuffers==0,"Old shader rejection must precede GPU allocation");}
    for(unsigned mode=0;mode<6;++mode){const bool guarded=mode!=0;const float margin=(mode==0||mode==4)?-1.F:mode==1?0.F:.125F;const bool zeroW=mode==3;
        auto values=cases(margin<0?0:margin,zeroW);if(margin<0)values.pop_back(); // legacy has no uncertain-edge retention
        if(mode==5)for(auto& v:values)v.expected=true;
        GpuCullParameters p;p.planes={{{1,0,0,2},{-1,0,0,2},{0,1,0,2},{0,-1,0,2},{0,0,1,2},{0,0,-1,2}}};p.conservativeMargin=margin;if(zeroW)p.referenceWPlane={1,0,0,0};
        if(mode==5)p.cullingEnabled=0;
        std::vector<GpuCullBounds> bounds;std::vector<VkDrawIndexedIndirectCommand> draws;std::vector<GpuSurfaceIdentity> ids;
        for(std::size_t i=0;i<values.size();++i){bounds.push_back(values[i].bounds);draws.push_back({3,1,unsigned(i*3),-7,unsigned(i)});ids.push_back({unsigned(i),unsigned(i%3),unsigned(i%7),0});const auto result=referenceCullCommand(draws.back(),bounds.back(),p);check(result.instanceCount==unsigned(values[i].expected),"CPU guard differs from analytic box oracle");}
        for(bool surfaces:{false,true}){
            GpuCullingResources resources(*host.backend);resources.initialize(surfaces?surface:shader,unsigned(values.size()),surfaces,guarded);resources.upload(bounds,draws,surfaces?ids:std::vector<GpuSurfaceIdentity>{});
            if(mode==0)host.submit([&](rhi::ICommandRecorder& c,VkCommandBuffer){auto unsupported=p;unsupported.conservativeMargin=0;rejects([&]{resources.record(c,unsupported);});});
            host.submit([&](rhi::ICommandRecorder& c,VkCommandBuffer){for(const auto* b:{&resources.bounds(),&resources.source()})c.bufferBarrier({b->buffer,0,b->size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
                if(surfaces){const auto& b=resources.surfaceSource();c.bufferBarrier({b.buffer,0,b.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});}resources.record(c,p);
                const auto& b=resources.output();c.bufferBarrier({b.buffer,0,b.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});if(surfaces){const auto& s=resources.surfaceOutput();c.bufferBarrier({s.buffer,0,s.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});}});
            host.allocator.invalidate(resources.output(),0,resources.output().size);const auto* actual=static_cast<const VkDrawIndexedIndirectCommand*>(resources.output().mapped);
            for(std::size_t i=0;i<values.size();++i){auto expected=draws[i];expected.instanceCount=unsigned(values[i].expected);check(std::memcmp(&expected,&actual[i],20)==0,"GPU guard changed visibility or stable draw identity");}
            if(surfaces){host.allocator.invalidate(resources.surfaceOutput(),0,resources.surfaceOutput().size);const auto* actualIds=static_cast<const GpuSurfaceIdentity*>(resources.surfaceOutput().mapped);for(std::size_t i=0;i<values.size();++i){auto expected=ids[i];expected.visible=unsigned(values[i].expected);check(std::memcmp(&expected,&actualIds[i],16)==0,"GPU guard changed stable surface identity");}}
            checked+=values.size();}
        check(host.allocator.statistics().liveBuffers==0&&host.allocator.statistics().liveImages==0,"Guard resources leaked");}
    check(!host.errors.load(),"Conservative cull Vulkan validation failed");std::cout<<host.properties.deviceName<<": "<<checked<<" exact draw records; surface identities, old shader preallocation rejection and guard contracts passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
