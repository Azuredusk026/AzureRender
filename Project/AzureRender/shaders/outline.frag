#version 450

layout(location = 0) out vec4 outputColor;
layout(location = 1) out vec4 outputNormal;

void main() {
    outputColor = vec4(0.018, 0.028, 0.042, 1.0);
    outputNormal = vec4(0.5, 0.5, 1.0, 0.0);
}
