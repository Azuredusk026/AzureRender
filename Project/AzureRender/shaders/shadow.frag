#version 450

layout(binding = 1) uniform sampler2D baseColorTexture;

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
} material;

layout(location = 0) in vec2 textureCoordinate;

void main() {
    float alpha = texture(baseColorTexture, textureCoordinate).a;
    if (material.alphaMode == 1 && alpha < material.alphaCutoff) {
        discard;
    }
    if (material.alphaMode == 2 && alpha < 0.35) {
        discard;
    }
}
