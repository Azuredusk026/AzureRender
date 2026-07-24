#version 450

layout(binding = 0) uniform CameraData {
    mat4 model;
    mat4 modelViewProjection;
    vec4 cameraPosition;
    vec4 renderingParameters;
} camera;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

void main() {
    vec3 expandedPosition =
        position + normalize(normal) * camera.renderingParameters.x;
    gl_Position =
        camera.modelViewProjection * vec4(expandedPosition, 1.0);
    gl_Position.xy *= 1.6;
}
