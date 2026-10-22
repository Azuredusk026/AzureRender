#ifndef AZURE_CHARACTER_DIFFUSE
#define AZURE_CHARACTER_DIFFUSE 1

// Pure linear-color response. Illumination, specular and emission are owned
// by the caller. Visibility inputs are explicit, independently supplied values.
struct CharacterDiffuseResponse { vec3 color; float litMask; float aoMask; };
struct CharacterDiffuseIllumination { vec3 ambient; vec3 direct; };
// Native skin irradiance is evaluated on the world horizontal plane.
// The nonzero vertical component also defines vertical and zero inputs.
vec3 worldHorizontalDiffuseNormal(vec3 normal) {
    return normalize(vec3(normal.x,6.103515625e-5,normal.z));
}
// The frame origin is explicitly host-authored. Native reserved skin-frame
// identity is a separate binding contract. Geometry and SDF blue normals
// retain distinct consumers.
vec3 faceFrameRadialHorizontalDiffuseNormal(vec3 worldPosition,vec3 frameOrigin,
    vec3 geometryNormal,float geometryMix) {
    vec3 radial=worldHorizontalDiffuseNormal(worldPosition-frameOrigin);
    vec3 geometry=worldHorizontalDiffuseNormal(geometryNormal);
    vec3 blended=mix(radial,geometry,geometryMix);
    return dot(blended,blended)>1e-12?normalize(blended):geometry;
}
float characterDiffuseLuma(vec3 color) {
    return dot(color,vec3(.21267290413379669,.7151522040367126,.07217500358819962));
}
vec3 characterDiffuseSaturation(vec3 color,float amount) {
    return mix(vec3(characterDiffuseLuma(color)),color,amount);
}
// Captured main/ambient composition. Colors and source policy belong to the
// host. Keeping the terms separate gives ambient/direct isolation exact sums.
CharacterDiffuseIllumination characterDiffuseIllumination(vec3 normal,vec3 indirectRadiance,
    vec4 mainRadiance,vec4 directionBias,vec4 gainFloor,vec4 composition,
    float indoorMix,float ambientOcclusion,float litMask,float visibility) {
    float directional=clamp(dot(normal,directionBias.xyz)+directionBias.w,0.0,1.0)*gainFloor.x+gainFloor.y;
    vec3 incident=directional*mix(indirectRadiance,vec3(1),litMask*composition.z);
    float shadowWeight=mix(min(.65+.35*ambientOcclusion,1.5),clamp(ambientOcclusion,1.25,1.75),indoorMix);
    vec3 shadow=incident*shadowWeight*composition.y;
    vec3 ambient=incident*clamp(ambientOcclusion,0.0,1.5)*mix(vec3(1),mainRadiance.rgb,composition.w)*composition.x;
    vec3 direct=characterDiffuseSaturation(mainRadiance.rgb*mainRadiance.a,litMask)*composition.x;
    return CharacterDiffuseIllumination(mix(shadow,ambient,visibility),direct*visibility);
}
#define AZURE_CHARACTER_WEIGHTED_DIFFUSE 1
// Material adapters own visibility and mask interpretation. Shared color
// response consumes their explicit weights, including captured hair inputs.
CharacterDiffuseResponse characterDiffuseWeightedResponse(vec3 litAlbedo,vec3 shadowAlbedo,
    float metallic,float shadowScale,vec4 ramp,float litMask,float aoMask,
    float shadowMix,float sceneVisibility) {
    vec3 lit=litAlbedo*(.96*(1.0-metallic));
    vec3 shadow=shadowAlbedo*(.96*(1.0-metallic)*shadowScale);
    vec3 shadowColor=mix(characterDiffuseSaturation(.65*shadow,1.2),shadow,shadowMix);
    vec3 untinted=mix(shadowColor,lit,litMask);
    float chroma=max(max(ramp.r,ramp.g),ramp.b)-min(min(ramp.r,ramp.g),ramp.b);
    vec3 tinted=untinted*(vec3(1.0-chroma)+ramp.rgb*chroma);
    vec3 indoor=mix(shadow,characterDiffuseSaturation(lit,1.2),aoMask);
    vec3 outdoor=tinted*clamp(characterDiffuseLuma(untinted)/max(characterDiffuseLuma(tinted),.001),0.0,1.5);
    return CharacterDiffuseResponse(mix(indoor,outdoor,sceneVisibility),litMask,aoMask);
}
CharacterDiffuseResponse characterDiffuseResponse(vec3 litAlbedo,vec3 shadowAlbedo,
    float metallic,float shadowScale,vec4 ramp,float baseAo,vec2 shadowMask,
    float cameraFacing,float characterVisibility,float sceneVisibility) {
    float cameraWeight=clamp((cameraFacing-.75)/(.25-.75),0.0,1.0);
    cameraWeight=cameraWeight*cameraWeight*(3.0-2.0*cameraWeight);
    float mask=max(shadowMask.x,shadowMask.y*cameraWeight);
    float visibility=mix(1.0,characterVisibility,mask);
    float litMask=min(min(visibility,baseAo),ramp.a);
    float aoMask=baseAo*visibility;
    float shadowMix=clamp(baseAo*((1.0-shadowMask.x)+visibility*shadowMask.x)+ramp.a,0.0,1.0);
    return characterDiffuseWeightedResponse(litAlbedo,shadowAlbedo,metallic,shadowScale,
        ramp,litMask,aoMask,shadowMix,sceneVisibility);
}
#endif
