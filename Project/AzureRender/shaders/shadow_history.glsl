#ifndef AZURE_SHADOW_HISTORY_GLSL
#define AZURE_SHADOW_HISTORY_GLSL
layout(location=15,component=1) flat in uvec2 shadowHistorySource;
layout(std430,binding=45) readonly buffer ShadowHistoryPreviousMain { uvec2 records[]; } shadowHistoryPreviousMain;
layout(std430,binding=46) coherent buffer ShadowHistoryCurrentMain { uvec2 records[]; } shadowHistoryCurrentMain;
layout(std430,binding=49) coherent buffer ShadowHistoryInputs {
    uvec4 extentAndReuse;
    vec4 dirty[4];
    uvec4 statistics;
    uvec2 ranges[];
} shadowHistoryInputs;
bool azureShadowHistoryCascadeClean(int cascade){
    vec4 rect=shadowHistoryInputs.dirty[cascade];
    if(rect.x>=rect.z||rect.y>=rect.w)return true;
    vec4 projected=shadowPositions[cascade];
    if(any(isnan(projected))||any(isinf(projected))||projected.w<=0)return false;
    vec2 pixel=(projected.xy/projected.w*.5+.5)*vec2(textureSize(AZ_TEX_SHADOW,0)/2)-.5;
    float reach=ceil(clamp(camera.renderingParameters.w,1.0,16.0)+.5)+2.0;
    return !(pixel.x+reach>=rect.x&&pixel.x-reach<rect.z&&pixel.y+reach>=rect.y&&pixel.y-reach<rect.w);
}
bool azureShadowHistoryClean(){
    float depth=dot(worldPosition-camera.cameraPosition.xyz,camera.cameraForward.xyz);
    int cascade=depth<=camera.cascadeSplits.x?0:(depth<=camera.cascadeSplits.y?1:(depth<=camera.cascadeSplits.z?2:3));
    if(!azureShadowHistoryCascadeClean(cascade))return false;
    if(cascade<3){float previous=cascade==0?camera.clusterDepth.x:camera.cascadeSplits[cascade-1];
        float blend=smoothstep(mix(previous,camera.cascadeSplits[cascade],.90),camera.cascadeSplits[cascade],depth);
        if(blend>0.0&&!azureShadowHistoryCascadeClean(cascade+1))return false;}
    return true;
}
float azureSampleShadowHistory(float normalDotLight){
    uvec2 pixel=uvec2(gl_FragCoord.xy);
    bool sampled=!gl_HelperInvocation&&(pixel.x&31u)==0u&&(pixel.y&31u)==0u;
    if(sampled)atomicAdd(shadowHistoryInputs.statistics.x,1u);
    uint instance=shadowHistorySource.x,first=shadowHistorySource.y,triangle=uint(gl_PrimitiveID);
    if(instance>=shadowHistoryInputs.extentAndReuse.w||first==0xffffffffu)return sampleShadowMap(normalDotLight);
    uvec2 range=shadowHistoryInputs.ranges[instance];
    if(range.x==0xffffffffu||first>=range.y||range.y-first<3u||triangle>(range.y-first-3u)/3u)return sampleShadowMap(normalDotLight);
    uint key=range.x+first+triangle*3u;
    if(any(greaterThanEqual(pixel,shadowHistoryInputs.extentAndReuse.xy)))return sampleShadowMap(normalDotLight);
    if(sampled)atomicAdd(shadowHistoryInputs.statistics.y,1u);
    uint offset=pixel.y*shadowHistoryInputs.extentAndReuse.x+pixel.x;
    bool reusable=(shadowHistoryInputs.extentAndReuse.z&1u)!=0u;
    uvec2 previous=reusable?shadowHistoryPreviousMain.records[offset]:uvec2(0xffffffffu);
    float visibility;
    bool hit=reusable&&previous.x==key&&azureShadowHistoryClean();
    if(hit){visibility=uintBitsToFloat(previous.y);if(sampled)atomicAdd(shadowHistoryInputs.statistics.z,1u);}
    else visibility=sampleShadowMap(normalDotLight);
    if(!gl_HelperInvocation){
        if(atomicCompSwap(shadowHistoryCurrentMain.records[offset].x,0xffffffffu,key)==0xffffffffu)
            shadowHistoryCurrentMain.records[offset].y=floatBitsToUint(visibility);
    }
    return visibility;
}
#endif
