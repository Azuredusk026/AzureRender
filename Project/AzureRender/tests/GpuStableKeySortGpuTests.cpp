#include "support/VulkanComputeFixture.hpp"
#include "render/GpuStableKeySort.hpp"
#include "resources/BinaryFile.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <numeric>
#include <random>
using namespace azurerender;
namespace {
using Record=std::array<std::uint32_t,2>;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::uint32_t depthKey(float value){
    std::uint32_t bits=0;if(value!=0)std::memcpy(&bits,&value,4);
    return (bits&0x80000000U)?bits:(bits^0x7fffffffU);
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected the stable key sort shader");
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    std::uint64_t checkedRecords=0;unsigned cases=0;
    {
        constexpr unsigned capacity=32768;
        GpuStableKeySort sort(backend,capacity,2,readBinaryFile(argv[1]));
        auto input=host.allocator.createBuffer(capacity*sizeof(Record),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        auto readback=host.allocator.createBuffer(capacity*sizeof(Record),VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
            host.allocator.destroyBuffer(readback);host.allocator.destroyBuffer(input);});
        const auto run=[&](unsigned slot,const std::vector<unsigned>& segments,unsigned seed,bool depth){
            const auto count=std::accumulate(segments.begin(),segments.end(),0U);
            std::mt19937 random(seed);std::vector<Record> records(count);std::vector<float> depths(count);
            const std::array<float,9> values{0.F,-0.F,1.F,-1.F,1000.F,-1000.F,.5F,-.5F,1e-20F};
            for(unsigned i=0;i<count;++i){
                depths[i]=values[random()%values.size()];
                const auto key=depth?depthKey(depths[i]):i%11==0?UINT32_MAX:random()%31;
                records[i]={key,i*2654435761U+seed};
            }
            auto expected=records;unsigned base=0;
            for(auto size:segments){
                if(depth){
                    std::vector<unsigned> order(size);std::iota(order.begin(),order.end(),base);
                    std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return depths[a]>depths[b];});
                    for(unsigned i=0;i<size;++i)expected[base+i]=records[order[i]];
                }else std::stable_sort(expected.begin()+base,expected.begin()+base+size,
                    [](const auto& a,const auto& b){return a[0]<b[0];});
                base+=size;
            }
            std::memcpy(input.mapped,records.data(),records.size()*sizeof(Record));
            host.allocator.flush(input,0,records.size()*sizeof(Record));
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer native){
                commands.bufferBarrier({input.buffer,0,input.size,VK_PIPELINE_STAGE_HOST_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
                const auto result=sort.record(commands,slot,input,segments);check(result.count==count,"GPU sort output size");
                commands.bufferBarrier({result.buffer->buffer,0,count*sizeof(Record),VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
                const VkBufferCopy copy{0,0,count*sizeof(Record)};
                vkCmdCopyBuffer(native,result.buffer->buffer,readback.buffer,1,&copy);
                commands.bufferBarrier({readback.buffer,0,count*sizeof(Record),VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
            });
            host.allocator.invalidate(readback,0,count*sizeof(Record));
            check(std::memcmp(readback.mapped,expected.data(),count*sizeof(Record))==0,
                "GPU sort changed ordering, stability, payload, or segment boundaries");
            checkedRecords+=count;++cases;
        };
        run(0,{257,0,3,1025,17},13,false);
        run(1,{16385,4097,513,0,1,17},29,true);
        run(0,{1,1,1,1,1},43,false);
        run(0,{1,1,1,1,1},51,true);
    }
    check(host.errors.load()==0,"Vulkan validation errors");
    check(host.allocator.statistics().liveBuffers==0&&host.allocator.statistics().liveImages==0,"GPU sort resources leaked");
    std::cout<<"GPU stable key sort: device="<<host.properties.deviceName<<" cases="<<cases
        <<" records="<<checkedRecords<<" validationErrors="<<host.errors.load()<<" liveBuffers=0 liveImages=0\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
