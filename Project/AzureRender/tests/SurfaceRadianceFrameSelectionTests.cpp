#include "render/SurfaceRadianceCache.hpp"
#include "rhi/NullRhi.hpp"
#include <cstring>
#include <iostream>
using namespace azurerender;
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main()try{
 rhi::NullRhi backend;const auto before=backend.allocator().statistics();
 {SurfaceRadianceCache plain(backend,{8,8},2);require(!plain.selectionEnabled(),"Ordinary cache owns no selection flags");}
 {SurfaceRadianceCache cache(backend,{8,8},2,SurfaceRadianceCache::kMaximumBytes,false,true);
  require(cache.selectionEnabled()&&cache.selectionBytes()==8,"Two bounded independent predicate slots");
  auto value=[&](unsigned slot){std::uint32_t n=0;std::memcpy(&n,cache.selectionBuffer(slot).mapped,4);return n;};
  require(value(0)==1&&value(1)==1,"New predicate slots select complete shading");
  cache.beginSelectionFrame(0,true);require(value(0)==0,"Explicit permission resets the validation predicate");
  rhi::NullCommandRecorder commands;cache.recordSelectionReady(commands,0);
  cache.beginSelectionFrame(0,true);require(cache.selectionStatistics().frames==0,"Unsubmitted command recordings have no GPU completion marker");
  const std::uint32_t completed=2;std::memcpy(cache.selectionBuffer(0).mapped,&completed,4);cache.recordSelectionReady(commands,0);
  cache.beginSelectionFrame(1);require(cache.selectionStatistics().frames==0&&value(1)==1,"Other slots remain isolated and fail closed");
  cache.beginSelectionFrame(0);auto s=cache.selectionStatistics();
  require(s.frames==1&&s.fastFrames==1&&s.fullFrames==0&&!s.lastFallback&&value(0)==1,"Retirement reads before fail-closed reset");
  const std::uint32_t mismatch=UINT32_MAX;std::memcpy(cache.selectionBuffer(0).mapped,&mismatch,4);cache.recordSelectionReady(commands,0);
  cache.beginSelectionFrame(0,true);s=cache.selectionStatistics();
  require(s.frames==2&&s.fastFrames==1&&s.fullFrames==1&&s.lastFallback,"Every nonzero predicate selects complete shading");
  cache.beginSelectionFrame(0);require(cache.selectionStatistics().frames==2,"Cancelled recordings add no completed frame");
 }
 require(backend.allocator().statistics().liveBuffers==before.liveBuffers&&backend.allocator().statistics().liveImages==before.liveImages,"Predicate resources retire with the cache");
 std::cout<<"Radiance frame predicates: fail-closed initialization, permission, retired slots and lifetime passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
