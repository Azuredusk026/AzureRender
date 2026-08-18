#version 450

// Temporal anti-aliasing + cheap single-pass bloom for the black hole
// tracer. The tracer writes a full HDR frame into a private ping-pong
// texture; this pass mixes it with the previous frame (denoise + TAA) and
// adds a small gaussian bloom from the bright (HDR) regions, then outputs
// into the engine Scene Color attachment.

layout(binding = 0) uniform TaaUniform {
    float blendWeight;      // temporal mix factor (1.0 = no history)
    float bloomThreshold;   // HDR threshold for bloom extraction
    float bloomIntensity;   // bloom contribution scale
    float renderWidth;      // for texel-size offsets
} ubo;

layout(binding = 1) uniform sampler2D currentFrame;
layout(binding = 2) uniform sampler2D previousFrame;

layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;

void main() {
    const ivec2 size = textureSize(currentFrame, 0);
    const vec2 texelSize = 1.0 / max(vec2(size), vec2(1.0));
    const float texel = texelSize.x;
    const float aspect = float(size.x) / max(float(size.y), 1.0);

    const vec3 current = texture(currentFrame, screenUv).rgb;
    vec3 neighborhoodMin = current;
    vec3 neighborhoodMax = current;
    const vec2 neighborhoodOffsets[4] = vec2[](
        vec2(texelSize.x, 0.0), vec2(-texelSize.x, 0.0),
        vec2(0.0, texelSize.y), vec2(0.0, -texelSize.y));
    for (int index = 0; index < 4; ++index) {
        const vec3 sampleColor = texture(
            currentFrame, screenUv + neighborhoodOffsets[index]).rgb;
        neighborhoodMin = min(neighborhoodMin, sampleColor);
        neighborhoodMax = max(neighborhoodMax, sampleColor);
    }
    const vec3 previous = clamp(
        texture(previousFrame, screenUv).rgb,
        neighborhoodMin,
        neighborhoodMax);
    vec3 color = mix(previous, current, clamp(ubo.blendWeight, 0.0, 1.0));

    // Single-pass gaussian bloom: three rings around the bright core.
    const float threshold = max(ubo.bloomThreshold, 0.0);
    vec3 bloom = vec3(0.0);
    // Ring 1 (tight, 1.5 px)
    vec2 o1 = vec2(1.5 * texel, 0.0);
    vec2 o2 = vec2(0.0, 1.5 * texel * aspect);
    vec2 o3 = vec2(1.1 * texel, 1.1 * texel * aspect);
    vec2 o4 = vec2(1.1 * texel, -1.1 * texel * aspect);
    bloom += max(texture(currentFrame, screenUv + o1).rgb - threshold, vec3(0.0)) * 0.28;
    bloom += max(texture(currentFrame, screenUv - o1).rgb - threshold, vec3(0.0)) * 0.28;
    bloom += max(texture(currentFrame, screenUv + o2).rgb - threshold, vec3(0.0)) * 0.28;
    bloom += max(texture(currentFrame, screenUv - o2).rgb - threshold, vec3(0.0)) * 0.28;
    bloom += max(texture(currentFrame, screenUv + o3).rgb - threshold, vec3(0.0)) * 0.22;
    bloom += max(texture(currentFrame, screenUv - o3).rgb - threshold, vec3(0.0)) * 0.22;
    bloom += max(texture(currentFrame, screenUv + o4).rgb - threshold, vec3(0.0)) * 0.22;
    bloom += max(texture(currentFrame, screenUv - o4).rgb - threshold, vec3(0.0)) * 0.22;
    // Ring 2 (wide, 5 px)
    vec2 w1 = vec2(5.0 * texel, 0.0);
    vec2 w2 = vec2(0.0, 5.0 * texel * aspect);
    bloom += max(texture(currentFrame, screenUv + w1).rgb - threshold, vec3(0.0)) * 0.12;
    bloom += max(texture(currentFrame, screenUv - w1).rgb - threshold, vec3(0.0)) * 0.12;
    bloom += max(texture(currentFrame, screenUv + w2).rgb - threshold, vec3(0.0)) * 0.12;
    bloom += max(texture(currentFrame, screenUv - w2).rgb - threshold, vec3(0.0)) * 0.12;

    color += bloom * max(ubo.bloomIntensity, 0.0);
    outputColor = vec4(color, 1.0);
}
