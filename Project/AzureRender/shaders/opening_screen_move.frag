#version 450

layout(binding = 0) uniform sampler2D bands;
layout(binding = 1) uniform sampler2D instability;
layout(binding = 2) uniform sampler2D flowMap;
layout(binding = 3) uniform sampler2D timeline;
layout(binding = 4) uniform sampler2D screenColor;
layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;

layout(push_constant) uniform ScreenMove {
    vec4 timeRatioSpeedUnstable;
    vec4 extentAndBandSampling;
} effect;

float readBand() {
    if (effect.extentAndBandSampling.z == 0.0)
        return textureLod(bands, screenUv * 1.2, 0.0).r;
    // D3D11 on the discrete reference adapter selects the preceding texel at an exact band boundary.
    uint height = uint(textureSize(bands, 0).y);
    uint numerator = (2u * uint(gl_FragCoord.y) + 1u) * 3u * height;
    uint row = (numerator - 1u) / (5u * uint(effect.extentAndBandSampling.y));
    return texelFetch(bands, ivec2(0, int(row % height)), 0).r;
}

void main() {
    vec4 settings = effect.timeRatioSpeedUnstable;
    if (settings.y == 0.0) {
        outputColor = vec4(texelFetch(screenColor, ivec2(gl_FragCoord.xy), 0).rgb, 1.0);
        return;
    }

    float phase = fract(settings.x * settings.z);
    float weight = (textureLod(timeline, vec2(phase), 0.0).r - .5) * 2.0;
    vec2 offsets = weight * (textureLod(flowMap, screenUv, 0.0).rg - .214041);
    float band = readBand() - .5;
    vec2 carrier = sin(settings.x * vec2(3.1, 7.0));
    float instabilityValue = textureLod(instability, vec2(settings.x * .1), 0.0).r;
    float ratio = fma(-instabilityValue, settings.w, 1.0) * settings.y;
    precise float horizontalBand = ((band * (carrier.x * carrier.y)) * ratio) * .01;
    vec2 displacement = fma(offsets, vec2(ratio * .1), vec2(horizontalBand, 0.0));
    ivec2 shifted = ivec2(fract(screenUv + displacement) * effect.extentAndBandSampling.xy);
    outputColor = vec4(texelFetch(screenColor, shifted, 0).rgb, 1.0);
}
