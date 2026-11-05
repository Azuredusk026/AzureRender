#ifndef AZURE_INSTANCE_DATA
#define AZURE_INSTANCE_DATA 1
#define AZURE_INSTANCE_DATA_LAYOUT_VERSION 2
const uint AZURE_INSTANCE_MATERIAL_RIM_ORIGIN = 1U;
struct InstanceTransforms {
    mat4 model;
    mat4 modelViewProjection;
    mat4 cascadeLightModelViewProjection[4];
    uvec4 meta;
    vec4 faceLight;
    vec2 morph;
    uint objectShadowIndex;
    uint instanceFlags;
    mat4 faceFrame;
};

mat4 capturedCameraMvp(mat4 model) {
    mat4 sourceProjection=mat4(-1.78402245,-2.15062152e-11,5.35182267e-15,-5.37656808e-10,-1.5473924e-18,-3.17134237,-1.25528786e-07,0.0126109188,9.59267776e-10,-0.0399967209,9.95318533e-06,-0.999920607,0.0,0.0,0.100000992,0.0);
    mat4 relative=model;
    for(int c=0;c<4;++c)relative[c].x=-relative[c].x;
    relative[3].xyz-=vec3(0.0,0.880004883,3.29998779);
    mat4 value=sourceProjection*relative;
    for(int c=0;c<4;++c){value[c].xy+=vec2(0.000195312503,0.000231481506)*value[c].w;value[c].z=value[c].w-value[c].z;}
    return value;
}
#endif
