#ifndef AZURE_LIGHT_ATTENUATION
#define AZURE_LIGHT_ATTENUATION
// Explicit models share distance/range semantics across raster and tracing.
// The inverse-square model has a one-square-metre denominator offset.
float localLightDistanceWeight(float distanceSquared,float radius,float model,float exponent) {
    float squaredDistance=max(distanceSquared,0.0);
    float safeRadius=max(radius,.001);
    float normalizedSquared=squaredDistance/(safeRadius*safeRadius);
    if(model>1.5) return pow(max(1.0-clamp(normalizedSquared,0.0,1.0),0.0),exponent);
    if(model>.5) {
        float window=max(1.0-normalizedSquared*normalizedSquared,0.0);
        return window*window/(squaredDistance+1.0);
    }
    float window=max(1.0-sqrt(squaredDistance)/safeRadius,0.0);
    return window*window/(1.0+squaredDistance*.12);
}
#endif
