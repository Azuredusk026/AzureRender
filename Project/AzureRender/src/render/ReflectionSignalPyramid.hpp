#pragma once
#include "render/ReflectionSpatialFilter.hpp"
namespace azurerender {
enum class ReflectionPyramidPrecision {PreferPacked,RequirePacked,Float32};
struct ReflectionPyramidShaders {std::vector<char> packed,wide;};
struct ReflectionPyramidParameters {
    float depthFalloff=.01F,minimumWeight=.01F;
    std::array<float,2> depthDecode{1,0};
    bool reciprocalDepth=false;
    ReflectionFilterFrame frame(unsigned width,unsigned height,unsigned mip,unsigned depthLevels) const;
};
struct ReflectionPyramidViews {
    VkImage colorImage=VK_NULL_HANDLE,confidenceImage=VK_NULL_HANDLE;
    VkImageView color=VK_NULL_HANDLE,confidence=VK_NULL_HANDLE;
    VkFormat colorFormat=VK_FORMAT_UNDEFINED,confidenceFormat=VK_FORMAT_UNDEFINED;
    unsigned width=0,height=0,levels=0;
};
// The caller fences slot reuse and destruction, then writes both base levels after beginBaseWrite.
// record consumes an external sampled depth pyramid and publishes sampled signal pyramids.
class ReflectionSignalPyramidGpu final {
public:
    ReflectionSignalPyramidGpu(rhi::IRhi&,VkExtent2D,unsigned levels,unsigned frames,ReflectionPyramidPrecision,const ReflectionPyramidShaders&);
    ~ReflectionSignalPyramidGpu();
    ReflectionSignalPyramidGpu(const ReflectionSignalPyramidGpu&)=delete;
    ReflectionSignalPyramidGpu& operator=(const ReflectionSignalPyramidGpu&)=delete;
    bool usesPackedFormats() const noexcept {return packed_;}
    ReflectionPyramidViews base(unsigned slot) const;
    ReflectionPyramidViews output(unsigned slot) const;
    void beginBaseWrite(rhi::ICommandRecorder&,unsigned slot);
    void record(rhi::ICommandRecorder&,unsigned slot,VkImageView depth,unsigned depthLevels,const ReflectionPyramidParameters&);
private:
    struct Mip {VkImageView color=VK_NULL_HANDLE,confidence=VK_NULL_HANDLE;VkDescriptorSet set=VK_NULL_HANDLE;rhi::GpuBuffer parameters{};};
    struct Slot {rhi::GpuImage color{},confidence{};VkImageView allColor=VK_NULL_HANDLE,allConfidence=VK_NULL_HANDLE;
        std::vector<Mip> mips;bool writing=false,ready=false;};
    void release() noexcept;
    rhi::IRhi& backend_;VkExtent2D extent_{};unsigned levels_=0;bool packed_=false;
    VkFormat colorFormat_=VK_FORMAT_UNDEFINED,confidenceFormat_=VK_FORMAT_UNDEFINED;
    std::vector<Slot> slots_;
    VkSampler point_=VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout_=VK_NULL_HANDLE;VkDescriptorPool pool_=VK_NULL_HANDLE;
    VkPipelineLayout layout_=VK_NULL_HANDLE;VkShaderModule shader_=VK_NULL_HANDLE;VkPipeline pipeline_=VK_NULL_HANDLE;
};
}
