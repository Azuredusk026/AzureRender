#ifndef AZURE_REFLECTION_SURFACE_GLSL
#define AZURE_REFLECTION_SURFACE_GLSL
vec2 reflectionSurfaceSign(vec2 v){return mix(vec2(-1),vec2(1),greaterThanEqual(v,vec2(0)));}
// R is positive camera-forward depth; GB is octahedral world normal; A is roughness.
vec4 encodeReflectionSurface(float depth,vec3 normal,float roughness){
    vec3 n=normalize(normal);n/=abs(n.x)+abs(n.y)+abs(n.z);
    if(n.z<0)n.xy=(1-abs(n.yx))*reflectionSurfaceSign(n.xy);
    return vec4(depth,n.xy*.5+.5,clamp(roughness,0,1));
}
vec3 decodeReflectionSurfaceNormal(vec4 surface){
    vec2 p=surface.gb*2-1;vec3 n=vec3(p,1-abs(p.x)-abs(p.y));
    if(n.z<0)n.xy=(1-abs(n.yx))*reflectionSurfaceSign(n.xy);
    return normalize(n);
}
#endif
