#version 450
#extension GL_GOOGLE_include_directive : require
#include "node_opacity.glsl"

#if defined(AZURE_BINDLESS)
#extension GL_EXT_nonuniform_qualifier : require
#endif
#define AZURE_SEPARATE_SHADOW_SAMPLER 1
#include "shadow_compare.glsl"
#include "surface_response.glsl"
#include "character_diffuse.glsl"
#include "environment_specular.glsl"
#include "light_attenuation.glsl"
#include "character_surface.glsl"

#if !defined(AZURE_OPAQUE_SNAPSHOT)
layout(binding = 18) uniform sampler2D overlaySceneDepth;
layout(binding = 19) uniform sampler2D opaqueSceneColor;
#include "screen_space_reflection.glsl"
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
    vec4 skyRight,skyUp,skyForward,skyColor,skyParameters;
    vec4 probeCenter,probeHalfExtents,probeCapture,probeParameters;
    vec4 irradianceOriginSpacing,irradianceDimensionsEnabled,irradianceParameters;
    vec4 cameraRight,cameraUp;
    mat4 screenReflectionWorldToClip;
    vec4 screenReflectionTrace,screenReflectionQuality;
    vec4 nprDirectionBias,nprGainFloor,nprAmbientColor,nprComposition,nprMainRadiance,nprLightingParameters;
} camera;
layout(binding=23) uniform samplerCube reflectionProbeTexture;
layout(std430,binding=24) readonly buffer IrradianceCells { vec4 irradianceCells[]; };

#if defined(AZURE_BINDLESS)
// Indexed slots 0..1 are environment and toon ramp; material blocks follow.
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
layout(binding = 11) uniform sampler2D toonRampTexture;
layout(binding = 12) uniform sampler2D faceSdfTexture;
layout(binding = 25) uniform sampler2D shadowColorLutTexture;
#endif

layout(binding=9) uniform texture2D mainShadowDepth;
layout(binding=28) uniform sampler sharedDepthSampler;
#define AZ_TEX_SHADOW sampler2D(mainShadowDepth,sharedDepthSampler)

struct ClusterLight {
    vec4 positionRadius;
    vec4 colorIntensity;
    vec4 directionOuter;
    vec4 coneType;
    vec4 boxRows[3];
    vec4 boxParameters;
    vec4 characterResponse;
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
layout(binding=20,std430) readonly buffer ShadowBounds { vec2 values[]; } shadowBounds;

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
    layout(offset = 140) float nodeOpacity;
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
#define AZ_TEX_SHADOW_COLOR_LUT textureArray[nonuniformEXT(material.textureBase + 8u)]
#define AZ_TEX_ENVIRONMENT textureArray[0u]
#define AZ_TEX_TOON_RAMP textureArray[1u]
#else
#define AZ_TEX_BASE_COLOR baseColorTexture
#define AZ_TEX_NORMAL normalTexture
#define AZ_TEX_METALLIC_ROUGHNESS metallicRoughnessTexture
#define AZ_TEX_SPECULAR_EMISSIVE specularEmissiveTexture
#define AZ_TEX_STYLE_MASK styleMaskTexture
#define AZ_TEX_MATCAP matcapTexture
#define AZ_TEX_HAIR_DATA hairDataTexture
#define AZ_TEX_FACE_SDF faceSdfTexture
#define AZ_TEX_SHADOW_COLOR_LUT shadowColorLutTexture
#define AZ_TEX_ENVIRONMENT environmentTexture
#define AZ_TEX_TOON_RAMP toonRampTexture
#endif

// Low bit selects the primitive; upper bits address the immutable material table.
#define AZURE_SELECTED_PADDING 1U

layout(location = 0) in vec3 worldNormal;
layout(location = 1) in vec4 worldTangent;
layout(location = 2) in vec2 textureCoordinate;
layout(location = 3) in vec3 worldPosition;
layout(location = 4) in vec4 shadowPositions[4];
layout(location = 8) in float eyebrowRegion;
layout(location = 9) flat in vec4 instanceFaceDirection;
layout(location = 10) flat in mat4 instanceFaceFrame;
layout(location = 14) in vec3 opacityCoordinate;
#if defined(AZURE_OBJECT_SHADOW)
layout(location=15) flat in uint instanceObjectShadow;
layout(binding=29) uniform texture2D objectShadowDepth;
#include "object_shadow.glsl"
vec2 objectShadowRecordPair(uint index) { return shadowBounds.values[index]; }
float materialObjectVisibility(vec3 surfaceNormal){
    ivec2 extent=textureSize(AZ_TEX_SHADOW,0)/8;
    ObjectShadowRecord record;
    if(!objectShadowLoadRecord(uint(extent.x*extent.y),instanceObjectShadow,record))return 1.0;
    vec3 receiverPosition=objectShadowReceiverPosition(worldPosition,record.receiverSampling.xy,
        camera.screenReflectionWorldToClip,camera.cameraRight.xyz,camera.cameraUp.xyz,camera.clusterDepth.zw);
    vec3 receiverNormal=record.receiverSampling.z>.5?surfaceNormal:normalize(worldNormal);
    return objectShadowVisibility(objectShadowDepth,sharedDepthSampler,record,receiverPosition,receiverNormal,
        ivec2(gl_FragCoord.x,camera.clusterDepth.w-gl_FragCoord.y));
}
#else
float materialObjectVisibility(vec3 surfaceNormal){return 1.0;}
#endif
layout(location = 0) out vec4 outputColor;
layout(location = 1) out vec4 outputNormal;

struct SceneLightSample { vec3 incoming;vec3 direction;float distanceSquared;float attenuation; };
SceneLightSample sampleSceneLight(ClusterLight light,vec3 position,bool projectedDistance) {
    vec3 toLight=light.positionRadius.xyz-position;
    if(projectedDistance)toLight=light.directionOuter.xyz*dot(toLight,light.directionOuter.xyz);
    float distanceSquared=dot(toLight,toLight);
    float distanceToLight=sqrt(distanceSquared);
    float lightRadius=max(light.positionRadius.w,.001);
    float attenuation=localLightDistanceWeight(distanceSquared,lightRadius,light.coneType.z,light.coneType.w);
    vec3 pointDirection=toLight/max(distanceToLight,.001);
    if(light.coneType.y>1.5){pointDirection=-light.directionOuter.xyz;attenuation=1.0;}
    else if(light.coneType.y>.5) {
        float cone=clamp((dot(-pointDirection,light.directionOuter.xyz)-light.directionOuter.w)
            /max(light.coneType.x-light.directionOuter.w,.000001),0.0,1.0);
        if(light.coneType.x==light.directionOuter.w)cone=step(light.directionOuter.w,dot(-pointDirection,light.directionOuter.xyz));
        attenuation*=cone*cone;
    }
    attenuation*=lightInfluenceBoxWeight(position,light.boxRows[0],light.boxRows[1],light.boxRows[2],light.boxParameters);
    return SceneLightSample(toLight,pointDirection,distanceSquared,attenuation);
}

struct ProjectedHairProfile {
    vec4 shiftsIntensityRange;
    vec4 edgeNormalDirection;
    vec4 secondaryColor;
    vec4 objectX,objectY,objectZ;
};
struct PackedMaterialSurfaceProfile { vec4 rows[12]; };
layout(std430,binding=27) readonly buffer MaterialSurfaceProfiles {
    PackedMaterialSurfaceProfile profiles[];
} materialSurfaceProfiles;
ProjectedHairProfile loadProjectedHairSurfaceProfile(uint index) {
    PackedMaterialSurfaceProfile p=materialSurfaceProfiles.profiles[index];
    return ProjectedHairProfile(p.rows[0],p.rows[1],p.rows[2],p.rows[3],p.rows[4],p.rows[5]);
}
struct EyeSurfaceProfile {
    vec4 shape,highlight,scattering,matcap,directWeights,uvScaleOffset;
};
EyeSurfaceProfile loadEyeSurfaceProfile(uint index) {
    PackedMaterialSurfaceProfile p=materialSurfaceProfiles.profiles[index];
    return EyeSurfaceProfile(p.rows[0],p.rows[1],p.rows[2],p.rows[3],p.rows[4],p.rows[5]);
}

// Packed normal z is reconstructed before the authored xy scale.
vec3 projectedHairNormal(vec2 encoded,float scale,float tangentSign) {
    return characterProjectedHairNormal(encoded,scale,worldNormal,vec4(worldTangent.xyz,worldTangent.w*tangentSign));
}

CharacterProjectedHairEnergy projectedHairResponse(vec3 view,vec3 light,bool directional,bool evaluateLine,out vec2 line) {
    ProjectedHairProfile p=loadProjectedHairSurfaceProfile(material.materialPadding>>1U);
    vec4 parameters=texture(AZ_TEX_FACE_SDF,textureCoordinate);
    vec4 packedNormals=texture(AZ_TEX_HAIR_DATA,textureCoordinate);
    vec4 tangent=vec4(worldTangent.xyz,worldTangent.w*p.objectY.w);
    vec3 normal=projectedHairNormal(packedNormals.ba,p.edgeNormalDirection.y,p.objectY.w);
    mat3 frame=mat3(instanceFaceFrame);
    vec3 objectX=normalize(frame*p.objectX.xyz);
    vec3 objectY=normalize(frame*p.objectY.xyz);
    vec3 objectZ=normalize(frame*p.objectZ.xyz);
    // Scene directional lights use the material's explicit virtual adapter.
    vec4 adapter=materialSurfaceProfiles.profiles[material.materialPadding>>1U].rows[6];
    vec3 halfVector=adapter.x>.5&&directional?characterProjectedVirtualHalf(view,light,objectX,objectY,objectZ,adapter.y)
        :characterProjectedSafeNormalize(view+light,normal);
    CharacterProjectedHairCoordinates coordinates=characterProjectedHairCoordinates(
        halfVector,normal,view,parameters,tangent,
        objectX,objectY,objectZ,p.shiftsIntensityRange.x,p.shiftsIntensityRange.y,
        p.shiftsIntensityRange.w,p.edgeNormalDirection.x,p.edgeNormalDirection.z);
    vec3 ramp=textureLod(AZ_TEX_MATCAP,coordinates.uv,0.0).rgb;
    CharacterProjectedHairEnergy energy=characterProjectedHairEnergy(coordinates,ramp,parameters,p.shiftsIntensityRange.z,p.secondaryColor.rgb);
    line=vec2(1);
    PackedMaterialSurfaceProfile packed=materialSurfaceProfiles.profiles[material.materialPadding>>1U];
    if(evaluateLine&&packed.rows[8].z>.5){
        vec4 uvTransform=packed.rows[9];
        float sampleValue=texture(AZ_TEX_SHADOW_COLOR_LUT,textureCoordinate*uvTransform.xy+uvTransform.zw).r;
        vec3 strand=characterProjectedHairStrand(normal,tangent,parameters.r,objectX,objectY,objectZ,p.edgeNormalDirection.z);
        line=characterProjectedHairLine(strand,normal,halfVector,textureCoordinate.x,sampleValue,parameters.g,energy.q,packed.rows[7],packed.rows[8].xy);
    }
    if(adapter.x>.5){energy.primary*=adapter.z;energy.secondary*=adapter.z;}
    return energy;
}

CharacterProjectedHairEnergy projectedHairDirect(vec3 view,vec3 light,bool directional){
    vec2 line;return projectedHairResponse(view,light,directional,false,line);
}

CharacterDiffuseResponse projectedHairDiffuseResponse(vec3 base,vec3 normal,vec3 light,float ao,
    float visibility,out float viewAlpha) {
    PackedMaterialSurfaceProfile profile=materialSurfaceProfiles.profiles[material.materialPadding>>1U];
    vec4 ramp=textureLod(AZ_TEX_STYLE_MASK,vec2(dot(normal,light)*.5+.5,.5),0.0);
    viewAlpha=textureLod(AZ_TEX_STYLE_MASK,vec2(dot(normal,-camera.cameraForward.xyz)*.5+.5,.5),0.0).a;
    float characterVisibility=texture(AZ_TEX_FACE_SDF,textureCoordinate).b;
    float litMask=min(min(characterVisibility,ao),ramp.a),aoMask=characterVisibility*ao;
    vec3 shadow=characterDiffuseSaturation(base*profile.rows[10].w,profile.rows[11].w);
    CharacterDiffuseResponse response=characterDiffuseWeightedResponse(base,shadow,0.0,profile.rows[5].w,ramp,
        litMask,aoMask,clamp(viewAlpha*aoMask+ramp.a,0.0,1.0),visibility);
    response.color*=profile.rows[3].w/.96;
    return response;
}

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

vec3 materialDiffuseRamp(float coordinate) {
    if(materialFeatureEnabled(8192U)>.5) {
        float width=float(textureSize(AZ_TEX_STYLE_MASK,0).x);
        return textureLod(AZ_TEX_STYLE_MASK,vec2((clamp(coordinate,0,1)*(width-1)+.5)/width,.5),0).rgb;
    }
    return sampleToonRamp(coordinate);
}

vec4 materialDiffuseRampRgba(float coordinate) {
    if(materialFeatureEnabled(8192U)>.5) {
        float width=float(textureSize(AZ_TEX_STYLE_MASK,0).x);
        return textureLod(AZ_TEX_STYLE_MASK,vec2((clamp(coordinate,0,1)*(width-1)+.5)/width,.5),0);
    }
    return vec4(materialDiffuseRamp(coordinate),1);
}

vec3 materialNprLitAlbedo(vec3 base,vec3 normal,vec3 view) {
    vec4 tint=materialSurfaceProfiles.profiles[material.materialPadding>>1U].rows[2];
    return characterDiffuseViewTint(base,normal,view,tint);
}
CharacterDiffuseResponse materialNprDiffuseResponse(vec4 base,float metallic,vec4 ramp,float visibility,float mainShadow,vec3 normal,vec3 view) {
    PackedMaterialSurfaceProfile profile=materialSurfaceProfiles.profiles[material.materialPadding>>1U];
    vec4 parameters=profile.rows[0];
    vec2 mask=parameters.y>1.5?vec2(1,0):parameters.y>.5?textureLod(AZ_TEX_STYLE_MASK,textureCoordinate,0).gb:vec2(0);
    vec3 shadow=sampleShadowColorLut(AZ_TEX_SHADOW_COLOR_LUT,base.rgb);
    float characterVisibility=profile.rows[3].x>1.5?materialObjectVisibility(normal):profile.rows[3].x>.5?mainShadow:visibility;
    return characterDiffuseResponse(materialNprLitAlbedo(base.rgb,normal,view),shadow,metallic,parameters.x,ramp,
        parameters.z>.5?base.a:1.0,mask,dot(normal,view),characterVisibility,visibility);
}
vec3 materialNprDiffuse(vec4 base,float metallic,vec4 ramp,float visibility,float mainShadow,vec3 normal,vec3 view){
    return materialNprDiffuseResponse(base,metallic,ramp,visibility,mainShadow,normal,view).color;
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
    vec4 depths=shadowTexelQuad(mainShadowDepth,sharedDepthSampler,base,lo,hi);
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
    vec2 centre=uv*vec2(extent)-.5;
    ivec2 base=ivec2(floor(centre));vec2 fraction=fract(centre);
    vec2 depthPerTexel=depthGradient/vec2(extent);
    // A full square bounds every possible disk texel, including atlas edges.
    int reach=int(ceil(maximumRadius+.5));
    ivec2 tileLow=clamp(base-ivec2(reach),lo,hi)/8,tileHigh=clamp(base+ivec2(reach+1),lo,hi)/8;
    float minimumDepth=1,maximumDepth=0;
    for(int ty=tileLow.y;ty<=tileHigh.y;++ty)for(int tx=tileLow.x;tx<=tileHigh.x;++tx){
        vec2 range=shadowBounds.values[ty*(extent.x/8)+tx];minimumDepth=min(minimumDepth,range.x);maximumDepth=max(maximumDepth,range.y);
    }
    float planeMargin=(float(reach)+2.0)*(abs(depthPerTexel.x)+abs(depthPerTexel.y));
    if(receiver+planeMargin<=minimumDepth)return 1.0;
    if(receiver-planeMargin>maximumDepth)return 0.0;
    bool interior=all(greaterThanEqual(base-ivec2(17),lo))&&all(lessThanEqual(base+ivec2(18),hi));
    float sum=0,blockers=0;
    // The 25 bilinear footprints integrate exactly to a weighted 6x6 grid.
    // Each texel is fetched once; only the outer rows have fractional mass.
    for(int y=-2;y<=2;y+=2)for(int x=-2;x<=2;x+=2){
        vec4 weights=shadowBlockerGridWeights(ivec2(x,y),fraction);
        vec4 plane=interior?dot(vec2(x,y)-fraction,depthPerTexel)+vec4(0,depthPerTexel.x,depthPerTexel.y,depthPerTexel.x+depthPerTexel.y):shadowPlaneQuad(base+ivec2(x,y),lo,hi,uv,vec2(extent),depthGradient);
        vec4 depths=shadowTexelQuad(mainShadowDepth,sharedDepthSampler,base+ivec2(x,y),lo,hi)-plane;
        vec4 mass=(vec4(1)-step(vec4(receiver),depths))*weights;
        sum+=dot(depths,mass);blockers+=dot(vec4(1),mass);
    }
    float separation=blockers>0?max(receiver-sum/blockers,0)*depthRange:0;
    float contactRadius=shadowContactRadius(float(tile.x),maximumRadius);
    float radius=clamp(contactRadius+separation*.035/max(worldTexel,1e-6),contactRadius,maximumRadius);
    float visible=0,totalWeight=0;
    int bound=int(ceil(radius+.5));
    for(int y=-bound;y<=bound;y+=2)for(int x=-bound;x<=bound;x+=2){
        vec4 weights=shadowDiskQuadWeights(vec2(x,y)-fraction,radius);
        vec4 planeDepths=vec4(receiver)+(interior?dot(vec2(x,y)-fraction,depthPerTexel)+vec4(0,depthPerTexel.x,depthPerTexel.y,depthPerTexel.x+depthPerTexel.y):shadowPlaneQuad(base+ivec2(x,y),lo,hi,uv,vec2(extent),depthGradient));
        float weight=dot(weights,vec4(1));
        if(weight<=0)continue;
        vec4 depths=shadowTexelQuad(mainShadowDepth,sharedDepthSampler,base+ivec2(x,y),lo,hi);
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
        if(blend>0.0)result=mix(result,sampleCascadeShadow(c+1,normalDotLight,gradients[c+1],normalCurvature),blend);}
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
vec4 evaluateIrradianceVolume(vec3 position,vec3 normal) {
    if(camera.irradianceDimensionsEnabled.w<.5)return vec4(0);
    vec3 q=(position-camera.irradianceOriginSpacing.xyz)/camera.irradianceOriginSpacing.w;
    ivec3 dims=ivec3(camera.irradianceDimensionsEnabled.xyz);
    if(any(lessThan(q,vec3(0)))||any(greaterThan(q,vec3(dims-ivec3(1)))))return vec4(0);
    ivec3 lo=ivec3(floor(q)),hi=min(lo+ivec3(1),dims-ivec3(1));
    vec3 f=q-vec3(lo);
    vec4 red=vec4(0),green=vec4(0),blue=vec4(0);
    float weight=0;
    for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x){
        ivec3 p=ivec3(x!=0?hi.x:lo.x,y!=0?hi.y:lo.y,z!=0?hi.z:lo.z);
        int base=4*(p.x+dims.x*(p.y+dims.y*p.z));
        float w=(x!=0?f.x:1-f.x)*(y!=0?f.y:1-f.y)*(z!=0?f.z:1-f.z)*irradianceCells[base+3].x;
        red+=irradianceCells[base]*w;green+=irradianceCells[base+1]*w;blue+=irradianceCells[base+2]*w;
        weight+=w;
    }
    vec4 basis=vec4(normal,1);
    return vec4(max(vec3(dot(red,basis),dot(green,basis),dot(blue,basis)),vec3(0))*camera.irradianceParameters.x,
        clamp(weight,0,1));
}

float dualFaceRampCoordinate(vec3 direction, vec3 normal, out vec4 distanceSample) {
    mat3 headFrame=mat3(instanceFaceFrame);
    vec3 localLight=transpose(headFrame)*direction;
    vec3 localCamera=transpose(headFrame)*(-camera.cameraForward.xyz);
    distanceSample=textureLod(AZ_TEX_FACE_SDF,faceSdfSampleUv(textureCoordinate,localLight.x),0.0);
    float geometryMask=textureLod(AZ_TEX_STYLE_MASK,textureCoordinate,0.0).g;
    return faceSdfRampCoordinate(distanceSample,geometryMask,localLight,localCamera,
        dot(normal,direction),material.hairParameters.xyz);
}

void main() {
    if (material.alphaMode != 2 && !azureNodeOpacityCovered(opacityCoordinate,material.nodeOpacity)) discard;
    bool eyeRadial=material.materialClass==6U&&materialFeatureEnabled(131072U)>.5;
    EyeSurfaceProfile eyeProfile;
    CharacterEyeCoordinates eyeCoordinates;
    vec4 baseColor = texture(AZ_TEX_BASE_COLOR, textureCoordinate);
    if(eyeRadial) {
        eyeProfile=loadEyeSurfaceProfile(material.materialPadding>>1U);
        vec2 originalUv=textureCoordinate*eyeProfile.uvScaleOffset.xy+eyeProfile.uvScaleOffset.zw;
        mat3 worldToView=transpose(mat3(camera.cameraRight.xyz,camera.cameraUp.xyz,-camera.cameraForward.xyz));
        eyeCoordinates=characterEyeCoordinates(originalUv,camera.cameraPosition.xyz-worldPosition,
            worldNormal,worldTangent,worldToView,eyeProfile.shape.x,eyeProfile.shape.y);
        baseColor=texture(AZ_TEX_BASE_COLOR,eyeCoordinates.baseUv);
    }
    bool eyeNpr=eyeRadial&&camera.clusterLighting.x>.5&&camera.nprLightingParameters.x>.5
        &&materialSurfaceProfiles.profiles[material.materialPadding>>1U].rows[6].w>.5;
    bool multiplyComposite = materialFeatureEnabled(32768U) > 0.5;
    bool overlayMaterial = (material.materialClass == 7U
        && materialFeatureEnabled(16U) > 0.5) || multiplyComposite;
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
    bool projectedHair=hairActive>0.0&&materialFeatureEnabled(65536U)>.5;
    bool strandDiffuse=projectedHair
        &&loadProjectedHairSurfaceProfile(material.materialPadding>>1U).secondaryColor.w>.5;
    bool projectedLighting=strandDiffuse&&camera.clusterLighting.x>.5&&camera.nprLightingParameters.x>.5
        &&materialSurfaceProfiles.profiles[material.materialPadding>>1U].rows[6].w>.5;
    float projectedDiffuseScale=projectedHair
        ?loadProjectedHairSurfaceProfile(material.materialPadding>>1U).objectX.w:1.0;
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
    if(projectedHair) {
        ProjectedHairProfile p=loadProjectedHairSurfaceProfile(material.materialPadding>>1U);
        shadedNormal=projectedHairNormal(hairData.rg,p.edgeNormalDirection.w,p.objectY.w)*(gl_FrontFacing?1.0:-1.0);
    }

    if(eyeRadial)shadedNormal=eyeCoordinates.lightingNormal;

    vec4 packedMaterial = texture(AZ_TEX_METALLIC_ROUGHNESS, textureCoordinate);
    float physicalAo=mix(1.0,packedMaterial.r,materialFeatureEnabled(128U));
    vec4 specularEmissive = texture(AZ_TEX_SPECULAR_EMISSIVE,
        textureCoordinate);
    float styleStrength = camera.renderingParameters.y;
    float diffuseBandThreshold = camera.renderingParameters.z;
    bool sceneCharacter=camera.clusterLighting.x>.5
        && sceneCharacterSurface(material.materialClass,material.materialProfileVersion);
    if(sceneCharacter && diffuseBandThreshold<0.0)diffuseBandThreshold=.40;
    float bandEnabled = step(0.0, diffuseBandThreshold);
    float styleMask = smoothstep(
        0.08,
        0.62,
        texture(AZ_TEX_STYLE_MASK, textureCoordinate).r)
        * styleStrength;
    if(materialFeatureEnabled(2048U)>0.5 || materialFeatureEnabled(8192U)>.5)styleMask=0.0;
    float normalVariance=max(1.0-normalSample.a,0.0);
    vec3 dx=dFdx(shadedNormal),dy=dFdy(shadedNormal);
    normalVariance+=min(dot(dx,dx)+dot(dy,dy),.25);
    float roughness = clamp(sqrt(packedMaterial.g*packedMaterial.g+normalVariance*.25), 0.08, 1.0);
    float metallic = clamp(packedMaterial.b, 0.0, 1.0);
    float specularLevel = clamp(specularEmissive.a, 0.0, 1.0);
    if(material.materialClass==0U||material.materialClass==4U||material.materialClass==5U) {
        vec2 inputProfile=materialSurfaceProfiles.profiles[material.materialPadding>>1U].rows[5].zw;
        specularLevel=clamp((inputProfile.x>.5?packedMaterial.a:specularEmissive.a)*inputProfile.y,0.0,1.0);
    }
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
    if(strandDiffuse)metallic=0.0;
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
#if defined(AZURE_OPAQUE_SNAPSHOT)
    // SSR radiance sources retain complete lighting under specular diagnostics.
    if(camera.screenReflectionTrace.x>.5){qaEffectMode=0;qaEffectDisabled=false;}
#endif
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
    if(strandDiffuse)f0=vec3(0);
    float normalDotView = max(dot(shadedNormal, viewDirection), 0.0);
    bool microfacetSurface = material.materialClass == 0U || material.materialClass == 4U
        || material.materialClass == 5U || material.materialClass == 9U;
    vec3 fresnel =
        f0 + (1.0 - f0) * pow(1.0 - normalDotView, 5.0);
    if(strandDiffuse)fresnel=vec3(0);
    bool fittedEnvironment=camera.clusterLighting.x>.5&&camera.clusterLighting.w>.5
        &&material.materialClass==0U;
    vec3 environmentResponse=fittedEnvironment
        ?ggxMultiscatterEnvironment(mix(vec3(.04),baseColor.rgb,metallic),roughness,normalDotView)
        :fresnel;
    vec3 diffuseColor = baseColor.rgb * (1.0 - metallic) * projectedDiffuseScale;
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
    vec4 faceSdfSample = vec4(0.0);
    float faceSdfEligible = material.materialClass == 2U
        ? materialFeatureEnabled(4U)
        : 0.0;
    float faceSdfEnabled = camera.faceSdfParameters.x
        * instanceFaceDirection.w
        * faceSdfEligible;
    if (qaEffectMode == 8 && qaEffectDisabled) {
        faceSdfEnabled = 0.0;
    }
    if (faceSdfEnabled > 0.0) {
        faceSdfSample = texture(AZ_TEX_FACE_SDF, textureCoordinate);
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
    bool dualFaceSdf=faceSdfEnabled>0.0 && materialFeatureEnabled(2048U)>0.5;
    bool signedMainRamp=false;
    if(eyeNpr){signedMainRamp=true;rampCoordinate=characterMainRampCoordinate(shadedNormal,lightDirection,0);}
    if(materialFeatureEnabled(524288U)>.5&&!dualFaceSdf) {
        vec4 rampInput=materialSurfaceProfiles.profiles[material.materialPadding>>1U].rows[1];
        signedMainRamp=rampInput.y>.5;
        if(signedMainRamp)rampCoordinate=characterMainRampCoordinate(shadedNormal,lightDirection,rampInput.z);
    }
    vec3 faceLightingNormal=shadedNormal;
    if(dualFaceSdf) {
        toonWeight=qaEffectMode==1 && qaEffectDisabled?0.0:1.0;
        rampCoordinate=dualFaceRampCoordinate(lightDirection,shadedNormal,faceSdfSample);
        mat3 headFrame=mat3(instanceFaceFrame);
        float localLightX=(transpose(headFrame)*lightDirection).x;
        float geometryMix=textureLod(AZ_TEX_STYLE_MASK,textureCoordinate,0.0).g;
        faceLightingNormal=faceSdfWorldNormal(faceSdfSample,localLightX,
            headFrame,shadedNormal,geometryMix);
        faceIllumination=rampCoordinate;
        faceSdfWeight=0.0;
    }
    vec4 classRampRgba=dualFaceSdf?textureLod(AZ_TEX_MATCAP,vec2(rampCoordinate,.5),0.0)
        :signedMainRamp?textureLod(AZ_TEX_STYLE_MASK,vec2(rampCoordinate,.5),0.0):materialDiffuseRampRgba(rampCoordinate);
    vec3 classRamp = classRampRgba.rgb;
    bool nprDiffuse=camera.clusterLighting.x>.5&&materialFeatureEnabled(524288U)>.5
        &&!(qaEffectMode==1&&qaEffectDisabled);
    vec3 indirectDiffuseNormal=shadedNormal;
    if(eyeNpr)indirectDiffuseNormal=worldHorizontalDiffuseNormal(shadedNormal);
    if(nprDiffuse) {
        float normalMode=materialSurfaceProfiles.profiles[material.materialPadding>>1U].rows[0].w;
        if(normalMode>1.5) {
            if(dualFaceSdf)indirectDiffuseNormal=faceFrameRadialHorizontalDiffuseNormal(
                worldPosition,instanceFaceFrame[3].xyz,geometricNormal,
                textureLod(AZ_TEX_STYLE_MASK,textureCoordinate,0).g);
        } else if(normalMode>.5)indirectDiffuseNormal=worldHorizontalDiffuseNormal(shadedNormal);
    }
    vec3 nprAmbientResponse=nprDiffuse
        ?materialNprDiffuse(baseColor,metallic,classRampRgba,shadowVisibility,shadowVisibility,shadedNormal,viewDirection):diffuseColor;
    float characterSceneVisibility=camera.nprLightingParameters.x>.5
        ?mix(shadowVisibility,1.0,camera.nprGainFloor.z):shadowVisibility;
    vec3 localCharacterDiffuse=diffuseColor;
    bool localCharacterResponseReady=false;
    float hairViewAlpha=1.0;
    CharacterDiffuseResponse hairDiffuseResponse;
    if(projectedLighting) {
        hairDiffuseResponse=projectedHairDiffuseResponse(baseColor.rgb,shadedNormal,lightDirection,physicalAo,
            characterSceneVisibility,hairViewAlpha);
        localCharacterDiffuse=hairDiffuseResponse.color;
        localCharacterResponseReady=true;
    }
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
    float diffuseScale = strandDiffuse ? 1.0 : mix(0.62, 0.68, toonWeight);
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
    vec3 sceneMainDiffuse = vec3(0.0);
    vec3 clusteredSpecular = vec3(0.0);
    vec3 sceneHairPrimary=vec3(0),sceneHairSecondary=vec3(0),sceneRim=vec3(0);
    vec3 mainHairPrimary=vec3(0),mainHairSecondary=vec3(0);
    vec3 eyeHostDiffuse=vec3(0),eyeHostMatcap=vec3(0);
    vec4 eyeMatcap=vec4(0);
    float eyeViewRampAlpha=0;
    vec3 eyeDiffuseMultiplier=vec3(0);
    float eyeAlphaMultiplier=1;
    CharacterDiffuseResponse eyeDiffuseResponse;
    if(eyeRadial) {
        eyeMatcap=eyeNpr?texture(AZ_TEX_MATCAP,eyeCoordinates.matcapUv):textureLod(AZ_TEX_MATCAP,eyeCoordinates.matcapUv,0.0);
        float viewCoordinate=dot(shadedNormal,-camera.cameraForward.xyz)*.5+.5;
        eyeViewRampAlpha=textureLod(AZ_TEX_STYLE_MASK,vec2(viewCoordinate,.5),0.0).a;
        eyeDiffuseMultiplier=characterEyeDiffuseMultiplier(baseColor,eyeCoordinates.whiteMask,
            eyeProfile.highlight.rgb,eyeProfile.scattering.rgb);
        eyeAlphaMultiplier=characterEyeAlphaMultiplier(baseColor.a,eyeProfile.shape.z);
        if(eyeNpr) {
            eyeDiffuseResponse=characterEyeNprResponse(baseColor,eyeCoordinates.whiteMask,
                eyeProfile.highlight.rgb,eyeProfile.scattering.rgb,metallic,
                materialSurfaceProfiles.profiles[material.materialPadding>>1U].rows[6].x,
                classRampRgba,eyeViewRampAlpha,characterSceneVisibility);
            localCharacterDiffuse=eyeDiffuseResponse.color;localCharacterResponseReady=true;
        }
    }
    bool characterLightComposition=(nprDiffuse||projectedLighting||eyeNpr)&&camera.nprLightingParameters.x>.5;
    vec4 characterBlend=vec4(0,0,0,1);
    uint sceneLightCount = uint(max(camera.clusterGrid.w, 0.0));
    for (uint lightOffset = 0U; lightOffset < lightRange.y; ++lightOffset) {
        uint lightIndex = clusterIndexData.indices[lightRange.x + lightOffset];
        if (lightIndex >= sceneLightCount) {
            continue;
        }
        ClusterLight light = sceneLightData.lights[lightIndex];
        uint receiverBit=sceneCharacter||nprDiffuse?2U:1U;
        if((uint(light.boxParameters.w)&receiverBit)==0U)continue;
        bool colorBlend=light.characterResponse.x==2.0||light.characterResponse.x==3.0;
        bool materialRim=light.characterResponse.x==6.0;
        bool screenRim=light.characterResponse.x==4.0||materialRim,specularOnly=light.characterResponse.x==5.0;
        if(colorBlend) {
            if(characterLightComposition) {
                SceneLightSample blendSample=sampleSceneLight(light,worldPosition,light.characterResponse.x==3.0);
                if(blendSample.attenuation>.0001) {
                    float weight=characterColorBlendWeight(blendSample.attenuation,dot(geometricNormal,blendSample.direction),light.characterResponse.yz);
                    characterBlend.rgb=mix(characterBlend.rgb,light.colorIntensity.rgb*light.colorIntensity.w,weight);
                    characterBlend.a*=1.0-weight;
                }
            }
            continue;
        }
        if((screenRim||specularOnly)&&(!characterLightComposition||material.alphaMode==2))continue;
        SceneLightSample sampled=sampleSceneLight(light,worldPosition,false);
        vec3 toLight=sampled.incoming,pointDirection=sampled.direction;
        float distanceSquared=sampled.distanceSquared,attenuation=sampled.attenuation;
        vec3 localLightingNormal=dualFaceSdf && light.coneType.y<=1.5
            ? faceLightingNormal : shadedNormal;
        float pointDiffuse = max(dot(localLightingNormal, pointDirection), 0.0);
        vec3 radiance = light.colorIntensity.rgb
            * light.colorIntensity.w * attenuation;
        if(light.coneType.y>1.5&&light.directionOuter.w>.5)radiance*=shadowVisibility;
        if(screenRim) {
            bool facingRim=materialRim&&nprDiffuse
                &&materialSurfaceProfiles.profiles[material.materialPadding>>1U].rows[1].x>.5;
            vec3 radial=worldPosition-instanceFaceFrame[3].xyz;radial.y=1.0/16384.0;radial=normalize(radial);
            if(facingRim) {
                if(!(qaEffectMode==4&&qaEffectDisabled)&&attenuation>.0001)
                    sceneRim+=radiance*characterFacingRimResponse(materialNprLitAlbedo(baseColor.rgb,shadedNormal,viewDirection),
                        light.characterResponse.z,light.characterResponse.y,shadedNormal,viewDirection,
                        pointDirection,camera.cameraForward.xyz,radial);
            } else {
#if !defined(AZURE_OPAQUE_SNAPSHOT)
            if(!(qaEffectMode==4&&qaEffectDisabled)&&attenuation>.0001) {
                vec2 screenNormal=vec2(dot(shadedNormal,camera.cameraRight.xyz),-dot(shadedNormal,camera.cameraUp.xyz));
                float normalSquared=dot(screenNormal,screenNormal);
                screenNormal=normalSquared>1e-12?screenNormal*inversesqrt(normalSquared):vec2(0);
                vec2 extent=vec2(textureSize(overlaySceneDepth,0));
                vec2 offset=screenNormal*vec2(extent.y/extent.x,1.0)*(.006*light.characterResponse.y);
                vec2 halfPixel=.5/extent;
                vec2 depthUv=clamp(gl_FragCoord.xy/extent+offset,halfPixel,1.0-halfPixel);
                float sceneDepth=textureLod(overlaySceneDepth,depthUv,0.0).r;
                float surfaceDepth=dot(worldPosition-camera.cameraPosition.xyz,camera.cameraForward.xyz);
                sceneRim+=radiance*characterScreenRimResponse(baseColor.rgb,light.characterResponse.z,shadedNormal,
                    pointDirection,camera.cameraForward.xyz,radial,sceneDepth,surfaceDepth);
            }
#endif
            }
            continue;
        }
        bool characterLight=light.characterResponse.x==1.0&&(sceneCharacter||nprDiffuse);
        if(characterLight&&light.characterResponse.w>0.0) {
            LineLightGeometry line=lineLightGeometry(toLight,light.directionOuter.xyz,light.characterResponse.w);
            radiance*=line.factor*(light.coneType.z<1.5?distanceSquared+1.0:1.0);
            pointDirection=line.direction;pointDiffuse=max(dot(localLightingNormal,pointDirection),0.0);
        }
        vec2 localResponse=light.characterResponse.yz;
        if(nprDiffuse)localResponse.x*=materialSurfaceProfiles.profiles[material.materialPadding>>1U].rows[3].y;
        vec3 characterRadiance=characterLight?characterLocalLightRadiance(radiance,
            dot(localLightingNormal,pointDirection),localResponse,characterSceneVisibility):radiance;
        if(!specularOnly) {
        if(eyeRadial&&!eyeNpr&&camera.clusterLighting.x>.5) {
            // This host adapter is independent from the unbound native NPR globals.
            vec4 ramp=textureLod(AZ_TEX_STYLE_MASK,vec2(dot(shadedNormal,pointDirection)*.5+.5,.5),0.0);
            vec3 illumination=radiance*pointDiffuse;
            eyeHostDiffuse+=baseColor.rgb*eyeDiffuseMultiplier*ramp.rgb*illumination*eyeAlphaMultiplier;
            float selection=light.coneType.y>1.5&&light.directionOuter.w>.5?1.0-shadowVisibility:0.0;
            eyeHostMatcap+=characterEyeMatcap(eyeMatcap,eyeProfile.matcap,eyeViewRampAlpha,ramp.a,
                selection,eyeProfile.shape.w,illumination);
        }
        vec3 localDiffuseResponse=vec3(pointDiffuse);
        if(dualFaceSdf) {
            vec4 localDistanceSample;
            float localCoordinate=dualFaceRampCoordinate(pointDirection,localLightingNormal,localDistanceSample);
            localDiffuseResponse=textureLod(AZ_TEX_MATCAP,vec2(localCoordinate,.5),0.0).rgb;
        } else if(sceneCharacter) {
            float coordinate=materialFeatureEnabled(8192U)>.5
                ? dot(shadedNormal,pointDirection)*.5+.5 : pointDiffuse;
            localDiffuseResponse=mix(vec3(pointDiffuse),materialDiffuseRamp(coordinate),toonWeight);
        }
        if(characterLight) {
            if(!localCharacterResponseReady) {
                if(nprDiffuse)localCharacterDiffuse=materialNprDiffuse(baseColor,metallic,classRampRgba,
                    characterSceneVisibility,shadowVisibility,shadedNormal,viewDirection);
                localCharacterResponseReady=true;
            }
            clusteredDiffuse+=localCharacterDiffuse*characterRadiance;
        } else if(eyeNpr) {
            bool composedMain=float(lightIndex)==camera.nprLightingParameters.y;
            if(!composedMain) {
                vec4 ramp=textureLod(AZ_TEX_STYLE_MASK,vec2(dot(shadedNormal,pointDirection)*.5+.5,.5),0);
                CharacterDiffuseResponse response=characterEyeNprResponse(baseColor,eyeCoordinates.whiteMask,
                    eyeProfile.highlight.rgb,eyeProfile.scattering.rgb,metallic,
                    materialSurfaceProfiles.profiles[material.materialPadding>>1U].rows[6].x,
                    ramp,eyeViewRampAlpha,light.coneType.y>1.5?shadowVisibility:1.0);
                clusteredDiffuse+=response.color*radiance*(light.coneType.y>1.5?1.0:pointDiffuse);
            }
        } else if(nprDiffuse) {
            float localCoordinate=dot(shadedNormal,pointDirection)*.5+.5;
            vec4 localRamp;
            if(dualFaceSdf) {
                vec4 distanceValue;
                localCoordinate=dualFaceRampCoordinate(pointDirection,localLightingNormal,distanceValue);
                localRamp=textureLod(AZ_TEX_MATCAP,vec2(localCoordinate,.5),0);
            } else localRamp=materialDiffuseRampRgba(localCoordinate);
            float visibility=light.coneType.y>1.5&&light.directionOuter.w>.5?shadowVisibility:1.0;
            // Source local-light modes are separate from its main-light
            // composition. This explicit host adapter retains cosine-weighted
            // local irradiance and the main-direction color response.
            vec3 response=light.coneType.y>1.5
                ?materialNprDiffuse(baseColor,metallic,localRamp,visibility,shadowVisibility,shadedNormal,viewDirection)
                :nprAmbientResponse*pointDiffuse;
            // Color selection and incident-energy visibility are independent.
            // The native illumination expression gates its main-light term
            // with scene visibility even when color selects an indoor tint.
            bool composedMain=camera.nprLightingParameters.x>.5&&float(lightIndex)==camera.nprLightingParameters.y;
            if(!composedMain)clusteredDiffuse+=response*radiance;
        } else {
            vec3 contribution=diffuseColor* radiance*localDiffuseResponse*diffuseScale;
            clusteredDiffuse+=contribution;
            if(float(lightIndex)==camera.nprLightingParameters.y)sceneMainDiffuse=contribution;
        }
        }
        vec3 pointHalfDirection = normalize(pointDirection + viewDirection);
        float pointSpecular = pow(
            max(dot(shadedNormal, pointHalfDirection), 0.0), specularPower);
        vec3 pointResponse = microfacetSurface
            ? directSurfaceSpecular(f0, roughness, pointDiffuse, normalDotView,
                max(dot(shadedNormal, pointHalfDirection), 0.0), max(dot(viewDirection, pointHalfDirection), 0.0))
            : f0 * pointSpecular * pointDiffuse * mix(0.7, 0.12, roughness);
        clusteredSpecular += pointResponse * radiance * light.boxParameters.z;
        if(sceneCharacter && hairActive>0.0 && !(qaEffectMode==3 && qaEffectDisabled)) {
            vec3 hairRadiance=(characterLight||specularOnly)?characterRadiance*pointDiffuse*light.boxParameters.z:radiance;
            if(projectedHair) {
                CharacterProjectedHairEnergy energy=projectedHairDirect(viewDirection,pointDirection,light.coneType.y>1.5);
                if(projectedLighting&&float(lightIndex)==camera.nprLightingParameters.y) {
                    mainHairPrimary=energy.primary;mainHairSecondary=energy.secondary;
                } else {
                    sceneHairPrimary+=energy.primary*hairRadiance*material.featureParameters.y;
                    // The observed local-light variant only contributes its main lobe.
                    if(light.coneType.y>1.5)sceneHairSecondary+=energy.secondary*hairRadiance*material.featureParameters.y;
                }
            } else {
            vec3 highlightNormal=normalize(tangentToWorld*decodePackedHairNormal(hairData.ba));
            vec2 lobes=characterHairLobes(shadedNormal,tangent,bitangent,highlightNormal,
                viewDirection,pointDirection,tangentToWorld*material.matcapColor.xyz,
                material.hairParameters.x,material.hairParameters.w);
            vec2 bands=smoothstep(vec2(.5,.43)-max(fwidth(lobes),vec2(.12)),
                vec2(.5,.43)+max(fwidth(lobes),vec2(.12)),lobes);
            float visibility=smoothstep(-.20,.45,dot(geometricNormal,viewDirection));
            vec2 rampExtent=vec2(textureSize(AZ_TEX_TOON_RAMP,0));
            float hairRamp=dot(textureLod(AZ_TEX_TOON_RAMP,vec2(
                clamp(lobes.x,.5/rampExtent.x,1-.5/rampExtent.x),
                (material.hairParameters.z+.5)/rampExtent.y),0).rgb,vec3(.2126,.7152,.0722));
            if(materialFeatureEnabled(512U)>.5) {
                float width=float(textureSize(AZ_TEX_MATCAP,0).x);
                hairRamp=textureLod(AZ_TEX_MATCAP,vec2((lobes.x*(width-1)+.5)/width,.5),0).r;
            }
            vec3 energy=((characterLight||specularOnly)?characterRadiance*light.boxParameters.z:radiance)*pointDiffuse*material.hairParameters.y
                *material.featureParameters.y*hairRamp;
            sceneHairPrimary+=mix(baseColor.rgb,vec3(1,.68,.74),.18)
                *bands.x*mix(.58,1.0,visibility)*.58*energy;
            sceneHairSecondary+=mix(baseColor.rgb,vec3(.72,.88,1),.12)
                *bands.y*mix(.35,.72,visibility)*.22*energy;
            }
        }
        if(sceneCharacter && !specularOnly && !(qaEffectMode==4 && qaEffectDisabled)) {
            sceneRim+=radiance*pow(1.0-normalDotView,3.2)
                *smoothstep(-.25,.65,dot(shadedNormal,pointDirection))
                *material.styleParameters.w*.12;
        }
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
    float faceActive = material.materialClass == 2U && !dualFaceSdf ? 1.0 : 0.0;
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
    float rimNormalDotView=dualFaceSdf
        ? abs(dot(faceLightingNormal,viewDirection)) : normalDotView;
    float rim = pow(1.0 - rimNormalDotView, 3.2)
        * smoothstep(-0.25, 0.65, dot(faceLightingNormal, -fillDirection));
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
    vec3 kkSpecular=vec3(0),secondaryKkSpecular=vec3(0);
    if(hairActive>0.0&&!projectedHair) {
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
    kkSpecular =
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
    secondaryKkSpecular =
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
    }
    if(projectedHair) {
        CharacterProjectedHairEnergy energy=projectedHairDirect(viewDirection,lightDirection,true);
        vec3 lightEnergy=keyColor*camera.showcaseParameters.y*keyVisibility
            *material.featureParameters.y*bandEnabled;
        kkSpecular=min(energy.primary*lightEnergy,vec3(material.matcapColor.w));
        secondaryKkSpecular=energy.secondary*lightEnergy;
    }
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
    vec3 matcapAccent=vec3(0);
    if(materialFeatureEnabled(4U)>0.5&&material.matcapColor.a!=0.0) {
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
    matcapAccent =
        matcapMask
        * matcapTint
        * material.matcapColor.a
        * material.featureParameters.w
        * materialFeatureEnabled(4U)
        * bandEnabled
        * 0.055;
    }
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
#if defined(AZURE_OPAQUE_SNAPSHOT)
    if(camera.screenReflectionTrace.x>.5)qaIsolationMode=0;
#endif
    vec3 sceneReflectionRadiance=vec3(0);
    vec3 sceneReflectionResponse=environmentResponse;
    bool overlayDisabled = qaEffectMode == 9 && qaEffectDisabled;
    float overlayVisible = overlayMaterial && !overlayDisabled ? 1.0 : 0.0;
    beautyColor = mix(beautyColor, baseColor.rgb, overlayVisible);
    if (overlayMaterial && overlayDisabled) {
        outputAlpha = 0.0;
    }
    if (camera.clusterLighting.x > .5) {
        // Scene mode consumes only declared light/environment contributions.
        // Specialized native character passes are selected separately.
        float angle = camera.clusterLighting.z;
        vec3 envNormal = indirectDiffuseNormal;
        vec3 envReflection = reflectionDirection;
        envNormal.xz = mat2(cos(angle),sin(angle),-sin(angle),cos(angle))*envNormal.xz;
        envReflection.xz = mat2(cos(angle),sin(angle),-sin(angle),cos(angle))*envReflection.xz;
        float lastMip = float(textureQueryLevels(AZ_TEX_ENVIRONMENT)-1);
        ambientDiffuse = (nprDiffuse?nprAmbientResponse:diffuseColor*physicalAo) * textureLod(AZ_TEX_ENVIRONMENT,
            directionToEquirectangular(envNormal),lastMip).rgb * camera.clusterLighting.y;
        sceneReflectionRadiance=textureLod(AZ_TEX_ENVIRONMENT,
            directionToEquirectangular(envReflection),roughness*lastMip).rgb*camera.clusterLighting.y;
        if(sceneCharacter)sceneReflectionResponse*=material.styleParameters.z*dielectricSpecularWeight;
        if(qaEffectMode==5&&qaEffectDisabled)sceneReflectionResponse=vec3(0);
        ambientSpecular=sceneReflectionResponse*sceneReflectionRadiance;
        directDiffuse = clusteredDiffuse;
        directSpecular = clusteredSpecular;
        if(qaEffectMode==5&&qaEffectDisabled)directSpecular=vec3(0);
        emissive = specularEmissive.rgb * material.emissiveStrength * materialFeatureEnabled(8U);
        rimLighting=vec3(0);kkSpecular=vec3(0);secondaryKkSpecular=vec3(0);
        if(sceneCharacter) {
            if(!nprDiffuse)ambientDiffuse*=indirectChroma;
            // Character response is applied to the shared incident radiance.
            directSpecular*=material.styleParameters.z*dielectricSpecularWeight;
            if(qaEffectMode==5 && qaEffectDisabled){ambientSpecular=vec3(0);directSpecular=vec3(0);}
            kkSpecular=min(sceneHairPrimary,vec3(material.matcapColor.w));
            secondaryKkSpecular=sceneHairSecondary;
            rimLighting=sceneRim;
        }
        if(!sceneCharacter&&nprDiffuse)rimLighting=sceneRim;
        beautyColor = directDiffuse + directSpecular + ambientDiffuse + ambientSpecular
            + emissive + kkSpecular + secondaryKkSpecular + rimLighting;
        if(overlayMaterial)beautyColor=overlayVisible>.5?baseColor.rgb:vec3(0);
    }
    if(camera.irradianceDimensionsEnabled.w>.5){
        vec4 volume=evaluateIrradianceVolume(worldPosition,indirectDiffuseNormal);
        vec3 blended=ambientDiffuse*(1-volume.w)+(nprDiffuse?nprAmbientResponse:diffuseColor*physicalAo)*volume.rgb;
        beautyColor+=blended-ambientDiffuse;ambientDiffuse=blended;
    }
    if((nprDiffuse||projectedLighting||eyeNpr)&&camera.nprLightingParameters.x>.5){
        float visibility=mix(shadowVisibility,1.0,camera.nprGainFloor.z);
        CharacterDiffuseResponse response;
        vec3 incident=camera.nprAmbientColor.rgb;
        vec4 mainRadiance=camera.nprMainRadiance;
        float hairShadowScale=1.0;
        if(eyeNpr) {
            PackedMaterialSurfaceProfile profile=materialSurfaceProfiles.profiles[material.materialPadding>>1U];
            response=eyeDiffuseResponse;incident=profile.rows[7].rgb;mainRadiance.rgb=profile.rows[8].rgb;
        } else if(projectedLighting) {
            PackedMaterialSurfaceProfile profile=materialSurfaceProfiles.profiles[material.materialPadding>>1U];
            hairShadowScale=profile.rows[5].w;
            response=hairDiffuseResponse;
            incident=profile.rows[10].rgb;mainRadiance.rgb=profile.rows[11].rgb;
        } else {
            response=materialNprDiffuseResponse(baseColor,metallic,classRampRgba,visibility,shadowVisibility,shadedNormal,viewDirection);
            PackedMaterialSurfaceProfile profile=materialSurfaceProfiles.profiles[material.materialPadding>>1U];
            if(profile.rows[10].w>.5) {
                incident=profile.rows[10].rgb;mainRadiance.rgb=profile.rows[11].rgb;
            }
        }
        if(camera.nprGainFloor.w>.5){
            vec3 envNormal=indirectDiffuseNormal;float angle=camera.clusterLighting.z;
            envNormal.xz=mat2(cos(angle),sin(angle),-sin(angle),cos(angle))*envNormal.xz;
            incident=textureLod(AZ_TEX_ENVIRONMENT,directionToEquirectangular(envNormal),float(textureQueryLevels(AZ_TEX_ENVIRONMENT)-1)).rgb*camera.clusterLighting.y;
            if(camera.irradianceDimensionsEnabled.w>.5){vec4 volume=evaluateIrradianceVolume(worldPosition,indirectDiffuseNormal);incident=mix(incident,volume.rgb,volume.w);}
        }
        CharacterDiffuseIllumination illumination=characterDiffuseIllumination(indirectDiffuseNormal,incident,mainRadiance,
            camera.nprDirectionBias,camera.nprGainFloor,camera.nprComposition,camera.nprAmbientColor.a,
            physicalAo*camera.nprLightingParameters.z,response.litMask,visibility);
        vec3 combinedAmbient=response.color*illumination.ambient;
        vec3 combinedDirect=response.color*illumination.direct+clusteredDiffuse-(projectedLighting?sceneMainDiffuse:vec3(0));
        if(eyeNpr) {
            combinedAmbient*=eyeAlphaMultiplier;
            vec3 eyeMainDirect=response.color*illumination.direct*eyeAlphaMultiplier;
            eyeHostMatcap=characterEyeMatcap(eyeMatcap,eyeProfile.matcap,eyeViewRampAlpha,classRampRgba.a,
                visibility,eyeProfile.shape.w,illumination.ambient+illumination.direct)*(1.0-eyeCoordinates.whiteMask);
            float saturation=characterEyeMainSaturation(combinedAmbient+eyeMainDirect+eyeHostMatcap);
            combinedAmbient=characterDiffuseSaturation(combinedAmbient,saturation);
            combinedDirect=characterDiffuseSaturation(eyeMainDirect,saturation)+clusteredDiffuse*eyeAlphaMultiplier;
            eyeHostMatcap=characterDiffuseSaturation(eyeHostMatcap,saturation);
        }
        sceneMainDiffuse=response.color*illumination.direct;
        beautyColor+=combinedAmbient+combinedDirect-ambientDiffuse-directDiffuse;
        ambientDiffuse=combinedAmbient;directDiffuse=combinedDirect;
        if(projectedLighting) {
            float weight=mix(hairViewAlpha*response.aoMask,response.litMask,visibility);
            float scale=mix(hairShadowScale,1.0,weight)*(.5+.5*weight);
            vec3 specularIllumination=(illumination.ambient+illumination.direct)*scale*material.featureParameters.y;
            vec3 primary=min(sceneHairPrimary+mainHairPrimary*specularIllumination,vec3(material.matcapColor.w));
            vec3 secondary=sceneHairSecondary+mainHairSecondary*specularIllumination;
            beautyColor+=primary+secondary-kkSpecular-secondaryKkSpecular;
            kkSpecular=primary;secondaryKkSpecular=secondary;
        }
    }
    if(projectedHair&&materialSurfaceProfiles.profiles[material.materialPadding>>1U].rows[8].z>.5
        &&(camera.clusterLighting.x<.5||camera.mainLightDirection.w>.5)){
        vec2 line;projectedHairResponse(viewDirection,lightDirection,true,true,line);
        vec3 dark=ambientDiffuse*line.x;
        vec3 adjusted=mix(vec3(dot(dark,vec3(.212673,.715152,.072175))),dark,line.y);
        beautyColor+=adjusted-ambientDiffuse;ambientDiffuse=adjusted;
        if(materialSurfaceProfiles.profiles[material.materialPadding>>1U].rows[8].w>.5) {
            vec3 mainDiffuse=camera.clusterLighting.x>.5?sceneMainDiffuse:directDiffuse;
            vec3 adjustedMain=characterDiffuseSaturation(mainDiffuse*line.x,line.y);
            vec3 difference=adjustedMain-mainDiffuse;
            directDiffuse+=difference;beautyColor+=difference;
        }
    }
    if(camera.probeParameters.x>.5){
        vec3 edge=camera.probeHalfExtents.xyz-abs(worldPosition-camera.probeCenter.xyz);
        float distanceToEdge=min(edge.x,min(edge.y,edge.z));
        float weight=distanceToEdge>0.0?(camera.probeParameters.z>0.0?clamp(distanceToEdge/camera.probeParameters.z,0.0,1.0):1.0):0.0;
        if(weight>0.0){
            vec3 direction=reflectionDirection;
            if(camera.probeParameters.w>.5){
                vec3 safeDirection=mix(vec3(1e-8),direction,greaterThan(abs(direction),vec3(1e-8)));
                vec3 face=camera.probeCenter.xyz+mix(-camera.probeHalfExtents.xyz,camera.probeHalfExtents.xyz,greaterThanEqual(direction,vec3(0.0)));
                vec3 distances=mix(vec3(1e30),(face-worldPosition)/safeDirection,greaterThan(abs(direction),vec3(1e-8)));
                float exitDistance=min(distances.x,min(distances.y,distances.z));
                direction=worldPosition+reflectionDirection*exitDistance-camera.probeCapture.xyz;
            }
            float lastMip=float(textureQueryLevels(reflectionProbeTexture)-1);
            vec3 localSpecular=environmentResponse*textureLod(reflectionProbeTexture,direction,roughness*lastMip).rgb*camera.probeParameters.y;
            if(sceneCharacter)localSpecular*=material.styleParameters.z*dielectricSpecularWeight;
            if(qaEffectMode==5 && qaEffectDisabled)localSpecular=vec3(0);
            vec3 blended=mix(ambientSpecular,localSpecular,weight);
            if(camera.clusterLighting.x>.5){
                vec3 localRadiance=textureLod(reflectionProbeTexture,direction,roughness*lastMip).rgb*camera.probeParameters.y;
                sceneReflectionRadiance=mix(sceneReflectionRadiance,localRadiance,weight);
                blended=sceneReflectionResponse*sceneReflectionRadiance;
            }
            beautyColor+=blended-ambientSpecular;
            ambientSpecular=blended;
        }
    }
#if !defined(AZURE_OPAQUE_SNAPSHOT)
    if(camera.clusterLighting.x>.5&&camera.screenReflectionTrace.x>.5&&microfacetSurface
        &&!overlayMaterial&&!(qaEffectMode==5&&qaEffectDisabled)){
        vec4 screenReflection=traceScreenSpaceReflection(opaqueSceneColor,overlaySceneDepth,
            camera.screenReflectionWorldToClip,camera.cameraPosition.xyz,camera.cameraForward.xyz,
            worldPosition,shadedNormal,reflectionDirection,roughness,
            camera.screenReflectionTrace,camera.screenReflectionQuality);
        vec3 radiance=mix(sceneReflectionRadiance,screenReflection.rgb,screenReflection.a);
        vec3 blended=sceneReflectionResponse*radiance;
        beautyColor+=blended-ambientSpecular;ambientSpecular=blended;
    }
    if(materialFeatureEnabled(1024U)>.5) {
        vec2 uv=gl_FragCoord.xy/camera.clusterDepth.zw;
        vec2 mapUv=textureCoordinate*material.styleParameters.xy+material.styleParameters.zw;
        vec2 offset=sceneRefractionOffset(texture(AZ_TEX_STYLE_MASK,mapUv).rg,material.hairParameters.z);
        float depth=dot(worldPosition-camera.cameraPosition.xyz,camera.cameraForward.xyz);
        float fullDepth=texture(overlaySceneDepth,sceneRefractionMirrorUv(uv+offset)).r;
        float quarterDepth=texture(overlaySceneDepth,sceneRefractionMirrorUv(uv+offset*.25)).r;
        vec2 safeUv=sceneRefractionUv(uv,offset,fullDepth,quarterDepth,depth);
        // The shared snapshot uses a nearest sampler. Perform linear filtering
        // explicitly with mirrored texel addressing for this material model.
        ivec2 extent=textureSize(opaqueSceneColor,0);
        vec2 pixel=safeUv*vec2(extent)-.5;ivec2 first=ivec2(floor(pixel));vec2 f=fract(pixel);
        vec3 taps[4];
        for(int tap=0;tap<4;++tap) {
            ivec2 p=first+ivec2(tap%2,tap/2);ivec2 period=extent*2;
            p=ivec2(mod(vec2(p),vec2(period)));p=min(p,period-1-p);
            taps[tap]=texelFetch(opaqueSceneColor,p,0).rgb;
        }
        vec3 background=mix(mix(taps[0],taps[1],f.x),mix(taps[2],taps[3],f.x),f.y);
        background*=material.matcapColor.rgb*material.hairParameters.y;
        beautyColor=sceneRefractionSurface(directDiffuse+ambientDiffuse,directSpecular+ambientSpecular,
            background,outputAlpha,material.hairParameters.x)+emissive;
    }
#endif
    if(materialFeatureEnabled(262144U)>.5) {
        // A public unlit surface consumes linear base color plus emission.
        // Lighting diagnostics describe energy actually used by this model.
        emissive=specularEmissive.rgb*material.emissiveStrength*materialFeatureEnabled(8U);
        if(qaEffectMode==6&&qaEffectDisabled)emissive=vec3(0);
        directDiffuse=vec3(0);ambientDiffuse=vec3(0);
        directSpecular=vec3(0);ambientSpecular=vec3(0);
        kkSpecular=vec3(0);secondaryKkSpecular=vec3(0);rimLighting=vec3(0);
        beautyColor=baseColor.rgb+emissive;
    }
    if(eyeRadial) {
        if(camera.clusterLighting.x<.5) {
            vec4 ramp=textureLod(AZ_TEX_STYLE_MASK,vec2(dot(shadedNormal,lightDirection)*.5+.5,.5),0.0);
            vec3 illumination=keyColor*camera.showcaseParameters.y*keyVisibility*max(dot(shadedNormal,lightDirection),0.0);
            eyeHostDiffuse=baseColor.rgb*eyeDiffuseMultiplier*ramp.rgb*illumination*eyeAlphaMultiplier;
            eyeHostMatcap=characterEyeMatcap(eyeMatcap,eyeProfile.matcap,eyeViewRampAlpha,ramp.a,
                1.0-shadowVisibility,eyeProfile.shape.w,illumination);
        }
        if(!eyeNpr){directDiffuse=eyeHostDiffuse;ambientDiffuse=vec3(0);}
        directSpecular=eyeHostMatcap;ambientSpecular=vec3(0);
        kkSpecular=vec3(0);secondaryKkSpecular=vec3(0);rimLighting=eyeNpr?sceneRim:vec3(0);
        emissive=characterEyeDirect(baseColor,eyeCoordinates.whiteMask,eyeProfile.highlight.rgb,
            eyeProfile.scattering.rgb,eyeProfile.directWeights.rgb,eyeProfile.shape.z);
        beautyColor=directDiffuse+ambientDiffuse+directSpecular+emissive+rimLighting;
        outputAlpha=1.0;
    }
    if(multiplyComposite)beautyColor=baseColor.rgb;
    if(characterBlend.a<1.0) {
        beautyColor=beautyColor*characterBlend.a+characterBlend.rgb;
        directDiffuse=directDiffuse*characterBlend.a+characterBlend.rgb;
        ambientDiffuse*=characterBlend.a;directSpecular*=characterBlend.a;ambientSpecular*=characterBlend.a;
        kkSpecular*=characterBlend.a;secondaryKkSpecular*=characterBlend.a;rimLighting*=characterBlend.a;emissive*=characterBlend.a;
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
    } else if (qaIsolationMode == 17) {
        qaColor = vec3(roughness);
    } else if (qaIsolationMode == 18) {
        qaColor = vec3(metallic);
    } else if (qaIsolationMode == 19) {
        qaColor = directSpecular;
    } else if (qaIsolationMode == 20) {
        qaColor = ambientSpecular;
    } else if (qaIsolationMode == 21) {
        qaColor = shadedNormal * 0.5 + 0.5;
    } else if (qaIsolationMode == 22) {
        qaColor = materialFeatureEnabled(4096U)>0.5
            ? sampleShadowColorLut(AZ_TEX_SHADOW_COLOR_LUT,baseColor.rgb)
            : vec3(0);
    } else if (qaIsolationMode == 23) {
        qaColor = vec3(materialObjectVisibility(shadedNormal));
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
    float compositeCoverage=overlayDisabled?0.0:clamp(outputAlpha*material.nodeOpacity,0.0,1.0);
    outputColor = vec4(qaColor, outputAlpha);
    if ((material.materialPadding & AZURE_SELECTED_PADDING) != 0U) {
        outputColor.rgb = mix(outputColor.rgb, vec3(0.96, 0.62, 0.10), 0.42);
        outputColor.a = 1.0;
    }
    if (material.alphaMode == 2) outputColor.a *= material.nodeOpacity;
    if(multiplyComposite) {
        outputColor.rgb=mix(vec3(1.0),outputColor.rgb,compositeCoverage);
        outputColor.a=0.0;
    }
    float innerOutlineParticipation =
        (1.0 - platformMask)
        * (overlayMaterial ? 0.0 : 1.0)
        * material.featureParameters.x
        * mix(
            1.0,
            0.68,
            max(hairActive, clamp(material.matcapColor.a, 0.0, 1.0)));
    if(camera.clusterLighting.x>.5 && !sceneCharacter)innerOutlineParticipation=0.0;
    vec3 innerOutlineNormal = geometricNormal;
#if defined(AZURE_OPAQUE_SNAPSHOT)
    outputNormal=vec4(dot(worldPosition-camera.cameraPosition.xyz,camera.cameraForward.xyz),0,0,1);
#else
    outputNormal = vec4(
        innerOutlineNormal * 0.5 + 0.5,
        innerOutlineParticipation);
#endif
}
