#version 450
layout(binding=0) uniform sampler2D sourceColor;
layout(location=0) out vec4 outputColor;
void main(){outputColor=texelFetch(sourceColor,ivec2(gl_FragCoord.xy),0);}
