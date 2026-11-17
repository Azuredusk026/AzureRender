#version 450

layout(location = 0) out vec4 outputColor;
layout(push_constant) uniform Noise { float alpha; float phase; vec2 extent; } noise;

void main() {
    precise vec2 screenUv = gl_FragCoord.xy / noise.extent;
    vec2 moving = fma(vec2(noise.phase), vec2(1.7, 1.3), screenUv);
    precise float horizontal = moving.x * 12.9898;
    float angle = fma(moving.y, 78.233002, horizontal);
    // Reduce the sine argument with a split period to keep the noise threshold stable.
    float periods = roundEven(angle * .15915494309189535);
    float reduced = fma(-periods, 6.2831854820251465, angle);
    reduced = fma(-periods, -.000000174845553, reduced);
    float white = fract(sin(reduced) * 43758.546875) <= .2 ? 1.0 : 0.0;
    outputColor = vec4(vec3(white), noise.alpha);
}
