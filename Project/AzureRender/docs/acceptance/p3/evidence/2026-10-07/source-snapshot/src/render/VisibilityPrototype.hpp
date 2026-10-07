#pragma once
#include "render/GpuCulling.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace azurerender {
struct VisibilityFrameKey {
    std::uint64_t scene = 0, camera = 0, frame = 0;
    bool operator==(const VisibilityFrameKey& other) const {
        return scene==other.scene && camera==other.camera && frame==other.frame;
    }
};
struct GpuSurfaceIdentity {
    std::uint32_t instance=0,primitive=0,material=0,visible=0;
};
static_assert(sizeof(GpuSurfaceIdentity)==16,"GPU surface identity std430 stride");
// Immutable frame mapping. GPU results may be queried only for the exact
// producing scene, camera and frame. No temporal occlusion is inferred here.
class VisibilityPrototype {
public:
    static bool supported(const VkPhysicalDeviceLimits& limits) {
        return limits.maxPerStageDescriptorStorageBuffers>=5 && limits.maxDescriptorSetStorageBuffers>=5 &&
            limits.maxPerStageResources>=5 && limits.maxPushConstantsSize>=sizeof(GpuCullParameters);
    }
    static void validateSources(const std::vector<GpuCullBounds>&,
        const std::vector<VkDrawIndexedIndirectCommand>&,const std::vector<GpuSurfaceIdentity>&);
    VisibilityPrototype(VisibilityFrameKey,
        std::vector<GpuCullBounds>,std::vector<VkDrawIndexedIndirectCommand>,std::vector<GpuSurfaceIdentity>);
    std::vector<GpuSurfaceIdentity> reference(const GpuCullParameters&) const;
    std::optional<GpuSurfaceIdentity> pick(VisibilityFrameKey,
        const std::vector<GpuSurfaceIdentity>&,std::size_t slot) const;
    const std::vector<GpuSurfaceIdentity>& surfaces() const {return surfaces_;}
private:
    VisibilityFrameKey key_;
    std::vector<GpuCullBounds> bounds_;
    std::vector<VkDrawIndexedIndirectCommand> commands_;
    std::vector<GpuSurfaceIdentity> surfaces_;
};
}
