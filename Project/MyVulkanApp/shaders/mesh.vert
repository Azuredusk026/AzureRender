#version 450

layout(binding = 0) uniform CameraData {
    mat4 model;
    mat4 modelViewProjection;
    vec4 cameraPosition;
    vec4 renderingParameters;
} camera;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 texcoord;
layout(location = 0) out vec3 worldNormal;
layout(location = 1) out vec4 worldTangent;
layout(location = 2) out vec2 textureCoordinate;
layout(location = 3) out vec3 worldPosition;

void main() {
    gl_Position = camera.modelViewProjection * vec4(position, 1.0);
    gl_Position.xy *= 1.6;
    worldNormal = normalize(mat3(camera.model) * normal);
    worldTangent = vec4(normalize(mat3(camera.model) * tangent.xyz), tangent.w);
    textureCoordinate = texcoord;
    worldPosition = (camera.model * vec4(position, 1.0)).xyz;
}
