cbuffer View:register(b0){float4 viewRows[3];};
cbuffer Settings:register(b1){float4 settingRows[10];};
Texture2D<float> depthPyramid:register(t0);
SamplerState depthSampler:register(s0);
RWTexture2D<float3> sourceColor:register(u0);
RWTexture2D<float3> targetColor:register(u1);
RWTexture2D<unorm float> sourceConfidence:register(u2);
RWTexture2D<unorm float> targetConfidence:register(u3);
float linearDepth(float value){return 1.0/mad(viewRows[2].z,value,viewRows[2].w);}
[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){
    float lod=settingRows[5].x;
    float centerDepth=linearDepth(depthPyramid.SampleLevel(depthSampler,(float2(p.xy)+.5)*settingRows[9].zw,lod));
    int2 base=int2(p.xy)*2;float3 sum=0;float confidence=0,weights=0;
    const float kernel[4]={1,3,3,1};
    [loop]for(int i=-1;i<=2;++i)[loop]for(int j=-1;j<=2;++j){
        int2 q=base+int2(i,j);
        float depth=linearDepth(depthPyramid.SampleLevel(depthSampler,(float2(q)+.5)*settingRows[8].zw,lod-1));
        float distanceWeight=exp2(abs(depth-centerDepth)*(-0.014426950365304947));
        float spatialWeight=kernel[i+1]*kernel[j+1]/64.0;
        float weight=distanceWeight*spatialWeight;
        weights=mad(spatialWeight,distanceWeight,weights);
        confidence=mad(sourceConfidence[q],weight,confidence);
        sum=mad(sourceColor[q],weight,sum);
    }
    targetColor[p.xy]=sum/max(weights,.01);
    targetConfidence[p.xy]=confidence/max(weights,.01);
}
