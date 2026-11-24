#pragma once
#include "render/ColorGradingLut.hpp"
#include "rhi/Rhi.hpp"
namespace azurerender {
enum class ColorGradingLutUpload { Float32, IdentityPlaceholder };
// Immutable sampled resource. The owner waits for all consuming submissions before destruction.
class ColorGradingLutGpu final {
public:
    ColorGradingLutGpu(rhi::IRhi& backend,const ColorGradingLut& lut,ColorGradingLutUpload upload=ColorGradingLutUpload::Float32);
    ~ColorGradingLutGpu();
    ColorGradingLutGpu(const ColorGradingLutGpu&)=delete;
    ColorGradingLutGpu& operator=(const ColorGradingLutGpu&)=delete;
    VkImageView view() const noexcept{return view_;}
    VkSampler sampler() const noexcept{return sampler_;}
    std::uint32_t dimension() const noexcept{return dimension_;}
    ColorGradingInputDomain inputDomain() const noexcept{return domain_;}
private:
    void release() noexcept;
    rhi::IRhi& backend_;
    rhi::GpuImage image_{};
    VkImageView view_=VK_NULL_HANDLE;
    VkSampler sampler_=VK_NULL_HANDLE;
    std::uint32_t dimension_=0;
    ColorGradingInputDomain domain_=ColorGradingInputDomain::Linear;
};
}
