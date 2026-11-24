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
 {SurfaceRadianceCache cache(backend,{8,8},2,SurfaceRadianceCache::kMaximumBytes,false,true,8);
  require(cache.selectionBytes()==112&&cache.selectionDrawCapacity()==8,"Per-draw flags share a bounded six-word header");
  cache.beginSelectionFrame(0,true,3);auto* words=static_cast<std::uint32_t*>(cache.selectionBuffer(0).mapped);
  require(words[0]==0&&words[1]==3&&words[6]==0&&words[8]==0,"Only the current draw range is initialized");
  words[0]=2;words[2]=2;words[3]=1;rhi::NullCommandRecorder commands;cache.recordSelectionReady(commands,0);
  cache.beginSelectionFrame(0,true,1);const auto stats=cache.selectionStatistics();
  require(stats.frames==1&&stats.fastFrames==1&&stats.fastDraws==2&&stats.fullDraws==1&&stats.lastFastDraws==2&&stats.lastFullDraws==1,"Retired mixed draws retain distinct totals");
  bool rejected=false;try{cache.beginSelectionFrame(0,true,9);}catch(const std::length_error&){rejected=true;}require(rejected,"Draw range remains within capacity");
 }
 for(const auto capacity:{1U,65537U}){bool rejected=false;try{SurfaceRadianceCache cache(backend,{8,8},2,SurfaceRadianceCache::kMaximumBytes,false,capacity>1,capacity);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Unsupported draw predicate storage is rejected");}
 require(backend.allocator().statistics().liveBuffers==before.liveBuffers&&backend.allocator().statistics().liveImages==before.liveImages,"Predicate resources retire with the cache");
 std::cout<<"Radiance frame predicates: fail-closed initialization, permission, retired slots and lifetime passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
