#pragma once
#include "render/MotionVectorTarget.hpp"
#include "render/RenderGraph.hpp"
#include "render/TemporalViewState.hpp"
#include <array>
#include <memory>
#include <optional>
#include <string>
namespace azurerender {
struct TemporalSceneSource {
    VkImage color=VK_NULL_HANDLE;VkImageView colorView=VK_NULL_HANDLE;
    VkImage depth=VK_NULL_HANDLE;VkImageView depthView=VK_NULL_HANDLE;
};
// One view's native-resolution HDR history. Sources and their depth views are
// borrowed. Retire recording and GPU work before destruction or replacement.
class TemporalReconstruction final {
public:
    static constexpr std::uint64_t kBytesPerPixel=52,kMaximumBytes=512ULL*1024*1024;
    TemporalReconstruction(rhi::IRhi&,rhi::IGpuAllocator&,VkExtent2D,VkFormat depthFormat,
        std::vector<TemporalSceneSource>,const std::string& shaderDirectory,MotionVectorTarget* sharedMotion=nullptr);
    ~TemporalReconstruction();
    TemporalReconstruction(const TemporalReconstruction&)=delete;
    TemporalReconstruction& operator=(const TemporalReconstruction&)=delete;
    MotionVectorTarget& motion() noexcept{return *motion_;}
    std::array<RenderGraph::ResourceId,2> importMotion(RenderGraph&,const std::string&) const;
    void prepare(const TemporalPreparedFrame&,std::uint32_t frame,std::uint32_t source);
    void schedule(RenderGraph&,RenderGraph::ResourceId color,RenderGraph::ResourceId depth,
        std::array<RenderGraph::ResourceId,2> motion,const std::string& name);
    bool commit(std::uint64_t submission) noexcept;
    void cancel() noexcept {pending_.reset();recorded_=false;}
    std::uint64_t byteSize() const noexcept{return bytes_;}
    std::uint64_t historySubmission() const noexcept{return lastSubmission_;}
    std::uint32_t historyIndex() const noexcept{return historyIndex_;}
    bool preparedHistoryValid() const noexcept{return pending_&&pending_->valid;}
private:
    struct Pending {std::uint32_t frame=0,source=0,previous=0,next=1;bool valid=false;};
    rhi::IRhi& rhi_;rhi::IGpuAllocator& allocator_;VkExtent2D extent_{};
    std::vector<TemporalSceneSource> sources_;
    std::unique_ptr<MotionVectorTarget> ownedMotion_;
    MotionVectorTarget* motion_=nullptr;
    // Scratch HDR, two HDR histories, two colour-surface depth histories.
    std::array<rhi::GpuImage,5> images_{};std::array<VkImageView,5> views_{};
    std::array<rhi::GpuBuffer,2> uniforms_{};
    VkSampler sampler_=VK_NULL_HANDLE;VkRenderPass copyPass_=VK_NULL_HANDLE,resolvePass_=VK_NULL_HANDLE;
    VkFramebuffer copyFramebuffer_=VK_NULL_HANDLE;std::vector<VkFramebuffer> resolveFramebuffers_;
    std::array<VkDescriptorSetLayout,2> descriptors_{};VkDescriptorPool pool_=VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> copySets_,resolveSets_;
    std::array<VkPipelineLayout,2> layouts_{};std::array<VkPipeline,2> pipelines_{};
    std::optional<Pending> pending_;bool recorded_=false;
    std::uint32_t historyIndex_=0;std::uint64_t lastSubmission_=0,bytes_=0;
    void reset() noexcept;
};
}
