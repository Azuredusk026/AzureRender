#include "render/ReflectionReprojection.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <limits>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F function){bool bad=false;try{function();}catch(const std::exception&){bad=true;}require(bad,"Invalid reprojection must reject");}
int main()try{
    rhi::NullRhi backend;std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    {
        ReflectionReprojectionGpu resource(backend,{17,9},2,code,2);
        require(resource.byteSize()==8*4*20*2&&backend.allocator().statistics().liveImages==4,"Reprojection must own two bounded images per slot");
        require(resource.output(0).extent.width==8&&resource.output(0).extent.height==4&&resource.output(0).motion!=resource.output(1).motion,"Reprojection slots must remain distinct");
        ReflectionReprojectionInputs input{reinterpret_cast<VkImageView>(900),reinterpret_cast<VkImageView>(901),VK_NULL_HANDLE,17,9};
        ReflectionReprojectionParameters parameters;rhi::NullCommandRecorder commands;const auto writes=backend.descriptorImageWrites.size();
        auto bad=input;bad.width=16;rejects([&]{resource.record(commands,0,bad,parameters);});
        bad=input;bad.motion=resource.output(0).motion;rejects([&]{resource.record(commands,0,bad,parameters);});
        auto p=parameters;p.historyValid=true;rejects([&]{resource.record(commands,0,input,p);});
        p=parameters;p.absoluteDepthTolerance=-1;rejects([&]{resource.record(commands,0,input,p);});
        p=parameters;p.previousDepthProjection={0,0,0,0};rejects([&]{resource.record(commands,0,input,p);});
        p=parameters;p.relativeDepthTolerance=std::numeric_limits<float>::quiet_NaN();rejects([&]{resource.record(commands,0,input,p);});
        rejects([&]{resource.record(commands,2,input,parameters);});
        require(commands.calls.empty()&&backend.descriptorImageWrites.size()==writes,"Reprojection rejection must precede mutations");
        resource.record(commands,0,input,parameters);require(!commands.calls.empty(),"First frame reprojection must record");
        commands.calls.clear();parameters.historyValid=true;input.previousSelectedMotion=resource.output(0).motion;
        rejects([&]{resource.record(commands,0,input,parameters);});require(commands.calls.empty(),"Same-slot history must reject before record");
        resource.record(commands,1,input,parameters);require(!commands.calls.empty(),"Cross-slot history must record");
    }
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Reprojection ownership must retire");
    rejects([&]{ReflectionReprojectionGpu resource(backend,{8192,8192},2,code);});
    rejects([&]{ReflectionReprojectionGpu resource(backend,{17,9},0,code);});
    rejects([&]{ReflectionReprojectionGpu resource(backend,{17,9},2,code,3);});
    rejects([&]{ReflectionReprojectionGpu resource(backend,{17,9},2,{});});
    backend.unavailableStorageFormats.insert(VK_FORMAT_R32_SFLOAT);
    rejects([&]{ReflectionReprojectionGpu resource(backend,{17,9},2,code);});
    backend.unavailableStorageFormats.clear();
    require(!backend.allocator().statistics().liveImages,"Invalid budgets must reject before allocation");
    std::cout<<"PASS: reflection reprojection ownership, budgets, aliases and mutation boundaries\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
