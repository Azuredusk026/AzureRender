#ifndef AZURE_MATERIAL_OUTPUT_PRECISION_GLSL
#define AZURE_MATERIAL_OUTPUT_PRECISION_GLSL
// Five exponent bits, fixed five/six fraction bits, round toward zero.
float azureUnsignedMaterialFloat(float value,uint mantissa) {
    uint word=floatBitsToUint(value),magnitude=word&0x7fffffffu;
    if(magnitude>0x7f800000u)return uintBitsToFloat(0x7fc00000u);
    if((word&0x80000000u)!=0u)return 0.0;
    if(magnitude==0x7f800000u)return value;
    uint maximum=mantissa==6u?0x477e0000u:0x477c0000u;
    if(magnitude>maximum)return uintBitsToFloat(maximum);
    uint exponent=magnitude>>23u;
    if(exponent<113u) {
        if(exponent<113u-mantissa)return 0.0;
        uint significand=(magnitude&0x7fffffu)|0x800000u;
        uint count=significand>>(23u+113u-exponent-mantissa);
        return float(count)*uintBitsToFloat((113u-mantissa)<<23u);
    }
    return uintBitsToFloat(magnitude&~((1u<<(23u-mantissa))-1u));
}
vec4 azureMaterialOutputPrecision(vec4 color,bool enabled) {
    if(!enabled)return color;
    return vec4(azureUnsignedMaterialFloat(color.r,6u),azureUnsignedMaterialFloat(color.g,6u),
        azureUnsignedMaterialFloat(color.b,5u),color.a);
}
#endif
