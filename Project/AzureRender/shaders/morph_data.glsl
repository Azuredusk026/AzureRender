#ifndef AZURE_MORPH_DATA
#define AZURE_MORPH_DATA
#ifndef AZURE_MORPH_DATA_BINDING
#define AZURE_MORPH_DATA_BINDING 21
#define AZURE_MORPH_WEIGHT_BINDING 22
#endif
layout(std430, set=0, binding=AZURE_MORPH_DATA_BINDING) readonly buffer MorphTargetWords { uint words[]; } morphTargets;
layout(std430, set=0, binding=AZURE_MORPH_WEIGHT_BINDING) readonly buffer MorphInstanceWeights { float values[]; } morphInstances;
vec3 morphVector(uint offset) {
    return vec3(uintBitsToFloat(morphTargets.words[offset]), uintBitsToFloat(morphTargets.words[offset+1u]), uintBitsToFloat(morphTargets.words[offset+2u]));
}
// Layout version 1. Positions, normals and tangents deform before skinning.
// A zero table base is reserved for legacy inline morph0/morph1 assets.
bool applyAssetMorph(uint tableBase, uint vertexIndex, uint weightBase,
                     inout vec3 position, inout vec3 normal, inout vec3 tangent) {
    if(tableBase==0u) return false;
    uint table=tableBase+vertexIndex*3u;
    uint delta=morphTargets.words[table];
    uint weights=weightBase+morphTargets.words[table+1u];
    uint count=morphTargets.words[table+2u];
    for(uint target=0u;target<count;++target) {
        float weight=morphInstances.values[weights+target];
        if(weight==0.0) continue;
        uint base=delta+target*9u;
        position+=weight*morphVector(base);
        normal+=weight*morphVector(base+3u);
        tangent+=weight*morphVector(base+6u);
    }
    return true;
}
#endif
