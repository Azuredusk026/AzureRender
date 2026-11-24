#include "render/FogVolumeGpu.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <cstring>
#include <functional>
using namespace azurerender;
namespace {
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void reject(const std::function<void()>& function){try{function();}catch(const std::exception&){return;}throw std::runtime_error("Invalid fog GPU resource must fail");}
}
int main()try{
 rhi::NullRhi backend;backend.collectImageUploads=true;FogVolumeData asset;asset.dimensions={2,2,2};
 for(unsigned i=0;i<8;++i)for(auto v:{std::uint16_t(0x3800),std::uint16_t(0x3c00),std::uint16_t(0x4000),std::uint16_t(0x3c00)})asset.rgba16f.push_back(v);
 {
  FogVolumeGpu volume(backend,asset);
  check(backend.allocator().statistics().liveImages==1,"Fog owner retains one three-dimensional image");
  check(backend.allocator().statistics().liveBuffers==0,"Fog upload retires its staging buffer");
  check(backend.imageUploads.size()==1,"Fog has one complete upload");const auto& upload=backend.imageUploads.front();
  check(upload.width==2&&upload.height==2&&upload.depth==2,"Fog upload retains all three dimensions");
  check(upload.pixels.size()==asset.rgba16f.size()*2&&std::memcmp(upload.pixels.data(),asset.rgba16f.data(),upload.pixels.size())==0,"Fog upload preserves every half-float bit");
  volume.writeTexture(VK_NULL_HANDLE,3);
 }
 check(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"Fog releases all GPU allocations");
 backend.calls.clear();backend.unavailableSampledFormats.insert(VK_FORMAT_R16G16B16A16_SFLOAT);
 reject([&]{FogVolumeGpu invalid(backend,asset);});check(backend.calls.empty(),"Unsupported fog format fails before allocation");
 backend.unavailableSampledFormats.clear();auto badAsset=asset;badAsset.rgba16f[3]=0x4000;
 reject([&]{FogVolumeGpu rejectedResource(backend,badAsset);});check(backend.calls.empty(),"Invalid optical payload fails before allocation");
 std::cout<<"Fog volume ownership passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
