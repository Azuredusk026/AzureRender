#ifndef AZURE_COLOR_GRADING_LUT_GLSL
#define AZURE_COLOR_GRADING_LUT_GLSL
// Input domains: 0=linear, 1=LogC3 full logarithmic branch without the piecewise toe.
vec3 colorGradingLutCoordinate(vec3 sceneLinear,float domain){
    if(domain<.5)return clamp(sceneLinear,vec3(0),vec3(1));
    vec3 argument=max(sceneLinear*5.555555820465088+vec3(.047995999455451965),vec3(1e-30));
    return clamp(log2(argument)*.3010300099849701*.244160994887352+vec3(.3860360085964203),vec3(0),vec3(1));
}
vec3 sampleColorGradingLut(sampler2D lut,vec3 sceneLinear,float dimension,float exposureMultiplier,float domain){
    vec3 coordinate=colorGradingLutCoordinate(sceneLinear*exposureMultiplier,domain);
    float blue=coordinate.b*(dimension-1);
    float first=floor(blue),second=min(first+1,dimension-1);
    vec2 inverseSize=vec2(1/(dimension*dimension),1/dimension);
    vec2 within=coordinate.rg*(dimension-1)*inverseSize+.5*inverseSize;
    vec3 lower=textureLod(lut,within+vec2(first/dimension,0),0).rgb;
    vec3 upper=textureLod(lut,within+vec2(second/dimension,0),0).rgb;
    return mix(lower,upper,fract(blue));
}
vec3 colorGradingLinearToSrgb(vec3 linearColor){
    vec3 bounded=clamp(linearColor,vec3(0),vec3(1));
    return mix(1.055*pow(bounded,vec3(1/2.4))-.055,12.92*bounded,lessThanEqual(bounded,vec3(.0031308)));
}
#endif
