#version 450
#extension GL_GOOGLE_include_directive : require
#include "fragment_sampling.glsl"
#if defined(AZURE_GEOMETRY_SAMPLES)
#define AZURE_MASK_TYPE sampler2DMS
#define AZURE_MASK_SIZE textureSize(outlineMask)
#define AZURE_MASK_SAMPLE gl_SampleID
#else
#define AZURE_MASK_TYPE sampler2D
#define AZURE_MASK_SIZE textureSize(outlineMask,0)
#define AZURE_MASK_SAMPLE 0
#endif
#if defined(AZURE_VIEW_SCALED_OUTLINE)
#include "outline_surface_fragment.glsl"
layout(binding=0) uniform Camera { vec4 position; vec4 forward; vec4 grid; vec4 depth; } camera;
layout(binding=26) uniform AZURE_MASK_TYPE outlineMask;
layout(location=0) out vec4 outputColor;
layout(location=1) out vec4 outputNormal;
void main() {
    if(!shellCovered())discard;
    if(shell.tile!=0xffffffffu) {
        ivec2 viewSize=ivec2(camera.depth.zw);
        uint columns=uint(AZURE_MASK_SIZE.x/viewSize.x);
        ivec2 offset=ivec2(shell.tile%columns,shell.tile/columns)*viewSize;
        if(texelFetch(outlineMask,offset+ivec2(AZURE_FRAGMENT_COORD.xy),AZURE_MASK_SAMPLE).r>.5)discard;
    }
    outputColor=shellColor;
    outputNormal=vec4(.5,.5,1,0);
}
#else
#include "node_opacity.glsl"

layout(push_constant) uniform NodeOpacity { layout(offset=136) uint tile; layout(offset=140) float value; } nodeOpacity;
layout(binding=0) uniform Camera { vec4 position; vec4 forward; vec4 grid; vec4 depth; } camera;
layout(binding=26) uniform AZURE_MASK_TYPE outlineMask;
layout(location = 0) AZURE_SAMPLE in vec3 opacityCoordinate;

layout(location = 0) out vec4 outputColor;
layout(location = 1) out vec4 outputNormal;

void main() {
    if(nodeOpacity.tile!=0xffffffffu) {
        ivec2 viewSize=ivec2(camera.depth.zw);
        uint columns=uint(AZURE_MASK_SIZE.x/viewSize.x);
        ivec2 offset=ivec2(nodeOpacity.tile%columns,nodeOpacity.tile/columns)*viewSize;
        if(texelFetch(outlineMask,offset+ivec2(AZURE_FRAGMENT_COORD.xy),AZURE_MASK_SAMPLE).r>.5)discard;
    }
    if(!azureNodeOpacityCovered(opacityCoordinate,nodeOpacity.value)) discard;
    outputColor = vec4(0.018, 0.028, 0.042, 1.0);
    outputNormal = vec4(0.5, 0.5, 1.0, 0.0);
}
#endif
