#version 450
#extension GL_GOOGLE_include_directive : require
#if defined(AZURE_BINDLESS)
#extension GL_EXT_nonuniform_qualifier : require
#endif
#include "material_primary_uv.glsl"
#include "node_opacity.glsl"

layout(push_constant) uniform Material {
    float alphaCutoff;
    int alphaMode;
    layout(offset=116) uint features;
    layout(offset=136) uint textureBase;
    layout(offset=140) float nodeOpacity;
} material;

#if defined(AZURE_BINDLESS)
layout(binding=1) uniform sampler2D textures[];
#define BASE textures[nonuniformEXT(material.textureBase)]
#define COVERAGE textures[nonuniformEXT(material.textureBase+2u)]
#else
layout(binding=1) uniform sampler2D baseColor;
layout(binding=3) uniform sampler2D independentCoverage;
#define BASE baseColor
#define COVERAGE independentCoverage
#endif

layout(location=2) in vec2 textureCoordinate;
layout(location=3) in vec3 worldPosition;
layout(location=14) in vec3 opacityCoordinate;
layout(location=0) out float referenceDepth;

float coverageAlpha(sampler2D image){
    const uint mirroredCoarseUv=1u<<27u;
    if((material.features&mirroredCoarseUv)==0u)return texture(image,textureCoordinate).a;
    return azureSampleCoverageAlpha(image,textureCoordinate,material.features);
}

void main(){
    azurePreparePrimaryUv(textureCoordinate);
    if(!azureNodeOpacityCovered(opacityCoordinate,material.nodeOpacity))discard;
    if(material.alphaMode==1){
        const uint independentCoverageBit=1u<<25u;
        float alpha=(material.features&independentCoverageBit)!=0u?coverageAlpha(COVERAGE):coverageAlpha(BASE);
        if(alpha<material.alphaCutoff)discard;
    }
    referenceDepth=gl_FragCoord.z;
}
