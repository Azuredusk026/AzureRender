#version 450

// Schwarzschild black hole geodesic tracer.
//
// For every pixel a null geodesic is integrated with explicit RK4 in
// Cartesian coordinates using the closed-form Schwarzschild acceleration
// (rs normalized to one world unit):
//
//   a = -(3/2) * (rs / r^2) * (1 - rs/r) * (n . v)^2 * n
//
// The ray terminates when it crosses the event horizon (black), escapes
// beyond the integration radius (procedural starfield), or exhausts the
// step budget (treated as escaping). Accretion disk emission is added in
// BH-2 by intersecting the equatorial plane during the integration.

layout(binding = 0) uniform BlackholeUniform {
    vec4 cameraPosition;   // xyz = camera position, w unused
    vec4 cameraRight;      // world-space right basis
    vec4 cameraUp;         // world-space up basis
    vec4 cameraForward;    // world-space forward basis
    vec4 physics;          // rs, escapeRadius, maxSteps, stepScale
    vec4 cameraFov;        // fovRadians, aspect, unused, unused
    vec4 diskParameters;   // diskInner, diskOuter, intensity, temperature (BH-2)
} ubo;

layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;

// ---------------------------------------------------------------------------
// Starfield helpers
// ---------------------------------------------------------------------------

float hash13(const vec3 position) {
    vec3 value = fract(position * 0.1031);
    value += dot(value, value.zyx + 31.32);
    return fract((value.x + value.y) * value.z);
}

vec3 starfieldColor(const vec3 direction) {
    const float grid = 96.0;
    const vec3 cellId = floor(direction * grid);
    const float starThreshold = 0.982;
    float brightness = 0.0;
    vec3 starColor = vec3(0.0);
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                const vec3 cell = cellId + vec3(dx, dy, dz);
                const float seed = hash13(cell);
                if (seed < starThreshold) {
                    continue;
                }
                const vec3 offset = vec3(
                    hash13(cell + 7.31),
                    hash13(cell + 13.73),
                    hash13(cell + 29.17)) - 0.5;
                const vec3 starDirection = normalize(cell + offset);
                const float alignment = max(dot(direction, starDirection), 0.0);
                const float spike = pow(alignment, 900.0);
                const float temperature = hash13(cell + 3.7);
                const vec3 tint = mix(
                    vec3(0.55, 0.65, 1.0),   // cool blue-white
                    vec3(1.0, 0.85, 0.60),   // warm orange-white
                    temperature);
                const float magnitude = 0.6 + 1.4 * hash13(cell + 17.9);
                brightness += spike * magnitude;
                starColor += tint * spike * magnitude;
            }
        }
    }
    // Faint galaxy band along a fixed great circle.
    const float band = exp(-abs(dot(normalize(direction + vec3(0.3, 0.6, 0.1)), vec3(0.0, 1.0, 0.0))) * 14.0);
    const vec3 galaxy = vec3(0.012, 0.016, 0.024) + band * vec3(0.010, 0.012, 0.016);
    return galaxy + starColor * 2.0 + vec3(brightness) * 0.12;
}

// ---------------------------------------------------------------------------
// Geodesic integration
// ---------------------------------------------------------------------------

vec3 schwarzschildAcceleration(const vec3 position, const vec3 velocity) {
    const float rs = ubo.physics.x;
    const float r = length(position);
    if (r < 1.0e-4) {
        return vec3(0.0);
    }
    const vec3 radial = position / r;
    const float radialVelocity = dot(radial, velocity);
    const float factor =
        -(1.5) * (rs / (r * r)) * (1.0 - rs / r);
    return factor * radialVelocity * radialVelocity * radial;
}

void main() {
    const float rs = ubo.physics.x;
    const float escapeRadius = ubo.physics.y;
    const int maxSteps = int(ubo.physics.z + 0.5);
    const float stepScale = ubo.physics.w;
    const float fov = ubo.cameraFov.x;
    const float aspect = ubo.cameraFov.y;

    const vec2 ndc = screenUv * 2.0 - 1.0;
    const float tanHalfFov = tan(fov * 0.5);
    // Flip the vertical component so up on screen means up in the world.
    vec3 direction = normalize(
        ubo.cameraForward.xyz
        + ubo.cameraRight.xyz * ndc.x * tanHalfFov * aspect
        - ubo.cameraUp.xyz * ndc.y * tanHalfFov);

    vec3 position = ubo.cameraPosition.xyz;
    vec3 velocity = direction;
    const float stepSize = stepScale * 0.12;
    bool crossedHorizon = false;

    for (int step = 0; step < maxSteps; ++step) {
        const float r = length(position);
        if (r <= rs) {
            crossedHorizon = true;
            break;
        }
        if (r > escapeRadius) {
            break;
        }

        // RK4 with the position as the derivative of the momentum.
        const vec3 k1x = velocity;
        const vec3 k1v = schwarzschildAcceleration(position, velocity);
        const vec3 k2x = velocity + 0.5 * stepSize * k1v;
        const vec3 k2v = schwarzschildAcceleration(
            position + 0.5 * stepSize * k1x,
            velocity + 0.5 * stepSize * k1v);
        const vec3 k3x = velocity + 0.5 * stepSize * k2v;
        const vec3 k3v = schwarzschildAcceleration(
            position + 0.5 * stepSize * k2x,
            velocity + 0.5 * stepSize * k2v);
        const vec3 k4x = velocity + stepSize * k3v;
        const vec3 k4v = schwarzschildAcceleration(
            position + stepSize * k3x,
            velocity + stepSize * k3v);
        position += (stepSize / 6.0) * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);
        velocity += (stepSize / 6.0) * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);
    }

    if (crossedHorizon) {
        outputColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    const vec3 background = starfieldColor(normalize(velocity));
    outputColor = vec4(background, 1.0);
}
