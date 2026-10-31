#pragma once
#include "rhi/Rhi.hpp"
#include <array>
#include <vector>
namespace azurerender {
using ProjectedReflectionPreparationFrame=std::array<std::array<float,4>,2>;
struct ProjectedReflectionPreparationParameters {
    float maxRoughness=.7F,strength=1;
    ProjectedReflectionPreparationFrame frame(VkExtent2D source,VkExtent2D trace,const std::array<float,2>& depthReciprocal) const;
};
struct ProjectedReflectionEligibilityTarget {
    // Borrowed R8_UNORM storage, already GENERAL and writable. Contents are replaced.
    VkImage image{};VkImageView view{};VkExtent2D extent{};
};
struct ProjectedReflectionPrepared {
    VkImage depthImage{},normalRoughnessImage{},maskImage{},footprintImage{};
    VkImageView depth{},normalRoughness{},mask{},footprint{};
    VkExtent2D extent{};VkBuffer tiles{};VkDeviceSize tileBytes=0;unsigned tileCount=0;
};
// Surface is borrowed sampled RGBA32F: positive linear depth, Z-pole oct RG, roughness.
// record publishes sampled depth/normal/mask, writable footprint and read/write eligibility.
// The caller retires all slot consumers, including history, before reuse/destruction.
class ProjectedReflectionPreparationGpu final {
public:
    static constexpr std::uint64_t kBytesPerPixel=25,kMaximumBytes=256ULL*1024*1024;
    ProjectedReflectionPreparationGpu(rhi::IRhi&,VkExtent2D source,unsigned frames,const std::vector<char>& shader,unsigned resolutionDivisor=1);
    ~ProjectedReflectionPreparationGpu();
    ProjectedReflectionPreparationGpu(const ProjectedReflectionPreparationGpu&)=delete;
    ProjectedReflectionPreparationGpu& operator=(const ProjectedReflectionPreparationGpu&)=delete;
    std::uint64_t imageByteSize() const noexcept{return bytes_;}
    ProjectedReflectionPrepared output(unsigned slot) const;
    void record(rhi::ICommandRecorder&,unsigned slot,VkImageView surface,const ProjectedReflectionEligibilityTarget&,
        const std::array<float,2>& depthReciprocal,const ProjectedReflectionPreparationParameters&);
private:
    struct Slot {std::array<rhi::GpuImage,4> images{};std::array<VkImageView,4> views{};rhi::GpuBuffer parameters{};VkDescriptorSet set{};};
    void release() noexcept;
    rhi::IRhi& backend_;VkExtent2D source_{},extent_{};std::uint64_t bytes_=0;unsigned tileCount_=0;rhi::GpuBuffer tiles_{};
    std::vector<Slot> slots_;VkSampler point_{};VkDescriptorSetLayout descriptors_{};VkDescriptorPool pool_{};
    VkPipelineLayout layout_{};VkShaderModule shader_{};VkPipeline pipeline_{};
};
}
