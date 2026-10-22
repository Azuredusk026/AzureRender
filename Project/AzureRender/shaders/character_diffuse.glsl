#ifndef AZURE_CHARACTER_DIFFUSE
#define AZURE_CHARACTER_DIFFUSE 1

// Pure linear-color response. Illumination, specular and emission are owned
// by the caller. Visibility inputs are explicit, independently supplied values.
struct CharacterDiffuseResponse { vec3 color; float litMask; float aoMask; };
// Native skin irradiance is evaluated on the world horizontal plane.
// The nonzero vertical component also defines vertical and zero inputs.
vec3 worldHorizontalDiffuseNormal(vec3 normal) {
    return normalize(vec3(normal.x,6.103515625e-5,normal.z));
}
float characterDiffuseLuma(vec3 color) {
    return dot(color,vec3(.21267290413379669,.7151522040367126,.07217500358819962));
}
vec3 characterDiffuseSaturation(vec3 color,float amount) {
    return mix(vec3(characterDiffuseLuma(color)),color,amount);
}
CharacterDiffuseResponse characterDiffuseResponse(vec3 litAlbedo,vec3 shadowAlbedo,
    float metallic,float shadowScale,vec4 ramp,float baseAo,vec2 shadowMask,
    float cameraFacing,float characterVisibility,float sceneVisibility) {
    vec3 lit=litAlbedo*(.96*(1.0-metallic));
    vec3 shadow=shadowAlbedo*(.96*(1.0-metallic)*shadowScale);
    float cameraWeight=clamp((cameraFacing-.75)/(.25-.75),0.0,1.0);
    cameraWeight=cameraWeight*cameraWeight*(3.0-2.0*cameraWeight);
    float mask=max(shadowMask.x,shadowMask.y*cameraWeight);
    float visibility=mix(1.0,characterVisibility,mask);
    float litMask=min(min(visibility,baseAo),ramp.a);
    float aoMask=baseAo*visibility;
    float shadowMix=clamp(baseAo*((1.0-shadowMask.x)+visibility*shadowMask.x)+ramp.a,0.0,1.0);
    vec3 shadowColor=mix(characterDiffuseSaturation(.65*shadow,1.2),shadow,shadowMix);
    vec3 untinted=mix(shadowColor,lit,litMask);
    float chroma=max(max(ramp.r,ramp.g),ramp.b)-min(min(ramp.r,ramp.g),ramp.b);
    vec3 tinted=untinted*(vec3(1.0-chroma)+ramp.rgb*chroma);
    vec3 indoor=mix(shadow,characterDiffuseSaturation(lit,1.2),aoMask);
    vec3 outdoor=tinted*clamp(characterDiffuseLuma(untinted)/max(characterDiffuseLuma(tinted),.001),0.0,1.5);
    return CharacterDiffuseResponse(mix(indoor,outdoor,sceneVisibility),litMask,aoMask);
}
#endif
