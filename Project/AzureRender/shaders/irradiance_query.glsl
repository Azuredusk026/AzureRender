#ifndef AZURE_IRRADIANCE_QUERY_GLSL
#define AZURE_IRRADIANCE_QUERY_GLSL
// Exact source literals and explicit contraction order preserve the native
// interleaved-gradient sequence. Pixel origin is top-left; phase is caller-owned.
float irradianceQuerySignedNoise(vec2 pixel,float phase){
    precise vec2 shifted=fma(vec2(trunc(phase)),
        vec2(uintBitsToFloat(0x40054fdfU),uintBitsToFloat(0x409bbe77U)),floor(pixel));
    precise float horizontal=shifted.x*uintBitsToFloat(0x3d897143U);
    precise float gradient=fma(shifted.y,uintBitsToFloat(0x3bbf4590U),horizontal);
    precise float scaled=fract(gradient)*uintBitsToFloat(0x4253ee82U);
    return fma(fract(scaled),2.0,-1.0);
}
vec3 irradianceQueryPosition(vec3 surface,vec3 normal,vec2 pixel,vec3 parameters){
    if(parameters.x==0.0&&parameters.y==0.0)return surface;
    precise vec3 offset=fma(normal,vec3(parameters.x),surface);
    return fma(vec3(irradianceQuerySignedNoise(pixel,parameters.z)),vec3(parameters.y),offset);
}
#endif
