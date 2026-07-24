#version 450

layout(binding = 0) uniform CameraData {
    mat4 model;
    mat4 modelViewProjection;
    vec4 cameraPosition;
    vec4 renderingParameters;
} camera;

layout(binding = 1) uniform sampler2D baseColorTexture;
layout(binding = 2) uniform sampler2D normalTexture;
layout(binding = 3) uniform sampler2D metallicRoughnessTexture;
layout(binding = 4) uniform sampler2D environmentTexture;
layout(binding = 5) uniform sampler2D specularEmissiveTexture;
layout(binding = 6) uniform sampler2D styleMaskTexture;
layout(binding = 7) uniform sampler2D matcapTexture;
layout(binding = 8) uniform sampler2D hairDataTexture;

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
layout(location = 0) out vec4 outputColor;

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

void main() {
    vec4 baseColor = texture(baseColorTexture, textureCoordinate);
    float platformMask = clamp(material.showcasePlatform, 0.0, 1.0);
    float platformRadius = length(textureCoordinate - vec2(0.5)) * 2.0;
    float contactShadow = 1.0 - smoothstep(0.10, 0.62, platformRadius);
    float platformRing = smoothstep(0.68, 0.98, platformRadius);
    baseColor.rgb *= mix(
        vec3(1.0),
        mix(vec3(0.58), vec3(1.12), platformRing)
            * mix(vec3(1.0), vec3(0.48), contactShadow),
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
            stylizedDiffuse * diffuseScale * vec3(1.04, 0.98, 0.94)
            + fillDiffuse * 0.13 * vec3(0.58, 0.76, 1.0));
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
        f0 * specularLobe * diffuse * mix(0.7, 0.12, roughness);
    float rim = pow(1.0 - normalDotView, 3.2)
        * smoothstep(-0.25, 0.65, dot(shadedNormal, -fillDirection));
    vec3 rimLighting =
        mix(vec3(0.20, 0.34, 0.52), baseColor.rgb, 0.22)
        * rim
        * 0.12
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
}
