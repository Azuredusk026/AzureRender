#pragma once

#include "render/EnvironmentAsset.hpp"
#include "rhi/Rhi.hpp"

namespace azurerender {

// Owns an immutable, authored HDR cube. The RHI and retired frame fences
// outlive this owner. Construction validates before allocating any resource.
class EnvironmentCubeTexture final {
public:
    EnvironmentCubeTexture(rhi::IRhi& backend, EnvironmentImage source);
    ~EnvironmentCubeTexture();
    EnvironmentCubeTexture(const EnvironmentCubeTexture&)=delete;
    EnvironmentCubeTexture& operator=(const EnvironmentCubeTexture&)=delete;
    [[nodiscard]] VkImageView view() const noexcept {return view_;}
    [[nodiscard]] VkSampler sampler() const noexcept {return sampler_;}
    [[nodiscard]] const rhi::GpuImage& image() const noexcept {return image_;}
private:
    void release() noexcept;
    rhi::IRhi& backend_;
    rhi::GpuImage image_;
    VkImageView view_=VK_NULL_HANDLE;
    VkSampler sampler_=VK_NULL_HANDLE;
};

}
