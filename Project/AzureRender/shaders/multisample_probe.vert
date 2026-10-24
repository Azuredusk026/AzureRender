#version 450
layout(location=0) out vec2 uv;
void main(){
    vec2 points[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    vec2 point=points[gl_VertexIndex];uv=point*.5+.5;gl_Position=vec4(point,0,1);
}
