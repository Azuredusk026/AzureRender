#ifndef AZURE_BEAUTY_COMPOSITION_GLSL
#define AZURE_BEAUTY_COMPOSITION_GLSL
// Ordered contribution boundaries; input evaluation remains caller-owned.
vec3 azureBeautySum(vec3 a,vec3 b,vec3 c,vec3 d,vec3 e,vec3 f,vec3 g,vec3 h){
    precise vec3 result=a+b+c+d+e+f+g+h;return result;
}
vec3 azureBeautyReplace(vec3 total,vec3 added,vec3 removed){
    precise vec3 result=total+(added-removed);return result;
}
#endif
