#ifndef AZURE_REFERENCE_POSITION_FRAME_GLSL
#define AZURE_REFERENCE_POSITION_FRAME_GLSL
#include "float32_reference.glsl"
// The caller validates three unique signed axes and finite inputs/results.
// Integer FMA preserves the binary32 addition, including denormal values.
vec3 azureReferenceRelativePosition(vec3 world,uint packedAxes,vec3 offset){
    vec3 result;
    for(uint axis=0u;axis<3u;++axis){
        uint code=(packedAxes>>(axis*3u))&7u;
        uint bits=floatBitsToUint(world[code&3u])^((code&4u)<<29u);
        uint other=floatBitsToUint(offset[axis]);
        if(((bits|other)&0x7fffffffu)==0u)result[axis]=uintBitsToFloat((bits&other)&0x80000000u);
        else result[axis]=float32ReferenceFma(1.0,uintBitsToFloat(bits),offset[axis]);
    }
    return result;
}
#endif
