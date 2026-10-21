#ifndef AZURE_LIGHT_ATTENUATION
#define AZURE_LIGHT_ATTENUATION
float lightInfluenceBoxWeight(vec3 position,vec4 row0,vec4 row1,vec4 row2,vec4 parameters) {
    if(parameters.x<.5)return 1.0;
    vec4 point=vec4(position,1);
    vec3 coordinates=abs(vec3(dot(row0,point),dot(row1,point),dot(row2,point)));
    float edge=max(max(coordinates.x,coordinates.y),coordinates.z);
    if(parameters.y==1.0)return edge<1.0?1.0:0.0;
    float inner=(1.0+parameters.y)*.5;
    float weight=1.0-clamp((edge-inner)/(1.0-inner),0.0,1.0);
    return weight*weight;
}
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
