#ifndef AZURE_OUTLINE_PROJECTION_DATA
#define AZURE_OUTLINE_PROJECTION_DATA
#if defined(AZURE_BINDLESS)
#extension GL_EXT_nonuniform_qualifier : require
layout(binding=31) uniform sampler2D projectionMasks[];
#define PROJECTION_MASK projectionMasks[nonuniformEXT(morphWeights.materialIndex)]
#else
layout(binding=31) uniform sampler2D projectionMask;
#define PROJECTION_MASK projectionMask
#endif
layout(std430,binding=30) readonly buffer OutlineProjectionWords { uint words[]; } outlineData;
vec4 outlineRecordVector(uint offset) {
    return vec4(uintBitsToFloat(outlineData.words[offset]),uintBitsToFloat(outlineData.words[offset+1u]),
        uintBitsToFloat(outlineData.words[offset+2u]),uintBitsToFloat(outlineData.words[offset+3u]));
}
#endif
