#version 450
layout(set=0,binding=0) uniform sampler2D toonInput;
layout(set=0,binding=1) uniform sampler2D normalInput;
layout(location=0) out vec4 color;
void main() {
    ivec2 p=ivec2(gl_FragCoord.xy), hi=textureSize(normalInput,0)-1;
    vec4 center=texelFetch(normalInput,p,0);
    float edge=0;
    const ivec2 offsets[4]=ivec2[](ivec2(1,0),ivec2(-1,0),ivec2(0,1),ivec2(0,-1));
    for(int i=0;i<4;i++) {
        vec4 neighbor=texelFetch(normalInput,clamp(p+offsets[i],ivec2(0),hi),0);
        if(abs(center.w-neighbor.w)>0.02 || length(center.xyz-neighbor.xyz)>0.35) edge=1;
    }
    color=vec4(mix(texelFetch(toonInput,p,0).rgb,vec3(0.015),edge),1);
}
