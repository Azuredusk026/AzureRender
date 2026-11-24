#version 450
layout(set=0,binding=44) uniform sampler2D sourceDepth;
layout(set=0,binding=43,std430) buffer Selection {uint flags;} selection;
void main(){
    float depth=texelFetch(sourceDepth,ivec2(gl_FragCoord.xy),0).r;
    bool safe=!isnan(depth)&&!isinf(depth)&&depth>=1.17549435e-38&&depth<=1;
    if(!safe&&!gl_HelperInvocation)atomicOr(selection.flags,9u);
    gl_FragDepth=safe?depth:1;
}
