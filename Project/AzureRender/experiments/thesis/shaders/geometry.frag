#version 450
layout(location=0) in vec3 normal;
layout(location=0) out vec4 albedo;
layout(location=1) out vec4 normalDepth;
void main() {
    albedo=vec4(0.85,0.38,0.12,1);
    normalDepth=vec4(normalize(normal),gl_FragCoord.z);
}
