#ifndef AZURE_MATERIAL_SAMPLING_GLSL
#define AZURE_MATERIAL_SAMPLING_GLSL
const uint azurePixelIntegralFeature=8388608u;
// Spatial extrapolation is restricted to a locally constant shading frame.
// Varying frames use the geometry actually rasterized by temporal samples.
bool azureUniformMaterialFrame(vec3 nx,vec3 ny,vec3 tx,vec3 ty){
    return length(nx)+length(ny)+length(tx)+length(ty)<=1e-6;
}
#endif
