#version 450
layout(push_constant) uniform Parameters {uint flags;} parameters;
void main(){
    vec2 position=gl_VertexIndex==0?vec2(-1,-1):gl_VertexIndex==1?vec2(3,-1):vec2(-1,3);
    if((parameters.flags&2u)!=0u)position=gl_VertexIndex==0?vec2(-1,-1):gl_VertexIndex==1?vec2(1,-1):vec2(-1,1);
    gl_Position=vec4(position,0,1);
}
