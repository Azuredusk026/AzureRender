#version 450
layout(location=0) in vec3 position;
layout(location=0) out vec3 normal;
layout(push_constant) uniform Parameters { float aspect; } pc;
void main() {
    normal=position;
    gl_Position=vec4(position.x*0.8/pc.aspect,position.y*0.8,0.5+position.z*0.3,1);
}
