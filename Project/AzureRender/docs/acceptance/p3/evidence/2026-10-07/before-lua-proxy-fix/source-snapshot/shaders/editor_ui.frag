#version 450
layout(location=0) out vec4 fColor;
layout(set=0,binding=0) uniform texture2D imageTexture;
layout(set=1,binding=0) uniform sampler imageSampler;
layout(location=0) in struct { vec4 Color; vec2 UV; } In;
vec3 srgbToLinear(vec3 value) {
    return mix(value/12.92,pow((value+.055)/1.055,vec3(2.4)),greaterThan(value,vec3(.04045)));
}
void main() {
    vec4 color=In.Color;
#ifdef AZURE_EDITOR_SRGB
    color.rgb=srgbToLinear(color.rgb);
#endif
    fColor=color*texture(sampler2D(imageTexture,imageSampler),In.UV);
}
