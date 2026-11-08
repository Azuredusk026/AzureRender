#include "render/GpuStableKeySort.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action){bool caught=false;try{action();}catch(const std::exception&){caught=true;}require(caught,"Invalid sort request accepted");}
}
int main()try{
    rhi::NullRhi backend;
    require(GpuStableKeySort::supported(1024,2),"Small sort fits");
    require(!GpuStableKeySort::supported(0,2)&&!GpuStableKeySort::supported(1048577,1),"Record bounds");
    require(!GpuStableKeySort::supported(1024,0)&&!GpuStableKeySort::supported(1024,9),"Slot bounds");
    require(!GpuStableKeySort::supported(1048576,8),"Total pool bound");
    std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    const auto before=backend.allocator().statistics().liveBuffers;
    {
        GpuStableKeySort sort(backend,1024,2,code);
        require(sort.bytes()==1024U*24U*2U,"Bounded scratch pool");
        auto source=backend.allocator().createBuffer(1024U*8U,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        rhi::NullCommandRecorder commands;
        rejects([&]{sort.record(commands,2,source,{4});});
        rejects([&]{sort.record(commands,0,source,{1024,1});});
        auto shortSource=source;shortSource.size=8;
        rejects([&]{sort.record(commands,0,shortSource,{2});});
        const auto first=sort.record(commands,0,source,{3,0,257,1});
        require(first.count==261&&first.buffer&&first.buffer->buffer,"Output count");
        const auto second=sort.record(commands,1,source,{261});
        require(first.buffer->buffer!=second.buffer->buffer,"Independent frame slots");
        rejects([&]{sort.record(commands,0,*first.buffer,{261});});
        const auto empty=sort.record(commands,0,{},{});require(empty.count==0&&!empty.buffer,"Empty input");
        backend.allocator().destroyBuffer(source);
    }
    require(backend.allocator().statistics().liveBuffers==before,"Sort resources released");
    std::cout<<"GPU stable key sort budget, segments, slots and lifecycle passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
