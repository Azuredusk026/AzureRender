#include "render/ProjectedScreenReflectionGpu.hpp"
#include "render/LinearMath.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F op){bool failed=false;try{op();}catch(const std::exception&){failed=true;}require(failed,"Invalid projected composition accepted");}
}
int main()try{
    rhi::NullRhi backend;std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    const ProjectedScreenReflectionShaders shaders{code,code,code,code,{code,code}};
    require(ProjectedScreenReflectionGpu::supported(backend,{35,19},2,7,2),"Valid composition unsupported");
    require(!ProjectedScreenReflectionGpu::supported(backend,{8192,8192},2),"Invalid composition budget supported");
    require(!ProjectedScreenReflectionGpu::supported(backend,{2048,2048},1),"Too many composition tiles supported");
    backend.unavailableStorageFormats.insert(VK_FORMAT_R16G16_UNORM);
    require(!ProjectedScreenReflectionGpu::supported(backend,{35,19},2),"Unsupported footprint format accepted");
    rejects([&]{ProjectedScreenReflectionGpu p(backend,{35,19},2,shaders);});backend.unavailableStorageFormats.clear();
    rejects([&]{ProjectedScreenReflectionGpu p(backend,{35,19},2,shaders,7,3);});
    auto invalidShaders=shaders;invalidShaders.ray.clear();rejects([&]{ProjectedScreenReflectionGpu p(backend,{35,19},2,invalidShaders);});
    require(!backend.allocator().statistics().liveBuffers&&!backend.allocator().statistics().liveImages,"Partially constructed composition leaked resources");
    {
        ProjectedScreenReflectionGpu resource(backend,{35,19},2,shaders,7,2);
        require(resource.levels()==5&&resource.output(0).extent.width==17&&resource.output(0).extent.height==9,"Composition extent or levels differ");
        require(resource.imageByteBudget()==16090,"Composition image accounting differs");
        require(resource.output(0).layout==VK_IMAGE_LAYOUT_GENERAL&&resource.output(0).view!=resource.output(1).view,"Composition output slots must be independent storage reads");
        require(resource.prepared(0).depth!=resource.prepared(1).depth&&resource.signal(0).color!=resource.signal(1).color,"Composition diagnostic resources alias slots");
        ProjectedScreenReflectionInputs inputs{reinterpret_cast<VkImageView>(900),reinterpret_cast<VkImageView>(901),VK_NULL_HANDLE,{35,19}};
        ReflectionTraceView camera;camera.projection=internal::perspective(1.1F,35.F/19,.1F,1000);camera.worldToClip=camera.projection;camera.right={1,0,0};camera.up={0,1,0};camera.forward={0,0,-1};
        ProjectedScreenReflectionParameters parameters;rhi::NullCommandRecorder commands;const auto writes=backend.descriptorImageWrites.size();
        auto bad=inputs;bad.extent.width=34;rejects([&]{resource.record(commands,0,bad,camera,parameters);});
        bad=inputs;bad.surface=resource.prepared(1).depth;rejects([&]{resource.record(commands,0,bad,camera,parameters);});
        bad=inputs;bad.color=resource.signal(1).color;rejects([&]{resource.record(commands,0,bad,camera,parameters);});
        bad=inputs;bad.motion=resource.output(1).view;rejects([&]{resource.record(commands,0,bad,camera,parameters);});
        auto p=parameters;p.maxSteps=0;rejects([&]{resource.record(commands,0,inputs,camera,p);});
        p=parameters;p.edgeFade=std::numeric_limits<float>::quiet_NaN();rejects([&]{resource.record(commands,0,inputs,camera,p);});
        p=parameters;p.strength=2;rejects([&]{resource.record(commands,0,inputs,camera,p);});
        auto invalid=camera;invalid.projection=internal::orthographic(-1,1,-1,1,.1F,1000);rejects([&]{resource.record(commands,0,inputs,invalid,parameters);});
        invalid=camera;invalid.projection[10]=-1;invalid.projection[14]=-.1F;rejects([&]{resource.record(commands,0,inputs,invalid,parameters);});
        ProjectedScreenReflectionHistory history{1,reinterpret_cast<VkImageView>(902),camera};
        rejects([&]{resource.record(commands,0,inputs,camera,parameters,&history);});
        bad=inputs;bad.motion=reinterpret_cast<VkImageView>(903);history.slot=0;rejects([&]{resource.record(commands,0,bad,camera,parameters,&history);});
        history.slot=2;rejects([&]{resource.record(commands,0,bad,camera,parameters,&history);});
        history.slot=1;history.color=resource.prepared(1).mask;rejects([&]{resource.record(commands,0,bad,camera,parameters,&history);});
        rejects([&]{resource.record(commands,2,inputs,camera,parameters);});
        require(commands.calls.empty()&&backend.descriptorImageWrites.size()==writes,"Composition rejection must precede mutations");
        resource.record(commands,0,inputs,camera,parameters);require(!commands.calls.empty(),"Current-only composition did not record");
        commands.calls.clear();resource.record(commands,0,inputs,camera,parameters);require(!commands.calls.empty(),"Canceled composition could not re-record");
        history={0,reinterpret_cast<VkImageView>(902),camera};resource.record(commands,1,bad,camera,parameters,&history);
    }
    require(!backend.allocator().statistics().liveBuffers&&!backend.allocator().statistics().liveImages,"Composition resources leaked");
    std::cout<<"PASS: projected composition budgets, inputs, history, mutation boundaries and retry\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
