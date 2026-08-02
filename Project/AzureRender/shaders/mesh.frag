#version 450

layout(binding = 0) uniform CameraData {
    mat4 model;
    mat4 modelViewProjection;
    mat4 lightModelViewProjection;
    vec4 cameraPosition;
    vec4 renderingParameters;
    vec4 showcaseParameters;
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

layout(push_constant) uniform MaterialData {
    float alphaCutoff;
    int alphaMode;
    float emissiveStrength;
    float showcasePlatform;
    vec4 aoColor;
    vec4 lamShadowColor;
    vec4 matcapColor;
    vec4 hairParameters;
} material;

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
    float platformMask = clamp(material.showcasePlatform, 0.0, 1.0);
    float platformRadius = length(textureCoordinate - vec2(0.5)) * 2.0;
    float contactShadow = 1.0 - smoothstep(0.10, 0.62, platformRadius);
    float platformRing = smoothstep(0.68, 0.98, platformRadius);
    float showcasePreset = floor(camera.showcaseParameters.x + 0.5);
    vec3 platformPresetTint = vec3(1.0);
    if (showcasePreset == 1.0) {
        platformPresetTint = vec3(0.62, 1.02, 1.08);
    } else if (showcasePreset == 2.0) {
        platformPresetTint = vec3(1.16, 1.14, 1.10);
    }
    baseColor.rgb *= mix(
        vec3(1.0),
        mix(vec3(0.58), vec3(1.12), platformRing)
            * mix(vec3(1.0), vec3(0.72), contactShadow)
            * platformPresetTint,
        platformMask);
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
    float hairActive = clamp(material.hairParameters.w, 0.0, 1.0);
    vec3 hairBaseNormal = normalize(
        tangentToWorld * decodePackedHairNormal(hairData.rg));
    shadedNormal = normalize(mix(
        shadedNormal,
        hairBaseNormal,
        hairActive * 0.45));

    vec4 packedMaterial = texture(metallicRoughnessTexture, textureCoordinate);
    vec4 specularEmissive = texture(
        specularEmissiveTexture,
        textureCoordinate);
    float styleStrength = camera.renderingParameters.y;
    float diffuseBandThreshold = camera.renderingParameters.z;
    float diffuseBandSoftness = camera.renderingParameters.w;
    float bandEnabled = step(0.0, diffuseBandThreshold);
    float styleMask = smoothstep(
        0.08,
        0.62,
        texture(styleMaskTexture, textureCoordinate).r);
    float roughness = clamp(packedMaterial.g, 0.08, 1.0);
    float metallic = clamp(packedMaterial.b, 0.0, 1.0);
    float specularLevel = clamp(specularEmissive.a, 0.0, 1.0);
    vec3 lightDirection = normalize(vec3(0.48, 0.82, 0.32));
    vec3 fillDirection = normalize(vec3(-0.62, 0.34, -0.48));
    vec3 viewDirection = normalize(camera.cameraPosition.xyz - worldPosition);
    vec3 halfDirection = normalize(lightDirection + viewDirection);
    vec3 reflectionDirection = reflect(-viewDirection, shadedNormal);
    float diffuse = max(dot(shadedNormal, lightDirection), 0.0);
    float fillDiffuse = max(dot(shadedNormal, fillDirection), 0.0);
    float shadowVisibility = sampleShadowMap(shadowPosition, diffuse);
    float keyVisibility = mix(1.0, shadowVisibility, 0.72);
    vec3 keyColor = vec3(1.04, 0.98, 0.94);
    vec3 fillColor = vec3(0.58, 0.76, 1.0);
    vec3 rimColor = vec3(0.20, 0.34, 0.52);
    if (showcasePreset == 1.0) {
        keyColor = vec3(0.90, 1.00, 1.02);
        fillColor = vec3(0.24, 0.82, 0.88);
        rimColor = vec3(0.16, 0.62, 0.72);
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
    vec3 roughReflection = normalize(mix(
        reflectionDirection,
        shadedNormal,
        roughness * roughness * 0.8));
    vec3 sharpEnvironment = texture(
        environmentTexture,
        directionToEquirectangular(reflectionDirection)).rgb;
    vec3 roughEnvironment = texture(
        environmentTexture,
        directionToEquirectangular(roughReflection)).rgb;
    vec3 environmentSpecular = mix(
        sharpEnvironment,
        roughEnvironment,
        roughness);
    environmentSpecular = mix(
        environmentSpecular,
        vec3(0.18, 0.22, 0.26),
        roughness * roughness * 0.65);
    vec3 ambientDiffuse =
        diffuseColor * (vec3(0.30) + environmentDiffuse * 0.70);
    float platformAmbientVisibility = mix(
        1.0,
        mix(0.44, 1.0, shadowVisibility),
        platformMask);
    ambientDiffuse *= platformAmbientVisibility;
    vec3 ambientSpecular =
        environmentSpecular * fresnel * mix(0.70, 0.20, roughness);
    ambientSpecular +=
        baseColor.rgb * metallic * mix(0.06, 0.015, roughness);
    float safeBandThreshold = max(diffuseBandThreshold, 0.0);
    float diffuseBand = smoothstep(
        safeBandThreshold - diffuseBandSoftness,
        safeBandThreshold + diffuseBandSoftness,
        diffuse);
    float stylizedDiffuse = mix(diffuse, diffuseBand, bandEnabled);
    float diffuseScale = mix(0.55, 0.50, bandEnabled);
    vec3 directDiffuse =
        diffuseColor
        * (
            stylizedDiffuse * diffuseScale * keyColor
                * camera.showcaseParameters.y
                * keyVisibility
            + fillDiffuse * camera.showcaseParameters.z * fillColor);
    float shadowWeight = (1.0 - diffuseBand) * bandEnabled;
    vec3 lamShadowTint = mix(
        vec3(1.0),
        material.lamShadowColor.rgb,
        material.lamShadowColor.a * shadowWeight * 0.35);
    vec3 aoShadowTint = mix(
        vec3(1.0),
        material.aoColor.rgb,
        material.aoColor.a * shadowWeight * 0.20);
    vec3 tintedDiffuse =
        (ambientDiffuse + directDiffuse) * lamShadowTint * aoShadowTint;
    vec3 directSpecular =
        f0 * specularLobe * diffuse * mix(0.7, 0.12, roughness)
        * keyVisibility;
    float rim = pow(1.0 - normalDotView, 3.2)
        * smoothstep(-0.25, 0.65, dot(shadedNormal, -fillDirection));
    vec3 rimLighting =
        mix(rimColor, baseColor.rgb, 0.22)
        * rim
        * camera.showcaseParameters.w
        * bandEnabled
        * (1.0 - platformMask);
    vec3 hairHighlightNormal = normalize(
        tangentToWorld * decodePackedHairNormal(hairData.ba));
    float hairShift = dot(hairHighlightNormal, tangent) * 0.16;
    vec3 hairStrandDirection = normalize(
        bitangent + shadedNormal * hairShift);
    float tangentDotHalf = dot(hairStrandDirection, halfDirection);
    float kkSine = sqrt(max(1.0 - tangentDotHalf * tangentDotHalf, 0.0));
    float kkPower = clamp(material.hairParameters.x * 0.15, 24.0, 96.0);
    float kkLobe = pow(kkSine, kkPower);
    float kkRampSoftness = mix(
        0.09,
        0.025,
        clamp(material.hairParameters.z / 8.0, 0.0, 1.0));
    float kkBand = smoothstep(
        0.34 - kkRampSoftness,
        0.34 + kkRampSoftness,
        kkLobe);
    float highlightFacing = smoothstep(
        0.22,
        0.78,
        max(dot(hairHighlightNormal, halfDirection), 0.0));
    vec3 kkTint = mix(
        baseColor.rgb,
        vec3(1.0, 0.68, 0.74),
        0.18);
    vec3 kkSpecular =
        kkTint
        * kkBand
        * mix(0.35, 1.0, highlightFacing)
        * max(diffuse, 0.18)
        * material.hairParameters.y
        * 0.48
        * keyVisibility
        * hairActive
        * bandEnabled;
    float outputAlpha = material.alphaMode == 2 ? baseColor.a : 1.0;
    vec3 emissive =
        specularEmissive.rgb * material.emissiveStrength * 3.0;
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
        * bandEnabled
        * 0.055;
    vec3 styleAccent =
        styleStrength
        * styleMask
        * (baseColor.rgb * 0.12 + vec3(0.10, 0.015, 0.004));
    outputColor = vec4(
        tintedDiffuse
            + ambientSpecular
            + directSpecular
            + rimLighting
            + kkSpecular
            + styleAccent
            + matcapAccent
            + emissive,
        outputAlpha);
    float innerOutlineParticipation =
        (1.0 - platformMask)
        * mix(
            1.0,
            0.22,
            max(hairActive, clamp(material.matcapColor.a, 0.0, 1.0)));
    outputNormal = vec4(
        geometricNormal * 0.5 + 0.5,
        innerOutlineParticipation);
}
