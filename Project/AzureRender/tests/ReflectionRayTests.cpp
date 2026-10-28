#include "render/ReflectionRay.hpp"
#include "render/CameraProjection.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
using namespace azurerender;
void require(bool value){if(!value)throw std::runtime_error("Reflection ray frame contract failed");}
template<class F>void rejects(F op){bool failed=false;try{op();}catch(const std::exception&){failed=true;}require(failed);}
int main()try{
    ReflectionTraceView view;view.projection=internal::perspective(1.F,1.F,.1F,100.F);
    view.worldToClip=view.projection;view.right={1,0,0};view.up={0,1,0};view.forward={0,0,-1};
    ScreenSpaceReflectionSettings settings;settings.enabled=true;
    const auto frame=view.pack(17,9,settings,ReflectionColorEncoding::MaxComponent);
    static_assert(sizeof(frame)==192);require(frame[11][2]==1&&frame[11][0]==17&&frame[11][1]==9);
    require(frame[8][0]==1.F/view.projection[0]&&frame[8][1]==1.F/view.projection[5]);
    auto invalid=view;invalid.right={2,0,0};rejects([&]{invalid.pack(17,9,settings,ReflectionColorEncoding::Linear);});
    invalid=view;invalid.forward={0,0,1};rejects([&]{invalid.pack(17,9,settings,ReflectionColorEncoding::Linear);});
    invalid=view;invalid.projection[0]=0;rejects([&]{invalid.pack(17,9,settings,ReflectionColorEncoding::Linear);});
    rejects([&]{view.pack(0,9,settings,ReflectionColorEncoding::Linear);});
    view.projection=internal::orthographic(-3,3,-2,2,.1F,100.F);view.projection[12]=.2F;view.projection[13]=-.1F;
    view.worldToClip=view.projection;const auto ortho=view.pack(17,9,settings,ReflectionColorEncoding::Linear);
    require(ortho[11][2]==0&&ortho[8][2]==-.2F&&ortho[8][3]==.1F);
    rhi::NullRhi backend;std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    const ReflectionTraceShaders shaders{code,code};
    rejects([&]{ReflectionRayGpu p(backend,{17,9},0,VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_R32_SFLOAT,shaders);});
    rejects([&]{ReflectionRayGpu p(backend,{17,9},2,VK_FORMAT_R32_SFLOAT,VK_FORMAT_R32_SFLOAT,shaders);});
    {
        ReflectionSignalPyramidGpu signals(backend,{17,9},1,2,ReflectionPyramidPrecision::Float32,{code,code});
        ReflectionRayGpu producer(backend,{17,9},2,VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_R32_SFLOAT,shaders);
        ReflectionTraceInputs input{reinterpret_cast<VkImageView>(999),reinterpret_cast<VkImageView>(1000),17,9};
        rhi::NullCommandRecorder commands;const auto writes=backend.descriptorImageWrites.size();
        auto bad=input;bad.width=16;rejects([&]{producer.record(commands,0,bad,signals.base(0),view,settings);});
        bad=input;bad.surface=signals.base(0).color;rejects([&]{producer.record(commands,0,bad,signals.base(0),view,settings);});
        rejects([&]{producer.record(commands,2,input,signals.base(0),view,settings);});
        require(commands.calls.empty()&&backend.descriptorImageWrites.size()==writes);
        signals.beginBaseWrite(commands,0);producer.record(commands,0,input,signals.base(0),view,settings);
        signals.record(commands,0,input.surface,1,{});
    }
    require(!backend.allocator().statistics().liveBuffers&&!backend.allocator().statistics().liveImages);
    std::cout<<"PASS: reflection ray projection and frame bounds\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
