#pragma once
#include "rhi/Rhi.hpp"
#include <array>
#include <memory>
#include <string>
namespace azurerender {
enum class ScreenObjectDepth {Forward,Reversed};
enum class ScreenObjectVisibilityPhaseOrigin {TopLeft,BottomLeft};
struct ScreenObjectVisibilityView {
    // Column-major device-clip to world. Pixel offsets are applied before NDC conversion.
    std::array<float,16> clipToWorld{};
    std::array<float,2> pixelOffset{.5F,.5F};
    bool flipNdcY=true;
    // Phase origin is independent of texture addressing and inverse-projection Y.
    ScreenObjectVisibilityPhaseOrigin phaseOrigin=ScreenObjectVisibilityPhaseOrigin::TopLeft;
};
struct ScreenObjectVisibilityRecord {
    std::array<float,16> worldToUnitShadow{};
    std::array<float,3> lightDirection{0,1,0};
    float lightBias=0,normalBias=0;
    std::array<float,4> atlasRect{0,0,1,1};
    std::array<float,2> filterRadiusUv{};
    float minimumReceiverDepth=.01F;
};
struct ScreenObjectVisibilityInputs {
    VkImageView depth{},normals{},objectIds{},atlas{};
    VkExtent2D extent{};
    VkFormat depthFormat=VK_FORMAT_R32_SFLOAT,normalFormat=VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    VkFormat objectIdFormat=VK_FORMAT_R8_UINT,atlasFormat=VK_FORMAT_R16_UNORM;
    VkImageLayout depthLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,atlasLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ScreenObjectDepth objectDepth=ScreenObjectDepth::Reversed;
};
struct ScreenObjectVisibilityShaders {std::vector<char> vertex,fragment;static ScreenObjectVisibilityShaders load(const std::string& directory);};
struct ScreenObjectVisibilityOutput {
    VkImage image{};VkImageView view{};VkSampler sampler{};VkExtent2D extent{};
    VkImageLayout layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
};
// Inputs are borrowed sampled images with matching declared formats and extents.
// Normals use RG Y-pole octahedral encoding; integer object IDs 1..8 select records, zero is background.
// Projection and atlas share objectDepth. The inverse view independently defines device depth.
// The caller publishes input dependencies, retires a slot's consumers before record, and waits before destruction.
// Output is valid after record submits; callers preserve its sampled layout until slot reuse.
class ScreenObjectVisibilityGpu final {
public:
    ScreenObjectVisibilityGpu(rhi::IRhi&,VkExtent2D,unsigned frames,const ScreenObjectVisibilityShaders&);
    ~ScreenObjectVisibilityGpu();
    ScreenObjectVisibilityGpu(const ScreenObjectVisibilityGpu&)=delete;
    ScreenObjectVisibilityGpu& operator=(const ScreenObjectVisibilityGpu&)=delete;
    static bool supported(rhi::IRhi&,VkExtent2D,unsigned frames);
    std::uint64_t byteBudget() const noexcept;
    ScreenObjectVisibilityOutput output(unsigned slot) const;
    void record(rhi::ICommandRecorder&,unsigned slot,const ScreenObjectVisibilityInputs&,const ScreenObjectVisibilityView&,
        const std::vector<ScreenObjectVisibilityRecord>&);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
