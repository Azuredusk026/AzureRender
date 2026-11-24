cbuffer Settings:register(b0){float4 rows[6];};
Texture2D<float> confidencePyramid:register(t0);
Texture2D<float2> surface:register(t1);
SamplerState pointSampler:register(s0);
SamplerState linearSampler:register(s1);
RWTexture2D<unorm float> outputConfidence:register(u0);
[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){
    float2 uv=(float2(p.xy)+.5)*rows[0].zw;
    float lod=floor(surface.SampleLevel(pointSampler,uv,0).y*rows[5].y);
    outputConfidence[p.xy]=confidencePyramid.SampleLevel(linearSampler,uv,lod);
}
