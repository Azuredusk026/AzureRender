#version 450
layout(constant_id=0) const float vertexValue=0.25;
layout(location=0) flat out float value;
void main(){
    vec2 points[3]=vec2[](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    gl_Position=vec4(points[gl_VertexIndex],0,1);value=vertexValue;
}
