RWTexture2D<float3> outputSignal:register(u0);
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){if(id.x<1280&&id.y<720)outputSignal[id.xy]=0;}
