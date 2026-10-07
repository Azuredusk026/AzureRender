#include "render/SkinningBatch.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace azurerender;
void check(bool condition, const char* reason) { if (!condition) throw std::runtime_error(reason); }
int main() { try {
    SkinningBatch first{337, 0, {0.2F,0.3F}, 0, 4, 1};
    const SkinningBatch next{337, 4, {0.2F,0.3F}, 337, 4, 1};
    check(appendSkinningSlice(first,next), "Independent contiguous joints must batch");
    check(first.instanceCount==2 && first.jointBase==0 && first.outputBase==0,
        "Batch preserves first slice and records both independent slices");
    for (unsigned change=0; change<5; ++change) {
        SkinningBatch batch{337, 0, {0.2F,0.3F}, 0, 4, 1};
        auto incompatible=next;
        if(change==0) incompatible.outputBase=674;
        if(change==1) incompatible.jointBase=8;
        if(change==2) incompatible.jointStride=5;
        if(change==3) incompatible.vertexCount=338;
        if(change==4) incompatible.morphWeights[0]=0.5F;
        check(!appendSkinningSlice(batch,incompatible) && batch.instanceCount==1,
            "Gaps, mesh shape and morph differences must retain separate work");
    }
    SkinningBatch full{1,0,{0,0},0,1,65535};
    check(!appendSkinningSlice(full,{1,65535,{0,0},65535,1,1}), "Vulkan portable Y limit");
    const auto maximum=std::numeric_limits<std::uint32_t>::max();
    SkinningBatch overflow{10,maximum-2,{0,0},maximum-2,10,1};
    check(!appendSkinningSlice(overflow,{10,7,{0,0},7,10,1}), "Wrapped offsets must not merge");
    SkinningBatch empty{0,0,{0,0},0,0,1};
    check(!appendSkinningSlice(empty,empty), "Empty geometry must not form a batch");
    std::cout << "Skinning slices, independent joints, morph boundaries and dispatch limits passed\n";
} catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } }
