#version 450
#extension GL_GOOGLE_include_directive : require

#if defined(AZURE_BINDLESS)
#extension GL_EXT_nonuniform_qualifier : require
#endif
#include "shadow_compare.glsl"
#include "surface_response.glsl"

#if !defined(AZURE_OPAQUE_SNAPSHOT)
layout(binding = 18) uniform sampler2D overlaySceneDepth;
layout(binding = 19) uniform sampler2D opaqueSceneColor;
#endif

layout(binding = 0) uniform CameraData {
    vec4 cameraPosition;
    vec4 cameraForward;
    vec4 clusterGrid;
    vec4 clusterDepth;
    vec4 clusterLighting;
    vec4 cascadeSplits;
    vec4 renderingParameters;
    vec4 showcaseParameters;
    vec4 qaParameters;
    vec4 faceLightDirection;
    vec4 faceSdfParameters;
    vec4 faceSdfShadowColor;
    vec4 mainLightDirection;
    vec4 cascadeDepthRanges;
} camera;

#if defined(AZURE_BINDLESS)
// One global array: slots 0..2 are the shared environment, shadow map and
// toon ramp; per-material blocks of 8 follow at 3 + materialIndex * 8.
layout(binding = 1) uniform sampler2D textureArray[];
#else
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
#endif

struct ClusterLight {
    vec4 positionRadius;
    vec4 colorIntensity;
};
layout(std430, binding = 14) readonly buffer SceneLightBuffer {
    ClusterLight lights[];
} sceneLightData;
layout(std430, binding = 15) readonly buffer ClusterHeaderBuffer {
    uvec2 clusters[];
} clusterHeaderData;
layout(std430, binding = 16) readonly buffer ClusterIndexBuffer {
    uint indices[];
} clusterIndexData;

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
#if defined(AZURE_BINDLESS)
    layout(offset = 136) uint textureBase;
#endif
} material;

#if defined(AZURE_BINDLESS)
#define AZ_TEX_BASE_COLOR textureArray[nonuniformEXT(material.textureBase + 0u)]
#define AZ_TEX_NORMAL textureArray[nonuniformEXT(material.textureBase + 1u)]
#define AZ_TEX_METALLIC_ROUGHNESS textureArray[nonuniformEXT(material.textureBase + 2u)]
#define AZ_TEX_SPECULAR_EMISSIVE textureArray[nonuniformEXT(material.textureBase + 3u)]
#define AZ_TEX_STYLE_MASK textureArray[nonuniformEXT(material.textureBase + 4u)]
#define AZ_TEX_MATCAP textureArray[nonuniformEXT(material.textureBase + 5u)]
#define AZ_TEX_HAIR_DATA textureArray[nonuniformEXT(material.textureBase + 6u)]
#define AZ_TEX_FACE_SDF textureArray[nonuniformEXT(material.textureBase + 7u)]
#define AZ_TEX_ENVIRONMENT textureArray[0u]
#define AZ_TEX_SHADOW textureArray[1u]
#define AZ_TEX_TOON_RAMP textureArray[2u]
#else
#define AZ_TEX_BASE_COLOR baseColorTexture
#define AZ_TEX_NORMAL normalTexture
#define AZ_TEX_METALLIC_ROUGHNESS metallicRoughnessTexture
#define AZ_TEX_SPECULAR_EMISSIVE specularEmissiveTexture
#define AZ_TEX_STYLE_MASK styleMaskTexture
#define AZ_TEX_MATCAP matcapTexture
#define AZ_TEX_HAIR_DATA hairDataTexture
#define AZ_TEX_FACE_SDF faceSdfTexture
#define AZ_TEX_ENVIRONMENT environmentTexture
#define AZ_TEX_SHADOW shadowMap
#define AZ_TEX_TOON_RAMP toonRampTexture
#endif

// materialPadding 复用为选中标志(1 = 视口拾取高亮)。
#define AZURE_SELECTED_PADDING 1U

layout(location = 0) in vec3 worldNormal;
layout(location = 1) in vec4 worldTangent;
layout(location = 2) in vec2 textureCoordinate;
layout(location = 3) in vec3 worldPosition;
layout(location = 4) in vec4 shadowPositions[4];
layout(location = 8) in float eyebrowRegion;
layout(location = 9) flat in vec4 instanceFaceDirection;
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
    vec2 atlasSize = vec2(textureSize(AZ_TEX_TOON_RAMP, 0));
    float minimumU = 0.5 / atlasSize.x;
    float maximumU = 1.0 - minimumU;
    float row = float(min(material.materialClass, 9U));
    vec2 uv = vec2(
        mix(minimumU, maximumU, clamp(coordinate, 0.0, 1.0)),
        (row + 0.5) / atlasSize.y);
    return texture(AZ_TEX_TOON_RAMP, uv).rgb;
}

vec4 shadowPlaneQuad(ivec2 base,ivec2 lo,ivec2 hi,vec2 uv,vec2 extent,vec2 gradient) {
    vec2 first=(vec2(clamp(base,lo,hi))+.5)/extent;
    vec2 last=(vec2(clamp(base+ivec2(1),lo,hi))+.5)/extent;
    vec4 x=vec4(first.x,last.x,first.x,last.x)-uv.x;
    vec4 y=vec4(first.y,first.y,last.y,last.y)-uv.y;
    return x*gradient.x+y*gradient.y;
}
vec2 filteredShadowBlockers(vec2 uv,float receiver,ivec2 lo,ivec2 hi,vec2 depthGradient) {
    vec2 extent=vec2(textureSize(AZ_TEX_SHADOW,0));
    vec2 point=uv*extent-.5;ivec2 base=ivec2(floor(point));vec2 weight=fract(point);
    vec4 depths=shadowTexelQuad(AZ_TEX_SHADOW,base,lo,hi);
    depths-=shadowPlaneQuad(base,lo,hi,uv,extent,depthGradient);
    return shadowBlockerMoments(depths,receiver,weight);
}
float sampleCascadeShadow(int cascade,float normalDotLight,vec2 depthGradient,float normalCurvature) {
    vec3 p=shadowPositions[cascade].xyz/shadowPositions[cascade].w;
    vec2 localUv=p.xy*.5+.5;
    if(p.z<=0||p.z>=1||any(lessThan(localUv,vec2(0)))||any(greaterThan(localUv,vec2(1))))return 1.0;
    ivec2 extent=textureSize(AZ_TEX_SHADOW,0),tile=extent/2,origin=ivec2(cascade%2,cascade/2)*tile;
    ivec2 lo=origin+ivec2(1),hi=origin+tile-ivec2(2);
    vec2 uv=(vec2(origin)+localUv*vec2(tile))/vec2(extent);
    float depthRange=camera.cascadeDepthRanges[cascade];
    float worldTexel=depthRange/4.1*2.0/float(tile.x);
    float maximumRadius=clamp(camera.renderingParameters.w,1.0,16.0);
    depthGradient*=shadowReceiverPlaneWeight(normalCurvature,worldTexel*maximumRadius);
    float receiver=p.z-shadowReceiverBias(normalDotLight,worldTexel)/depthRange;
    float sum=0,blockers=0;
    for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x){
        vec2 offset=vec2(x,y)/vec2(extent);
        vec2 moments=filteredShadowBlockers(uv+offset,receiver+dot(depthGradient,offset),lo,hi,depthGradient);
        // Convert each moment back to the centre receiver plane before the
        // PCSS separation estimate; coverage is interpolated after comparison.
        sum+=moments.x-dot(depthGradient,offset)*moments.y;
        blockers+=moments.y;
    }
    float separation=blockers>0?max(receiver-sum/blockers,0)*depthRange:0;
    float contactRadius=shadowContactRadius(float(tile.x),maximumRadius);
    float radius=clamp(contactRadius+separation*.035/max(worldTexel,1e-6),contactRadius,maximumRadius);
    float visible=0,totalWeight=0;
    vec2 centre=uv*vec2(extent)-.5;
    ivec2 base=ivec2(floor(centre));vec2 fraction=fract(centre);
    int bound=int(ceil(radius+.5));
    for(int y=-bound;y<=bound;y+=2)for(int x=-bound;x<=bound;x+=2){
        vec4 weights=shadowDiskQuadWeights(vec2(x,y)-fraction,radius);
        vec4 planeDepths=vec4(receiver)+shadowPlaneQuad(base+ivec2(x,y),lo,hi,uv,vec2(extent),depthGradient);
        float weight=dot(weights,vec4(1));
        if(weight<=0)continue;
        vec4 depths=shadowTexelQuad(AZ_TEX_SHADOW,base+ivec2(x,y),lo,hi);
        visible+=dot(weights,step(planeDepths,depths));
        totalWeight+=weight;
    }
    return visible/max(totalWeight,1e-6);
}
float sampleShadowMap(float normalDotLight) {
    // Compute derivatives before nonuniform cascade selection. Each sampled
    // depth is compared against the receiver plane at that texel centre.
    vec2 gradients[4];
    vec3 receiverNormal=normalize(worldNormal);
    float normalCurvature=max(length(dFdx(receiverNormal)),length(dFdy(receiverNormal)))
        /max(max(length(dFdx(worldPosition)),length(dFdy(worldPosition))),1e-6);
    for(int i=0;i<4;++i){
        vec3 p=shadowPositions[i].xyz/shadowPositions[i].w;
        vec2 uv=(p.xy*.5+.5)*.5;
        vec2 dx=dFdx(uv),dy=dFdy(uv);float zx=dFdx(p.z),zy=dFdy(p.z);
        float determinant=dx.x*dy.y-dx.y*dy.x;
        gradients[i]=abs(determinant)>1e-12?vec2(zx*dy.y-zy*dx.y,zy*dx.x-zx*dy.x)/determinant:vec2(0);
    }
    float depth=dot(worldPosition-camera.cameraPosition.xyz,camera.cameraForward.xyz);
    int c=depth<=camera.cascadeSplits.x?0:(depth<=camera.cascadeSplits.y?1:(depth<=camera.cascadeSplits.z?2:3));
    float result=sampleCascadeShadow(c,normalDotLight,gradients[c],normalCurvature);
    if(c<3){float previous=c==0?camera.clusterDepth.x:camera.cascadeSplits[c-1];
        float blend=smoothstep(mix(previous,camera.cascadeSplits[c],.90),camera.cascadeSplits[c],depth);
        result=mix(result,sampleCascadeShadow(c+1,normalDotLight,gradients[c+1],normalCurvature),blend);}
    return result;
}

vec3 rgbToHsv(vec3 c){
    float hi=max(c.r,max(c.g,c.b)),lo=min(c.r,min(c.g,c.b)),chroma=hi-lo,h=0;
    if(chroma>1e-6){if(hi==c.r)h=(c.g-c.b)/chroma;else if(hi==c.g)h=2+(c.b-c.r)/chroma;else h=4+(c.r-c.g)/chroma;h=fract(h/6);}
    return vec3(h,hi>1e-6?chroma/hi:0,hi);
}
vec3 hsvToRgb(vec3 hsv){
    vec3 triangle=clamp(abs(fract(vec3(hsv.x)+vec3(0,2.0/3.0,1.0/3.0))*6-3)-1,0,1);
    return hsv.z*mix(vec3(1),triangle,clamp(hsv.y,0,1));
}
void main() {
    vec4 baseColor = texture(AZ_TEX_BASE_COLOR, textureCoordinate);
    bool overlayMaterial = material.materialClass == 7U
        && materialFeatureEnabled(16U) > 0.5;
    bool browOverlay = overlayMaterial
        && materialFeatureEnabled(64U) > 0.5;
    if (browOverlay) {
        // M_Common_Brow samples Face-D directly; the card geometry and its
        // authored UVs provide the shape without a separate brow mask.
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
    vec4 normalSample=texture(AZ_TEX_NORMAL,textureCoordinate);
    vec3 sampledNormal = normalSample.xyz * 2.0 - 1.0;
    vec3 shadedNormal = normalize(tangentToWorld * sampledNormal);
    vec4 hairData = texture(AZ_TEX_HAIR_DATA, textureCoordinate);
    float hairActive = material.materialClass==3U ? materialFeatureEnabled(2U) : 0.0;
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

    vec4 packedMaterial = texture(AZ_TEX_METALLIC_ROUGHNESS, textureCoordinate);
    vec4 specularEmissive = texture(AZ_TEX_SPECULAR_EMISSIVE,
        textureCoordinate);
    float styleStrength = camera.renderingParameters.y;
    float diffuseBandThreshold = camera.renderingParameters.z;
    float bandEnabled = step(0.0, diffuseBandThreshold);
    float styleMask = smoothstep(
        0.08,
        0.62,
        texture(AZ_TEX_STYLE_MASK, textureCoordinate).r)
        * styleStrength;
    float normalVariance=max(1.0-normalSample.a,0.0);
    vec3 dx=dFdx(shadedNormal),dy=dFdy(shadedNormal);
    normalVariance+=min(dot(dx,dx)+dot(dy,dy),.25);
    float roughness = clamp(sqrt(packedMaterial.g*packedMaterial.g+normalVariance*.25), 0.08, 1.0);
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
        roughness = max(roughness, 0.44);
        specularLevel = min(specularLevel, 0.30);
    } else if (material.materialClass == 6U) {
        metallic = 0.0;
    }
    vec3 lightDirection = showcasePreset == 1.0
        ? normalize(vec3(0.62, 0.68, 0.38))
        : normalize(vec3(0.48, 0.82, 0.32));
    if (camera.mainLightDirection.w > 0.5) lightDirection = camera.mainLightDirection.xyz;
    vec3 fillDirection = showcasePreset == 1.0
        ? normalize(vec3(-0.48, 0.24, -0.64))
        : normalize(vec3(-0.62, 0.34, -0.48));
    vec3 viewDirection = normalize(camera.cameraPosition.xyz - worldPosition);
    vec3 halfDirection = normalize(lightDirection + viewDirection);
    vec3 reflectionDirection = reflect(-viewDirection, shadedNormal);
    float diffuse = max(dot(shadedNormal, lightDirection), 0.0);
    float fillDiffuse = max(dot(shadedNormal, fillDirection), 0.0);
    float shadowVisibility = sampleShadowMap(max(dot(geometricNormal, lightDirection),0.0));
    int qaEffectMode = int(floor(camera.qaParameters.y + 0.5));
    bool qaEffectDisabled = camera.qaParameters.z < 0.5;
    if (qaEffectMode == 2 && qaEffectDisabled) {
        shadowVisibility = 1.0;
    }
    float keyVisibility = mix(
        1.0,
        shadowVisibility,
        showcasePreset == 1.0 ? 0.88 : 0.72);
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
    vec3 f0 = mix(vec3(dielectricF0(specularLevel)), baseColor.rgb, metallic);
    float normalDotView = max(dot(shadedNormal, viewDirection), 0.0);
    bool microfacetSurface = material.materialClass == 0U || material.materialClass == 4U
        || material.materialClass == 5U || material.materialClass == 9U;
    vec3 fresnel =
        f0 + (1.0 - f0) * pow(1.0 - normalDotView, 5.0);
    vec3 diffuseColor = baseColor.rgb * (1.0 - metallic);
    vec3 environmentDiffuse = textureLod(
        AZ_TEX_ENVIRONMENT,
        directionToEquirectangular(shadedNormal),
        6.0).rgb;
    // Prefiltered specular: sample the HDR environment mip chain by
    // roughness (mip 0 is the sharp sun, higher mips are prefiltered).
    float envMipCount = 7.0;
    float specularMip = clamp(roughness * (envMipCount - 1.0), 0.0, envMipCount - 1.0);
    vec3 environmentSpecular = textureLod(AZ_TEX_ENVIRONMENT,
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
    // Endfield is a key-light presentation, not a flat HDRI preview. Keep
    // the sky directional, but leave enough energy headroom for the fixed
    // world-space key and real-time shadow map to create readable turns.
    ambientDiffuse *= showcasePreset == 1.0 ? 0.85 : 1.0;
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
    vec4 faceSdfSample = texture(AZ_TEX_FACE_SDF, textureCoordinate);
    float faceSdfEligible = material.materialClass == 2U
        ? materialFeatureEnabled(4U)
        : 0.0;
    float faceSdfEnabled = camera.faceSdfParameters.x
        * instanceFaceDirection.w
        * faceSdfEligible;
    if (qaEffectMode == 8 && qaEffectDisabled) {
        faceSdfEnabled = 0.0;
    }
    float faceCoordinate = camera.faceSdfParameters.w > 0.5
        ? 1.0 - faceSdfSample.r
        : faceSdfSample.r;
    float lateralLight = instanceFaceDirection.x;
    float frontLight = max(-instanceFaceDirection.z, 0.0);
    // Crossing the head-local lateral axis must not switch the mirrored SDF
    // in one frame. Use a broad angular window so the lit-side transition
    // remains continuous at normal turntable speed.
    float faceSideBlend = smoothstep(-0.60, 0.60, lateralLight);
    float orientedCoordinate = mix(
        1.0 - faceCoordinate,
        faceCoordinate,
        faceSideBlend);
    float faceThreshold = clamp(
        camera.faceSdfParameters.y
            - frontLight * 0.34
            + (1.0 - abs(lateralLight)) * 0.04,
        0.08,
        0.92);
    float faceSoftness = max(camera.faceSdfParameters.z, 0.18);
    float faceIllumination = smoothstep(
        faceThreshold - faceSoftness,
        faceThreshold + faceSoftness,
        orientedCoordinate);
    float faceSdfWeight = faceSdfEnabled * faceSdfSample.a * 0.82;
    rampCoordinate = mix(
        rampCoordinate,
        mix(0.44, 0.74, faceIllumination),
        faceSdfWeight);
    vec3 classRamp = sampleToonRamp(rampCoordinate);
    vec3 indirectChroma = toonIndirectChroma(classRamp, toonWeight, material.materialClass);
    float rampLuminance = dot(classRamp, vec3(0.2126, 0.7152, 0.0722));
    vec3 diffuseResponse = mix(vec3(diffuse), classRamp, toonWeight);
    float ambientRampVisibility = mix(
        1.0,
        mix(
            showcasePreset == 1.0 ? 0.72 : 0.72,
            0.98,
            rampLuminance),
        toonWeight);
    ambientDiffuse *= ambientRampVisibility * indirectChroma;
    float diffuseScale = mix(0.62, 0.68, toonWeight);
    vec3 directDiffuse =
        diffuseColor
        * (
            diffuseResponse * diffuseScale * keyColor
                * camera.showcaseParameters.y
                * keyVisibility
            + fillDiffuse * camera.showcaseParameters.z * fillColor * indirectChroma);
    directDiffuse *= mix(1.0, 0.72, hairActive);
    uint gridX = max(uint(camera.clusterGrid.x), 1U);
    uint gridY = max(uint(camera.clusterGrid.y), 1U);
    uint gridZ = max(uint(camera.clusterGrid.z), 1U);
    vec2 screenUv = gl_FragCoord.xy / max(camera.clusterDepth.zw, vec2(1.0));
    uint clusterX = min(uint(clamp(screenUv.x, 0.0, 0.999999) * float(gridX)), gridX - 1U);
    uint clusterY = min(uint(clamp(screenUv.y, 0.0, 0.999999) * float(gridY)), gridY - 1U);
    float nearDepth = max(camera.clusterDepth.x, 0.001);
    float farDepth = max(camera.clusterDepth.y, nearDepth + 0.001);
    float viewDepth = max(
        dot(worldPosition - camera.cameraPosition.xyz, camera.cameraForward.xyz),
        nearDepth);
    float logarithmicDepth = log(viewDepth / nearDepth)
        / log(farDepth / nearDepth);
    uint clusterZ = min(
        uint(clamp(logarithmicDepth, 0.0, 0.999999) * float(gridZ)),
        gridZ - 1U);
    uint clusterIndex = (clusterZ * gridY + clusterY) * gridX + clusterX;
    uvec2 lightRange = clusterHeaderData.clusters[clusterIndex];
    vec3 clusteredDiffuse = vec3(0.0);
    vec3 clusteredSpecular = vec3(0.0);
    uint sceneLightCount = uint(max(camera.clusterGrid.w, 0.0));
    for (uint lightOffset = 0U; lightOffset < lightRange.y; ++lightOffset) {
        uint lightIndex = clusterIndexData.indices[lightRange.x + lightOffset];
        if (lightIndex >= sceneLightCount) {
            continue;
        }
        ClusterLight light = sceneLightData.lights[lightIndex];
        vec3 toLight = light.positionRadius.xyz - worldPosition;
        float distanceSquared = dot(toLight, toLight);
        float distanceToLight = sqrt(distanceSquared);
        float lightRadius = max(light.positionRadius.w, 0.001);
        float rangeFalloff = clamp(1.0 - distanceToLight / lightRadius, 0.0, 1.0);
        float attenuation = rangeFalloff * rangeFalloff
            / (1.0 + distanceSquared * 0.12);
        vec3 pointDirection = toLight / max(distanceToLight, 0.001);
        float pointDiffuse = max(dot(shadedNormal, pointDirection), 0.0);
        vec3 radiance = light.colorIntensity.rgb
            * light.colorIntensity.w * attenuation;
        clusteredDiffuse += diffuseColor * radiance * pointDiffuse * diffuseScale;
        vec3 pointHalfDirection = normalize(pointDirection + viewDirection);
        float pointSpecular = pow(
            max(dot(shadedNormal, pointHalfDirection), 0.0), specularPower);
        vec3 pointResponse = microfacetSurface
            ? directSurfaceSpecular(f0, roughness, pointDiffuse, normalDotView,
                max(dot(shadedNormal, pointHalfDirection), 0.0), max(dot(viewDirection, pointHalfDirection), 0.0))
            : f0 * pointSpecular * pointDiffuse * mix(0.7, 0.12, roughness);
        clusteredSpecular += pointResponse * radiance;
    }
    directDiffuse += clusteredDiffuse * mix(1.0, 0.72, hairActive);
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
        ? 0.28
        : (material.materialClass == 1U ? 0.62 : 1.0);
    vec3 lamShadowTint = mix(
        vec3(1.0),
        material.lamShadowColor.rgb,
        material.lamShadowColor.a * shadowWeight
            * (showcasePreset == 1.0 ? 0.36 : 0.36));
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
    float physicalAo=mix(1.0,packedMaterial.r,materialFeatureEnabled(128U));
    vec3 hairAoTint=mix(vec3(1),hairAoColor,hairActive*(1.0-physicalAo)*.65);
    aoShadowTint*=hairAoTint;
    vec3 tintedDiffuse=ambientDiffuse*aoShadowTint*physicalAo + directDiffuse*lamShadowTint;
    // Toon/environment energy may raise the hair value but must not erase its
    // authored red hue. Reproject only the diffuse hair layer onto the base
    // hue; specular and KK remain independent highlights.
    float hairDiffusePeak = max(
        max(tintedDiffuse.r, tintedDiffuse.g),
        tintedDiffuse.b);
    float hairDiffuseFloor = hairBasePeak * mix(0.24, 0.42, rampLuminance);
    hairDiffusePeak = max(hairDiffusePeak, hairDiffuseFloor);
    vec3 huePreservedHair = hairBaseHue * hairDiffusePeak;
    tintedDiffuse = mix(
        tintedDiffuse,
        huePreservedHair,
        hairActive * 0.80);
    float faceActive = material.materialClass == 2U ? 1.0 : 0.0;
    tintedDiffuse *= mix(vec3(1.0), vec3(0.86, 0.77, 0.74), faceActive);
    tintedDiffuse *= mix(
        vec3(1.0),
        camera.faceSdfShadowColor.rgb,
        (1.0 - faceIllumination)
            * faceSdfWeight
            * camera.faceSdfShadowColor.a
            * 0.55);
    vec3 directSpecular =
        (microfacetSurface
            ? directSurfaceSpecular(f0, roughness, diffuse, normalDotView,
                max(dot(shadedNormal, halfDirection), 0.0), max(dot(viewDirection, halfDirection), 0.0))
                * keyColor * camera.showcaseParameters.y
            : f0 * specularLobe * diffuse * mix(0.7, 0.12, roughness))
        * keyVisibility * material.styleParameters.z;
    directSpecular += clusteredSpecular * material.styleParameters.z;
    ambientSpecular *= material.styleParameters.z;
    float dielectricSpecularWeight = material.materialClass == 1U
        ? 0.05
        : (material.materialClass == 2U
            ? 0.03
            : (material.materialClass == 3U ? 0.08 : 1.0));
    ambientSpecular *= dielectricSpecularWeight;
    directSpecular *= dielectricSpecularWeight;
    // Prevent a bright sky texel from bleaching red hair to grey. Hair uses
    // its base hue as the energy-preserving environment response.
    ambientSpecular = mix(
        ambientSpecular,
        ambientSpecular * mix(baseColor.rgb, vec3(1.0), 0.18),
        hairActive);
    directSpecular *= mix(1.0, 1.30, styleMask * toonWeight)
        * mix(1.0, 0.72, hairActive);
    ambientSpecular *= mix(1.0, 1.12, styleMask * toonWeight)
        * mix(1.0, 0.68, hairActive);
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
        + shadedNormal * (hairShift + material.hairParameters.w));
    vec3 hairHalf=normalize(lightDirection+normalize(viewDirection+tangentToWorld*material.matcapColor.xyz));
    float tangentDotHalf = dot(hairStrandDirection, hairHalf);
    float secondaryTangentDotHalf = dot(
        secondaryHairStrand,
        hairHalf);
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
    float kkBand = smoothstep(.5-max(fwidth(kkLobe),.12),.5+max(fwidth(kkLobe),.12),kkLobe);
    float secondaryKkBand = smoothstep(.43-max(fwidth(secondaryKkLobe),.12),.43+max(fwidth(secondaryKkLobe),.12),secondaryKkLobe);
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
        * material.hairParameters.y
        * 0.58
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
        * material.hairParameters.y
        * 0.22
        * hairActive
        * material.featureParameters.y
        * bandEnabled;
    vec2 rampSize=vec2(textureSize(AZ_TEX_TOON_RAMP,0));
    float kkRamp=dot(textureLod(AZ_TEX_TOON_RAMP,vec2(clamp(kkLobe,.5/rampSize.x,1-.5/rampSize.x),(material.hairParameters.z+.5)/rampSize.y),0).rgb,vec3(.2126,.7152,.0722));
    if (materialFeatureEnabled(512U)>0.5) {
        float rampWidth=float(textureSize(AZ_TEX_MATCAP,0).x);
        kkRamp=textureLod(AZ_TEX_MATCAP,vec2((clamp(kkLobe,0,1)*(rampWidth-1)+.5)/rampWidth,.5),0).r;
    }
    kkSpecular*=kkRamp;
    secondaryKkSpecular*=kkRamp;
    kkSpecular=min(kkSpecular,vec3(material.matcapColor.w));
    if (qaEffectMode == 3 && qaEffectDisabled) {
        kkSpecular = vec3(0.0);
        secondaryKkSpecular = vec3(0.0);
    }
    float outputAlpha = material.alphaMode == 2 ? baseColor.a : 1.0;
#if !defined(AZURE_OPAQUE_SNAPSHOT)
    if (browOverlay) {
        // M_Common_Brow uses a constant Opaccity parameter; the face texture
        // alpha is not the brow mask and is intentionally ignored here.
        float sceneDepth=texture(overlaySceneDepth,gl_FragCoord.xy/camera.clusterDepth.zw).r;
        float overlayDepth=dot(worldPosition-camera.cameraPosition.xyz,camera.cameraForward.xyz);
        float fade=clamp((sceneDepth-overlayDepth)/max(material.featureParameters.z,.0001),0.0,1.0);
        outputAlpha = material.featureParameters.y * fade;
    }
#endif
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
    vec2 matcapTexel = 5.0 / vec2(textureSize(AZ_TEX_MATCAP, 0));
    float matcapMask =
        texture(AZ_TEX_MATCAP, matcapUv).r * 0.20
        + texture(AZ_TEX_MATCAP, matcapUv + vec2(matcapTexel.x, 0.0)).r * 0.125
        + texture(AZ_TEX_MATCAP, matcapUv - vec2(matcapTexel.x, 0.0)).r * 0.125
        + texture(AZ_TEX_MATCAP, matcapUv + vec2(0.0, matcapTexel.y)).r * 0.125
        + texture(AZ_TEX_MATCAP, matcapUv - vec2(0.0, matcapTexel.y)).r * 0.125
        + texture(AZ_TEX_MATCAP, matcapUv + matcapTexel).r * 0.075
        + texture(AZ_TEX_MATCAP, matcapUv - matcapTexel).r * 0.075
        + texture(AZ_TEX_MATCAP,
            matcapUv + vec2(matcapTexel.x, -matcapTexel.y)).r * 0.075
        + texture(AZ_TEX_MATCAP,
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
    } else if (qaIsolationMode == 16) {
        qaColor = vec3(eyebrowRegion > 0.5 ? 1.0 : 0.0);
        if (overlayMaterial) outputAlpha = eyebrowRegion > 0.5 ? 1.0 : 0.0;
    }
#if !defined(AZURE_OPAQUE_SNAPSHOT)
    if (materialFeatureEnabled(256U) > 0.5) {
        vec2 uv=gl_FragCoord.xy/camera.clusterDepth.zw;
        float sceneDepth=texture(overlaySceneDepth,uv).r;
        outputAlpha=clamp(1.0-sceneDepth/max(material.featureParameters.z,.001),0.0,1.0)*material.featureParameters.y;
        if (overlayDisabled) outputAlpha=0.0;
        vec3 scene=texture(opaqueSceneColor,uv).rgb;
        vec3 hsv=rgbToHsv(scene)*vec3(material.featureParameters.x,material.styleParameters.w,material.featureParameters.w);
        qaColor=hsvToRgb(hsv)*material.styleParameters.rgb;
    }
#endif
    outputColor = vec4(qaColor, outputAlpha);
    if (material.materialPadding == AZURE_SELECTED_PADDING) {
        outputColor.rgb = mix(outputColor.rgb, vec3(0.96, 0.62, 0.10), 0.42);
        outputColor.a = 1.0;
    }
    float innerOutlineParticipation =
        (1.0 - platformMask)
        * (overlayMaterial ? 0.0 : 1.0)
        * material.featureParameters.x
        * mix(
            1.0,
            0.68,
            max(hairActive, clamp(material.matcapColor.a, 0.0, 1.0)));
    vec3 innerOutlineNormal = geometricNormal;
#if defined(AZURE_OPAQUE_SNAPSHOT)
    outputNormal=vec4(dot(worldPosition-camera.cameraPosition.xyz,camera.cameraForward.xyz),0,0,1);
#else
    outputNormal = vec4(
        innerOutlineNormal * 0.5 + 0.5,
        innerOutlineParticipation);
#endif
}
