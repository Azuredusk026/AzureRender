#version 450

layout(binding = 0) uniform sampler2D opaqueColor;
layout(binding = 1) uniform sampler2D ramp;
layout(binding = 2) uniform sampler2D timeline;
layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;

layout(push_constant) uniform Blackout {
    vec4 timeRatioBlindUnstable;
    vec4 lengthScaleSpeed;
    vec4 extentTopBottom;
} effect;

float hashCell(vec2 cell) {
    precise float horizontal = cell.x * 12.9898;
    float angle = fma(cell.y, 78.233002, horizontal);
    return fract(sin(angle) * 43758.546875);
}

vec3 rampColor(vec2 uv) {
    if (effect.lengthScaleSpeed.w == 0.0) return textureLod(ramp, uv, 0.0).rgb;
    vec2 dimensions = vec2(textureSize(ramp, 0));
    vec2 position = uv * dimensions;
    // Match the D3D11 point sample at a vertical boundary on the discrete reference adapter.
    vec2 texel = vec2(floor(position.x), ceil(position.y) - 1.0);
    return texelFetch(ramp, ivec2(mod(texel, dimensions)), 0).rgb;
}

void main() {
    vec4 time = effect.timeRatioBlindUnstable;
    vec3 shape = effect.lengthScaleSpeed.xyz;
    vec2 extent = effect.extentTopBottom.xy;
    precise vec2 uv = gl_FragCoord.xy / extent;
    float seed = hashCell(vec2(floor(time.x * shape.z)));
    float shift = floor(seed * 20.0);
    float blindEdge = fma(seed, 3.0, effect.extentTopBottom.z);
    float blindBottom = fma(-seed, 3.0, effect.extentTopBottom.w);
    float row = floor(uv.y * shape.y);
    float blind = time.z == 0.0 || blindBottom >= row || blindEdge < row ? 1.0 : 0.0;

    precise vec2 scaled = uv * vec2(shape.x, 1.0);
    vec2 block = fma(scaled, vec2(shape.y), vec2(shift));
    float instability = textureLod(timeline, vec2(time.x * .1), 0.0).r;
    float ratio = fma(-instability, time.w, 1.0) * time.y;
    float covered = hashCell(floor(block * vec2(1.0, .5))) < ratio ? 1.0 : 0.0;
    float transparent = hashCell(floor(block)) >= ratio ? blind : 0.0;

    vec2 wideUv = screenUv * vec2(extent.x / extent.y, 1.0);
    vec3 lit = textureLod(opaqueColor, uv, 0.0).rgb * rampColor(wideUv * 8.0) * rampColor(wideUv * 16.0);
    outputColor = vec4(lit * (covered * transparent), 1.0 + covered - transparent);
}
