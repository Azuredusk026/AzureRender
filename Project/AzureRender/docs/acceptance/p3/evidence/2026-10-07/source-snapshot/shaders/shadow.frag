#version 450

#if defined(AZURE_BINDLESS)
#extension GL_EXT_nonuniform_qualifier : require
layout(binding = 1) uniform sampler2D textureArray[];
#else
layout(binding = 1) uniform sampler2D baseColorTexture;
#endif

layout(push_constant) uniform MaterialData {
    float alphaCutoff;
    int alphaMode;
    float emissiveStrength;
    float showcasePlatform;
    vec4 aoColor;
    vec4 lamShadowColor;
    vec4 matcapColor;
    vec4 hairParameters;
    vec4 styleParameters;
    vec4 featureParameters;
    uint materialClass;
    uint materialFeatures;
    uint materialProfileVersion;
    uint materialPadding;
#if defined(AZURE_BINDLESS)
    layout(offset = 136) uint textureBase;
#endif
} material;

#if defined(AZURE_BINDLESS)
#define AZ_TEX_BASE_COLOR textureArray[nonuniformEXT(material.textureBase + 0u)]
#else
#define AZ_TEX_BASE_COLOR baseColorTexture
#endif

layout(location = 0) in vec2 textureCoordinate;

void main() {
    float alpha = texture(AZ_TEX_BASE_COLOR, textureCoordinate).a;
    if (material.alphaMode == 1 && alpha < material.alphaCutoff) {
        discard;
    }
    if (material.alphaMode == 2 && alpha < 0.35) {
        discard;
    }
}
