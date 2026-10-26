#ifndef AZURE_SPECULAR_RAMP_GLSL
#define AZURE_SPECULAR_RAMP_GLSL
#include "character_diffuse.glsl"
struct SpecularRampCoordinates {vec2 uv;float lobe;float distribution;};
float specularRampDot(vec3 a,vec3 b){
    // A one-ULP dot change is amplified by the sharp lobe; keep XY then Z.
    precise float xy=a.x*b.x+a.y*b.y;
    precise float result=xy+a.z*b.z;
    return result;
}
vec3 specularRampDirection(vec3 value,vec3 fallbackValue){
    float squared=dot(value,value);
    return squared>1e-12?value*inversesqrt(squared):fallbackValue;
}
SpecularRampCoordinates specularRampCoordinates(vec3 normal,vec3 view,vec3 camera,
    vec3 light,float roughness,float metallic,float visibility,float iridescence) {
    precise vec3 adjusted=vec3(camera.x,mix(.5,light.y,visibility),camera.z);
    precise vec3 q=specularRampDirection(adjusted,specularRampDirection(camera,vec3(0,0,1)));
    precise vec3 halfVector=specularRampDirection(light*visibility+2*q+view*(visibility+2),view);
    precise float nh=specularRampDot(normal,halfVector),nv=clamp(specularRampDot(normal,view),0,1);
    precise float alpha=max(roughness*roughness,1.0/128.0),alpha2=alpha*alpha;
    precise float difference=nh*alpha2-nh;
    precise float denominator=difference*nh+1;denominator*=denominator;
    float distribution=alpha2==denominator?1:alpha2/denominator;
    float lobe=clamp(distribution*.5/(2*nv+alpha+.0001)-.00006103515625,0,20);
    return SpecularRampCoordinates(vec2(mix(distribution*(alpha2+.0001),nv*nv,iridescence),roughness*(1-metallic)),lobe,distribution);
}
float specularRampFullDiffuse(float viewAlpha,float aoMask,float litMask,float visibility){
    return mix(viewAlpha*aoMask,litMask,visibility);
}
vec3 specularRampMain(vec3 reflectance,vec3 ramp,float lobe,float fullDiffuse,vec4 mainRadiance,float floorValue){
    return reflectance*ramp*lobe*mainRadiance.rgb*mainRadiance.a
        *(.5+.5*fullDiffuse)*mix(floorValue,1.0,fullDiffuse);
}
vec3 specularRampEnvironment(vec3 reflectance,vec3 ramp,float iridescence){
    return mix(reflectance,reflectance*ramp,iridescence);
}
#endif
