#include "render/ShadowTileBounds.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <algorithm>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()try{
 rhi::NullRhi backend;
 for(unsigned batch:{1U,8U}){
  ShadowTileBounds bounds(backend);bounds.initialize({0,0,0,0},VK_NULL_HANDLE,VK_NULL_HANDLE,40,1,batch);
  require(bounds.tilesPerGroup()==batch&&bounds.depthBoundsBytes()==200&&bounds.objectRecordsOffset()==208,"Batching preserves the tile and object-record storage contract");
  rhi::NullCommandRecorder commands;bounds.record(commands);
  const auto found=std::find_if(commands.calls.begin(),commands.calls.end(),[](const auto& call){return call.name=="dispatch";});
  require(found!=commands.calls.end()&&found->detail==(batch==1?"5x5x1":"1x5x1"),"Tail groups use ceiling division without extending output ownership");
 }
 for(unsigned batch:{0U,2U,16U}){
  const auto before=backend.allocator().statistics().bufferAllocations;bool rejected=false;
  try{ShadowTileBounds bounds(backend);bounds.initialize({0,0,0,0},VK_NULL_HANDLE,VK_NULL_HANDLE,40,0,batch);}catch(const std::invalid_argument&){rejected=true;}
  require(rejected&&backend.allocator().statistics().bufferAllocations==before,"Unsupported batch profiles reject before allocation");
 }
 require(backend.allocator().statistics().liveBuffers==0,"Batch resources release");
 std::cout<<"Shadow batch dispatch, tail ownership, invalid profiles and storage layout passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
