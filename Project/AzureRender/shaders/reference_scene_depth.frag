#version 450
#extension GL_GOOGLE_include_directive : require
#if defined(AZURE_BINDLESS)
#extension GL_EXT_nonuniform_qualifier : require
#endif
#include "material_primary_uv.glsl"
#include "node_opacity.glsl"
#if defined(AZURE_REFERENCE_SCENE_VISIBILITY)
#include "character_diffuse.glsl"
#include "character_surface.glsl"
#endif

layout(push_constant) uniform Material {
    float alphaCutoff;
    int alphaMode;
#if defined(AZURE_REFERENCE_SCENE_VISIBILITY)
    layout(offset=64) vec4 normalControl;
    layout(offset=80) float referenceNormalMipBias;
#endif
    layout(offset=116) uint features;
    layout(offset=136) uint textureBase;
    layout(offset=140) float nodeOpacity;
} material;

#if defined(AZURE_BINDLESS)
layout(binding=1) uniform sampler2D textures[];
#define BASE textures[nonuniformEXT(material.textureBase)]
#define COVERAGE textures[nonuniformEXT(material.textureBase+2u)]
#define NORMAL_TEXTURE textures[nonuniformEXT(material.textureBase+1u)]
#define HAIR_DATA textures[nonuniformEXT(material.textureBase+7u)]
#else
layout(binding=1) uniform sampler2D baseColor;
layout(binding=3) uniform sampler2D independentCoverage;
#define BASE baseColor
#define COVERAGE independentCoverage
#if defined(AZURE_REFERENCE_SCENE_VISIBILITY)
layout(binding=2) uniform sampler2D normalTexture;
layout(binding=8) uniform sampler2D hairDataTexture;
#define NORMAL_TEXTURE normalTexture
#define HAIR_DATA hairDataTexture
#endif
#endif

layout(location=2) in vec2 textureCoordinate;
layout(location=3) in vec3 worldPosition;
layout(location=14) in vec3 opacityCoordinate;
layout(location=0) out float referenceDepth;
#if defined(AZURE_REFERENCE_SCENE_VISIBILITY)
layout(location=0) in vec3 worldNormal;
layout(location=1) in vec4 worldTangent;
layout(location=15) flat in uint instanceObjectShadow;
layout(location=1) out vec4 packedNormal;
layout(location=2) out uint objectId;

vec4 sampleSurfaceTexture(sampler2D image){
    const uint mirroredCoarseUv=1u<<27u;
    return (material.features&mirroredCoarseUv)!=0u?azureSamplePrimaryUv(image,textureCoordinate):texture(image,textureCoordinate);
}

vec3 surfaceNormal(){
    float facing=gl_FrontFacing?1.0:-1.0;
    float mode=material.normalControl.x;
    if (mode == 4.0)
        return characterProjectedSafeNormalize(worldNormal, vec3(0, 0, 1)) * facing;
    if(mode==3.0){
        vec2 xy=fma(sampleSurfaceTexture(NORMAL_TEXTURE).rg,vec2(2.0),vec2(-1.0));
        float z=sqrt(max(1.0-min(dot(xy,xy),1.0),0.0));
        vec3 bitangent=cross(worldNormal,worldTangent.xyz)*worldTangent.w;
        vec3 mapped=fma(worldTangent.xyz,vec3(xy.x),bitangent*xy.y);
        mapped=fma(worldNormal,vec3(z),mapped);
        return characterProjectedSafeNormalize(mapped,characterProjectedSafeNormalize(worldNormal,vec3(0,0,1)))*facing;
    }
    if(mode>0.0){
        // The authored snapshot normal follows the reference attachment's raster rows.
        bool materialNormal=mode==2.0||mode==5.0;
        vec2 encoded;
        if(mode==5.0){
            vec2 size=vec2(textureSize(NORMAL_TEXTURE,0));
            float lod=azureAxisDiagonalMipLod(azurePrimaryDx(textureCoordinate)*size,azurePrimaryDy(textureCoordinate)*size);
            precise float biasedLod=lod+material.referenceNormalMipBias;
            bool atBoundary=abs(fract(biasedLod)-0.5)<=1.0/256.0;
            vec2 implicitNormal=texture(NORMAL_TEXTURE,textureCoordinate).rg;
            vec2 explicitNormal=textureLod(NORMAL_TEXTURE,textureCoordinate,lod).rg;
            encoded=mix(explicitNormal,implicitNormal,bvec2(atBoundary));
        }else{
            encoded=materialNormal?texture(NORMAL_TEXTURE,textureCoordinate).rg:sampleSurfaceTexture(HAIR_DATA).rg;
        }
        if(materialNormal)encoded.y=1.0-encoded.y;
        return characterProjectedHairNormal(encoded,material.normalControl.y,worldNormal,
            vec4(worldTangent.xyz,worldTangent.w*material.normalControl.z))*facing;
    }
    vec3 normal=characterProjectedSafeNormalize(worldNormal,vec3(0,0,1))*facing;
    vec3 tangent=characterProjectedSafeNormalize(worldTangent.xyz-normal*dot(normal,worldTangent.xyz),vec3(1,0,0));
    vec3 bitangent=cross(normal,tangent)*worldTangent.w;
    vec3 sampled=sampleSurfaceTexture(NORMAL_TEXTURE).xyz*2.0-1.0;
    return characterProjectedSafeNormalize(mat3(tangent,bitangent,normal)*sampled,normal);
}

vec3 referenceNormal(vec3 world){
    uint axes=uint(material.normalControl.w);
    vec3 result;
    for(uint axis=0u;axis<3u;++axis){
        uint code=(axes>>(axis*3u))&7u;
        result[axis]=uintBitsToFloat(floatBitsToUint(world[code&3u])^((code&4u)<<29u));
    }
    return result;
}

vec4 encodeVisibilityNormal(vec3 normal){
    vec2 oct=normal.xz/(abs(normal.x)+abs(normal.y)+abs(normal.z));
    if(normal.y<0.0)oct=(1.0-abs(oct.yx))*mix(vec2(-1),vec2(1),greaterThanEqual(oct,vec2(0)));
    return vec4(oct*0.5+0.5,0.0,1.0);
}
#endif

float coverageAlpha(sampler2D image){
    const uint mirroredCoarseUv=1u<<27u;
    if((material.features&mirroredCoarseUv)==0u)return texture(image,textureCoordinate).a;
    return azureSampleCoverageAlpha(image,textureCoordinate,material.features);
}

void main(){
    azurePreparePrimaryUv(textureCoordinate);
#if defined(AZURE_REFERENCE_SCENE_VISIBILITY)
    vec3 normal=surfaceNormal();
#endif
    if(!azureNodeOpacityCovered(opacityCoordinate,material.nodeOpacity))discard;
    if(material.alphaMode==1){
        const uint independentCoverageBit=1u<<25u;
        float alpha=(material.features&independentCoverageBit)!=0u?coverageAlpha(COVERAGE):coverageAlpha(BASE);
        if(alpha<material.alphaCutoff)discard;
    }
    referenceDepth=gl_FragCoord.z;
#if defined(AZURE_REFERENCE_SCENE_VISIBILITY)
    packedNormal=encodeVisibilityNormal(referenceNormal(normal));
    objectId=instanceObjectShadow;
#endif
}
