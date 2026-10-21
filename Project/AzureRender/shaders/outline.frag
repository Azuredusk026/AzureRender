#version 450
#extension GL_GOOGLE_include_directive : require
#include "node_opacity.glsl"

layout(push_constant) uniform NodeOpacity { layout(offset=136) uint tile; layout(offset=140) float value; } nodeOpacity;
layout(binding=0) uniform Camera { vec4 position; vec4 forward; vec4 grid; vec4 depth; } camera;
layout(binding=26) uniform sampler2D outlineMask;
layout(location = 0) in vec3 opacityCoordinate;

layout(location = 0) out vec4 outputColor;
layout(location = 1) out vec4 outputNormal;

void main() {
    if(nodeOpacity.value<1.0) {
        ivec2 viewSize=ivec2(camera.depth.zw);
        uint columns=uint(textureSize(outlineMask,0).x/viewSize.x);
        ivec2 offset=ivec2(nodeOpacity.tile%columns,nodeOpacity.tile/columns)*viewSize;
        if(texelFetch(outlineMask,offset+ivec2(gl_FragCoord.xy),0).r>.5)discard;
    }
    if(!azureNodeOpacityCovered(opacityCoordinate,nodeOpacity.value)) discard;
    outputColor = vec4(0.018, 0.028, 0.042, 1.0);
    outputNormal = vec4(0.5, 0.5, 1.0, 0.0);
}
