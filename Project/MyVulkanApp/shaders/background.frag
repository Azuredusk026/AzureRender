#version 450

layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;

void main() {
    vec2 uv = clamp(screenUv, vec2(0.0), vec2(1.0));
    float vertical = smoothstep(0.0, 1.0, uv.y);
    vec3 bottomColor = vec3(0.010, 0.016, 0.028);
    vec3 topColor = vec3(0.038, 0.060, 0.080);
    vec3 color = mix(bottomColor, topColor, vertical);

    vec2 haloOffset = (uv - vec2(0.50, 0.56)) * vec2(1.05, 0.82);
    float halo = exp(-dot(haloOffset, haloOffset) * 5.2);
    color += vec3(0.038, 0.016, 0.028) * halo;

    vec2 vignetteOffset = uv - vec2(0.5);
    float vignette = smoothstep(0.20, 0.78, dot(vignetteOffset, vignetteOffset));
    color *= mix(1.0, 0.60, vignette);
    outputColor = vec4(color, 1.0);
}
