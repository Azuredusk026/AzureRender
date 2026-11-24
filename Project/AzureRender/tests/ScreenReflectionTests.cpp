#include "render/ScreenReflectionGpu.hpp"
#include "render/LinearMath.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
using namespace azurerender;
void require(bool v){if(!v)throw std::runtime_error("Screen reflection contract failed");}
template<class F>void rejects(F op){bool failed=false;try{op();}catch(const std::exception&){failed=true;}require(failed);}
int main()try{
    rhi::NullRhi backend;std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    ScreenReflectionShaders shaders{code,code,{code,code},{code,code},{code,code}};
    require(ScreenReflectionGpu::supported(backend,{17,9},2));
    require(!ScreenReflectionGpu::supported(backend,{8192,8192},2));
    require(ScreenReflectionGpu::supported(backend,{2048,2048},2));
    require(!ScreenReflectionGpu::supported(backend,{2048,2048},2,7,ReflectionPyramidPrecision::Float32,1,true));
    rejects([&]{ScreenReflectionGpu p(backend,{2048,2048},2,shaders,7,ReflectionPyramidPrecision::Float32,1,true);});
    require(!backend.allocator().statistics().liveImages);
    backend.unavailableStorageFormats.insert(VK_FORMAT_R32_SFLOAT);
    require(!ScreenReflectionGpu::supported(backend,{17,9},2));backend.unavailableStorageFormats.clear();
    rejects([&]{ScreenReflectionGpu p(backend,{8192,8192},2,shaders);});
    rejects([&]{ScreenReflectionGpu p(backend,{0,9},2,shaders);});
    rejects([&]{ScreenReflectionGpu p(backend,{17,9},2,shaders,7,ReflectionPyramidPrecision::Float32,3);});
    {
        ScreenReflectionGpu half(backend,{35,19},2,shaders,7,ReflectionPyramidPrecision::Float32,2);
        ScreenReflectionGpu full(backend,{35,19},2,shaders);
        require(half.output(0).extent.width==17&&half.output(0).extent.height==9&&half.levels()==5);
        require(half.imageByteBudget()<full.imageByteBudget()/3);
    }
    {
        ScreenReflectionGpu ordinary(backend,{17,9},2,shaders,7,ReflectionPyramidPrecision::Float32);
        ScreenReflectionGpu temporal(backend,{17,9},2,shaders,7,ReflectionPyramidPrecision::Float32,1,true);
        require(temporal.imageByteBudget()==ordinary.imageByteBudget()+17*9*20*2);
        require(temporal.history(0).color!=temporal.history(1).color);
        ReflectionTraceView view;view.projection=internal::perspective(1,1,.1F,100);view.worldToClip=view.projection;
        view.right={1,0,0};view.up={0,1,0};view.forward={0,0,-1};ScreenSpaceReflectionSettings settings;settings.enabled=true;
        const ReflectionTraceInputs inputs{reinterpret_cast<VkImageView>(999),reinterpret_cast<VkImageView>(1000),17,9};
        ScreenReflectionHistoryInputs history;history.motion=reinterpret_cast<VkImageView>(1001);history.acceptance=reinterpret_cast<VkImageView>(1002);
        rhi::NullCommandRecorder commands;const auto writes=backend.descriptorImageWrites.size();
        rejects([&]{ordinary.record(commands,0,inputs,view,settings,&history);});
        auto bad=history;bad.motion=VK_NULL_HANDLE;rejects([&]{temporal.record(commands,0,inputs,view,settings,&bad);});
        bad=history;bad.parameters.historyValid=true;rejects([&]{temporal.record(commands,0,inputs,view,settings,&bad);});
        bad=history;bad.parameters.historyWeight=2;rejects([&]{temporal.record(commands,0,inputs,view,settings,&bad);});
        bad=history;bad.motion=temporal.history(0).color;rejects([&]{temporal.record(commands,0,inputs,view,settings,&bad);});
        require(commands.calls.empty()&&backend.descriptorImageWrites.size()==writes);
        temporal.record(commands,0,inputs,view,settings,&history);require(!commands.calls.empty());
        history.parameters.historyValid=true;history.previousColor=temporal.history(0).color;history.previousConfidence=temporal.history(0).confidence;
        commands.calls.clear();rejects([&]{temporal.record(commands,0,inputs,view,settings,&history);});require(commands.calls.empty());
        temporal.record(commands,1,inputs,view,settings,&history);require(!commands.calls.empty());
    }
    {
        ScreenReflectionGpu p(backend,{17,9},2,shaders);require(p.levels()==5);
        require(p.output(0).layout==VK_IMAGE_LAYOUT_GENERAL&&p.output(0).view!=p.output(1).view);
        ReflectionTraceView view;view.projection=internal::perspective(1,1,.1F,100);view.worldToClip=view.projection;
        view.right={1,0,0};view.up={0,1,0};view.forward={0,0,-1};ScreenSpaceReflectionSettings settings;settings.enabled=true;
        ReflectionTraceInputs inputs{reinterpret_cast<VkImageView>(999),reinterpret_cast<VkImageView>(1000),17,9};
        rhi::NullCommandRecorder commands;auto alias=inputs;alias.surface=p.output(1).view;
        rejects([&]{p.record(commands,0,alias,view,settings);});require(commands.calls.empty());
        p.record(commands,0,inputs,view,settings);require(!commands.calls.empty());
    }
    require(!backend.allocator().statistics().liveBuffers&&!backend.allocator().statistics().liveImages);
    std::cout<<"PASS: composed reflection budgets, ownership and recording\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
