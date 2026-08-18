#version 450

layout(binding = 0) uniform sampler2D accumulatedFrame;

layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;

void main() {
    outputColor = vec4(texture(accumulatedFrame, screenUv).rgb, 1.0);
}
