#if defined(AZURE_SURFACE_RADIANCE_DIAGNOSTICS)
layout(std430,binding=42) buffer SurfaceRadianceStatistics {uint values[6];} surfaceRadianceStatistics;
void azureRadianceCount(uint counter){
    if(gl_HelperInvocation)return;
    uint previous=atomicAdd(surfaceRadianceStatistics.values[counter],1u);
    atomicOr(surfaceRadianceStatistics.values[5],2u);
    if(previous==0xffffffffu)atomicOr(surfaceRadianceStatistics.values[5],1u);
}
#endif
