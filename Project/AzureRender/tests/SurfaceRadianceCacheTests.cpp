#include "render/SurfaceRadianceCache.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}}
int main()try{
    rhi::NullRhi backend;
    require(SurfaceRadianceCache::supported(backend,{1920,1080},2),"Native attachments fit");
    require(SurfaceRadianceCache::supported(backend,{2560,1440},2),"Frozen image attachments fit");
    require(!SurfaceRadianceCache::supported(backend,{3840,2160},2),"Large view uses full shading");
    require(!SurfaceRadianceCache::supported(backend,{UINT32_MAX,UINT32_MAX},8),"Budget arithmetic cannot wrap");
    require(!SurfaceRadianceCache::supported(backend,{64,48},0)&&!SurfaceRadianceCache::supported(backend,{64,48},9),"Slot count bounded");
    require(!SurfaceRadianceCache::supported(backend,{0,48},2),"Empty extent rejected");
    backend.unavailableSampledFormats.insert(VK_FORMAT_R32G32B32A32_UINT);
    require(!SurfaceRadianceCache::supported(backend,{64,48},2),"Integer identity format required");backend.unavailableSampledFormats.clear();
    const auto key=surfaceRadianceIdentity(.25F,7,123,11,19);
    require(key.words==std::array<std::uint32_t,4>{0x3e800000U,7,156,19},"Full triangle identity");
    require(surfaceRadianceIdentity(0.F,0,0,0,0).words!=surfaceRadianceIdentity(-0.F,0,0,0,0).words,"Depth identity keeps signed zero");
    require(!surfaceRadianceIdentity(.5F,0,UINT32_MAX-1,1,0).valid(),"Index addition cannot wrap");
    require(!surfaceRadianceIdentity(std::numeric_limits<float>::infinity(),0,0,0,0).valid(),"Non-finite depth rejected");
    require(!surfaceRadianceIdentity(std::numeric_limits<float>::quiet_NaN(),0,0,0,0).valid(),"NaN depth rejected");
    const auto before=backend.allocator().statistics().liveImages;
    {SurfaceRadianceCache cache(backend,{64,48},2);require(cache.bytes()==64U*48U*64U*2U,"Byte accounting");
        require(backend.allocator().statistics().liveImages==before+8,"Four images per slot");
        for(unsigned kind=0;kind<4;++kind)require(cache.texture(0,kind).image.image!=cache.texture(1,kind).image.image,"Frame slots independent");
        bool rejected=false;try{cache.texture(2,0);}catch(const std::out_of_range&){rejected=true;}require(rejected,"Slot bounds checked");
        rejected=false;try{cache.texture(0,4);}catch(const std::out_of_range&){rejected=true;}require(rejected,"Attachment bounds checked");}
    require(backend.allocator().statistics().liveImages==before,"Images released");
    require(SurfaceRadianceCache::clearValue(3).color.uint32[2]==UINT32_MAX,"Clear identity rejects reuse");
    std::cout<<"Radiance cache identity, formats, budget and lifetime passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
