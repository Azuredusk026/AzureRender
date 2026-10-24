#version 450
#extension GL_GOOGLE_include_directive : require
#if defined(AZURE_BINDLESS)
#extension GL_EXT_nonuniform_qualifier : require
layout(binding=1) uniform sampler2D textures[];
layout(push_constant) uniform Material { float cutoff; int alphaMode; layout(offset=116) uint features; layout(offset=136) uint textureBase; } material;
#define BASE textures[nonuniformEXT(material.textureBase)]
#else
layout(binding=1) uniform sampler2D baseColor;
layout(push_constant) uniform Material { float cutoff; int alphaMode; layout(offset=116) uint features; } material;
#define BASE baseColor
#endif
#include "material_sampling.glsl"
layout(location=2) in vec2 textureCoordinate;
layout(location=0) out float coverage;
void main() {
    if(material.alphaMode==1){
        float alpha=(material.features&0x40000000u)!=0u
            ?azureTemporalMaskAlpha(BASE,textureCoordinate,dFdx(textureCoordinate),dFdy(textureCoordinate))
            :texture(BASE,textureCoordinate).a;
        if(alpha<material.cutoff)discard;
    }
    coverage=1.0;
}
