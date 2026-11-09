#version 450
#ifdef LOCAL_READ
layout(input_attachment_index=0,set=0,binding=0) uniform subpassInput albedoInput;
layout(input_attachment_index=1,set=0,binding=1) uniform subpassInput normalInput;
#else
layout(set=0,binding=0) uniform sampler2D albedoInput;
layout(set=0,binding=1) uniform sampler2D normalInput;
#endif
#ifdef DYNAMIC_PATH
layout(location=2) out vec4 color;
#else
layout(location=0) out vec4 color;
#endif
void main() {
#ifdef LOCAL_READ
    vec4 a=subpassLoad(albedoInput), n=subpassLoad(normalInput);
#else
    ivec2 p=ivec2(gl_FragCoord.xy);
    vec4 a=texelFetch(albedoInput,p,0), n=texelFetch(normalInput,p,0);
#endif
    if(n.w>=1) { color=vec4(0.12,0.16,0.22,1); return; }
    float light=max(dot(normalize(n.xyz),normalize(vec3(-0.4,0.6,-1))),0);
    float band=light>0.65?1:(light>0.3?0.65:0.3);
    color=vec4(a.rgb*band,1);
}
