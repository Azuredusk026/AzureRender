#pragma once
#include "render/RenderGraph.hpp"
#include <array>
namespace azurerender {
enum class SceneSecondaryResolve : std::uint32_t { Average, Minimum };
// Attachment order: HDR color, depth, secondary color. Destination images and
// views are borrowed. Retire GPU work before replacement; destinations outlive us.
struct SceneMultisampleDestination {
    std::array<VkImage,3> images{};
    std::array<VkImageView,3> views{};
};
class SceneMultisampling final {
public:
    static constexpr VkSampleCountFlagBits kSamples=VK_SAMPLE_COUNT_4_BIT;
    static constexpr std::uint64_t kMaximumBytes=512ULL*1024*1024;
    SceneMultisampling(rhi::IRhi&,rhi::IGpuAllocator&,VkExtent2D,VkFormat depth,VkFormat secondary,
        SceneSecondaryResolve,std::uint32_t sourceSlots,std::vector<SceneMultisampleDestination>,const std::string& shaderDirectory);
    ~SceneMultisampling();
    SceneMultisampling(const SceneMultisampling&)=delete;
    SceneMultisampling& operator=(const SceneMultisampling&)=delete;
    VkRenderPass renderPass() const noexcept{return sourcePass_;}
    VkFramebuffer framebuffer(std::uint32_t slot) const{return sources_.at(slot).framebuffer;}
    VkImage sourceImage(std::uint32_t slot,std::uint32_t attachment) const{return sources_.at(slot).images.at(attachment).image;}
    // Borrowed source view; source slots share this owner's GPU retirement boundary.
    VkImageView sourceView(std::uint32_t slot,std::uint32_t attachment) const{return sources_.at(slot).views.at(attachment);}
    std::uint64_t allocatedByteSize() const noexcept{return allocatedBytes_;}
    std::uint64_t byteSize() const noexcept{return bytes_;}
    std::array<RenderGraph::ResourceId,3> importSources(RenderGraph&,std::uint32_t slot,const std::string&) const;
    // Caller supplies source attachment-write to fragment-read synchronization.
    void recordResolve(rhi::ICommandRecorder&,std::uint32_t slot,std::uint32_t destination) const;
    void schedule(RenderGraph&,std::uint32_t slot,std::uint32_t destination,
        std::array<RenderGraph::ResourceId,3> sources,std::array<RenderGraph::ResourceId,3> outputs,const std::string&) const;
private:
    struct Source {std::array<rhi::GpuImage,3> images{};std::array<VkImageView,3> views{};VkFramebuffer framebuffer{};};
    rhi::IRhi& rhi_;rhi::IGpuAllocator& allocator_;VkExtent2D extent_{};SceneSecondaryResolve secondaryResolve_{};
    std::vector<Source> sources_;std::vector<SceneMultisampleDestination> destinations_;
    std::vector<VkFramebuffer> outputs_;std::vector<VkDescriptorSet> sets_;
    VkRenderPass sourcePass_{},resolvePass_{};VkSampler sampler_{};VkDescriptorSetLayout descriptors_{};
    VkDescriptorPool pool_{};VkPipelineLayout layout_{};VkPipeline pipeline_{};std::uint64_t bytes_=0,allocatedBytes_=0;
    void recordResolveDraw(rhi::ICommandRecorder&,std::uint32_t slot) const;
    void reset() noexcept;
};
}
