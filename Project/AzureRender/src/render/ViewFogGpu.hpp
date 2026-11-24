#pragma once
#include "render/ViewFogBinding.hpp"
#include "rhi/Rhi.hpp"
#include <memory>

namespace azurerender {
struct FogVolumeData;
struct alignas(16) PackedViewFogFrame {
    static constexpr unsigned layoutVersion=2;
    std::array<std::array<float,4>,21> rows{};
};
static_assert(sizeof(PackedViewFogFrame)==336&&alignof(PackedViewFogFrame)==16);
static_assert(std::is_standard_layout_v<PackedViewFogFrame>);
// Backend outlives this owner; retire submitted frame use before destruction.
class ViewFogGpu final {
public:
    ViewFogGpu(rhi::IRhi&,const ViewFogBinding&,const FogVolumeData&,unsigned frames);
    ~ViewFogGpu();
    ViewFogGpu(const ViewFogGpu&)=delete;
    ViewFogGpu& operator=(const ViewFogGpu&)=delete;
    FogViewMatch updateFrame(unsigned,const CameraViewDescriptor&,std::array<std::uint32_t,2> viewport);
    const rhi::GpuBuffer& parameters(unsigned) const;
    void writeDescriptors(VkDescriptorSet,unsigned frame,unsigned parameterBinding,unsigned textureBinding) const;
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
