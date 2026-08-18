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

// Shared environment map (binding 4). The background samples the equirect
// environment so external HDR assets are visible in the backdrop.
layout(binding = 4) uniform sampler2D environmentTexture;

layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;
layout(location = 1) out vec4 outputNormal;

vec2 directionToEquirectangular(vec3 direction) {
    const float pi = 3.14159265358979323846;
    vec3 normalizedDirection = normalize(direction);
    float u = atan(normalizedDirection.z, normalizedDirection.x) * 0.5 / pi + 0.5;
    float v = acos(clamp(normalizedDirection.y, -1.0, 1.0)) / pi;
    return vec2(u, v);
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
        vec3 bottomColor = vec3(0.030, 0.035, 0.038);
        vec3 topColor = vec3(0.075, 0.082, 0.086);
        color = mix(bottomColor, topColor, vertical);
        vec2 haloOffset = (uv - vec2(0.50, 0.51)) * vec2(0.92, 0.72);
        float halo = exp(-dot(haloOffset, haloOffset) * 5.2);
        color += vec3(0.050, 0.057, 0.058) * halo;
        float floorSeparation = smoothstep(0.30, 0.44, 1.0 - uv.y);
        color += vec3(0.010, 0.012, 0.012) * floorSeparation;
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

    // Sample the shared HDR environment (equirect) as the backdrop. The
    // procedural gradient above remains as a subtle tint fallback so the
    // environment stays visible without overpowering the subject.
    vec2 envUv = directionToEquirectangular(
        normalize(vec3(
            uv.x * 2.0 - 1.0,
            uv.y * 2.0 - 1.0,
            1.0)));
    vec3 environmentColor = texture(environmentTexture, envUv).rgb * 1.15;
    // Endfield previously suppressed the loaded sky to 8%, making the
    // environment indistinguishable from its procedural fallback.
    float environmentMix = preset == 1.0 ? 0.82 : 0.88;
    color = mix(color, environmentColor, environmentMix);

    vec2 vignetteOffset = uv - vec2(0.5);
    float vignette = smoothstep(0.20, 0.78, dot(vignetteOffset, vignetteOffset));
    float vignetteFloor = preset == 1.0 ? 0.92 : (preset == 2.0 ? 0.82 : 0.68);
    color *= mix(1.0, vignetteFloor, vignette);
    outputColor = vec4(color, 1.0);
    outputNormal = vec4(0.5, 0.5, 1.0, 0.0);
}
