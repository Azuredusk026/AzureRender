cbuffer Frame:register(b0){float4 extent;}
SamplerState pointClamp:register(s0);
#ifdef CONFIDENCE
#define VALUE float
Texture2D<float> currentSignal:register(t0);
Texture2D<float2> motion:register(t1);
Texture2D<float> previousSignal:register(t3);
RWTexture2D<float> outputSignal:register(u0);
float transform(float v){return v;}
#else
#define VALUE float3
Texture2D<float2> motion:register(t0);
Texture2D<float3> currentSignal:register(t1);
Texture2D<float3> previousSignal:register(t3);
RWTexture2D<float3> outputSignal:register(u0);
float3 transform(float3 v){return float3(dot(v.xzy,float3(.25,.25,.5)),dot(v.xz,float2(.5,-.5))+.501960814, dot(v.xzy,float3(-.25,-.25,.5))+.501960814);}
#endif
Texture2D<float2> surface:register(t2);
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){
 if(any(id.xy>=uint2(extent.xy)))return;
 const int2 offsets[9]={int2(0,-1),int2(-1,-1),int2(1,-1),int2(-1,0),int2(0,0),int2(1,0),int2(-1,1),int2(0,1),int2(1,1)};
 VALUE n[9];
 [unroll]for(uint i=0;i<9;++i)n[i]=transform(currentSignal.SampleLevel(pointClamp,(float2(int2(id.xy)+offsets[i])+.5)*extent.zw,0));
 precise VALUE squares=n[0]*n[0];
 [unroll]for(uint j=1;j<9;++j)squares=mad(n[j],n[j],squares);
 precise VALUE sum=n[0]+n[1];
 [unroll]for(uint k=2;k<9;++k)sum+=n[k];
 precise VALUE mean=sum*(1.0/9.0), deviation=sqrt(mad(squares,1.0/9.0,-mean*mean))*1.25;
 VALUE cornerLow=min(min(n[1],n[2]),min(n[6],n[8]));
 VALUE cornerHigh=max(max(n[1],n[2]),max(n[6],n[8]));
 VALUE crossLow=min(min(min(n[7],n[5]),min(n[4],n[3])),n[0]);
 VALUE crossHigh=max(max(max(n[7],n[5]),max(n[4],n[3])),n[0]);
 precise VALUE low=min(mad(sum,1.0/9.0,-deviation),(min(cornerLow,crossLow)+crossLow)*.5);
 precise VALUE high=max(mad(sum,1.0/9.0,deviation),(max(cornerHigh,crossHigh)+crossHigh)*.5);
 precise float2 uv=(float2(id.xy)+.5)*extent.zw;
 float2 packed=motion.SampleLevel(pointClamp,uv,0),delta=abs(packed)*2-1;delta*=delta;delta*=delta;
 float2 historyUv=mad(-delta,sign(packed-.5),uv);
 float weight=surface.SampleLevel(pointClamp,uv,0).r*.97;
 VALUE previous=transform(previousSignal.SampleLevel(pointClamp,historyUv,0));
 VALUE center=(low+high)*.5,radius=(high-low)*.5,difference=previous-center;
#ifdef CONFIDENCE
 float maximum=abs(difference/radius);
#else
 float3 normalized=abs(difference/radius);float maximum=max(max(normalized.z,normalized.y),normalized.x);
#endif
 VALUE clipped=maximum>1?center+difference/maximum:previous;
#ifndef CONFIDENCE
 clipped.yz-=.501960814;
 clipped=float3(clipped.x+clipped.y-clipped.z,clipped.x+clipped.z,clipped.x-clipped.y-clipped.z);
#endif
 VALUE current=currentSignal.SampleLevel(pointClamp,uv,0);
 VALUE result=mad(weight,clipped-current,current);
#ifndef CONFIDENCE
 result=max(result,.001);
#endif
 outputSignal[id.xy]=result;
}
