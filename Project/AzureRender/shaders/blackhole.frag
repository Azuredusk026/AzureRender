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
//  5. In-shader 2x supersampling (two jittered traces per pixel) as a
//     cheap temporal-style denoise (TAA buffering lives in the renderer
//     in later iterations).

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

float hash13(const vec3 position) {
    vec3 value = fract(position * 0.1031);
    value += dot(value, value.zyx + 31.32);
    return fract((value.x + value.y) * value.z);
}

vec3 starfieldColor(const vec3 direction) {
    const float grid = 120.0;
    const vec3 cellId = floor(direction * grid);
    const float starThreshold = 0.988;
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
                const float spike = pow(alignment, 2500.0);
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
    const vec3 posOnDisk = rayPosition;
    const float posR = length(posOnDisk.xz);
    const float posY = posOnDisk.y;
    const float lastY = lastRayPosition.y;

    // Interpolate the exact equatorial crossing point.
    if (lastY * posY >= 0.0) {
        return vec4(0.0);
    }
    vec3 hitPosition = (-posOnDisk * lastY + lastRayPosition * posY) / (posY - lastY);
    hitPosition += min(thin, length(hitPosition - lastRayPosition)) * rayDirection *
                   (-1.0 + 2.0 * RandomStep(1.0e10 * posOnDisk.zx, time));

    const float hitR = length(hitPosition.xz);
    const float hitY = hitPosition.y;
    if (abs(hitY) >= thin || hitR >= diskOuter || hitR <= diskInner) {
        return vec4(0.0);
    }

    // Effective radial coordinate (density peaks near inner edge).
    float effectiveRadius = 1.0 - ((hitR - diskInner) / (diskOuter - diskInner) * 0.5);
    if ((diskOuter - diskInner) > 9.0 * rs) {
        if (hitR < 5.0 * rs + diskInner) {
            effectiveRadius = 1.0 - ((hitR - diskInner) / (9.0 * rs) * 0.5);
        } else {
            effectiveRadius = 1.0 - (0.5 / 0.9 * 0.5 + ((hitR - diskInner) / (diskOuter - diskInner) -
                              5.0 * rs / (diskOuter - diskInner)) / (1.0 - 5.0 * rs / (diskOuter - diskInner)) * 0.5);
        }
    }

    const float density = Shape(effectiveRadius, 4.0, 0.9);
    if (abs(hitY) >= thin * density) {
        return vec4(0.0);
    }

    // Keplerian velocity of the disk material at the hit point.
    const float angularVelocity = GetKeplerianAngularVelocity(hitR, rs);
    const vec3 cloudVelocity =
        angularVelocity * cross(vec3(0.0, 1.0, 0.0), hitPosition);
    const float relativeVelocity = dot(-rayDirection, cloudVelocity);
    const float doppler = sqrt(max((1.0 + relativeVelocity) / (1.0 - relativeVelocity), 1.0e-6));
    const float cameraR = length(ubo.cameraPosition.xyz);
    const float redshift =
        doppler * sqrt(max(1.0 - rs / hitR, 1.0e-6)) /
        sqrt(max(1.0 - rs / max(cameraR, 1.001 * rs), 1.0e-6));

    // Disk temperature (standard thin disk, T^4 ~ Mdot/r^3).
    float diskTemperature = pow(
        diskA * pow(max(rs / hitR, 0.10), 3.0) * max(1.0 - sqrt(diskInner / hitR), 1.0e-6),
        0.25);
    // Redshift shifts the blackbody temperature.
    if (diskTemperature > 1000.0) {
        diskTemperature = max(1000.0, diskTemperature * redshift * doppler * doppler);
    }
    diskTemperature = min(100000.0, diskTemperature);

    // Spiral-in angle of the disk material at this radius.
    const float spiralTheta =
        12.0 * 2.0 / sqrt(3.0) * atan(sqrt(max(0.6666666 * (hitR / rs) - 1.0, 0.0)));
    const float theta = Vec2ToTheta(hitPosition.zx, vec2(cos(spiralTheta), sin(spiralTheta)));
    // Rotating radius coordinate drives the cloud texture over time.
    const float rotPosR = hitR / rs + time * 0.05;
    const float thick = thin * density * (0.4 + 0.6 * SoftSaturate(
        GenerateAccretionDiskNoise(vec3(1.5 * theta, rotPosR, 1.0), 1, 3, 80.0)));
    const float verticalMix = max(0.0, 1.0 - abs(hitY) / max(thick, 1.0e-4));

    const float cloudDetail = GenerateAccretionDiskNoise(
        vec3(1.0 * rotPosR, 1.0 * hitY / min(rs, thin / 0.1), 0.5 * theta), 3, 6, 80.0);
    vec3 cloudColor = vec3(cloudDetail) * density * 1.4 * (0.2 + 0.8 * verticalMix +
        (0.8 - 0.8 * verticalMix) * GenerateAccretionDiskNoise(
            vec3(rotPosR, 1.5 * theta, hitY / min(rs, thin / 0.1)), 1, 3, 80.0));
    float alpha = density * (1.0 - verticalMix * 0.5);

    // Inner-edge density boost + outer-edge fade.
    cloudColor *= 1.0 + 20.0 * exp(-10.0 * (hitR - diskInner) / (diskOuter - diskInner));
    const float outerFade = min(1.0, 1.8 * (diskOuter - hitR) / (diskOuter - diskInner));
    const float brightWithoutRedshift = 4.5 * diskTemperature * diskTemperature * diskTemperature * diskTemperature / peakT4;
    const vec3 rgb = KelvinToRgb(
        diskTemperature / exp((hitR - diskInner) / (0.6 * (diskOuter - diskInner))));

    vec3 diskRgb = cloudColor * brightWithoutRedshift * outerFade *
                   min(shiftMax, redshift) * min(shiftMax, doppler) * rgb;
    const float stepFactor = stepLength / rs;
    return vec4(diskRgb * stepFactor, alpha * stepFactor);
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

    // (Wide-angle gravitational lensing correction is currently disabled:
    //  it requires much larger per-pixel step budgets than ours. Will be
    //  re-enabled under BH-2.2 with adaptive subpixel jitter.)

    // Wide-angle gravitational lensing correction: a ray that *originates*

    vec4 accumulated = vec4(0.0);
    float lastR = length(rayPos);
    bool escaped = false;
    bool fellIn = false;
    float stepLength = 0.0;
    int count = 0;

    for (int step = 0; step < maxSteps; ++step) {
        const float distance = length(rayPos);

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
            timeSeed);
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
        const float deflectionRate = -1.0 * cosTheta * cosTheta * cosTheta * (1.5 * rs / distance);

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
        color += starfieldColor(normalize(rayDir));
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