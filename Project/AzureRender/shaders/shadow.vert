#version 450

layout(binding = 0) uniform CameraData {
    mat4 model;
    mat4 modelViewProjection;
    mat4 lightModelViewProjection;
    vec4 cameraPosition;
    vec4 renderingParameters;
    vec4 showcaseParameters;
} camera;

layout(std430, binding = 10) readonly buffer JointData {
    mat4 matrices[];
} jointData;

layout(location = 0) in vec3 position;
layout(location = 3) in vec2 texcoord;
layout(location = 4) in uvec4 jointIndices;
layout(location = 5) in vec4 jointWeights;
layout(location = 0) out vec2 textureCoordinate;

void main() {
    mat4 skinMatrix =
        jointWeights.x * jointData.matrices[jointIndices.x]
        + jointWeights.y * jointData.matrices[jointIndices.y]
        + jointWeights.z * jointData.matrices[jointIndices.z]
        + jointWeights.w * jointData.matrices[jointIndices.w];
    gl_Position =
        camera.lightModelViewProjection
        * skinMatrix
        * vec4(position, 1.0);
    textureCoordinate = texcoord;
}
