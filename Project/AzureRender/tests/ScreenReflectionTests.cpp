#include "render/ScreenReflectionGpu.hpp"
#include "render/LinearMath.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
using namespace azurerender;
void require(bool v){if(!v)throw std::runtime_error("Screen reflection contract failed");}
template<class F>void rejects(F op){bool failed=false;try{op();}catch(const std::exception&){failed=true;}require(failed);}
int main()try{
    rhi::NullRhi backend;std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    ScreenReflectionShaders shaders{code,code,{code,code},{code,code}};
    require(ScreenReflectionGpu::supported(backend,{17,9},2));
    require(!ScreenReflectionGpu::supported(backend,{8192,8192},2));
    backend.unavailableStorageFormats.insert(VK_FORMAT_R32_SFLOAT);
    require(!ScreenReflectionGpu::supported(backend,{17,9},2));backend.unavailableStorageFormats.clear();
    rejects([&]{ScreenReflectionGpu p(backend,{8192,8192},2,shaders);});
    rejects([&]{ScreenReflectionGpu p(backend,{0,9},2,shaders);});
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
