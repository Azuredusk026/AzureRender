#pragma once
#include "rhi/Rhi.hpp"
#include <cstdint>
#include <vector>
namespace azurerender {
// Owns one motion attachment and framebuffers borrowing the scene depth views.
// The caller retires GPU work and destroys this owner before those depth views.
class MotionVectorTarget final {
public:
    static constexpr VkFormat kFormat=VK_FORMAT_R32G32B32A32_SFLOAT;
    static constexpr VkFormat kSurfaceDepthFormat=VK_FORMAT_R32_SFLOAT;
    static constexpr std::uint64_t kBytesPerPixel=20,kMaximumBytes=256ULL*1024*1024;
    MotionVectorTarget(rhi::IRhi&,rhi::IGpuAllocator&,VkExtent2D,VkFormat depthFormat,const std::vector<VkImageView>& depths);
    ~MotionVectorTarget();
    MotionVectorTarget(const MotionVectorTarget&)=delete;
    MotionVectorTarget& operator=(const MotionVectorTarget&)=delete;
    const rhi::GpuImage& image() const noexcept {return image_;}
    const rhi::GpuImage& surfaceDepthImage() const noexcept {return surfaceDepth_;}
    VkImageView view() const noexcept {return view_;}
    VkImageView surfaceDepthView() const noexcept {return surfaceDepthView_;}
    VkRenderPass renderPass() const noexcept {return pass_;}
    VkFramebuffer framebuffer(std::uint32_t source) const {return framebuffers_.at(source);}
    std::uint64_t byteSize() const noexcept {return bytes_;}
private:
    rhi::IRhi& rhi_;rhi::IGpuAllocator& allocator_;
    rhi::GpuImage image_{},surfaceDepth_{};VkImageView view_=VK_NULL_HANDLE,surfaceDepthView_=VK_NULL_HANDLE;VkRenderPass pass_=VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;std::uint64_t bytes_=0;
    void reset() noexcept;
};
}
