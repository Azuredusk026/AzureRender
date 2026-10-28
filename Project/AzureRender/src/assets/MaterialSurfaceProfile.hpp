#pragma once
#include "assets/GltfLoader.hpp"
#include <cstddef>
#include <type_traits>

namespace azurerender {
// Binding 27 is a versioned union, indexed by the stable material order.
struct alignas(16) PackedMaterialSurfaceProfile {
    static constexpr unsigned kLayoutVersion=17;
    using Row=std::array<float,4>;
    std::array<Row,21> rows{};
};
static_assert(std::is_standard_layout_v<PackedMaterialSurfaceProfile>);
static_assert(sizeof(PackedMaterialSurfaceProfile)==336);
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
    rows[4][3]=hair.tangentSign;
    if(hair.virtualLight)rows[6]={1,hair.virtualBlend,hair.objectGain,0};
    if(hair.line.present){
        rows[7]={hair.line.frequency,hair.line.strandShift,hair.line.range,hair.line.depth};
        rows[8]={hair.line.alternation,hair.line.saturation,1,hair.line.mainDiffuse?1.F:0.F};rows[9]=hair.line.uvTransform;
    }
    if(hair.lighting.present) {
        const auto& light=hair.lighting;
        const auto source=light.characterVisibility==AssetMaterial::CharacterVisibility::One
            ?0U:static_cast<unsigned>(light.characterVisibility)+1U;
        rows[6][0]=static_cast<float>((hair.virtualLight?1U:0U)|(source<<1U));
        rows[5][3]=light.shadowScale;rows[6][3]=1;
        rows[10]={light.ambientColor[0],light.ambientColor[1],light.ambientColor[2],light.shadowBrightness};
        rows[11]={light.mainColor[0],light.mainColor[1],light.mainColor[2],light.shadowSaturation};
    }
    return result;
}

inline PackedMaterialSurfaceProfile packEyeSurfaceProfile(const AssetMaterial::EyeSurface& eye) {
    PackedMaterialSurfaceProfile result{{{{eye.parallaxScale,eye.matcapNormalScale,eye.alphaPremultiply,eye.matcapShadowFloor},
        {eye.highlightColor[0],eye.highlightColor[1],eye.highlightColor[2],0},
        {eye.scatteringColor[0],eye.scatteringColor[1],eye.scatteringColor[2],0},
        eye.matcapColor,{eye.directWeights[0],eye.directWeights[1],eye.directWeights[2],0},eye.uvScaleOffset}}};
    if(eye.nprLighting){
        result.rows[6]={eye.shadowScale,0,0,1};
        result.rows[7]={eye.ambientColor[0],eye.ambientColor[1],eye.ambientColor[2],0};
        result.rows[8]={eye.mainColor[0],eye.mainColor[1],eye.mainColor[2],0};
    }
    return result;
}

inline PackedMaterialSurfaceProfile packMaterialSurfaceProfile(const AssetMaterial& material) {
    PackedMaterialSurfaceProfile result;
    if(material.unlitTextureLayers.present) {
        const auto& p=material.unlitTextureLayers;auto& r=result.rows;
        r[0]=p.layers[0].scaleOffset;r[1]=p.layers[0].rotation;
        r[2]={p.layers[0].speed[0],p.layers[0].speed[1],0,0};
        if(p.layerCount==2) {r[2][2]=p.layers[1].speed[0];r[2][3]=p.layers[1].speed[1];
            r[3]=p.layers[1].scaleOffset;r[4]=p.layers[1].rotation;}
        r[5]={p.clockRate,p.clockOffsetSeconds,p.softDistance,p.softBias};
        r[6]=p.tint;r[7]=p.blendTint;
        r[8]={p.highlightThreshold,p.highlightGain,p.postExposureWeight,p.globalColorWeight};
        r[9]={p.layers[0].sampleMode==UnlitTextureLayer::SampleMode::RedAsAlpha?1.F:0.F,
            p.layers[1].sampleMode==UnlitTextureLayer::SampleMode::RedAsAlpha?1.F:0.F,
            p.layerCount==1?0.F:p.layers[1].role==UnlitTextureLayer::Role::Mask?1.F:2.F,p.softIntersectionEnabled?1.F:0.F};
        if(p.viewDepthFadePresent){r[10]=p.viewDepthFade;r[11][0]=1;}
        r[11][1]=p.useVertexColor?1.F:0.F;
        if(p.viewCoveragePresent){r[11][2]=1;r[12]=p.viewCoverage;}
        if(p.coverageLayer.present) {
            const auto& coverage=p.coverageLayer;const auto& layer=coverage.sample;r[11][3]=1;
            r[13]=layer.scaleOffset;r[14]=layer.rotation;
            r[15]={layer.speed[0],layer.speed[1],layer.sampleMode==UnlitTextureLayer::SampleMode::RedAsAlpha?1.F:0.F,0};
            r[16]={coverage.threshold,coverage.gain,0,0};
        }
        if(p.baseUvDistortion.present) {
            const auto& d=p.baseUvDistortion;const auto& layer=d.sample;r[16][2]=1;
            r[17]=layer.scaleOffset;r[18]=layer.rotation;
            r[19]={layer.speed[0],layer.speed[1],d.channels==UnlitTextureLayers::BaseUvDistortion::Channels::Rg?1.F:0.F,0};
            r[20]={d.scale[0],d.scale[1],d.bias[0],d.bias[1]};
        }
        r[16][3]=p.fogMode==UnlitTextureLayers::FogMode::Scene?1.F:0.F;
        r[15][3]=p.composite==UnlitTextureLayers::Composite::Additive?1.F:0.F;
        return result;
    }
    if(material.nprDiffuse.present) {
        result.rows[0]={material.nprDiffuse.shadowScale,static_cast<float>(material.nprDiffuse.shadowMask),
            static_cast<float>(material.nprDiffuse.baseAo),
            static_cast<float>(material.nprDiffuse.indirectNormal)};
        result.rows[1][0]=static_cast<float>(material.nprDiffuse.rimResponse);
        result.rows[3][0]=static_cast<float>(material.nprDiffuse.characterVisibility);
        result.rows[3][1]=material.nprDiffuse.localDiffuseFloorScale;
        if(material.nprDiffuse.mainRamp.present) {
            result.rows[1][1]=1;
            result.rows[1][2]=material.nprDiffuse.mainRamp.bias;
        }
        if(material.nprDiffuse.viewTint.present) {
            const auto& tint=material.nprDiffuse.viewTint;
            result.rows[2]={tint.color[0],tint.color[1],tint.color[2],tint.scale};
        }
        if(material.nprDiffuse.lighting.present) {
            const auto& light=material.nprDiffuse.lighting;
            result.rows[10]={light.ambientColor[0],light.ambientColor[1],light.ambientColor[2],1};
            result.rows[11]={light.mainColor[0],light.mainColor[1],light.mainColor[2],0};
        }
    } else {
        result=material.eyeSurface.present?packEyeSurfaceProfile(material.eyeSurface)
            :packProjectedHairSurfaceProfile(material.projectedHairRamp);
    }
    if(material.materialClass==AssetMaterialClass::Generic||material.materialClass==AssetMaterialClass::Fabric
        ||material.materialClass==AssetMaterialClass::Metal) {
        result.rows[5][2]=static_cast<float>(material.specularInput.source);
        result.rows[5][3]=material.specularInput.scale;
        if(material.specularRamp.present) {
            const auto& ramp=material.specularRamp;
            result.rows[4]={0,0,ramp.iridescence,ramp.flipV?-1.F:1.F};
        }
    }
    return result;
}
}
