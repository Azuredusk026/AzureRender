#version 450

layout(binding = 0) uniform sampler2D normalTexture;
layout(binding = 1) uniform sampler2D depthTexture;

layout(push_constant) uniform OutlineParameters {
    float strength;
    float depthThreshold;
    float normalThreshold;
    float padding;
} outline;

layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;

void main() {
    ivec2 textureExtent = textureSize(normalTexture, 0);
    vec2 texelSize = 1.0 / vec2(textureExtent);
    vec4 centerSample = texture(normalTexture, screenUv);
    if (centerSample.a < 0.01 || outline.strength <= 0.0) {
        outputColor = vec4(0.0);
        return;
    }

    vec3 centerNormal = normalize(centerSample.xyz * 2.0 - 1.0);
    float centerDepth = texture(depthTexture, screenUv).r;
    const ivec2 offsets[8] = ivec2[](
        ivec2(-1, 0),
        ivec2(1, 0),
        ivec2(0, -1),
        ivec2(0, 1),
        ivec2(-1, -1),
        ivec2(1, -1),
        ivec2(-1, 1),
        ivec2(1, 1));
    float maximumDepthDifference = 0.0;
    float maximumNormalDifference = 0.0;
    for (int index = 0; index < 8; ++index) {
        vec2 sampleUv = clamp(
            screenUv + vec2(offsets[index]) * texelSize,
            texelSize * 0.5,
            vec2(1.0) - texelSize * 0.5);
        vec4 neighbourSample = texture(normalTexture, sampleUv);
        if (neighbourSample.a < 0.01) {
            continue;
        }
        float participation = min(centerSample.a, neighbourSample.a);
        vec3 neighbourNormal =
            normalize(neighbourSample.xyz * 2.0 - 1.0);
        float neighbourDepth = texture(depthTexture, sampleUv).r;
        maximumDepthDifference = max(
            maximumDepthDifference,
            abs(centerDepth - neighbourDepth) * 1400.0
                * participation);
        maximumNormalDifference = max(
            maximumNormalDifference,
            (1.0 - max(dot(centerNormal, neighbourNormal), 0.0))
                * participation);
    }

    float depthEdge = smoothstep(
        outline.depthThreshold,
        outline.depthThreshold + 0.28,
        maximumDepthDifference);
    float normalEdge = smoothstep(
        outline.normalThreshold,
        outline.normalThreshold + 0.22,
        maximumNormalDifference);
    float edge = max(depthEdge, normalEdge) * outline.strength;
    outputColor = vec4(vec3(0.008, 0.013, 0.022), edge);
}
