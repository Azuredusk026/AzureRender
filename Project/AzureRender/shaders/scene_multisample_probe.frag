#version 450
layout(location=0) sample in vec2 uv;
layout(push_constant) uniform Probe {uint phase;uint linearDepth;} probe;
layout(location=0) out vec4 color;
layout(location=1) out vec4 secondary;
void main(){
    vec2 samplePosition=fract(uv*32);
    color=vec4(samplePosition.x+2*probe.phase,samplePosition.y,float(gl_SampleID)*.25,1);
    float value=probe.linearDepth!=0?.3+.2*gl_SampleID:float(gl_SampleID%2==0);
    secondary=vec4(value);gl_FragDepth=.2+.1*gl_SampleID+.3*probe.phase;
}
