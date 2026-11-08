#include "render/GpuPredicateDrawSelection.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(int argc,char** argv)try{
 require(argc==2,"Expected selection compute program");test::VulkanComputeFixture host;
 constexpr std::uint32_t capacity=137,count=131;
 {
  GpuPredicateDrawSelection owner(*host.backend,capacity,2,readBinaryFile(argv[1]));
  auto source=host.allocator.createBuffer(count*20,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
  auto predicate=host.allocator.createBuffer(4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false);
  auto readback=host.allocator.createBuffer(capacity*40,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
  auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){host.allocator.destroyBuffer(source);host.allocator.destroyBuffer(predicate);host.allocator.destroyBuffer(readback);});
  std::array<VkDrawIndexedIndirectCommand,count> commands{};
  for(unsigned i=0;i<count;++i)commands[i]={i%7?i*3:0,i%4,i*11,std::numeric_limits<std::int32_t>::min()+std::int32_t(i*19),i*31};
  std::memcpy(source.mapped,commands.data(),sizeof(commands));host.allocator.flush(source,0,source.size);
  const std::array<std::pair<std::uint32_t,bool>,5> cases{{{0,true},{1,true},{UINT32_MAX,true},{0,false},{1,false}}};
  unsigned at=0;
  for(auto [fallback,permitted]:cases){
   const auto ticket=owner.prepare(at++%2,source,predicate,count,permitted);
   host.submit([&](rhi::ICommandRecorder& recorder,VkCommandBuffer raw){
    recorder.bufferBarrier({source.buffer,0,source.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    recorder.bufferBarrier({predicate.buffer,0,4,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT});
    vkCmdFillBuffer(raw,predicate.buffer,0,4,fallback);
    recorder.bufferBarrier({predicate.buffer,0,4,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    owner.record(recorder,ticket);const auto& output=owner.output(ticket.slot);
    recorder.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
    const VkBufferCopy copy{0,0,output.size};vkCmdCopyBuffer(raw,output.buffer,readback.buffer,1,&copy);
    recorder.bufferBarrier({readback.buffer,0,readback.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
   });
   host.allocator.invalidate(readback,0,readback.size);
   const auto* full=static_cast<const VkDrawIndexedIndirectCommand*>(readback.mapped);
   const auto* fast=reinterpret_cast<const VkDrawIndexedIndirectCommand*>(static_cast<const char*>(readback.mapped)+owner.fastOffset());
   for(unsigned i=0;i<count;++i){auto expectedFull=commands[i],expectedFast=commands[i];
    if(permitted&&fallback==0)expectedFull.instanceCount=0;else expectedFast.instanceCount=0;
    require(std::memcmp(full+i,&expectedFull,20)==0&&std::memcmp(fast+i,&expectedFast,20)==0,"GPU selection changed geometry, order or fallback policy");
   }
  }
 }
 require(host.errors.load()==0,"Selection synchronization validation failed");
 require(!host.allocator.statistics().liveBuffers&&!host.allocator.statistics().liveImages,"Selection resources leaked");
 std::cout<<"GPU predicate selection: device="<<host.properties.deviceName<<" cases=5 commands=655 regions=2 validationErrors=0 liveBuffers=0 liveImages=0\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
