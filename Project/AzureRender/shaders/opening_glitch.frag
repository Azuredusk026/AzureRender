#version 450

layout(binding = 0) uniform sampler2D timeline;
layout(binding = 1) uniform sampler2D cameraColor;
layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;

layout(push_constant) uniform Glitch {
    vec4 timeRatioLengthDotScale;
    vec4 speedUnstable;
    uvec4 displaySizeAndSampling;
} effect;

float hashAngle(float angle) {
    return fract(sin(angle) * 43758.546875);
}

float hashCell(vec2 cell) {
    precise float horizontal = cell.x * 12.9898;
    precise float angle = fma(cell.y, 78.233002, horizontal);
    return hashAngle(angle);
}

void main() {
    vec4 settings = effect.timeRatioLengthDotScale;
    float noise = textureLod(timeline, vec2(settings.x * .1), 0.0).r;
    float ratio = fma(-noise, effect.speedUnstable.y, 1.0) * settings.y;
    precise float step = floor(settings.x * effect.speedUnstable.x);
    // The Intel reference path supplies the rounded time angle before evaluating sine on the GPU.
    float seed = effect.speedUnstable.w != 0.0 ? hashAngle(effect.speedUnstable.z) : hashCell(vec2(step));
    float position = floor(seed * 20.0);
    float shiftedLength = fma(hashCell(vec2(position)), .3, settings.z);
    precise vec2 primary = screenUv * vec2(shiftedLength, 1.0);
    float region = hashCell(floor(fma(primary, vec2(settings.w), vec2(position)))) < ratio ? 1.0 : 0.0;

    precise vec2 secondary = (screenUv * vec2(settings.z, 1.0)) * settings.w;
    vec2 pattern = fma(secondary, vec2(8.0, 1.0), vec2(seed * 142.0));
    bool hatch = hashCell(floor(pattern * vec2(2.0, 4.0))) <= .5;
    float invert = hatch && hashCell(floor(pattern)) < ratio * .03 ? 1.0 : 0.0;
    uvec2 size = uvec2(textureSize(cameraColor, 0));
    uvec2 pixelCenter = 2u * uvec2(gl_FragCoord.xy) + 1u;
    uvec2 texel = (pixelCenter * size - effect.displaySizeAndSampling.zw) /
                  (2u * effect.displaySizeAndSampling.xy);
    vec3 camera = texelFetch(cameraColor, ivec2(texel), 0).rgb;
    vec3 inverted = abs(1.0 - camera) - camera;
    outputColor = vec4(fma(vec3(invert), inverted, camera), region + invert);
}
