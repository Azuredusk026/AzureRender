#include "render/SurfaceRadianceCache.hpp"
#include "rhi/NullRhi.hpp"
#include <cstring>
#include <iostream>
using namespace azurerender;
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main()try{
 rhi::NullRhi backend;const auto before=backend.allocator().statistics();
 {SurfaceRadianceCache plain(backend,{8,8},2);require(!plain.statisticsEnabled(),"Ordinary cache has no counter buffers");}
 {SurfaceRadianceCache cache(backend,{8,8},2,SurfaceRadianceCache::kMaximumBytes,true);
  require(cache.statisticsEnabled(),"Explicit diagnostics own counter buffers");
  require(cache.statisticsBytes()==48,"Two independent six-word frame slots");
  cache.beginStatisticsFrame(0);cache.beginStatisticsFrame(1);
  auto write=[&](unsigned slot,std::array<std::uint32_t,6> values){
   const auto& buffer=cache.statisticsBuffer(slot);require(buffer.mapped!=nullptr,"Host readback is mapped");
   std::memcpy(buffer.mapped,values.data(),sizeof(values));rhi::NullCommandRecorder commands;
   cache.recordStatisticsReady(commands,slot);
  };
  write(0,{50,40,30,20,10,2});cache.beginStatisticsFrame(1);
  require(cache.statistics().frames==0,"Unretired frame slots stay isolated");
  cache.beginStatisticsFrame(0);auto count=cache.statistics();
  require(count.frames==1&&count.producerStores==50&&count.consumerVisits==40&&count.eligibleRequests==30
   &&count.pixelMatches==20&&count.quadMatches==10&&!count.overflow,"Retired counters preserve their scope");
  std::array<std::uint32_t,6> zero{};require(std::memcmp(cache.statisticsBuffer(0).mapped,zero.data(),sizeof(zero))==0,"Reuse resets every counter word");
  cache.beginStatisticsFrame(0);require(cache.statistics().frames==1,"Empty or cancelled recordings do not duplicate results");
  write(1,{1,4,3,2,1,3});cache.beginStatisticsFrame(1);count=cache.statistics();
  require(count.frames==2&&count.producerStores==51&&count.consumerVisits==44&&count.overflow,"Overflow remains explicit across frames");
 }
 require(backend.allocator().statistics().liveBuffers==before.liveBuffers&&backend.allocator().statistics().liveImages==before.liveImages,"All diagnostic and image allocations retire");
 std::cout<<"Radiance statistics frame slots, reset, overflow and lifetime passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
