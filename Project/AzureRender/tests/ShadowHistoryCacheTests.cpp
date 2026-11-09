#include "render/ShadowHistoryCache.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <set>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void reject(F action){bool failed=false;try{action();}catch(const std::exception&){failed=true;}require(failed,"Invalid shadow history request accepted");}
}
int main()try{
    require(ShadowHistoryCache::fits({1920,1080},2,2),"Two native-resolution workflows fit the history budget");
    require(ShadowHistoryCache::fits({2560,1440},2,2),"Frozen-image extent fits the history budget");
    require(!ShadowHistoryCache::fits({3840,2160},2,2),"Large extent must use full shading");
    require(!ShadowHistoryCache::fits({3840,2160},2,2,UINT64_MAX),"Caller cannot exceed the hard pool budget");
    require(!ShadowHistoryCache::fits({UINT32_MAX,UINT32_MAX},8,2),"Byte arithmetic cannot wrap");
    for(unsigned slots:{0U,1U,9U})require(!ShadowHistoryCache::fits({32,32},slots,1),"History requires bounded distinct slots");
    require(!ShadowHistoryCache::fits({0,32},2,1)&&!ShadowHistoryCache::fits({32,32},2,0)&&!ShadowHistoryCache::fits({32,32},2,3),"Empty extent and workflow bounds are rejected");
    rhi::NullRhi backend;std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    const auto before=backend.allocator().statistics().liveBuffers;
    {
        ShadowHistoryCache owner(backend,{17,9},2,2,code);rhi::NullCommandRecorder commands;
        require(owner.bytes()==17U*9U*8U*4U,"Eight bytes per pixel and independent workflow/slot storage");
        std::set<VkBuffer> handles;
        for(unsigned slot=0;slot<2;++slot)for(unsigned workflow=0;workflow<2;++workflow){
            const auto& buffer=owner.buffer(slot,workflow);handles.insert(buffer.buffer);
            require(buffer.size==17U*9U*8U,"Each descriptor covers exactly its frame extent");
            require((buffer.usage&(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT))
                ==(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT),"History supports shader writes and diagnostic transfers");
        }
        require(handles.size()==4,"Workflow and frame outputs cannot alias");
        auto first=owner.prepareClear(0,0);owner.recordClear(commands,first);reject([&]{owner.recordClear(commands,first);});
        require(commands.calls.front().name=="bufferBarrier"&&commands.calls.back().name=="bufferBarrier","Clear is ordered between prior readers and fragment consumers");
        unsigned dispatches=0;for(const auto& call:commands.calls)dispatches+=call.name=="dispatch";require(dispatches==1,"Clear covers a non-workgroup-aligned extent in one dispatch");
        auto stale=owner.prepareClear(0,1);auto next=owner.prepareClear(0,1);reject([&]{owner.recordClear(commands,stale);});owner.recordClear(commands,next);
        auto wrong=owner.prepareClear(1,0);++wrong.generation;reject([&]{owner.recordClear(commands,wrong);});
        ShadowHistoryCache other(backend,{17,9},2,1,code);reject([&]{other.recordClear(commands,first);});
        reject([&]{owner.prepareClear(2,0);});reject([&]{owner.prepareClear(0,2);});reject([&]{owner.buffer(2,0);});
    }
    require(backend.allocator().statistics().liveBuffers==before,"All history storage releases with its owner");
    reject([&]{ShadowHistoryCache owner(backend,{32,32},1,1,code);});
    reject([&]{ShadowHistoryCache owner(backend,{32,32},2,1,{});});
    require(backend.allocator().statistics().liveBuffers==before,"Rejected initialization leaves no buffers");
    std::cout<<"Shadow history: byte budgets, workflows, slots, generation tickets, barriers and lifetime passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
