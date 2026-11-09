#version 450
layout(push_constant) uniform Parameters { uint seed; } parameters;
layout(location=0) flat out uint key;
void main(){
    uint vertex=uint(gl_VertexIndex)%3u;
    gl_Position=vec4(vertex==0u?vec2(-1,-1):vertex==1u?vec2(3,-1):vec2(-1,3),0,1);
    key=(uint(gl_VertexIndex)/3u+parameters.seed)%128u;
}
