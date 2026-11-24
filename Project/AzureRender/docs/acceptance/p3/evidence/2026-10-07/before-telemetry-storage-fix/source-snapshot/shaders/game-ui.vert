#version 450
layout(location=0) in vec2 position;
layout(location=1) in vec4 colour;
layout(location=2) in vec2 texCoord;
layout(push_constant) uniform Ui { mat4 transform; vec2 translation; vec2 extent; } ui;
layout(location=0) out vec4 vertexColour;
layout(location=1) out vec2 uv;
void main(){vec4 point=ui.transform*vec4(position+ui.translation,0,1);gl_Position=vec4(2*point.xy/ui.extent-vec2(1),0,1);vertexColour=colour;uv=texCoord;}
