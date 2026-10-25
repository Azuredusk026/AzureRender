#version 450
#extension GL_GOOGLE_include_directive : require
#include "node_opacity.glsl"
#include "fragment_sampling.glsl"
#if defined(AZURE_SNAPSHOT_DEPTH)
#include "material_sampling.glsl"
#define DEPTH_FEATURES layout(offset=116) uint features;
#else
#define DEPTH_FEATURES
#endif
#if defined(AZURE_BINDLESS)
#extension GL_EXT_nonuniform_qualifier : require
layout(binding=1) uniform sampler2D textures[];
layout(push_constant) uniform Material { float alphaCutoff; int alphaMode; DEPTH_FEATURES layout(offset=136) uint textureBase; layout(offset=140) float nodeOpacity; } material;
#define BASE textures[nonuniformEXT(material.textureBase)]
#else
layout(binding=1) uniform sampler2D baseColor;
layout(push_constant) uniform Material { float alphaCutoff; int alphaMode; DEPTH_FEATURES layout(offset=140) float nodeOpacity; } material;
#define BASE baseColor
#endif
layout(binding=0) uniform Camera { vec4 position; vec4 forward; } camera;
layout(location=2) AZURE_SAMPLE in vec2 textureCoordinate;
layout(location=3) AZURE_SAMPLE in vec3 worldPosition;
layout(location=14) AZURE_SAMPLE in vec3 opacityCoordinate;
#if defined(AZURE_SNAPSHOT_DEPTH)
layout(location=1) out float linearDepth;
#else
layout(location=0) out float linearDepth;
#endif
void main() {
    if(!azureNodeOpacityCovered(opacityCoordinate,material.nodeOpacity)) discard;
#if defined(AZURE_SNAPSHOT_DEPTH)
    if(material.alphaMode==1){
        #if defined(AZURE_GEOMETRY_SAMPLES)
        float alpha=azureTemporalMaskAlpha(BASE,textureCoordinate,dFdx(textureCoordinate),dFdy(textureCoordinate));
#else
        float alpha=(material.features&0x40000000u)!=0u
            ?azureTemporalMaskAlpha(BASE,textureCoordinate,dFdx(textureCoordinate),dFdy(textureCoordinate))
            :texture(BASE,textureCoordinate).a;
#endif
        if(alpha<material.alphaCutoff)discard;
    }
#else
    if(material.alphaMode==1 && texture(BASE,textureCoordinate).a<material.alphaCutoff) discard;
#endif
    linearDepth=dot(worldPosition-camera.position.xyz,camera.forward.xyz);
}
