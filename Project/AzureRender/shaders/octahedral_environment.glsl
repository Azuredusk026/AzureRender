#ifndef AZURE_OCTAHEDRAL_ENVIRONMENT_GLSL
#define AZURE_OCTAHEDRAL_ENVIRONMENT_GLSL
// Callers provide a finite, nonzero direction. The atlas owns padding and the
// zero-sign fold convention independently from the image's array layer.
vec2 octahedralEnvironmentUv(vec3 direction,vec4 atlas,bool positiveZero){
    precise vec3 n=normalize(direction);
    vec3 signs=sign(n);
    precise float denominator=dot(n,signs);
    precise vec3 oct=n/denominator;
    if(oct.z<0.0)oct.xy=(vec2(1)-abs(oct.yx))*(positiveZero?mix(vec2(-1),vec2(1),greaterThanEqual(n.xy,vec2(0))):signs.xy);
    precise vec2 uv=fma(oct.xy,vec2(.5),vec2(.5));
    return fma(uv,atlas.xy,atlas.zw);
}
#endif
