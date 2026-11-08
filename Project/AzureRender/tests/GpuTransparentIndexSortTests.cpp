#include "render/GpuTransparentIndexSort.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void reject(F action){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,"Invalid transparent GPU request accepted");}
}
int main()try{
    rhi::NullRhi backend;LoadedAsset asset;asset.vertices.resize(4);asset.indices={0,1,2,2,1,3};
    asset.vertices[0].position={0,0,1};asset.vertices[1].position={1,0,1};
    asset.vertices[2].position={0,1,2};asset.vertices[3].position={1,1,3};
    auto topology=prepareTransparentTopology(asset.indices,asset.vertices.size(),{{0,6}});
    GpuTransparentSortLimits limits;limits.vertices=32;limits.triangles=16;limits.instances=4;limits.morphWeights=16;limits.slots=2;
    std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    GpuTransparentSortShaders shaders{code,code,code,code};
    const auto before=backend.allocator().statistics().liveBuffers;
    {
        GpuTransparentIndexSort owner(backend,{{&asset,&topology}},limits,shaders);
        check(owner.bytes()<1024*1024,"Small topology uses bounded buffers");
        check(owner.matchesGeometry(0,asset,topology),"Immutable source layout matches");
        GpuTransparentSortInstance instance;instance.geometry=0;
        const auto ticket=owner.prepare(0,{}, {instance},{0,0,0},{0,0,1});
        check(ticket.vertexCount==4&&ticket.indexCount==6,"Prepared compact workload");
        rhi::NullCommandRecorder commands;owner.record(commands,ticket);
        reject([&]{owner.record(commands,ticket);});
        const auto next=owner.prepare(0,{}, {instance},{0,0,0},{0,0,1});
        reject([&]{owner.record(commands,ticket);});owner.record(commands,next);
        const auto other=owner.prepare(1,{}, {instance},{0,0,0},{0,0,1});
        check(owner.indices(0).buffer!=owner.indices(1).buffer,"Frame slots are independent");
        owner.record(commands,other);
        auto invalid=instance;invalid.geometry=1;reject([&]{owner.prepare(0,{}, {invalid},{},{0,0,1});});
        reject([&]{owner.prepare(0,{}, {instance},{std::numeric_limits<float>::quiet_NaN(),0,0},{0,0,1});});
        reject([&]{owner.prepare(2,{}, {instance},{},{0,0,1});});
        reject([&]{owner.prepare(0,{},std::vector<GpuTransparentSortInstance>(5),{},{0,0,1});});
    }
    check(backend.allocator().statistics().liveBuffers==before,"Owned buffers released");
    auto bad=limits;bad.vertices=0;reject([&]{GpuTransparentIndexSort owner(backend,{{&asset,&topology}},bad,shaders);});
    check(backend.allocator().statistics().liveBuffers==before,"Rejected owner allocated nothing");
    std::cout<<"GPU transparent source, capacity, frame tickets and lifecycle passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
