#version 450

layout(binding = 0) uniform sampler2D normalTexture;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler2D shadowTexture;
layout(binding = 3) uniform sampler2D sceneColorTexture;

layout(push_constant) uniform OutlineParameters {
    float strength;
    float depthThreshold;
    float normalThreshold;
    float diagnosticView;
    float exposureEv;
    float toneMappingEnabled;
    float padding0;
    float padding1;
} outline;

layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;

vec3 acesFitted(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp(
        (color * (a * color + b))
            / (color * (c * color + d) + e),
        0.0,
        1.0);
}

void main() {
    ivec2 textureExtent = textureSize(normalTexture, 0);
    vec2 texelSize = 1.0 / vec2(textureExtent);
    vec4 centerSample = texture(normalTexture, screenUv);
    int diagnosticView = int(floor(outline.diagnosticView + 0.5));
    if (diagnosticView == 1) {
        vec3 normalColor = centerSample.a < 0.01
            ? vec3(0.018, 0.024, 0.040)
            : centerSample.rgb;
        outputColor = vec4(normalColor, 1.0);
        return;
    }
    if (diagnosticView == 3) {
        float shadowDepth = texture(shadowTexture, screenUv).r;
        float readableDepth = pow(clamp(shadowDepth, 0.0, 1.0), 4.0);
        outputColor = vec4(vec3(readableDepth), 1.0);
        return;
    }
    float edge = 0.0;
    if (centerSample.a >= 0.01 && outline.strength > 0.0) {
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
        edge = max(depthEdge, normalEdge) * outline.strength;
    }
    if (diagnosticView == 2) {
        vec3 background = vec3(0.018, 0.024, 0.040);
        vec3 edgeColor = vec3(0.35, 0.86, 1.0);
        outputColor = vec4(mix(background, edgeColor, edge), 1.0);
    } else {
        vec3 hdrColor = texture(sceneColorTexture, screenUv).rgb;
        vec3 outlineColor = vec3(0.008, 0.013, 0.022);
        hdrColor = mix(hdrColor, outlineColor, edge);
        hdrColor *= exp2(outline.exposureEv);
        vec3 displayLinear = outline.toneMappingEnabled > 0.5
            ? acesFitted(hdrColor)
            : clamp(hdrColor, 0.0, 1.0);
        outputColor = vec4(displayLinear, 1.0);
    }
}
