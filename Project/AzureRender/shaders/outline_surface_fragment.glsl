#include "node_opacity.glsl"
#include "fragment_sampling.glsl"
#include "material_sampling.glsl"
#if defined(AZURE_BINDLESS)
#extension GL_EXT_nonuniform_qualifier : require
layout(binding=1) uniform sampler2D textures[];
#define SHELL_BASE textures[nonuniformEXT(shell.textureBase)]
#else
layout(binding=1) uniform sampler2D baseColor;
#define SHELL_BASE baseColor
#endif
layout(push_constant) uniform ShellInputs {
    float alphaCutoff;
    uint alphaMode;
    uint features;
    layout(offset=120) uint textureBase;
    layout(offset=136) uint tile;
    layout(offset=140) float opacity;
} shell;
layout(location=0) AZURE_SAMPLE in vec3 opacityCoordinate;
layout(location=1) AZURE_SAMPLE in vec2 textureCoordinate;
layout(location=2) flat in vec4 shellColor;
layout(location=3) flat in vec4 shellDepthProjection;
bool shellCovered() {
    bool covered=azureNodeOpacityCovered(opacityCoordinate,shell.opacity);
    if(shell.alphaMode!=1u)return covered;
    float alpha=(shell.features&0x40000000u)!=0u
        ?azureTemporalMaskAlpha(SHELL_BASE,textureCoordinate,dFdx(textureCoordinate),dFdy(textureCoordinate))
        :texture(SHELL_BASE,textureCoordinate).a;
    return covered&&alpha>=shell.alphaCutoff;
}
