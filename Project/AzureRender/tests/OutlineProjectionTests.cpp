#include "assets/OutlineProjectionData.hpp"
#include "render/OutlineProjectionResources.hpp"
#include "rhi/NullRhi.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace azurerender;
void requireOutline(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
int main() try {
    static_assert(sizeof(PackedOutlineProjectionRecord)==64);
    static_assert(alignof(PackedOutlineProjectionRecord)==16);
    LoadedAsset first;first.vertices.resize(3);first.indices={0,1,2};first.materials.resize(1);
    AssetPrimitive primitive;primitive.indexCount=3;primitive.hasTexcoord1=true;first.primitives.push_back(primitive);
    first.texcoord1={{{.1F,.2F}},{{-.5F,.25F}},{{0,1}}};
    auto& shape=first.materials[0].outlineSurface;shape.present=true;shape.depthPrepass=true;
    shape.normalSource=AssetMaterial::OutlineSurface::NormalSource::TangentUv1;shape.width=.75F;shape.depthOffset=.02F;
    LoadedAsset second;second.vertices.resize(4);second.materials.resize(2);
    second.materials[0].outlineSurface.present=true;second.materials[0].outlineSurface.width=1.5F;
    const auto data=buildOutlineProjectionData({&first,&second},4096);
    requireOutline(data.header[0]==1&&data.header[1]==3&&data.header[2]==52&&data.header[3]==3,
        "Outline header must declare version, record count and the compact UV1 stream");
    requireOutline(data.records.size()==3&&data.texcoord1==first.texcoord1,"Only declared consumers upload UV1");
    requireOutline(data.records[0].parameters==std::array<float,4>{.75F,.5F,.02F,1},"Shape fields must retain their explicit record");
    requireOutline(data.records[0].indices==std::array<std::uint32_t,4>{1,1,52,3},"UV1 addresses must use absolute word offsets");
    requireOutline(data.records[1].parameters[0]==1.5F&&data.records[1].indices[2]==0&&data.records[1].indices[3]==0,
        "Geometry normals must not borrow another asset's stream");
    requireOutline(data.records[2].parameters[3]==0,"An absent profile must keep the fixed-pixel path");
    requireOutline(data.byteSize()==232,"Storage size includes the header, records and optional data");
    auto octahedral=first;
    octahedral.materials[0].outlineSurface.normalSource=AssetMaterial::OutlineSurface::NormalSource::TangentOctahedralUv1;
    const auto fullSphere=buildOutlineProjectionData({&octahedral},4096);
    requireOutline(fullSphere.records[0].indices[0]==2&&fullSphere.header[3]==3
        &&fullSphere.texcoord1==first.texcoord1,"Octahedral records must retain their independent encoding and UV1 stream");
    for(auto budget:{std::uint64_t(231),std::uint64_t(16)}) {
        bool rejected=false;try{(void)buildOutlineProjectionData({&first,&second},budget);}catch(const std::invalid_argument&){rejected=true;}
        requireOutline(rejected,"Oversized tables must fail before GPU allocation");
    }
    LoadedAsset third=first;const auto pair=buildOutlineProjectionData({&first,&third},4096);
    requireOutline(pair.header[2]==36&&pair.header[3]==6&&pair.records[1].indices[2]==42,
        "A second asset must receive its own vertex base");
    rhi::NullRhi backend;SampledTextureCache cache(backend.allocator(),backend);
    {
        OutlineProjectionResources projection(backend.allocator(),4096);
        auto invalidData=data;invalidData.header[0]=2;bool invalidRejected=false;
        try{projection.initialize(invalidData,cache);}catch(const std::invalid_argument&){invalidRejected=true;}
        requireOutline(invalidRejected&&backend.allocator().statistics().liveBuffers==0&&!shape.maskPixels.empty(),
            "A failed initialization must retain inputs and permit a valid retry");
        projection.initialize(data,cache);
        requireOutline(projection.recordCount()==3&&projection.coordinateCount()==3&&projection.buffer().size==232,
            "The GPU owner must retain the packed table dimensions");
        const auto* header=static_cast<const std::uint32_t*>(projection.buffer().mapped);
        requireOutline(header&&header[0]==1&&header[1]==3&&header[2]==52,"GPU storage must start with the typed header");
        requireOutline(projection.masks().size()==3&&projection.masks()[0].view==projection.masks()[1].view,
            "Equal masks must share the public texture cache");
        requireOutline(backend.allocator().statistics().liveBuffers==1&&cache.statistics().uniqueTextures==1,
            "Projection storage and mask images must have explicit owners");
        projection.reset();
        requireOutline(backend.allocator().statistics().liveBuffers==0&&backend.allocator().statistics().liveImages==1,
            "Reset retires projection storage while the cache retains its sampled images");
    }
    cache.clear();requireOutline(backend.allocator().statistics().liveImages==0,"The texture cache must release mask images");
    first.primitives[0].hasTexcoord1=false;
    bool rejected=false;try{(void)buildOutlineProjectionData({&first},4096);}catch(const std::invalid_argument&){rejected=true;}
    requireOutline(rejected,"Missing UV1 must fail before packing");
    first.primitives[0].hasTexcoord1=true;shape.width=std::numeric_limits<float>::quiet_NaN();rejected=false;
    try{(void)buildOutlineProjectionData({&first},4096);}catch(const std::invalid_argument&){rejected=true;}
    requireOutline(rejected,"Procedural typed inputs must reject non-finite shape data");
    std::cout<<"Outline record and optional-stream checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
