#version 450

// Schwarzschild black hole geodesic tracer with accretion disk.
//
// Per pixel a null geodesic is integrated with explicit RK4 in Cartesian
// coordinates using the closed-form Schwarzschild acceleration
// (rs normalized to one world unit):
//
//   a = -(3/2) * (rs / r^2) * (1 - rs/r) * (n . v)^2 * n
//
// Whenever the ray crosses the equatorial accretion disk plane
// (y = 0, r in [diskInner, diskOuter]) we accumulate a sampled emission
// that incorporates Keplerian Doppler shift, gravitational redshift and
// relativistic beaming (I ~ delta^3). Successive crossings compose the
// higher-order photon ring images naturally.

layout(binding = 0) uniform BlackholeUniform {
    vec4 cameraPosition;   // xyz = camera position, w unused
    vec4 cameraRight;      // world-space right basis
    vec4 cameraUp;         // world-space up basis
    vec4 cameraForward;    // world-space forward basis
    vec4 physics;          // rs, escapeRadius, maxSteps, stepScale
    vec4 cameraFov;        // fovRadians, aspect, unused, unused
    vec4 diskParameters;   // diskInner, diskOuter, intensity, temperature
} ubo;

layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;

// ---------------------------------------------------------------------------
// Starfield helpers (BH-1 baseline)
// ---------------------------------------------------------------------------

float hash13(const vec3 position) {
    vec3 value = fract(position * 0.1031);
    value += dot(value, value.zyx + 31.32);
    return fract((value.x + value.y) * value.z);
}

vec3 starfieldColor(const vec3 direction) {
    const float grid = 96.0;
    const vec3 cellId = floor(direction * grid);
    const float starThreshold = 0.985;
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
                const float spike = pow(alignment, 1500.0);
                const float temperature = hash13(cell + 3.7);
                const vec3 tint = mix(
                    vec3(0.65, 0.78, 1.0),
                    vec3(1.0, 0.92, 0.74),
                    temperature);
                const float magnitude = 0.6 + 1.2 * hash13(cell + 17.9);
                brightness += spike * magnitude;
                starColor += tint * spike * magnitude;
            }
        }
    }
    const vec3 galaxy = vec3(0.012, 0.016, 0.024);
    return galaxy + starColor * 1.5 + vec3(brightness) * 0.06;
}

// ---------------------------------------------------------------------------
// Accretion disk helpers (BH-2)
// ---------------------------------------------------------------------------

vec3 temperatureToColor(float temperature) {
    temperature = clamp(temperature, 800.0, 30000.0);
    float t = temperature / 100.0;
    vec3 red = vec3(1.0, 0.40, 0.05);
    vec3 yellow = vec3(1.0, 0.85, 0.35);
    vec3 white = vec3(1.0, 0.96, 0.78);
    vec3 blue = vec3(0.75, 0.88, 1.0);
    if (t < 20.0) {
        return mix(vec3(0.10, 0.02, 0.04), red, smoothstep(8.0, 20.0, t));
    }
    if (t < 40.0) {
        return mix(red, yellow, smoothstep(20.0, 40.0, t));
    }
    if (t < 65.0) {
        return mix(yellow, white, smoothstep(40.0, 65.0, t));
    }
    return mix(white, blue, smoothstep(65.0, 130.0, t));
}

vec3 sampleDisk(
    const vec3 hitPosition,
    const vec3 rayDirection,
    const float diskRadius) {
    const float rs = ubo.physics.x;
    const float diskInner = ubo.diskParameters.x;
    const float diskOuter = ubo.diskParameters.y;
    const float diskIntensity = ubo.diskParameters.z;
    const float temperatureScale = ubo.diskParameters.w;

    const vec3 radial = normalize(vec3(hitPosition.x, 0.0, hitPosition.z));
    const vec3 diskVelocity =
        cross(vec3(0.0, 1.0, 0.0), radial) * sqrt(rs / (2.0 * diskRadius));

    const float beta = dot(diskVelocity, -rayDirection);
    const float gamma = 1.0 / sqrt(max(1.0 - beta * beta, 1.0e-4));
    const float dopplerFactor = 1.0 / (gamma * (1.0 - beta));
    const float gravFactor = sqrt(max(1.0 - rs / diskRadius, 0.0));
    const float redshift = dopplerFactor * gravFactor;

    const float radiusRatio = diskInner / max(diskRadius, 1.0e-3);
    const float temperature =
        temperatureScale * 6500.0 * pow(radiusRatio, 0.75);
    const vec3 emitColor = temperatureToColor(temperature);
    const float beaming = pow(max(dopplerFactor, 1.0e-3), 3.0);
    const float envelope =
        step(diskInner, diskRadius) * (1.0 - step(diskOuter, diskRadius));

    return emitColor * (diskIntensity * envelope * beaming * redshift);
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
    const vec3 direction = normalize(
        ubo.cameraForward.xyz
        + ubo.cameraRight.xyz * ndc.x * tanHalfFov * aspect
        - ubo.cameraUp.xyz * ndc.y * tanHalfFov);

    vec3 position = ubo.cameraPosition.xyz;
    vec3 velocity = direction;
    const float stepSize = stepScale * 0.12;

    vec3 diskColor = vec3(0.0);
    int totalCrossings = 0;
    bool crossedHorizon = false;
    vec3 prevPosition = position;

    for (int step = 0; step < maxSteps; ++step) {
        const float r = length(position);
        if (r <= rs) {
            crossedHorizon = true;
            break;
        }
        if (r > escapeRadius) {
            break;
        }

        // Equatorial disk-crossing detection: when world-y changes sign
        // across a step the ray crossed the disk plane.
        if (prevPosition.y * position.y < 0.0
            && abs(prevPosition.y) > 1.0e-3
            && abs(position.y) > 1.0e-3) {
            const float fraction =
                abs(prevPosition.y)
                / (abs(prevPosition.y) + abs(position.y));
            const vec3 hitPosition =
                mix(prevPosition, position, fraction);
            const float diskRadius =
                length(vec2(hitPosition.x, hitPosition.z));
            const vec3 sampleColor =
                sampleDisk(hitPosition, direction, diskRadius);
            // Higher-order images (n-th disk crossing) compose the photon
            // ring; attenuate by 0.6 per order so n>=2 reads dim but visible.
            const float weight = exp(-float(totalCrossings) * 0.6);
            diskColor += sampleColor * weight;
            ++totalCrossings;
        }

        // RK4 integration step.
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
        prevPosition = position;
    }

    if (crossedHorizon) {
        outputColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    if (dot(diskColor, vec3(1.0)) > 0.0) {
        outputColor = vec4(diskColor, 1.0);
        return;
    }
    const vec3 background = starfieldColor(normalize(velocity));
    outputColor = vec4(background, 1.0);
}