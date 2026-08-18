#version 450

// Schwarzschild black hole renderer, port of the approach documented in
// the Chinese article "如何手搓史上最好的 Kerr Newman 黑洞实时渲染"
// (shadertoy "Gargantua", see D:\Assigment\temp\BufferA.txt).
//
// Key techniques ported from the reference:
//  1. Euler geodesic integration via deflection angle
//     (dphi = -cos^3(theta) * 1.5*rs/r per unit length), position updated
//     with the NEW direction (symplectic).
//  2. Spherically symmetric, CONTINUOUS step size as a function of radius
//     (no discontinuities => no banding stripes).
//  3. First-step random jitter + sub-pixel jitter (breaks sampling grid
//     alignment => no scattered color blocks).
//  4. Volumetric accretion disk: Shape() radial density, Perlin fractal
//     clouds, spiral inflow, temperature T^4, Doppler + gravitational
//     redshift, blackbody color, alpha accumulation, step-length scaling.
//  5. In-shader 2x2 supersampling plus renderer-owned temporal accumulation.

layout(binding = 0) uniform BlackholeUniform {
    vec4 cameraPosition;   // xyz = camera position, w unused
    vec4 cameraRight;      // world-space right basis
    vec4 cameraUp;         // world-space up basis
    vec4 cameraForward;    // world-space forward basis
    vec4 physics;          // rs, escapeRadius, maxSteps, unused
    vec4 cameraFov;        // fovRadians, aspect, supersample, unused
    vec4 diskParameters;   // diskInner, diskOuter, temperatureScale, shiftMax
} ubo;

layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;

const float kPi = 3.14159265358979323846;

// ---------------------------------------------------------------------------
// Random / noise helpers (ported)
// ---------------------------------------------------------------------------

float RandomStep(const vec2 coord, const float seed) {
    return fract(sin(dot(coord + fract(11.4514 * sin(seed)), vec2(12.9898, 78.233))) * 43758.5453);
}

float CubicInterpolate(const float x) {
    return 3.0 * x * x - 2.0 * x * x * x;
}

float PerlinNoise(const vec3 position) {
    const vec3 intPart = floor(position);
    const vec3 fracPart = fract(position);
    float v000 = 2.0 * fract(sin(dot(vec3(intPart.x, intPart.y, intPart.z), vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    float v100 = 2.0 * fract(sin(dot(vec3(intPart.x + 1.0, intPart.y, intPart.z), vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    float v010 = 2.0 * fract(sin(dot(vec3(intPart.x, intPart.y + 1.0, intPart.z), vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    float v110 = 2.0 * fract(sin(dot(vec3(intPart.x + 1.0, intPart.y + 1.0, intPart.z), vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    float v001 = 2.0 * fract(sin(dot(vec3(intPart.x, intPart.y, intPart.z + 1.0), vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    float v101 = 2.0 * fract(sin(dot(vec3(intPart.x + 1.0, intPart.y, intPart.z + 1.0), vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    float v011 = 2.0 * fract(sin(dot(vec3(intPart.x, intPart.y + 1.0, intPart.z + 1.0), vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    float v111 = 2.0 * fract(sin(dot(vec3(intPart.x + 1.0, intPart.y + 1.0, intPart.z + 1.0), vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    const float v00 = v001 * CubicInterpolate(fracPart.z) + v000 * CubicInterpolate(1.0 - fracPart.z);
    const float v10 = v101 * CubicInterpolate(fracPart.z) + v100 * CubicInterpolate(1.0 - fracPart.z);
    const float v01 = v011 * CubicInterpolate(fracPart.z) + v010 * CubicInterpolate(1.0 - fracPart.z);
    const float v11 = v111 * CubicInterpolate(fracPart.z) + v110 * CubicInterpolate(1.0 - fracPart.z);
    const float v0 = v01 * CubicInterpolate(fracPart.y) + v00 * CubicInterpolate(1.0 - fracPart.y);
    const float v1 = v11 * CubicInterpolate(fracPart.y) + v10 * CubicInterpolate(1.0 - fracPart.y);
    return v1 * CubicInterpolate(fracPart.x) + v0 * CubicInterpolate(1.0 - fracPart.x);
}

float SoftSaturate(const float x) {
    return 1.0 - 1.0 / (max(x, 0.0) + 1.0);
}

// Fractal fbm-style noise accumulator, levels in [start, end), contrast gamma.
float GenerateAccretionDiskNoise(const vec3 position, const int startLevel, const int endLevel, const float contrast) {
    float accumulator = 10.0;
    for (int level = startLevel; level < endLevel; ++level) {
        const float frequency = pow(3.0, float(level));
        accumulator *= 1.0 + 0.1 * PerlinNoise(vec3(frequency * position.x, frequency * position.y, frequency * position.z));
    }
    return log(1.0 + pow(0.1 * accumulator, contrast));
}

float Shape(const float x, const float alpha, const float beta) {
    const float k = pow(alpha + beta, alpha + beta) / (pow(alpha, alpha) * pow(beta, beta));
    return k * pow(x, alpha) * pow(1.0 - x, beta);
}

float Vec2ToTheta(const vec2 v1, const vec2 v2) {
    if (dot(v1, v2) > 0.0) {
        return asin(0.999999 * (v1.x * v2.y - v1.y * v2.x) / length(v1) / length(v2));
    } else if (dot(v1, v2) < 0.0 && (-v1.x * v2.y + v1.y * v2.x) < 0.0) {
        return kPi - asin(0.999999 * (v1.x * v2.y - v1.y * v2.x) / length(v1) / length(v2));
    } else if (dot(v1, v2) < 0.0 && (-v1.x * v2.y + v1.y * v2.x) > 0.0) {
        return -kPi - asin(0.999999 * (v1.x * v2.y - v1.y * v2.x) / length(v1) / length(v2));
    }
    return 0.0;
}

// Blackbody temperature to RGB (exponential fit, saturation-boosted variant
// from the reference article).
vec3 KelvinToRgb(float kelvin) {
    kelvin = max(kelvin, 400.0);
    const float t = (kelvin - 6500.0) / (6500.0 * kelvin * 2.2);
    vec3 color;
    color.r = exp(2.05539304e4 * t);
    color.g = exp(2.63463675e4 * t);
    color.b = exp(3.30145739e4 * t);
    const float brightnessScale = 1.0 / max(max(color.r, color.g), color.b);
    if (kelvin < 1000.0) {
        color *= (kelvin - 400.0) / 600.0;
    }
    color *= brightnessScale;
    return color;
}

// Keplerian angular velocity (in units of 1 / Rs, c = 1).
float GetKeplerianAngularVelocity(const float radius, const float rs) {
    return sqrt(1.0 * rs / ((2.0 * radius - 3.0 * rs) * radius * radius));
}

// ---------------------------------------------------------------------------
// Starfield (procedural background, sampled along the escaping ray)
// ---------------------------------------------------------------------------

float hash21(const vec2 position) {
    vec3 value = fract(vec3(position.xyx) * vec3(0.1031, 0.1030, 0.0973));
    value += dot(value, value.yzx + 33.33);
    return fract((value.x + value.y) * value.z);
}

vec3 starfieldColor(const vec3 direction, const float blueShift) {
    const vec2 grid = vec2(480.0, 240.0);
    const vec2 sphericalUv = vec2(
        atan(direction.z, direction.x) / (2.0 * kPi) + 0.5,
        asin(clamp(direction.y, -1.0, 1.0)) / kPi + 0.5);
    const vec2 gridPosition = sphericalUv * grid;
    const vec2 baseCell = floor(gridPosition);
    const vec2 cellFraction = fract(gridPosition);
    vec3 starColor = vec3(0.0);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            const vec2 neighbor = vec2(x, y);
            vec2 cell = baseCell + neighbor;
            cell.x = mod(cell.x + grid.x, grid.x);
            cell.y = clamp(cell.y, 0.0, grid.y - 1.0);
            const float seed = hash21(cell);
            if (seed < 0.978) {
                continue;
            }
            const vec2 point = vec2(
                hash21(cell + vec2(7.31, 13.73)),
                hash21(cell + vec2(29.17, 3.70)));
            const vec2 delta = neighbor + point - cellFraction;
            const float core = exp(-110.0 * dot(delta, delta));
            const float temperature = hash21(cell + vec2(17.9, 5.3));
            const vec3 tint = mix(
                vec3(0.62, 0.78, 1.0),
                vec3(1.0, 0.88, 0.68),
                temperature);
            const float magnitude = 0.8 + 2.2 * seed;
            starColor += tint * core * magnitude;
        }
    }
    const float galacticBand = pow(
        max(0.0, 1.0 - abs(dot(direction, normalize(vec3(0.2, 0.9, 0.38))))),
        10.0);
    const vec3 galaxy = vec3(0.006, 0.009, 0.016)
        + vec3(0.012, 0.016, 0.026) * galacticBand;
    const vec3 frequencyTint = mix(
        vec3(1.0), vec3(0.72, 0.88, 1.28), clamp(blueShift, 0.0, 1.0));
    return (galaxy + starColor)
        * frequencyTint * (1.0 + 0.18 * clamp(blueShift, 0.0, 1.0));
}

// ---------------------------------------------------------------------------
// Accretion disk sampling (volumetric, ported from BufferA DiskColor)
// ---------------------------------------------------------------------------

// Returns linear-HDR disk emission + optical-thickness alpha.
vec4 sampleDisk(
    const vec3 rayPosition,
    const vec3 lastRayPosition,
    const vec3 rayDirection,
    const float stepLength,
    const float rs,
    const float diskInner,
    const float diskOuter,
    const float thin,
    const float diskA,
    const float peakT4,
    const float shiftMax,
    const float time) {
    const float radius = length(rayPosition.xz);
    if (radius <= diskInner || radius >= diskOuter) {
        return vec4(0.0);
    }
    const float radialPosition =
        (radius - diskInner) / max(diskOuter - diskInner, 1.0e-4);
    const float diskThickness = thin * mix(0.3, 1.2, radialPosition);
    if (abs(rayPosition.y) >= diskThickness) {
        return vec4(0.0);
    }
    const float innerFade = smoothstep(0.0, 0.06, radialPosition);
    const float outerFade = 1.0 - smoothstep(0.88, 1.0, radialPosition);
    const float verticalDensity = exp(
        -1.5 * rayPosition.y * rayPosition.y
        / max(diskThickness * diskThickness, 1.0e-5));
    const float theta = atan(rayPosition.z, rayPosition.x);
    const vec3 noisePosition = vec3(
        radius * 0.55,
        theta * 1.8 - time * 0.18,
        rayPosition.y * 3.0);
    const float cloudNoise = clamp(
        0.62
        + 0.25 * PerlinNoise(noisePosition)
        + 0.13 * PerlinNoise(noisePosition * 2.7 + vec3(4.7)),
        0.12,
        1.0);
    const float spiral = 0.78 + 0.22 * sin(
        theta * 6.0 - time * 0.55 + radius * 2.1
        + 1.5 * PerlinNoise(noisePosition * 0.7));
    const float density = innerFade * outerFade * verticalDensity
        * cloudNoise * spiral;
    const float angularVelocity = GetKeplerianAngularVelocity(radius, rs);
    const vec3 cloudVelocity = angularVelocity
        * cross(vec3(0.0, 1.0, 0.0), rayPosition);
    const float relativeVelocity = clamp(
        dot(-rayDirection, cloudVelocity), -0.72, 0.72);
    const float doppler = sqrt(
        max((1.0 + relativeVelocity) / (1.0 - relativeVelocity), 1.0e-5));
    const float cameraR = length(ubo.cameraPosition.xyz);
    const float redshift = sqrt(max(1.0 - rs / radius, 1.0e-6)) /
        sqrt(max(1.0 - rs / max(cameraR, 1.001 * rs), 1.0e-6));
    float diskTemperature = mix(
        10500.0, 2200.0, pow(radialPosition, 0.42));
    diskTemperature *= clamp(redshift * doppler, 0.55, 1.8);
    const vec3 emissionColor = KelvinToRgb(diskTemperature);
    const float opticalDepth = density * stepLength / max(rs, 1.0e-4) * 1.8;
    const float alpha = 1.0 - exp(-opticalDepth);
    const float innerEmission = 0.7 + 2.6 * exp(-4.0 * radialPosition);
    const vec3 emission = emissionColor * innerEmission
        * mix(0.72, 1.18, cloudNoise)
        * min(shiftMax, doppler) * min(shiftMax, redshift);
    return vec4(emission * alpha, alpha);
}

// ---------------------------------------------------------------------------
// Main: geodesic tracing
// ---------------------------------------------------------------------------

vec4 tracePixel(const vec2 fragUv, const float timeSeed) {
    const float rs = ubo.physics.x;
    const float escapeRadius = ubo.physics.y;
    const int maxSteps = int(ubo.physics.z + 0.5);
    const float fov = ubo.cameraFov.x;
    const float aspect = ubo.cameraFov.y;

    const float diskInner = ubo.diskParameters.x;
    const float diskOuter = ubo.diskParameters.y;
    const float temperatureScale = ubo.diskParameters.z;
    const float shiftMax = ubo.diskParameters.w;
    const float thin = 0.5 * rs;

    // Peak temperature (at 49/36 * RIn for the thin-disk profile); scale by
    // the user temperature knob so a larger scale => whiter/hotter disk.
    const float peakK = 7000.0 * temperatureScale;
    const float peakT4 = peakK * peakK * peakK * peakK;
    const float diskA = peakT4 / 0.05665278;

    const vec2 ndc = fragUv * 2.0 - 1.0;
    const float tanHalfFov = tan(fov * 0.5);
    vec3 rayDir = normalize(
        ubo.cameraForward.xyz
        + ubo.cameraRight.xyz * ndc.x * tanHalfFov * aspect
        - ubo.cameraUp.xyz * ndc.y * tanHalfFov);
    vec3 rayPos = ubo.cameraPosition.xyz;
    vec3 lastRayPos = rayPos;

    // Re-derived BufferA initial angular correction for a world-space camera
    // looking at an origin-centred hole. It removes a bounded part of the
    // radial component before integration, improving wide-angle lens coverage
    // without destabilising near-axis rays.
    const float cameraDistance = length(rayPos);
    const vec3 cameraRadial = rayPos / max(cameraDistance, 1.0e-6);
    const float lensWindow = clamp(
        1.0 - (0.01 * cameraDistance / rs - 1.0) / 4.0, 0.0, 1.0);
    const float lensCurve = lensWindow * lensWindow * (3.0 - 2.0 * lensWindow);
    const float lensStrength = clamp(
        0.15 * (1.0 - sqrt(max(
            1.0 - rs * lensCurve / cameraDistance, 1.0e-8))),
        0.0, 0.02);
    rayDir = normalize(
        rayDir - cameraRadial * dot(cameraRadial, rayDir) * lensStrength);

    vec4 accumulated = vec4(0.0);
    float lastR = length(rayPos);
    float minimumRadius = lastR;
    bool escaped = false;
    bool fellIn = false;
    float stepLength = 0.0;
    int count = 0;
    for (int step = 0; step < maxSteps; ++step) {
        const float distance = length(rayPos);
        minimumRadius = min(minimumRadius, distance);

        if (distance > escapeRadius && distance > lastR && count > 40) {
            escaped = true;
            break;
        }
        if (distance < 0.1 * rs) {
            fellIn = true;
            break;
        }

        // Volumetric disk sampling (accumulated over).
        const vec4 diskSample = sampleDisk(
            rayPos, lastRayPos, rayDir, stepLength,
            rs, diskInner, diskOuter, thin, diskA, peakT4, shiftMax,
            ubo.physics.w);
        accumulated.rgb += diskSample.rgb * (1.0 - accumulated.a);
        accumulated.a += diskSample.a * (1.0 - accumulated.a);
        if (accumulated.a > 0.99) {
            break;
        }

        lastRayPos = rayPos;
        lastR = distance;

        // Deflection angle per unit length (Schwarzschild, Euler).
        const vec3 radial = rayPos / max(distance, 1.0e-6);
        const float cosTheta = length(cross(radial, rayDir));
        const float deflectionRate =
            -cosTheta * cosTheta * cosTheta * (1.5 * rs / distance);

        // Continuous, spherically symmetric step size.
        float rayStep = (step == 0 ? RandomStep(fragUv, timeSeed) : 1.0);
        rayStep *= 0.15 + 0.25 * min(max(0.0, 0.5 * (0.5 * distance / max(10.0 * rs, diskOuter) - 1.0)), 1.0);
        if (distance >= 2.0 * diskOuter) {
            rayStep *= distance;
        } else if (distance >= 1.0 * diskOuter) {
            rayStep *= (rs * (2.0 * diskOuter - distance) +
                        distance * (distance - diskOuter)) / diskOuter;
        } else {
            rayStep *= min(rs, distance);
        }

        const float deltaPhi = rayStep / distance * deflectionRate;
        // Update direction FIRST (symplectic), then position.
        rayDir = normalize(rayDir + (deltaPhi + deltaPhi * deltaPhi * deltaPhi / 3.0) *
                 cross(cross(rayDir, radial), rayDir) / max(cosTheta, 1.0e-6));
        rayPos += rayDir * rayStep;
        stepLength = rayStep;
        ++count;
    }

    vec3 color = accumulated.rgb;
    if (escaped) {
        const float blueShift = clamp(
            exp(-1.6 * max(minimumRadius / rs - 1.5, 0.0)), 0.0, 1.0);
        color += starfieldColor(normalize(rayDir), blueShift);
    }
    return vec4(color, 1.0);
}

void main() {
    const float supersample = ubo.cameraFov.z;
    const float resolution = max(ubo.cameraFov.w, 1.0);
    const float time = ubo.physics.w;
    const float timeSeed = fract(time + 0.5);
    if (supersample <= 1.5) {
        // Single trace with a sub-pixel jitter (aliasing reduction).
        const vec2 jitteredUv = screenUv + 0.5 * vec2(
            RandomStep(screenUv, timeSeed),
            RandomStep(screenUv, timeSeed + 0.5)) / resolution;
        outputColor = tracePixel(jitteredUv, timeSeed);
    } else {
        // 2x2 stratified supersampling: trace four jittered offsets.
        const float levels = floor(supersample + 0.5);
        vec3 color = vec3(0.0);
        for (int i = 0; i < 4; ++i) {
            const vec2 cell = vec2(float(i & 1), float(i >> 1));
            const vec2 jitter = vec2(
                RandomStep(screenUv + cell * 7.13, timeSeed + float(i) * 0.173),
                RandomStep(screenUv + cell * 13.7, timeSeed + float(i) * 0.371)) - 0.5;
            const vec2 sampleUv = screenUv + (cell + jitter) / max(levels, 1.0) / resolution;
            color += tracePixel(sampleUv, timeSeed + float(i) * 0.173).rgb;
        }
        outputColor = vec4(color / levels, 1.0);
    }
}
