RWTexture2D<float4> outputSurface:register(u4);
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){if(id.x<1280&&id.y<720)outputSurface[id.xy]=0;}
