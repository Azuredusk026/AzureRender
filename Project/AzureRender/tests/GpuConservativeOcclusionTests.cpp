#include "render/GpuConservativeOcclusion.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void reject(F action){bool failed=false;try{action();}catch(const std::exception&){failed=true;}require(failed,"Invalid occlusion request accepted");}
}
int main()try{
    rhi::NullRhi backend;std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    auto source=backend.allocator().createBuffer(16*sizeof(VkDrawIndexedIndirectCommand),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    const auto count=backend.allocator().statistics().liveBuffers;
    {
        GpuConservativeOcclusion owner(backend,16,2,code);
        require(owner.retiredZeroCommandCount(0)==0,"Unrecorded output must not report rejected work");
        require(owner.bytes()<1024*1024,"Small occlusion pool must be bounded");
        DepthPyramidOutput depth;depth.image=(VkImage)101;depth.view=(VkImageView)102;depth.sampler=(VkSampler)103;depth.extent={65,33};depth.levels=7;
        GpuOcclusionBounds bounds;bounds.minimum={-.25F,-.25F,.8F,0};bounds.maximum={.25F,.25F,.9F,0};
        const VkViewport viewport{0,0,65,33,0,1};
        const auto first=owner.prepare(0,source,{bounds},depth,viewport);rhi::NullCommandRecorder recorder;
        owner.record(recorder,first);reject([&]{owner.record(recorder,first);});
        const auto next=owner.prepare(0,source,{bounds},depth,viewport);
        reject([&]{owner.record(recorder,first);});owner.record(recorder,next);
        auto other=owner.prepare(1,source,{bounds},depth,viewport);owner.record(recorder,other);
        require(owner.output(0).buffer!=owner.output(1).buffer,"Frame outputs must be independent");
        reject([&]{owner.prepare(2,source,{bounds},depth,viewport);});
        reject([&]{owner.prepare(0,source,std::vector<GpuOcclusionBounds>(17),depth,viewport);});
        reject([&]{owner.prepare(0,owner.output(0),{bounds},depth,viewport);});
        auto bad=bounds;bad.minimum[0]=std::numeric_limits<float>::quiet_NaN();reject([&]{owner.prepare(0,source,{bad},depth,viewport);});
        bad=bounds;bad.maximum[0]=-2;reject([&]{owner.prepare(0,source,{bad},depth,viewport);});
        auto invalidDepth=depth;invalidDepth.levels=8;reject([&]{owner.prepare(0,source,{bounds},invalidDepth,viewport);});
        reject([&]{owner.prepare(0,source,{bounds},depth,viewport,-1);});
        const auto empty=owner.prepare(0,{}, {},depth,viewport);owner.record(recorder,empty);require(empty.count==0,"Empty preparation must be a valid no-op");
    }
    require(backend.allocator().statistics().liveBuffers==count,"Occlusion pool leaked buffers");
    reject([&]{GpuConservativeOcclusion owner(backend,0,2,code);});
    reject([&]{GpuConservativeOcclusion owner(backend,1048576,8,code);});
    backend.allocator().destroyBuffer(source);
    require(!backend.allocator().statistics().liveBuffers,"External test input was not released");
    std::cout<<"Conservative occlusion: capacities, input ranges, frame tickets and ownership passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
