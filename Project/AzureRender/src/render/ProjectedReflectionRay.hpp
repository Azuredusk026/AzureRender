#pragma once
#include "rhi/Rhi.hpp"
#include <array>
#include <vector>
namespace azurerender {
using ProjectedReflectionVector=std::array<float,4>;
using ProjectedReflectionMatrix3=std::array<ProjectedReflectionVector,3>;
using ProjectedReflectionMatrix4=std::array<ProjectedReflectionVector,4>;
using ProjectedReflectionFrame=std::array<ProjectedReflectionVector,30>;
enum class ProjectedReflectionConvention {MirroredViewY,VulkanUpperLeft};
enum class ProjectedReflectionMotionEncoding {FourthRoot,LinearUv};
struct ProjectedReflectionSettings {
    unsigned jitterSeed=0,maxSteps=32;
    float edgeFade=.1F; // Normalized distance from the hit UV to the nearest screen edge.
    float motionBlend=0,maximumFootprintLevel=6;
    ProjectedReflectionMotionEncoding motionEncoding=ProjectedReflectionMotionEncoding::FourthRoot;
};
// Matrices store columns. Clip coordinates use device depth in [0,1].
// The caller supplies the selected current/previous projection convention.
struct ProjectedReflectionView {
    ProjectedReflectionMatrix3 worldNormalToView{},viewDirectionToClip{};
    ProjectedReflectionMatrix4 clipToView{},relativeWorldToClip{},clipToWorld{},previousClipToWorld{};
    std::array<float,3> position{};
    // Positive forward distance is 1/(depthReciprocal[0]*deviceDepth+depthReciprocal[1]).
    std::array<float,2> depthReciprocal{1,0};
    ProjectedReflectionConvention convention=ProjectedReflectionConvention::MirroredViewY;
    ProjectedReflectionFrame pack(VkExtent2D sourceExtent,VkExtent2D traceExtent,const ProjectedReflectionSettings&) const;
};
struct ProjectedReflectionInputs {
    // Borrowed sampled views: source-sized color; trace-sized depth/mask/normal.
    // Motion is a normalized-UV field; a 1x1 view represents constant displacement.
    // Normal RG stores Y-pole octahedral world coordinates; B stores roughness.
    // Motion RG follows ProjectedReflectionSettings::motionEncoding.
    VkImageView previousColor{},previousDepth{},currentDepth{},motion{},surfaceMask{},normalRoughness{};
    VkExtent2D sourceExtent{},traceExtent{};
    // The caller produces unique in-bounds 8x8 tiles (X in high 16 bits, Y in low 16).
    VkBuffer tiles{};VkDeviceSize tileBytes=0;unsigned tileCount=0;
};
struct ProjectedReflectionOutputs {
    // Borrowed GENERAL views use R11G11B10_FLOAT, R16G16_UNORM and R8_UNORM.
    // Confidence initially contains reflection eligibility and is read before writing.
    VkImageView color{},footprint{},confidence{};VkExtent2D extent{};
};
// Caller publishes input visibility, retires each slot before reuse and waits before destruction.
// This owner records no image transitions and owns no sampled/storage image.
class ProjectedReflectionRayGpu final {
public:
    ProjectedReflectionRayGpu(rhi::IRhi&,unsigned frames,const std::vector<char>& shader);
    ~ProjectedReflectionRayGpu();
    ProjectedReflectionRayGpu(const ProjectedReflectionRayGpu&)=delete;
    ProjectedReflectionRayGpu& operator=(const ProjectedReflectionRayGpu&)=delete;
    void record(rhi::ICommandRecorder&,unsigned slot,const ProjectedReflectionInputs&,const ProjectedReflectionOutputs&,
        const ProjectedReflectionView&,const ProjectedReflectionSettings&);
private:
    struct Slot {VkDescriptorSet set{};rhi::GpuBuffer parameters{};};
    void release() noexcept;
    rhi::IRhi& backend_;std::vector<Slot> slots_;
    VkSampler point_{};VkDescriptorSetLayout descriptors_{};VkDescriptorPool pool_{};
    VkPipelineLayout layout_{};VkShaderModule shader_{};VkPipeline pipeline_{};
};
}
