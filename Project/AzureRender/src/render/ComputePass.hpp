#pragma once

#include "rhi/Rhi.hpp"

#include <cstdint>

namespace azurerender {

struct ComputeDispatchSize {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t z = 0;
};

constexpr bool operator==(const ComputeDispatchSize& a,
                          const ComputeDispatchSize& b) noexcept {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

struct ComputePassDesc {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t localSizeX = 1;
    std::uint32_t localSizeY = 1;
    std::uint32_t localSizeZ = 1;
    bool enabled = true;
};

class ComputePass final {
public:
    explicit ComputePass(ComputePassDesc desc);

    [[nodiscard]] ComputeDispatchSize dispatchSize() const noexcept;
    [[nodiscard]] bool enabled() const noexcept { return desc_.enabled; }

    void record(rhi::ICommandRecorder& recorder,
                VkPipeline pipeline,
                VkPipelineLayout layout,
                VkDescriptorSet descriptorSet) const;

private:
    ComputePassDesc desc_;
};

}  // namespace azurerender
