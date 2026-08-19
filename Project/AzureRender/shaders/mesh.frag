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

layout(binding = 1) uniform sampler2D baseColorTexture;
layout(binding = 2) uniform sampler2D normalTexture;
layout(binding = 3) uniform sampler2D metallicRoughnessTexture;
layout(binding = 4) uniform sampler2D environmentTexture;
layout(binding = 5) uniform sampler2D specularEmissiveTexture;
layout(binding = 6) uniform sampler2D styleMaskTexture;
layout(binding = 7) uniform sampler2D matcapTexture;
layout(binding = 8) uniform sampler2D hairDataTexture;
layout(binding = 9) uniform sampler2D shadowMap;
layout(binding = 11) uniform sampler2D toonRampTexture;
layout(binding = 12) uniform sampler2D faceSdfTexture;

layout(push_constant) uniform MaterialData {
    float alphaCutoff;
    int alphaMode;
    float emissiveStrength;
    float showcasePlatform;
    vec4 aoColor;
    vec4 lamShadowColor;
    vec4 matcapColor;
    vec4 hairParameters;
    vec4 styleParameters;
    vec4 featureParameters;
    uint materialClass;
    uint materialFeatures;
    uint materialProfileVersion;
    uint materialPadding;
} material;

// materialPadding 复用为选中标志(1 = 视口拾取高亮)。
#define AZURE_SELECTED_PADDING 1U

layout(location = 0) in vec3 worldNormal;
layout(location = 1) in vec4 worldTangent;
layout(location = 2) in vec2 textureCoordinate;
layout(location = 3) in vec3 worldPosition;
layout(location = 4) in vec4 shadowPosition;
layout(location = 0) out vec4 outputColor;
layout(location = 1) out vec4 outputNormal;

vec2 directionToEquirectangular(vec3 direction) {
    const float pi = 3.14159265358979323846;
    vec3 normalizedDirection = normalize(direction);
    return vec2(
        atan(normalizedDirection.z, normalizedDirection.x)
            / (2.0 * pi) + 0.5,
        acos(clamp(normalizedDirection.y, -1.0, 1.0)) / pi);
}

vec3 decodePackedHairNormal(vec2 encodedNormal) {
    vec2 xy = vec2(
        encodedNormal.x * 2.0 - 1.0,
        1.0 - encodedNormal.y * 2.0);
    float z = sqrt(max(1.0 - dot(xy, xy), 0.001));
    return normalize(vec3(xy, z));
}

vec3 materialClassColor(uint materialClass) {
    const vec3 palette[10] = vec3[10](
        vec3(0.45, 0.45, 0.45),
        vec3(0.96, 0.58, 0.48),
        vec3(1.00, 0.78, 0.66),
        vec3(0.82, 0.20, 0.42),
        vec3(0.20, 0.58, 0.92),
        vec3(0.78, 0.82, 0.88),
        vec3(0.45, 0.92, 0.96),
        vec3(0.74, 0.34, 0.92),
        vec3(1.00, 0.32, 0.08),
        vec3(0.12, 0.78, 0.58));
    return palette[min(materialClass, 9U)];
}

float materialFeatureEnabled(uint feature) {
    return (material.materialFeatures & feature) != 0U ? 1.0 : 0.0;
}

vec3 sampleToonRamp(float coordinate) {
    vec2 atlasSize = vec2(textureSize(toonRampTexture, 0));
    float minimumU = 0.5 / atlasSize.x;
    float maximumU = 1.0 - minimumU;
    float row = float(min(material.materialClass, 9U));
    vec2 uv = vec2(
        mix(minimumU, maximumU, clamp(coordinate, 0.0, 1.0)),
        (row + 0.5) / atlasSize.y);
    return texture(toonRampTexture, uv).rgb;
}

float sampleShadowMap(vec4 lightClipPosition, float normalDotLight) {
    vec3 projected = lightClipPosition.xyz / lightClipPosition.w;
    vec2 shadowUv = projected.xy * 0.5 + 0.5;
    if (projected.z <= 0.0 || projected.z >= 1.0
        || any(lessThan(shadowUv, vec2(0.0)))
        || any(greaterThan(shadowUv, vec2(1.0)))) {
        return 1.0;
    }

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float bias = max(0.0011 * (1.0 - normalDotLight), 0.00025);
    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float storedDepth = texture(
                shadowMap,
                shadowUv + vec2(x, y) * texelSize).r;
            visibility +=
                projected.z - bias <= storedDepth ? 1.0 : 0.0;
        }
    }
    return visibility / 9.0;
}

void main() {
    vec4 baseColor = texture(baseColorTexture, textureCoordinate);
    bool overlayMaterial = material.materialClass == 7U
        && materialFeatureEnabled(16U) > 0.5;
    bool browOverlay = overlayMaterial
        && materialFeatureEnabled(64U) > 0.5;
    if (browOverlay) {
        float basePower = max(material.featureParameters.w, 0.001);
        baseColor.rgb = clamp(
            pow(max(baseColor.rgb, vec3(0.0)), vec3(basePower)),
            vec3(0.0),
            vec3(1.0));
    }
    float platformMask = clamp(material.showcasePlatform, 0.0, 1.0);
    float platformRadius = length(textureCoordinate - vec2(0.5)) * 2.0;
    float contactShadow = 1.0 - smoothstep(0.10, 0.62, platformRadius);
    float platformRing = smoothstep(0.68, 0.98, platformRadius);
    float showcasePreset = floor(camera.showcaseParameters.x + 0.5);
    vec3 platformPresetTint = vec3(1.0);
    if (showcasePreset == 1.0) {
        platformPresetTint = vec3(0.72, 0.76, 0.77);
    } else if (showcasePreset == 2.0) {
        platformPresetTint = vec3(1.16, 1.14, 1.10);
    }
    baseColor.rgb *= mix(
        vec3(1.0),
        mix(vec3(0.52), vec3(1.08), platformRing)
            * mix(vec3(1.0), vec3(0.60), contactShadow)
            * platformPresetTint,
        platformMask);
    if (showcasePreset == 1.0 && platformMask > 0.0) {
        float signalRing = smoothstep(0.72, 0.75, platformRadius)
            * (1.0 - smoothstep(0.78, 0.81, platformRadius));
        float tickAngle = atan(
            textureCoordinate.y - 0.5,
            textureCoordinate.x - 0.5);
        float ticks = smoothstep(0.78, 0.94, abs(cos(tickAngle * 12.0)));
        baseColor.rgb += vec3(0.34, 0.105, 0.012)
            * signalRing * ticks * platformMask;
    }
    if (material.alphaMode == 1 && baseColor.a < material.alphaCutoff) {
        discard;
    }

    vec3 geometricNormal = normalize(worldNormal);
    if (!gl_FrontFacing) {
        geometricNormal = -geometricNormal;
    }
    vec3 tangent = normalize(
        worldTangent.xyz
        - geometricNormal * dot(geometricNormal, worldTangent.xyz));
    vec3 bitangent = normalize(cross(geometricNormal, tangent)) * worldTangent.w;
    mat3 tangentToWorld = mat3(tangent, bitangent, geometricNormal);
    vec3 sampledNormal = texture(normalTexture, textureCoordinate).xyz * 2.0 - 1.0;
    vec3 shadedNormal = normalize(tangentToWorld * sampledNormal);
    vec4 hairData = texture(hairDataTexture, textureCoordinate);
    float hairActive = clamp(material.hairParameters.w, 0.0, 1.0)
        * materialFeatureEnabled(2U);
    float hairBasePeak = max(
        max(baseColor.r, baseColor.g),
        max(baseColor.b, 0.001));
    vec3 hairBaseHue = baseColor.rgb / hairBasePeak;
    vec3 hairBaseNormal = normalize(
        tangentToWorld * decodePackedHairNormal(hairData.rg));
    shadedNormal = normalize(mix(
        shadedNormal,
        hairBaseNormal,
        hairActive * 0.14));

    vec4 packedMaterial = texture(metallicRoughnessTexture, textureCoordinate);
    vec4 specularEmissive = texture(
        specularEmissiveTexture,
        textureCoordinate);
    float styleStrength = camera.renderingParameters.y;
    float diffuseBandThreshold = camera.renderingParameters.z;
    float bandEnabled = step(0.0, diffuseBandThreshold);
    float styleMask = smoothstep(
        0.08,
        0.62,
        texture(styleMaskTexture, textureCoordinate).r)
        * styleStrength;
    float roughness = clamp(packedMaterial.g, 0.08, 1.0);
    float metallic = clamp(packedMaterial.b, 0.0, 1.0);
    float specularLevel = clamp(specularEmissive.a, 0.0, 1.0);
    // Character surface classes use authored packed maps from several source
    // conventions. Keep dielectric surfaces physically bounded while leaving
    // the dedicated Metal class untouched.
    if (material.materialClass == 1U || material.materialClass == 2U) {
        metallic = min(metallic, 0.015);
        roughness = max(roughness, material.materialClass == 2U ? 0.58 : 0.52);
        specularLevel = min(specularLevel, material.materialClass == 2U ? 0.22 : 0.32);
    } else if (material.materialClass == 3U) {
        metallic = min(metallic, 0.04);
        roughness = max(roughness, 0.32);
        specularLevel = min(specularLevel, 0.46);
    } else if (material.materialClass == 4U) {
        metallic = min(metallic, 0.06);
        roughness = max(roughness, 0.42);
    } else if (material.materialClass == 6U) {
        metallic = 0.0;
    }
    vec3 lightDirection = normalize(vec3(0.48, 0.82, 0.32));
    vec3 fillDirection = normalize(vec3(-0.62, 0.34, -0.48));
    vec3 viewDirection = normalize(camera.cameraPosition.xyz - worldPosition);
    vec3 halfDirection = normalize(lightDirection + viewDirection);
    vec3 reflectionDirection = reflect(-viewDirection, shadedNormal);
    float diffuse = max(dot(shadedNormal, lightDirection), 0.0);
    float fillDiffuse = max(dot(shadedNormal, fillDirection), 0.0);
    float shadowVisibility = sampleShadowMap(shadowPosition, diffuse);
    int qaEffectMode = int(floor(camera.qaParameters.y + 0.5));
    bool qaEffectDisabled = camera.qaParameters.z < 0.5;
    if (qaEffectMode == 2 && qaEffectDisabled) {
        shadowVisibility = 1.0;
    }
    float keyVisibility = mix(1.0, shadowVisibility, 0.72);
    if (material.materialClass == 2U) {
        keyVisibility = max(keyVisibility, 0.84);
    } else if (material.materialClass == 1U) {
        keyVisibility = max(keyVisibility, 0.58);
    }
    vec3 keyColor = vec3(1.04, 0.98, 0.94);
    vec3 fillColor = vec3(0.58, 0.76, 1.0);
    vec3 rimColor = vec3(0.20, 0.34, 0.52);
    if (showcasePreset == 1.0) {
        keyColor = vec3(1.04, 0.98, 0.90);
        fillColor = vec3(0.38, 0.53, 0.61);
        rimColor = vec3(0.20, 0.55, 0.64);
    } else if (showcasePreset == 2.0) {
        keyColor = vec3(1.0);
        fillColor = vec3(0.72, 0.78, 0.85);
        rimColor = vec3(0.62, 0.68, 0.74);
    }
    float specularPower = mix(96.0, 6.0, roughness);
    float specularLobe = pow(
        max(dot(shadedNormal, halfDirection), 0.0),
        specularPower);
    vec3 f0 = mix(vec3(0.04 * specularLevel), baseColor.rgb, metallic);
    float normalDotView = max(dot(shadedNormal, viewDirection), 0.0);
    vec3 fresnel =
        f0 + (1.0 - f0) * pow(1.0 - normalDotView, 5.0);
    vec3 diffuseColor = baseColor.rgb * (1.0 - metallic);
    vec3 environmentDiffuse = texture(
        environmentTexture,
        directionToEquirectangular(shadedNormal)).rgb;
    // Prefiltered specular: sample the HDR environment mip chain by
    // roughness (mip 0 is the sharp sun, higher mips are prefiltered).
    float envMipCount = 7.0;
    float specularMip = clamp(roughness * (envMipCount - 1.0), 0.0, envMipCount - 1.0);
    vec3 environmentSpecular = textureLod(
        environmentTexture,
        directionToEquirectangular(reflectionDirection),
        specularMip).rgb;
    environmentSpecular = mix(
        environmentSpecular,
        vec3(0.18, 0.22, 0.26),
        roughness * roughness * 0.65);
    // Directional environment irradiance: upward and horizon-facing normals
    // receive the authored sky color while occluded/downward surfaces retain
    // a controlled floor instead of collapsing to black.
    vec3 ambientDiffuse =
        diffuseColor * (vec3(0.32) + environmentDiffuse * 0.82);
    float environmentLuminance = dot(
        environmentDiffuse, vec3(0.2126, 0.7152, 0.0722));
    vec3 hairAmbientDiffuse = diffuseColor
        * (0.42 + min(environmentLuminance, 0.80) * 0.26);
    ambientDiffuse = mix(ambientDiffuse, hairAmbientDiffuse, hairActive);
    float platformAmbientVisibility = mix(
        1.0,
        mix(0.44, 1.0, shadowVisibility),
        platformMask);
    ambientDiffuse *= platformAmbientVisibility;
    vec3 ambientSpecular =
        environmentSpecular * fresnel * mix(0.70, 0.20, roughness);
    ambientSpecular +=
        baseColor.rgb * metallic * mix(0.06, 0.015, roughness);
    float toonEnabled = qaEffectMode == 1 && qaEffectDisabled
        ? 0.0
        : bandEnabled;
    float toonWeight = clamp(toonEnabled * material.styleParameters.x, 0.0, 1.0);
    float rampThresholdOffset = 0.40 - max(diffuseBandThreshold, 0.0);
    float rampCoordinate = clamp(
        diffuse + rampThresholdOffset
            - styleMask * mix(0.04, 0.13, material.styleParameters.y),
        0.0,
        1.0);
    vec4 faceSdfSample = texture(faceSdfTexture, textureCoordinate);
    float faceSdfEligible = material.materialClass == 2U
        ? materialFeatureEnabled(4U)
        : 0.0;
    float faceSdfEnabled = camera.faceSdfParameters.x
        * camera.faceLightDirection.w
        * faceSdfEligible;
    if (qaEffectMode == 8 && qaEffectDisabled) {
        faceSdfEnabled = 0.0;
    }
    float faceCoordinate = camera.faceSdfParameters.w > 0.5
        ? 1.0 - faceSdfSample.r
        : faceSdfSample.r;
    float lateralLight = camera.faceLightDirection.x;
    float frontLight = max(-camera.faceLightDirection.z, 0.0);
    float orientedCoordinate = lateralLight >= 0.0
        ? faceCoordinate
        : 1.0 - faceCoordinate;
    float faceThreshold = clamp(
        camera.faceSdfParameters.y
            - frontLight * 0.34
            + (1.0 - abs(lateralLight)) * 0.04,
        0.08,
        0.92);
    float faceIllumination = smoothstep(
        faceThreshold - camera.faceSdfParameters.z,
        faceThreshold + camera.faceSdfParameters.z,
        orientedCoordinate);
    float faceSdfWeight = faceSdfEnabled * faceSdfSample.a * 0.62;
    rampCoordinate = mix(
        rampCoordinate,
        mix(0.74, 0.98, faceIllumination),
        faceSdfWeight);
    vec3 classRamp = sampleToonRamp(rampCoordinate);
    float rampLuminance = dot(classRamp, vec3(0.2126, 0.7152, 0.0722));
    vec3 diffuseResponse = mix(vec3(diffuse), classRamp, toonWeight);
    float ambientRampVisibility = mix(
        1.0,
        mix(0.64, 0.98, rampLuminance),
        toonWeight);
    ambientDiffuse *= ambientRampVisibility;
    float diffuseScale = mix(0.62, 0.68, toonWeight);
    vec3 directDiffuse =
        diffuseColor
        * (
            diffuseResponse * diffuseScale * keyColor
                * camera.showcaseParameters.y
                * keyVisibility
            + fillDiffuse * camera.showcaseParameters.z * fillColor);
    directDiffuse *= mix(1.0, 0.72, hairActive);
    float shadowRegion = 1.0 - smoothstep(0.38, 0.66, rampLuminance);
    float shadowSystemWeight = clamp(
        bandEnabled * material.styleParameters.x,
        0.0,
        1.0);
    float shadowWeight =
        max(shadowRegion, (1.0 - shadowVisibility) * 0.72)
        * shadowSystemWeight * material.styleParameters.y
        * materialFeatureEnabled(1U);
    shadowWeight *= material.materialClass == 2U
        ? 0.18
        : (material.materialClass == 1U ? 0.48 : 1.0);
    vec3 lamShadowTint = mix(
        vec3(1.0),
        material.lamShadowColor.rgb,
        material.lamShadowColor.a * shadowWeight * 0.58);
    float aoClassWeight = material.materialClass == 2U
        ? 0.10
        : (material.materialClass == 1U ? 0.28 : 1.0);
    vec3 aoShadowTint = mix(
        vec3(1.0),
        material.aoColor.rgb,
        material.aoColor.a
            * (0.10 + clamp(shadowWeight + styleMask * 0.42, 0.0, 1.0)
                * 0.38)
            * aoClassWeight);
    // Hair AO is a stable stylized volume layer, not merely a multiplier in
    // already shadowed pixels. Several source instances author AO alpha as
    // zero, so Hair uses the authored RGB with a conservative class fallback.
    vec3 hairAoColor = material.aoColor.a > 0.01
        ? material.aoColor.rgb
        : vec3(0.255);
    float hairCavity = clamp(
        styleMask * 0.62
            + (1.0 - max(dot(geometricNormal, viewDirection), 0.0)) * 0.22,
        0.0,
        1.0);
    vec3 hairAoTint = mix(
        vec3(1.0),
        hairAoColor,
        hairActive * (0.10 + hairCavity * 0.32));
    aoShadowTint *= hairAoTint;
    vec3 tintedDiffuse = ambientDiffuse * aoShadowTint
        + directDiffuse * lamShadowTint * aoShadowTint;
    // Toon/environment energy may raise the hair value but must not erase its
    // authored red hue. Reproject only the diffuse hair layer onto the base
    // hue; specular and KK remain independent highlights.
    float hairDiffusePeak = max(
        max(tintedDiffuse.r, tintedDiffuse.g),
        tintedDiffuse.b);
    vec3 huePreservedHair = hairBaseHue * hairDiffusePeak;
    tintedDiffuse = mix(
        tintedDiffuse,
        huePreservedHair,
        hairActive * 0.68);
    tintedDiffuse *= mix(
        vec3(1.0),
        camera.faceSdfShadowColor.rgb,
        (1.0 - faceIllumination)
            * faceSdfWeight
            * camera.faceSdfShadowColor.a
            * 0.20);
    vec3 directSpecular =
        f0 * specularLobe * diffuse * mix(0.7, 0.12, roughness)
        * keyVisibility * material.styleParameters.z;
    ambientSpecular *= material.styleParameters.z;
    float dielectricSpecularWeight = material.materialClass == 1U
        ? 0.05
        : (material.materialClass == 2U
            ? 0.03
            : (material.materialClass == 3U ? 0.14 : 1.0));
    ambientSpecular *= dielectricSpecularWeight;
    directSpecular *= dielectricSpecularWeight;
    // Prevent a bright sky texel from bleaching red hair to grey. Hair uses
    // its base hue as the energy-preserving environment response.
    ambientSpecular = mix(
        ambientSpecular,
        ambientSpecular * mix(baseColor.rgb, vec3(1.0), 0.18),
        hairActive);
    directSpecular *= mix(1.0, 1.30, styleMask * toonWeight);
    ambientSpecular *= mix(1.0, 1.12, styleMask * toonWeight);
    if (qaEffectMode == 5 && qaEffectDisabled) {
        ambientSpecular = vec3(0.0);
        directSpecular = vec3(0.0);
    }
    float rim = pow(1.0 - normalDotView, 3.2)
        * smoothstep(-0.25, 0.65, dot(shadedNormal, -fillDirection));
    vec3 rimLighting =
        mix(rimColor, baseColor.rgb, 0.22)
        * rim
        * camera.showcaseParameters.w
        * bandEnabled
        * material.styleParameters.w
        * (1.0 - platformMask);
    rimLighting *= mix(1.0, 0.22, hairActive);
    rimLighting = mix(
        rimLighting,
        rimLighting * hairBaseHue,
        hairActive * 0.75);
    if (qaEffectMode == 4 && qaEffectDisabled) {
        rimLighting = vec3(0.0);
    }
    vec3 hairHighlightNormal = normalize(
        tangentToWorld * decodePackedHairNormal(hairData.ba));
    float hairShift = dot(hairHighlightNormal, tangent) * 0.16;
    vec3 hairStrandDirection = normalize(
        bitangent + shadedNormal * hairShift);
    vec3 secondaryHairStrand = normalize(
        bitangent
        + shadedNormal * (hairShift + material.hairParameters.z * 0.018));
    float tangentDotHalf = dot(hairStrandDirection, halfDirection);
    float secondaryTangentDotHalf = dot(
        secondaryHairStrand,
        halfDirection);
    float kkSine = sqrt(max(1.0 - tangentDotHalf * tangentDotHalf, 0.0));
    float secondaryKkSine = sqrt(max(
        1.0 - secondaryTangentDotHalf * secondaryTangentDotHalf,
        0.0));
    float kkPower = clamp(material.hairParameters.x * 0.40, 80.0, 220.0);
    float secondaryKkPower = clamp(kkPower * 0.42, 38.0, 96.0);
    float kkLobe = pow(kkSine, kkPower);
    float secondaryKkLobe = pow(secondaryKkSine, secondaryKkPower);
    // Direct lobe-to-ramp mapping preserves a narrow anti-aliased band. The
    // previous high threshold discarded the complete lobe at normal camera
    // distances and made Hair KK effectively black in its QA isolation.
    float kkBand = smoothstep(0.28, 0.72, kkLobe);
    float secondaryKkBand = smoothstep(0.22, 0.64, secondaryKkLobe);
    float hairViewVisibility = smoothstep(
        -0.20,
        0.45,
        dot(geometricNormal, viewDirection));
    vec3 kkTint = mix(
        baseColor.rgb,
        vec3(1.0, 0.68, 0.74),
        0.18);
    vec3 kkSpecular =
        kkTint
        * kkBand
        * mix(0.58, 1.0, hairViewVisibility)
        * max(material.hairParameters.y, 0.16)
        * 0.82
        * hairActive
        * material.featureParameters.y
        * bandEnabled;
    vec3 secondaryKkTint = mix(
        baseColor.rgb,
        vec3(0.72, 0.88, 1.0),
        0.12);
    vec3 secondaryKkSpecular =
        secondaryKkTint
        * secondaryKkBand
        * mix(0.35, 0.72, hairViewVisibility)
        * max(material.hairParameters.y, 0.16)
        * 0.34
        * hairActive
        * material.featureParameters.y
        * bandEnabled;
    if (qaEffectMode == 3 && qaEffectDisabled) {
        kkSpecular = vec3(0.0);
        secondaryKkSpecular = vec3(0.0);
    }
    float outputAlpha = material.alphaMode == 2 ? baseColor.a : 1.0;
    if (browOverlay) {
        // M_Common_Brow uses a constant Opaccity parameter; the face texture
        // alpha is not the brow mask and is intentionally ignored here.
        outputAlpha = material.featureParameters.y;
    }
    vec3 emissive =
        specularEmissive.rgb * material.emissiveStrength * 6.0
        * material.featureParameters.z
        * materialFeatureEnabled(8U);
    if (qaEffectMode == 6 && qaEffectDisabled) {
        emissive = vec3(0.0);
    }
    vec3 cameraForward = normalize(worldPosition - camera.cameraPosition.xyz);
    vec3 cameraRight = normalize(cross(cameraForward, vec3(0.0, 1.0, 0.0)));
    vec3 cameraUp = normalize(cross(cameraRight, cameraForward));
    vec2 matcapUv = clamp(
        vec2(
            dot(shadedNormal, cameraRight) * 0.5 + 0.5,
            0.5 - dot(shadedNormal, cameraUp) * 0.5),
        vec2(0.002),
        vec2(0.998));
    vec2 matcapTexel = 5.0 / vec2(textureSize(matcapTexture, 0));
    float matcapMask =
        texture(matcapTexture, matcapUv).r * 0.20
        + texture(matcapTexture, matcapUv + vec2(matcapTexel.x, 0.0)).r * 0.125
        + texture(matcapTexture, matcapUv - vec2(matcapTexel.x, 0.0)).r * 0.125
        + texture(matcapTexture, matcapUv + vec2(0.0, matcapTexel.y)).r * 0.125
        + texture(matcapTexture, matcapUv - vec2(0.0, matcapTexel.y)).r * 0.125
        + texture(matcapTexture, matcapUv + matcapTexel).r * 0.075
        + texture(matcapTexture, matcapUv - matcapTexel).r * 0.075
        + texture(
            matcapTexture,
            matcapUv + vec2(matcapTexel.x, -matcapTexel.y)).r * 0.075
        + texture(
            matcapTexture,
            matcapUv + vec2(-matcapTexel.x, matcapTexel.y)).r * 0.075;
    vec3 matcapTint = mix(
        baseColor.rgb,
        material.matcapColor.rgb,
        0.10);
    vec3 matcapAccent =
        matcapMask
        * matcapTint
        * material.matcapColor.a
        * material.featureParameters.w
        * materialFeatureEnabled(4U)
        * bandEnabled
        * 0.055;
    vec3 beautyColor =
        tintedDiffuse
        + ambientSpecular
        + directSpecular
        + rimLighting
        + kkSpecular
        + secondaryKkSpecular
        + matcapAccent
        + emissive;
    int qaIsolationMode = int(floor(camera.qaParameters.x + 0.5));
    bool overlayDisabled = qaEffectMode == 9 && qaEffectDisabled;
    float overlayVisible = overlayMaterial && !overlayDisabled ? 1.0 : 0.0;
    beautyColor = mix(beautyColor, baseColor.rgb, overlayVisible);
    if (overlayMaterial && overlayDisabled) {
        outputAlpha = 0.0;
    }
    vec3 qaColor = beautyColor;
    if (qaIsolationMode == 1) {
        qaColor = baseColor.rgb;
    } else if (qaIsolationMode == 2) {
        qaColor = mix(vec3(diffuse), classRamp, toonWeight);
    } else if (qaIsolationMode == 3) {
        qaColor = vec3(shadowVisibility);
    } else if (qaIsolationMode == 4) {
        qaColor = (kkSpecular + secondaryKkSpecular) * 3.0;
    } else if (qaIsolationMode == 5) {
        qaColor = rimLighting * 3.0;
    } else if (qaIsolationMode == 6) {
        qaColor = (ambientSpecular + directSpecular) * 3.0;
    } else if (qaIsolationMode == 7) {
        qaColor = emissive;
    } else if (qaIsolationMode == 8) {
        qaColor = materialClassColor(material.materialClass);
    } else if (qaIsolationMode == 9) {
        qaColor = vec3(styleMask);
    } else if (qaIsolationMode == 10) {
        qaColor = ambientDiffuse;
    } else if (qaIsolationMode == 11) {
        qaColor = directDiffuse;
    } else if (qaIsolationMode == 12) {
        qaColor = clamp(
            (vec3(1.0) - lamShadowTint * aoShadowTint) * 5.0,
            vec3(0.0),
            vec3(1.0));
    } else if (qaIsolationMode == 13) {
        qaColor = material.materialClass == 2U
            ? mix(vec3(0.04), vec3(faceIllumination), faceSdfSample.a)
            : vec3(0.0);
    } else if (qaIsolationMode == 14) {
        qaColor = overlayMaterial ? baseColor.rgb : vec3(0.0);
    }
    outputColor = vec4(qaColor, outputAlpha);
    if (material.materialPadding == AZURE_SELECTED_PADDING) {
        outputColor.rgb = mix(outputColor.rgb, vec3(0.96, 0.62, 0.10), 0.42);
        outputColor.a = 1.0;
    }
    float innerOutlineParticipation =
        (1.0 - platformMask)
        * material.featureParameters.x
        * mix(
            1.0,
            0.22,
            max(hairActive, clamp(material.matcapColor.a, 0.0, 1.0)));
    outputNormal = vec4(
        geometricNormal * 0.5 + 0.5,
        innerOutlineParticipation);
}
