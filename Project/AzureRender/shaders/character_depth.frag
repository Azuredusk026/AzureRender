#version 450
#if defined(AZURE_BINDLESS)
#extension GL_EXT_nonuniform_qualifier : require
layout(binding=1) uniform sampler2D textures[];
layout(push_constant) uniform Material { float alphaCutoff; int alphaMode; layout(offset=136) uint textureBase; } material;
#define BASE textures[nonuniformEXT(material.textureBase)]
#else
layout(binding=1) uniform sampler2D baseColor;
layout(push_constant) uniform Material { float alphaCutoff; int alphaMode; } material;
#define BASE baseColor
#endif
layout(binding=0) uniform Camera { vec4 position; vec4 forward; } camera;
layout(location=2) in vec2 textureCoordinate;
layout(location=3) in vec3 worldPosition;
layout(location=0) out float linearDepth;
void main() {
    if(material.alphaMode==1 && texture(BASE,textureCoordinate).a<material.alphaCutoff) discard;
    linearDepth=dot(worldPosition-camera.position.xyz,camera.forward.xyz);
}
