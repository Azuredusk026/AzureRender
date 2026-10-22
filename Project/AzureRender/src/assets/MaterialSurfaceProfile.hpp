#pragma once
#include "assets/GltfLoader.hpp"
#include <cstddef>
#include <type_traits>

namespace azurerender {
// Binding 27 is a versioned union, indexed by the stable material order.
struct alignas(16) PackedMaterialSurfaceProfile {
    static constexpr unsigned kLayoutVersion=1;
    using Row=std::array<float,4>;
    std::array<Row,6> rows{};
};
static_assert(std::is_standard_layout_v<PackedMaterialSurfaceProfile>);
static_assert(sizeof(PackedMaterialSurfaceProfile)==96);
static_assert(alignof(PackedMaterialSurfaceProfile)==16);
static_assert(offsetof(PackedMaterialSurfaceProfile,rows)==0);
static_assert(sizeof(PackedMaterialSurfaceProfile::Row)==16);
static_assert(offsetof(PackedMaterialSurfaceProfile,rows)+5*sizeof(PackedMaterialSurfaceProfile::Row)==80);

inline PackedMaterialSurfaceProfile packProjectedHairSurfaceProfile(const AssetMaterial::ProjectedHairRamp& hair) {
    PackedMaterialSurfaceProfile result;
    auto& rows=result.rows;
    rows[0]={hair.primaryShift,hair.secondaryShift,hair.intensity,hair.secondaryRange};
    rows[1]={hair.edgeFade,hair.specularNormalScale,hair.directionX,hair.surfaceNormalScale};
    rows[2]={hair.secondaryColor[0],hair.secondaryColor[1],hair.secondaryColor[2],
        hair.surfaceResponse==AssetMaterial::ProjectedHairRamp::SurfaceResponse::StrandDiffuse?1.F:0.F};
    for(unsigned axis=0;axis<3;++axis)for(unsigned component=0;component<3;++component)
        rows[axis+3][component]=hair.objectBasis[axis*3+component];
    rows[3][3]=hair.diffuseScale;
    return result;
}

inline PackedMaterialSurfaceProfile packEyeSurfaceProfile(const AssetMaterial::EyeSurface& eye) {
    return {{{{eye.parallaxScale,eye.matcapNormalScale,eye.alphaPremultiply,eye.matcapShadowFloor},
        {eye.highlightColor[0],eye.highlightColor[1],eye.highlightColor[2],0},
        {eye.scatteringColor[0],eye.scatteringColor[1],eye.scatteringColor[2],0},
        eye.matcapColor,{eye.directWeights[0],eye.directWeights[1],eye.directWeights[2],0},eye.uvScaleOffset}}};
}

inline PackedMaterialSurfaceProfile packMaterialSurfaceProfile(const AssetMaterial& material) {
    if(material.nprDiffuse.present) {
        PackedMaterialSurfaceProfile result;
        result.rows[0]={material.nprDiffuse.shadowScale,material.nprDiffuse.styleMaskGb?1.F:0.F,
            material.nprDiffuse.albedoAlpha?1.F:0.F,0};
        return result;
    }
    return material.eyeSurface.present?packEyeSurfaceProfile(material.eyeSurface)
        :packProjectedHairSurfaceProfile(material.projectedHairRamp);
}
}
