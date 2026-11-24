cbuffer Factors:register(b0){float4 value[4];};
RWStructuredBuffer<float4> output:register(u0);
[numthreads(1,1,1)]void main(uint3 thread:SV_DispatchThreadID){
 output[0]=mad(value[2],value[3],value[0]*value[1]);
}
