#version 450
#extension GL_GOOGLE_include_directive : require
#include "motion_vector_push.glsl"
#include "motion_projection.glsl"
#include "node_opacity.glsl"
#include "material_sampling.glsl"
layout(binding=5) uniform sampler2D baseColor;
layout(binding=8) uniform sampler2D coverageMask;
layout(location=0) in vec2 textureCoordinate;
layout(location=1) in vec3 opacityCoordinate;
layout(location=2) in vec4 currentClip;
layout(location=3) in vec4 previousClip;
layout(location=4) in float previousValid;
layout(location=5) in vec3 materialFrameNormal;
layout(location=6) in vec3 materialFrameTangent;
layout(location=0) out vec4 outputMotion;
layout(location=1) out float outputSurfaceDepth;
void main(){
    bool integrated=false;
    if((motion.flags&512u)==0u&&(floatBitsToUint(motion.detail.y)&azurePixelIntegralFeature)!=0u)
        integrated=azureUniformMaterialFrame(dFdx(materialFrameNormal),dFdy(materialFrameNormal),
            dFdx(materialFrameTangent),dFdy(materialFrameTangent));
    bool blend=(motion.flags&512u)==0u&&(motion.flags&3u)==2u;
    if(blend){if(motion.outline.w<=0)discard;}
    else if(!azureNodeOpacityCovered(opacityCoordinate,motion.outline.w))discard;
    if((motion.flags&512u)!=0u){
        uint tile=floatBitsToUint(motion.camera.w);
        if(tile!=0xffffffffu){ivec2 extent=ivec2(motion.outline.yz);uint columns=uint(textureSize(coverageMask,0).x/extent.x);
            if(columns>0u&&texelFetch(coverageMask,ivec2(tile%columns,tile/columns)*extent+ivec2(gl_FragCoord.xy),0).r>.5)discard;}
    }else{
        uint alphaMode=motion.flags&3u;
        if(alphaMode==1u&&texture(baseColor,textureCoordinate).a<motion.alphaCutoff)discard;
        if(alphaMode==2u){
            float alpha=(floatBitsToUint(motion.detail.y)&0x40000000u)!=0u
                ?azureTemporalBlendAlpha(baseColor,textureCoordinate,dFdx(textureCoordinate),dFdy(textureCoordinate),motion.detail.zw*motion.outline.yz*.5)
                :texture(baseColor,textureCoordinate).a;
            if(alpha<=0)discard;
        }
    }
    outputMotion=azureMotionProjection(currentClip,previousClip,previousValid>=.999999);
    if((motion.flags&16u)!=0u)outputMotion.w=-2;
    else if(outputMotion.w==0)outputMotion.w=-1;
    else if(integrated)outputMotion.w=-.5;
    outputSurfaceDepth=gl_FragCoord.z;
}
