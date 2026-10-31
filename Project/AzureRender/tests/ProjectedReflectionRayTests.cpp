#include "render/ProjectedReflectionRay.hpp"
#include "rhi/NullRhi.hpp"
#include <cmath>
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F operation){bool failed=false;try{operation();}catch(const std::exception&){failed=true;}require(failed,"Invalid projected ray input accepted");}
ProjectedReflectionView camera(){
    ProjectedReflectionView view;
    for(unsigned i=0;i<3;++i){view.worldNormalToView[i][i]=1;view.viewDirectionToClip[i][i]=1;}
    for(auto* matrix:{&view.clipToView,&view.relativeWorldToClip,&view.clipToWorld,&view.previousClipToWorld})
        for(unsigned i=0;i<4;++i)(*matrix)[i][i]=1;
    view.position={2,3,4};view.depthReciprocal={9.99F,.001F};return view;
}
}
int main()try{
    const auto view=camera();ProjectedReflectionSettings settings;settings.jitterSeed=42;
    const auto packed=view.pack({34,18},{17,9},settings);static_assert(sizeof(packed)==480);
    require(packed[22][0]==2&&packed[22][1]==3&&packed[22][2]==4,"Camera position packing differs");
    require(packed[23][0]==34&&packed[23][1]==18&&packed[23][2]==1.F/34,"Source dimensions differ");
    require(packed[24][2]==9.99F&&packed[24][3]==.001F,"Depth convention differs");
    require(packed[25][0]==17&&packed[25][1]==9&&packed[25][3]==1.F/9,"Trace dimensions differ");
    require(packed[26][0]==42&&packed[26][1]==.1F&&packed[28][0]==32&&packed[29][1]==6,"Trace settings differ");
    require(view.pack({17,9},{17,9},settings)[23][0]==17,"Full resolution rejected");
    require(view.pack({35,19},{17,9},settings)[23][1]==19,"Odd floor-half dimensions rejected");
    rejects([&]{view.pack({33,18},{17,9},settings);});rejects([&]{view.pack({0,18},{17,9},settings);});
    auto invalid=view;invalid.position[1]=std::numeric_limits<float>::quiet_NaN();rejects([&]{invalid.pack({34,18},{17,9},settings);});
    invalid=view;invalid.clipToWorld[2][0]=std::numeric_limits<float>::infinity();rejects([&]{invalid.pack({34,18},{17,9},settings);});
    auto narrow=settings;narrow.edgeFade=.05F;auto wide=settings;wide.edgeFade=.5F;
    require(view.pack({34,18},{17,9},narrow)[26][1]==.05F&&view.pack({34,18},{17,9},wide)[26][1]==.5F,"Edge fade width packing differs");
    for(float value:{0.F,-1.F,std::numeric_limits<float>::quiet_NaN()}){auto bad=settings;bad.edgeFade=value;rejects([&]{view.pack({34,18},{17,9},bad);});}
    auto badSettings=settings;badSettings.motionBlend=1.1F;rejects([&]{view.pack({34,18},{17,9},badSettings);});
    badSettings=settings;badSettings.maximumFootprintLevel=0;rejects([&]{view.pack({34,18},{17,9},badSettings);});
    badSettings=settings;badSettings.maxSteps=0;rejects([&]{view.pack({34,18},{17,9},badSettings);});
    rhi::NullRhi backend;std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    rejects([&]{ProjectedReflectionRayGpu ray(backend,0,code);});
    rejects([&]{ProjectedReflectionRayGpu ray(backend,9,code);});
    rejects([&]{ProjectedReflectionRayGpu ray(backend,2,{});});
    backend.unavailableStorageFormats.insert(VK_FORMAT_R16G16_UNORM);
    rejects([&]{ProjectedReflectionRayGpu ray(backend,2,code);});backend.unavailableStorageFormats.clear();
    {
        ProjectedReflectionRayGpu ray(backend,2,code);
        require(backend.allocator().statistics().liveBuffers==2,"Each frame must own its parameter buffer");
        ProjectedReflectionInputs inputs;
        inputs.previousColor=reinterpret_cast<VkImageView>(1001);inputs.previousDepth=reinterpret_cast<VkImageView>(1002);
        inputs.currentDepth=reinterpret_cast<VkImageView>(1003);inputs.motion=reinterpret_cast<VkImageView>(1004);
        inputs.surfaceMask=reinterpret_cast<VkImageView>(1005);inputs.normalRoughness=reinterpret_cast<VkImageView>(1006);
        inputs.sourceExtent={34,18};inputs.traceExtent={17,9};inputs.tiles=reinterpret_cast<VkBuffer>(1007);inputs.tileBytes=4;inputs.tileCount=1;
        ProjectedReflectionOutputs outputs{reinterpret_cast<VkImageView>(2001),reinterpret_cast<VkImageView>(2002),reinterpret_cast<VkImageView>(2003),{17,9}};
        rhi::NullCommandRecorder commands;const auto writes=backend.descriptorImageWrites.size();
        auto bad=inputs;bad.previousColor=outputs.color;rejects([&]{ray.record(commands,0,bad,outputs,view,settings);});
        bad=inputs;bad.tileCount=2;rejects([&]{ray.record(commands,0,bad,outputs,view,settings);});
        bad=inputs;bad.normalRoughness=VK_NULL_HANDLE;rejects([&]{ray.record(commands,0,bad,outputs,view,settings);});
        auto badOutput=outputs;badOutput.footprint=outputs.confidence;rejects([&]{ray.record(commands,0,inputs,badOutput,view,settings);});
        badOutput=outputs;badOutput.extent.width=18;rejects([&]{ray.record(commands,0,inputs,badOutput,view,settings);});
        rejects([&]{ray.record(commands,2,inputs,outputs,view,settings);});
        require(commands.calls.empty()&&backend.descriptorImageWrites.size()==writes,"Rejected input changed recording state");
        bad=inputs;bad.tileCount=0;ray.record(commands,0,bad,outputs,view,settings);
        require(commands.calls.empty()&&backend.descriptorImageWrites.size()==writes,"Zero dispatch changed recording state");
        ray.record(commands,0,inputs,outputs,view,settings);require(!commands.calls.empty(),"Valid ray dispatch missing");
        const auto firstSet=backend.descriptorImageWrites.back().set;commands.calls.clear();
        ray.record(commands,1,inputs,outputs,view,settings);
        require(!commands.calls.empty()&&backend.descriptorImageWrites.back().set!=firstSet,"Frame slots share descriptor state");
    }
    require(!backend.allocator().statistics().liveBuffers&&!backend.allocator().statistics().liveImages,"Projected ray leaked resources");
    std::cout<<"PASS: projected ray parameters, borrowed views, rejection and frame ownership\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
