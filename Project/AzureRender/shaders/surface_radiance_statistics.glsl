#if defined(AZURE_SURFACE_RADIANCE_DIAGNOSTICS)
layout(std430,binding=42) buffer SurfaceRadianceStatistics {uint values[];} surfaceRadianceStatistics;
bool azureRadianceSample(){
    if(gl_HelperInvocation)return false;
#if defined(AZURE_SURFACE_RADIANCE_SPARSE_DIAGNOSTICS)
    return all(equal(ivec2(gl_FragCoord.xy)&ivec2(31),ivec2(0)));
#else
    return true;
#endif
}
void azureRadianceCount(uint counter){
    if(!azureRadianceSample())return;
    uint previous=atomicAdd(surfaceRadianceStatistics.values[counter],1u);
    atomicOr(surfaceRadianceStatistics.values[5],2u);
    if(previous==0xffffffffu)atomicOr(surfaceRadianceStatistics.values[5],1u);
}
#if defined(AZURE_SURFACE_RADIANCE_CAPTURE_FAILURES)
void azureRadianceFailure(uint reason,uvec4 requested,uvec4 observed,uint modeBits){
    if(!azureRadianceSample())return;
    uint words=uint(surfaceRadianceStatistics.values.length());if(words<7u)return;
    uint capacity=min((words-7u)/12u,16u);if(capacity==0u)return;
    uint record=atomicAdd(surfaceRadianceStatistics.values[6],1u);
    atomicOr(surfaceRadianceStatistics.values[5],2u);
    if(record==0xffffffffu)atomicOr(surfaceRadianceStatistics.values[5],1u);
    if(record>=capacity)return;
    uint at=7u+record*12u;
    surfaceRadianceStatistics.values[at]=reason;
    surfaceRadianceStatistics.values[at+1u]=uint(gl_FragCoord.x);
    surfaceRadianceStatistics.values[at+2u]=uint(gl_FragCoord.y);
    surfaceRadianceStatistics.values[at+3u]=modeBits;
    for(uint i=0u;i<4u;++i){surfaceRadianceStatistics.values[at+4u+i]=requested[i];surfaceRadianceStatistics.values[at+8u+i]=observed[i];}
}
#endif
#endif
