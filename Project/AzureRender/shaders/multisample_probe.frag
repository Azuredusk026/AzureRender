#version 450
layout(location=0) sample in vec2 uv;
layout(binding=0) uniform sampler2D coverage;
layout(push_constant) uniform Probe {uint masked;} probe;
layout(location=0) out vec4 color;
void main(){
    vec2 position=fract(uv*32);
    if(probe.masked!=0u&&texture(coverage,uv*32).a<.5)discard;
    color=vec4(position,float(gl_SampleID)*.25,1);
}
