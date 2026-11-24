#ifndef AZURE_REFLECTION_LOD_GLSL
#define AZURE_REFLECTION_LOD_GLSL
// A positive minimum roughness selects the explicitly authored log2 mapping.
float reflectionProbeLod(float roughness,float lastMip,vec3 mapping){
    float r=clamp(roughness,0.0,1.0);
    return mapping.z>0.0?clamp(mapping.x*log2(max(r,mapping.z))+mapping.y,0.0,lastMip):r*lastMip;
}
#endif
