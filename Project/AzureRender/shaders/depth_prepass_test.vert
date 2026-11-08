#version 450
layout(push_constant) uniform Input {vec4 color;vec4 parameters;} inputData;
void main(){
    const vec2 positions[3]=vec2[3](vec2(-1,-1),vec2(1,-1),vec2(0,1));
    vec2 p=positions[gl_VertexIndex]*inputData.parameters.z+vec2(inputData.parameters.y,0);
    gl_Position=vec4(p,inputData.parameters.x,1);
}
