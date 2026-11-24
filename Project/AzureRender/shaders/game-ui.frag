#version 450
layout(set=0,binding=0) uniform sampler2D image;
layout(location=0) in vec4 vertexColour;
layout(location=1) in vec2 uv;
layout(location=0) out vec4 outColour;

vec3 srgbToLinear(vec3 value) {
    return mix(value / 12.92, pow((value + .055) / 1.055, vec3(2.4)),
               greaterThan(value, vec3(.04045)));
}

void main() {
    vec4 colour = texture(image, uv) * vertexColour;
    vec3 straight = colour.a > 0 ? max(colour.rgb / colour.a, vec3(0)) : vec3(0);
    outColour = vec4(srgbToLinear(straight) * colour.a, colour.a);
}
