#pragma once
#include "render/MotionPoseData.hpp"
#include "render/MotionVectorTarget.hpp"
#include "render/SampledTextureCache.hpp"
#include <array>
#include <string>
namespace azurerender {
// Borrowed geometry and material inputs. Owners retire GPU work before release.
struct MotionVectorInputs {
    const rhi::GpuBuffer *instances=nullptr,*joints=nullptr,*weights=nullptr,*targets=nullptr,*outline=nullptr;
    std::vector<SampledTextureBinding> colors,masks;
    SampledTextureBinding coverage;
};
struct MotionVectorPush {
    float alphaCutoff=.5F;std::uint32_t flags=0,material=0,primitive=0;
    MotionMatrix gizmo=motionIdentityMatrix();
    std::array<float,4> camera{};
    std::array<float,4> detail{};
    std::array<float,4> outline{};
};
static_assert(sizeof(MotionVectorPush)==128);
struct MotionVectorDraw {
    VkBuffer vertices=VK_NULL_HANDLE,indices=VK_NULL_HANDLE;
    std::uint32_t firstIndex=0,indexCount=0,instance=0;
    bool mirrored=false,doubleSided=false,shell=false;
    MotionVectorPush push;
};
// One view owns the target; its producer owns descriptors, pipelines and pose
// uploads. prepare touches only a fence-retired frame slot, record is read-only.
class MotionVectorPass final {
public:
    MotionVectorPass(rhi::IRhi&,rhi::IGpuAllocator&,MotionVectorTarget&,const std::string& shaders,
        std::uint32_t materials,std::uint32_t frames,bool culling,bool depthTest);
    ~MotionVectorPass();
    MotionVectorPass(const MotionVectorPass&)=delete;
    MotionVectorPass& operator=(const MotionVectorPass&)=delete;
    void prepare(std::uint32_t frame,const PackedMotionPoseFrame&,const MotionVectorInputs&);
    void record(rhi::ICommandRecorder&,std::uint32_t frame,std::uint32_t image,VkExtent2D,
        const std::vector<MotionVectorDraw>&) const;
    // The caller owns a compatible framebuffer with this target's two colours
    // and the selected borrowed read-only depth; all uses retire before release.
    void recordToFramebuffer(rhi::ICommandRecorder&,std::uint32_t frame,VkFramebuffer,VkExtent2D,
        const std::vector<MotionVectorDraw>&) const;
    const rhi::GpuBuffer& previous(std::uint32_t frame) const{return previous_.at(frame);}
private:
    rhi::IRhi& rhi_;rhi::IGpuAllocator& allocator_;MotionVectorTarget& target_;
    std::uint32_t materials_=0;
    VkDescriptorSetLayout descriptors_=VK_NULL_HANDLE;VkDescriptorPool pool_=VK_NULL_HANDLE;
    VkPipelineLayout layout_=VK_NULL_HANDLE;
    std::array<VkPipeline,8> pipelines_{};
    std::vector<VkDescriptorSet> sets_;
    std::vector<rhi::GpuBuffer> previous_;
    void reset() noexcept;
};
}
