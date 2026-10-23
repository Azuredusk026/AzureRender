#ifndef AZURE_MOTION_PROJECTION_GLSL
#define AZURE_MOTION_PROJECTION_GLSL
bool azureFiniteClip(vec4 clip) {return !any(isnan(clip))&&!any(isinf(clip));}
bool azureInsideClip(vec3 ndc) {
    return all(greaterThanEqual(ndc,vec3(-1,-1,0)))&&all(lessThanEqual(ndc,vec3(1)));
}
// Unjittered Vulkan clip positions, positive viewport height, UV upper-left.
// XY is current-minus-previous UV. Z is previous device depth; W explicitly
// marks usable history. Invalid positions cannot leak NaNs into the resolve.
vec4 azureMotionProjection(vec4 currentClip,vec4 previousClip,bool historyValid) {
    if(!historyValid||!azureFiniteClip(currentClip)||!azureFiniteClip(previousClip)
        ||currentClip.w<=0||previousClip.w<=0)return vec4(0);
    vec3 current=currentClip.xyz/currentClip.w,previous=previousClip.xyz/previousClip.w;
    if(any(isnan(current))||any(isinf(current))||any(isnan(previous))||any(isinf(previous))
        ||!azureInsideClip(current)||!azureInsideClip(previous))return vec4(0);
    return vec4((current.xy-previous.xy)*.5,previous.z,1);
}
#endif
