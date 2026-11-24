#version 450
layout(set=0,binding=0) uniform sampler2D image;
layout(location=0) in vec4 vertexColour;
layout(location=1) in vec2 uv;
layout(location=0) out vec4 outColour;
void main(){vec4 c=texture(image,uv)*vertexColour;vec3 straight=c.a>0?c.rgb/c.a:vec3(0);outColour=vec4(pow(max(straight,vec3(0)),vec3(2.2))*c.a,c.a);}
