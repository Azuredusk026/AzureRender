#version 450

layout(binding = 0) uniform sampler2D colorNoise;
layout(binding = 1) uniform sampler2D timeline;
layout(binding = 2) uniform sampler2D screenColor;
layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;

layout(push_constant) uniform Chromatic {
    vec4 timeRatioUnstableFilament;
    vec4 rageExtent;
} effect;

float hashPosition(vec2 position) {
    precise float horizontal = position.x * 12.9898;
    float angle = fma(position.y, 78.233002, horizontal);
    return fract(sin(angle) * 43758.546875);
}

void main() {
    vec4 settings = effect.timeRatioUnstableFilament;
    vec2 extent = effect.rageExtent.yz;
    vec3 baseColor = texelFetch(screenColor, ivec2(gl_FragCoord.xy), 0).rgb;
    if (settings.y == 0.0) {
        outputColor = vec4(baseColor, 1.0);
        return;
    }

    vec4 cells = floor(settings.x * vec4(11.0, 3.0, 7.0, 5.0));
    vec2 displacement = fma(vec2(hashPosition(cells.xy), hashPosition(cells.zw)), vec2(2.0), vec2(-1.0));
    float timeNoise = textureLod(timeline, vec2(settings.x * .1), 0.0).r;
    float ratio = fma(-timeNoise, settings.z, 1.0) * settings.y;
    vec2 shrunkUv = fma(screenUv, vec2(fma(settings.y, -.04, 1.0)), vec2(settings.y * .02));
    vec2 displacedUv = fma(displacement, vec2(ratio * effect.rageExtent.x), shrunkUv);
    ivec2 shifted = ivec2(max(displacedUv * extent, vec2(0.0)));
    float red = all(lessThan(shifted, ivec2(extent))) ? texelFetch(screenColor, shifted, 0).r : 0.0;

    vec2 timeOffsets = settings.x * vec2(1.3, 1.7);
    vec2 offset = vec2(hashPosition(vec2(timeOffsets.x)), hashPosition(vec2(timeOffsets.y)));
    vec2 noiseUv = (screenUv + offset) * extent * (1.0 / 256.0);
    vec3 noise = textureLod(colorNoise, noiseUv, 0.0).rgb;
    vec3 color = vec3(red, baseColor.gb) * fma(vec3(ratio), noise - 1.0, vec3(1.0));
    float row = fract(screenUv.y * extent.y / settings.w) * settings.w;
    outputColor = vec4(color * (row >= 1.0 ? 1.0 : 1.0 - ratio), 1.0);
}
