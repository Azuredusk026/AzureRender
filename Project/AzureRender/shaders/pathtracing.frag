#version 450
layout(std140,binding=0) uniform Params {vec4 origin,right,up,forward,skyColor,sky;uvec4 counts,image;} p;
layout(std430,binding=5) readonly buffer History{vec4 history[];};
layout(location=0) out vec4 color;
layout(location=1) out vec4 normal;
void main(){uvec2 pixel=min(uvec2(gl_FragCoord.xy),p.image.xy-1u);color=history[pixel.y*p.image.x+pixel.x];normal=vec4(.5,.5,1,0);}
