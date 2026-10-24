#version 450
layout(set=0,binding=0) uniform sampler2DMS sourceColor;
layout(set=0,binding=1) uniform sampler2DMS sourceDepth;
layout(set=0,binding=2) uniform sampler2DMS sourceSecondary;
layout(push_constant) uniform ResolvePolicy {uint secondaryMinimum;} policy;
layout(location=0) out vec4 outColor;
layout(location=1) out vec4 outSecondary;
void main(){
    ivec2 pixel=ivec2(gl_FragCoord.xy);
    vec4 color=vec4(0),secondary=vec4(0),nearest=texelFetch(sourceSecondary,pixel,0);
    float depth=1;
    for(int sampleIndex=0;sampleIndex<4;++sampleIndex){
        color+=texelFetch(sourceColor,pixel,sampleIndex);
        vec4 value=texelFetch(sourceSecondary,pixel,sampleIndex);secondary+=value;nearest=min(nearest,value);
        depth=min(depth,texelFetch(sourceDepth,pixel,sampleIndex).r);
    }
    outColor=color*.25;outSecondary=policy.secondaryMinimum==0?secondary*.25:nearest;gl_FragDepth=depth;
}
