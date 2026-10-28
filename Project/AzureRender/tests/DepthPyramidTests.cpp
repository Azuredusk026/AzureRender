#include "render/DepthPyramid.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
using namespace azurerender;
void require(bool value){if(!value)throw std::runtime_error("Depth pyramid contract failed");}
template<class F>void rejects(F op){bool failed=false;try{op();}catch(const std::exception&){failed=true;}require(failed);}
int main()try{
    rhi::NullRhi backend;std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    rejects([&]{DepthPyramidGpu p(backend,{0,9},4,2,code);});
    rejects([&]{DepthPyramidGpu p(backend,{17,9},6,2,code);});
    rejects([&]{DepthPyramidGpu p(backend,{8192,8192},14,2,code);});
    backend.unavailableStorageFormats.insert(VK_FORMAT_R32_SFLOAT);
    rejects([&]{DepthPyramidGpu p(backend,{17,9},4,2,code);});backend.unavailableStorageFormats.clear();
    {
        DepthPyramidGpu p(backend,{17,9},4,2,code);rhi::NullCommandRecorder commands;
        require(p.output(0).view!=p.output(1).view);
        const auto borrowed=reinterpret_cast<VkImageView>(999);
        rejects([&]{p.record(commands,0,VK_NULL_HANDLE,DepthReduction::Minimum);});
        rejects([&]{p.record(commands,0,p.output(1).view,DepthReduction::Minimum);});
        rejects([&]{p.record(commands,0,borrowed,static_cast<DepthReduction>(99));});
        rejects([&]{p.record(commands,0,borrowed,DepthReduction::Minimum,{0,18});});
        rejects([&]{p.record(commands,0,borrowed,DepthReduction::Minimum,{40,18});});
        rejects([&]{p.record(commands,0,borrowed,DepthReduction::Minimum,{9000,18});});
        require(commands.calls.empty());
        p.record(commands,0,borrowed,DepthReduction::Minimum);
        p.record(commands,1,borrowed,DepthReduction::Maximum);
        p.record(commands,0,borrowed,DepthReduction::Minimum,{35,19});
        require(!commands.calls.empty());
    }
    require(!backend.allocator().statistics().liveBuffers&&!backend.allocator().statistics().liveImages);
    std::cout<<"PASS: depth bounds, capabilities, aliases, slots and retirement\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
