#version 450
layout(set=0,binding=0) uniform sampler2DMS sourceColor;
layout(set=0,binding=1) uniform sampler2DMS sourceDepth;
layout(set=0,binding=2) uniform sampler2DMS sourceSecondary;
layout(set=0,binding=3) uniform sampler2DMS sourceDisplayMask;
layout(push_constant) uniform ResolvePolicy {uint secondaryMinimum;} policy;
layout(location=0) out vec4 outColor;
layout(location=1) out vec4 outSecondary;
#ifdef AZURE_DISPLAY_MASK
layout(location=2) out vec4 outDisplayMask;
#endif
void main(){
    ivec2 pixel=ivec2(gl_FragCoord.xy);
    vec4 color=vec4(0),secondary=vec4(0),nearest=texelFetch(sourceSecondary,pixel,0);
    vec4 nearestSample=nearest;float depth=1,displayWeight=0;
    bool displayMask=(policy.secondaryMinimum&256u)!=0u;
    uint secondaryPolicy=policy.secondaryMinimum&255u;
    for(int sampleIndex=0;sampleIndex<4;++sampleIndex){
        color+=texelFetch(sourceColor,pixel,sampleIndex);
        vec4 value=texelFetch(sourceSecondary,pixel,sampleIndex);secondary+=value;nearest=min(nearest,value);
        if(value.r<nearestSample.r)nearestSample=value;
        depth=min(depth,texelFetch(sourceDepth,pixel,sampleIndex).r);
        if(displayMask)displayWeight+=texelFetch(sourceDisplayMask,pixel,sampleIndex).r;
    }
    outColor=color*.25;outSecondary=secondaryPolicy==0?secondary*.25:secondaryPolicy==1?nearest:nearestSample;gl_FragDepth=depth;
#ifdef AZURE_DISPLAY_MASK
    outDisplayMask=vec4(displayMask?displayWeight*.25:1,0,0,1);
#endif
}
