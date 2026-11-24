#version 450
layout(set=0,binding=0) uniform sampler2DMS sourceColor;
layout(set=0,binding=1) uniform sampler2DMS sourceDepth;
layout(set=0,binding=2) uniform sampler2DMS sourceSecondary;
layout(set=0,binding=3) uniform sampler2DMS sourceDisplayMask;
layout(push_constant) uniform ResolvePolicy {uint flags;} policy;
layout(location=0) out vec4 outColor;
layout(location=1) out vec4 outSecondary;
#ifdef AZURE_DISPLAY_MASK
layout(location=2) out vec4 outDisplayMask;
#endif
void main(){
    ivec2 pixel=ivec2(gl_FragCoord.xy);
    vec4 color=vec4(0),secondary=vec4(0),nearest=texelFetch(sourceSecondary,pixel,0);
    vec4 nearestSample=nearest;float depth=1,displayWeight=0;
    bool displayMask=(policy.flags&256u)!=0u;
    bool snapshotReceivers=(policy.flags&512u)!=0u;
    uint secondaryPolicy=policy.flags&255u,receiverMask=0u;
    for(int sampleIndex=0;sampleIndex<4;++sampleIndex){
        vec4 source=texelFetch(sourceColor,pixel,sampleIndex);color+=source;
        vec4 value=texelFetch(sourceSecondary,pixel,sampleIndex);secondary+=value;nearest=min(nearest,value);
        // Snapshot alpha packs four two-bit classes. RGBA16F stores every value exactly.
        if(snapshotReceivers){
            uint receiver=value.r>=100000.0?3u:uint(source.a);
            receiverMask|=receiver<<(2*sampleIndex);
        }
        if(value.r<nearestSample.r)nearestSample=value;
        depth=min(depth,texelFetch(sourceDepth,pixel,sampleIndex).r);
        if(displayMask)displayWeight+=texelFetch(sourceDisplayMask,pixel,sampleIndex).r;
    }
    outColor=color*.25;outSecondary=secondaryPolicy==0?secondary*.25:secondaryPolicy==1?nearest:nearestSample;gl_FragDepth=depth;
    if(snapshotReceivers)outColor.a=float(receiverMask);
#ifdef AZURE_DISPLAY_MASK
    outDisplayMask=vec4(displayMask?displayWeight*.25:1,0,0,1);
#endif
}
