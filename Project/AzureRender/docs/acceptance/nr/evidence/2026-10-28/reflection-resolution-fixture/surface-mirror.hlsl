cbuffer Camera:register(b0){float4 camera[28];}
cbuffer View:register(b1){float4 view[3];}
cbuffer Settings:register(b2){float4 settings[7];}
Texture2D<float> fullDepth:register(t0);
Texture2D<float> halfDepth:register(t1);
Texture2D<float4> materialNormal:register(t2);
Texture2D<float4> reflectionNormal:register(t3);
SamplerState pointClamp:register(s0);
RWTexture2D<float4> outputSurface:register(u4);
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){
 if(any(id.xy>=uint2(settings[0].xy)))return;
 precise float2 uv=(float2(id.xy)+.5)*settings[0].zw;
 float d=halfDepth.SampleLevel(pointClamp,uv,0);
 float4 value=0;
 if(d!=0){
  float fd=fullDepth.SampleLevel(pointClamp,uv,0);
  float2 ndc=uv*2-1;
  float4 base=-ndc.y*camera[25]+ndc.x*camera[24];
  float4 fullPosition=mad(camera[26],fd,base)+camera[27];
  float4 halfPosition=mad(camera[26],d,base)+camera[27];
  float amount=min(length(fullPosition.xyz/fullPosition.w-halfPosition.xyz/halfPosition.w)*10,1);
  float3 a=materialNormal.SampleLevel(pointClamp,uv,0).xyz;
  float3 b=reflectionNormal.SampleLevel(pointClamp,uv,0).xyz;
  float3 surface=mad(amount,b.zxy-a.zxy,a.zxy);
  float2 encoded=surface.yz*2-1;
  float3 n=float3(encoded.x,1-dot(abs(encoded),float2(1,1)),encoded.y);
  if(n.y<0)n.xz=(1-abs(encoded.yx))*float2(encoded.x>=0?1:-1,encoded.y>=0?1:-1);
  n=normalize(n);
  float2 oct=n.xz/dot(abs(n),float3(1,1,1));
  if(n.y<=0)oct=(1-abs(oct.yx))*float2(oct.x>=0?1:-1,oct.y>=0?1:-1);
  value=float4(oct*.5+.5,surface.x,0);
 }
 outputSurface[id.xy]=value;
}
