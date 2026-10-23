#ifndef AZURE_LIGHT_ATTENUATION
#define AZURE_LIGHT_ATTENUATION
#define AZURE_LINE_LIGHT_GEOMETRY 1
#define AZURE_CHARACTER_SCREEN_RIM 1
vec3 characterScreenRimResponse(vec3 base,float tint,vec3 normal,vec3 light,vec3 cameraAxis,
    vec3 radial,float sceneDepth,float surfaceDepth) {
    vec3 planar=light-cameraAxis*dot(light,cameraAxis);
    float squared=dot(planar,planar);
    planar=squared>1e-12?planar*inversesqrt(squared):vec3(0);
    float facing=clamp(dot(normal,planar),0.0,1.0);
    float depth=clamp((sceneDepth-surfaceDepth-.1)*10.0,0.0,1.0);
    float visibility=clamp(dot(radial,light)+1.0,0.0,1.0);
    return (.5+tint*(base*.96-.5))*(depth*depth*(3.0-2.0*depth)*visibility*facing);
}
float characterColorBlendWeight(float attenuation,float normalDotLight,vec2 weights) {
    float facing=clamp(normalDotLight+.5,0.0,1.0);
    float smoothFacing=facing*facing*(3.0-2.0*facing);
    return attenuation*weights.x*mix(1.0,smoothFacing,weights.y);
}
struct LineLightGeometry { vec3 direction;float factor; };
LineLightGeometry lineLightGeometry(vec3 incoming,vec3 axis,float length) {
    float squared=dot(incoming,incoming);
    vec3 centre=squared>1e-12?incoming*inversesqrt(squared):vec3(0,0,1);
    if(length<=0.0)return LineLightGeometry(centre,1.0/(1.0+squared));
    vec3 perpendicular=incoming-axis*dot(incoming,axis);
    float perpendicularSquared=dot(perpendicular,perpendicular);
    vec3 direction=perpendicularSquared>1e-12?perpendicular*inversesqrt(perpendicularSquared):centre;
    vec3 a=incoming-axis*(length*.5),b=incoming+axis*(length*.5);
    float lengthA=max(sqrt(dot(a,a)),1e-6),lengthB=max(sqrt(dot(b,b)),1e-6);
    float numerator=clamp(.5*(dot(direction,a)/lengthA+dot(direction,b)/lengthB),0.0,1.0);
    float denominator=max(.5*(lengthA*lengthB+dot(a,b))+1.0,1.0);
    return LineLightGeometry(direction,numerator/denominator);
}
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
