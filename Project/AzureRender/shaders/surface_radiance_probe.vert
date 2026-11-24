#version 450
layout(push_constant) uniform Probe {float depth;float value;uint firstIndex;uint material;float coverage;float mismatchFromX;} probe;
layout(location=0) flat out uvec2 source;
void main(){
    vec2 points[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    gl_Position=vec4((points[gl_VertexIndex%3]+1)*probe.coverage-1,probe.depth,1);
    source=uvec2(uint(gl_InstanceIndex),probe.firstIndex);
}
