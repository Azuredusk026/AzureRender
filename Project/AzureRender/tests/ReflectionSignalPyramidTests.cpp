#include "render/ReflectionSignalPyramid.hpp"
#include "rhi/NullRhi.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
using namespace azurerender;
void require(bool p,const char* message){if(!p)throw std::runtime_error(message);}
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"Invalid reflection pyramid input must fail");}
int main()try{
    ReflectionPyramidParameters parameters;const auto frame=parameters.frame(1280,720,1,7);
    static_assert(sizeof(frame)==48,"Pyramid frame uses three vec4 rows");
    require(frame[0][0]==1.F/1280&&frame[1][0]==1.F/640&&frame[0][2]==1,"Mip uses actual source and destination dimensions");
    rejects([&]{(void)parameters.frame(1280,720,0,7);});
    rejects([&]{(void)parameters.frame(1280,720,7,7);});
    auto invalid=parameters;invalid.depthFalloff=-1;rejects([&]{(void)invalid.frame(1280,720,1,7);});
    invalid=parameters;invalid.minimumWeight=0;rejects([&]{(void)invalid.frame(1280,720,1,7);});
    invalid=parameters;invalid.depthDecode[0]=std::numeric_limits<float>::quiet_NaN();rejects([&]{(void)invalid.frame(1280,720,1,7);});
    std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    ReflectionPyramidShaders shaders{code,code};rhi::NullRhi backend;
    {
        ReflectionSignalPyramidGpu pyramid(backend,{17,9},4,2,ReflectionPyramidPrecision::PreferPacked,shaders);
        require(pyramid.usesPackedFormats()&&pyramid.output(0).colorFormat==VK_FORMAT_B10G11R11_UFLOAT_PACK32,"Preferred packed formats are selected when supported");
        require(pyramid.base(0).color!=pyramid.output(0).color&&pyramid.output(0).color!=pyramid.output(1).color,"Mip and in-flight views are independent");
        rhi::NullCommandRecorder commands;const auto depth=reinterpret_cast<VkImageView>(std::uintptr_t(100000));
        rejects([&]{pyramid.record(commands,0,depth,4,parameters);});
        require(commands.calls.empty(),"Record requires base-write ordering");
        pyramid.beginBaseWrite(commands,0);const auto calls=commands.calls.size();
        rejects([&]{pyramid.beginBaseWrite(commands,0);});
        rejects([&]{pyramid.record(commands,0,VK_NULL_HANDLE,4,parameters);});
        rejects([&]{pyramid.record(commands,0,depth,3,parameters);});
        require(commands.calls.size()==calls,"Rejected recording leaves commands unchanged");
        pyramid.record(commands,0,depth,4,parameters);
        require(std::count_if(commands.calls.begin(),commands.calls.end(),[](const auto& c){return c.name=="dispatch";})==3,"All three dependent mips are recorded");
        rejects([&]{pyramid.record(commands,0,depth,4,parameters);});
        pyramid.beginBaseWrite(commands,0);pyramid.record(commands,0,depth,4,parameters);
    }
    require(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"Pyramid owns and releases its resources");
    backend.unavailableStorageFormats.insert(VK_FORMAT_R8_UNORM);
    rejects([&]{ReflectionSignalPyramidGpu p(backend,{17,9},4,2,ReflectionPyramidPrecision::RequirePacked,shaders);});
    {
        ReflectionSignalPyramidGpu p(backend,{17,9},4,2,ReflectionPyramidPrecision::PreferPacked,shaders);
        require(!p.usesPackedFormats()&&p.output(0).colorFormat==VK_FORMAT_R32G32B32A32_SFLOAT,"Unsupported packed storage selects explicit float fallback");
    }
    require(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"Fallback resources are released");
    std::cout<<"PASS: pyramid bounds, format selection, base-write ordering, dependent mips and frame ownership\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
