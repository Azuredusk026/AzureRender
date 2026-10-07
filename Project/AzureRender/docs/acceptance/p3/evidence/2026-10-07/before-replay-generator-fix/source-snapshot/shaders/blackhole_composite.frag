#version 450

layout(binding = 0) uniform sampler2D accumulatedFrame;

layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;
layout(location = 1) out vec4 outputNormal;

void main() {
    outputColor = vec4(texture(accumulatedFrame, screenUv).rgb, 1.0);
    outputNormal = vec4(0.5, 0.5, 1.0, 0.0);
}
