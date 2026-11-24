#pragma once
#include <array>
#include <cstdint>

namespace azurerender {
// Vulkan push-constant layout. Every Y invocation reads its own joint slice
// and writes its own vertex slice; only morph weights are shared by a batch.
struct SkinningBatch {
    std::uint32_t vertexCount = 0;
    std::uint32_t jointBase = 0;
    std::array<float,2> morphWeights{};
    std::uint32_t outputBase = 0;
    std::uint32_t jointStride = 0;
    std::uint32_t instanceCount = 1;
    std::uint32_t morphTableBase = 0;
    std::uint32_t morphWeightBase = 0;
    std::uint32_t morphWeightStride = 0;
};
static_assert(sizeof(SkinningBatch)==40);
inline bool appendSkinningSlice(SkinningBatch& batch, const SkinningBatch& slice) {
    if (!batch.vertexCount || slice.instanceCount!=1 || !batch.instanceCount
        || batch.instanceCount>=65535 || batch.vertexCount!=slice.vertexCount
        || batch.jointStride!=slice.jointStride || batch.morphTableBase!=slice.morphTableBase
        || batch.morphWeightStride!=slice.morphWeightStride
        || (!batch.morphTableBase&&batch.morphWeights!=slice.morphWeights))
        return false;
    const auto count=static_cast<std::uint64_t>(batch.instanceCount);
    if (std::uint64_t(batch.jointBase)+count*batch.jointStride!=slice.jointBase
        || std::uint64_t(batch.outputBase)+count*batch.vertexCount!=slice.outputBase
        || std::uint64_t(batch.morphWeightBase)+count*batch.morphWeightStride!=slice.morphWeightBase)
        return false;
    ++batch.instanceCount;
    return true;
}
}
