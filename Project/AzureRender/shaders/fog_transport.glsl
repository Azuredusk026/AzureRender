#ifndef AZURE_FOG_TRANSPORT_GLSL
#define AZURE_FOG_TRANSPORT_GLSL
struct AzureFogTransport { vec3 scattering;vec3 transmittance; };
AzureFogTransport azureFogVolumeTransport(vec4 value) {
    return AzureFogTransport(value.rgb,vec3(value.a));
}
vec3 azureApplyFogTransport(AzureFogTransport fog,vec3 linearColor,float scatteringWeight) {
    return linearColor*fog.transmittance+scatteringWeight*fog.scattering;
}
#endif
