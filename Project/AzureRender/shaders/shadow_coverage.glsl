#ifndef AZURE_SHADOW_COVERAGE
#define AZURE_SHADOW_COVERAGE 1
float shadowSqrtAlphaCoverage(float alpha,float opacity) {
    return sqrt(clamp(alpha,0.0,1.0))*clamp(opacity,0.0,1.0);
}
float shadowPixelNoise(vec2 position) {
    vec2 frequency=vec2(uintBitsToFloat(0x3d897143u),uintBitsToFloat(0x3bbf4590u));
    precise float coordinate=position.x*frequency.x+position.y*frequency.y;
    precise float shaped=uintBitsToFloat(0x4253ee82u)*fract(coordinate);
    return fract(shaped);
}
#endif
