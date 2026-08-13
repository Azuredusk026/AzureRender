#version 450

layout(binding = 0) uniform CameraData {
    mat4 model;
    mat4 modelViewProjection;
    mat4 lightModelViewProjection;
    vec4 cameraPosition;
    vec4 renderingParameters;
    vec4 showcaseParameters;
    vec4 qaParameters;
    vec4 faceLightDirection;
    vec4 faceSdfParameters;
    vec4 faceSdfShadowColor;
} camera;

layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;
layout(location = 1) out vec4 outputNormal;

float gridLines(vec2 coordinate) {
    vec2 cell = fract(coordinate);
    vec2 edgeDistance = min(cell, 1.0 - cell);
    return 1.0 - smoothstep(0.0, 0.045, min(edgeDistance.x, edgeDistance.y));
}

void main() {
    vec2 uv = clamp(screenUv, vec2(0.0), vec2(1.0));
    float vertical = smoothstep(0.0, 1.0, uv.y);
    float preset = floor(camera.showcaseParameters.x + 0.5);
    vec3 color;
    if (preset == 4.0) {
        vec3 bottomColor = vec3(0.002, 0.003, 0.005);
        vec3 topColor = vec3(0.008, 0.010, 0.014);
        color = mix(bottomColor, topColor, vertical);
    } else if (preset == 3.0) {
        vec3 bottomColor = vec3(0.008, 0.012, 0.018);
        vec3 topColor = vec3(0.024, 0.032, 0.044);
        color = mix(bottomColor, topColor, vertical);
        vec2 haloOffset = (uv - vec2(0.50, 0.56)) * vec2(1.0, 0.82);
        float halo = exp(-dot(haloOffset, haloOffset) * 5.6);
        color += vec3(0.010, 0.018, 0.030) * halo;
    } else if (preset == 1.0) {
        vec3 bottomColor = vec3(0.006, 0.015, 0.020);
        vec3 topColor = vec3(0.016, 0.042, 0.049);
        color = mix(bottomColor, topColor, vertical);
        vec2 haloOffset = (uv - vec2(0.50, 0.56)) * vec2(1.0, 0.78);
        float halo = exp(-dot(haloOffset, haloOffset) * 5.8);
        color += vec3(0.006, 0.040, 0.046) * halo;
        float grid = gridLines(uv * vec2(30.0, 17.0));
        color += vec3(0.0025, 0.0070, 0.0075) * grid;
        float stripeDistance =
            abs(fract((uv.x + uv.y * 0.34) * 8.0) - 0.5);
        float stripes = 1.0 - smoothstep(0.035, 0.075, stripeDistance);
        float stripeMask =
            smoothstep(0.80, 0.93, uv.x)
            * smoothstep(0.06, 0.18, uv.y)
            * (1.0 - smoothstep(0.82, 0.96, uv.y));
        color += vec3(0.026, 0.013, 0.002) * stripes * stripeMask;
    } else if (preset == 2.0) {
        vec3 bottomColor = vec3(0.030, 0.034, 0.040);
        vec3 topColor = vec3(0.080, 0.086, 0.092);
        color = mix(bottomColor, topColor, vertical);
        vec2 haloOffset = (uv - vec2(0.50, 0.55)) * vec2(1.0, 0.82);
        float halo = exp(-dot(haloOffset, haloOffset) * 5.0);
        color += vec3(0.020, 0.022, 0.024) * halo;
    } else {
        vec3 bottomColor = vec3(0.010, 0.016, 0.028);
        vec3 topColor = vec3(0.038, 0.060, 0.080);
        color = mix(bottomColor, topColor, vertical);
        vec2 haloOffset = (uv - vec2(0.50, 0.56)) * vec2(1.05, 0.82);
        float halo = exp(-dot(haloOffset, haloOffset) * 5.2);
        color += vec3(0.038, 0.016, 0.028) * halo;
    }

    if (camera.qaParameters.x > 0.5) {
        color = vec3(0.018, 0.024, 0.040);
    }

    vec2 vignetteOffset = uv - vec2(0.5);
    float vignette = smoothstep(0.20, 0.78, dot(vignetteOffset, vignetteOffset));
    float vignetteFloor = preset == 2.0 ? 0.78 : 0.60;
    color *= mix(1.0, vignetteFloor, vignette);
    outputColor = vec4(color, 1.0);
    outputNormal = vec4(0.5, 0.5, 1.0, 0.0);
}
