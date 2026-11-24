#pragma once
#include "extensions/ISceneRenderer.hpp"
#include "render/pathtracing/TraceScene.hpp"
#include "render/RenderSettings.hpp"
#include "rhi/IGpuAllocator.hpp"
#include "rhi/Rhi.hpp"
#include <cstddef>
namespace azurerender {
// Host owns the queue and fences. Each frame slot is updated after its fence.
class PathTracingSceneRenderer final: public ISceneRenderer {
public:
 std::string_view name() const noexcept override{return "pathtracing";}
 SceneRendererCapabilities capabilities() const override{return {true,true,{"Beauty"},true};}
 void onLoad(const RenderContext&) override;
 void onUnload(const RenderContext&) override;
 void onSwapchainRecreate(const RenderContext&) override;
 void updateFrame(const SceneFrameData&) override;
 void recordScene(const RenderContext&) override;
 const RendererSceneState* sceneState() const noexcept override{return &state_;}
 void appendHudText(std::ostringstream&) const override;
 void appendCaptureManifestFields(std::ostream&) const override;
private:
 struct Uniform {static constexpr unsigned kLayoutVersion=2;tracing::Vec4 origin,right,up,forward,skyColor,sky;std::array<unsigned,4> counts{},image{};tracing::Vec4 rayOffset{};};
 static_assert(sizeof(Uniform)==144&&offsetof(Uniform,rayOffset)==128);
 struct Slot {rhi::GpuBuffer uniform,triangles,nodes,lights;std::uint64_t generation=0;};
 rhi::IRhi* rhi_=nullptr;rhi::IGpuAllocator* allocator_=nullptr;
 VkDescriptorSetLayout setLayout_=VK_NULL_HANDLE;VkDescriptorPool pool_=VK_NULL_HANDLE;
 VkPipelineLayout layout_=VK_NULL_HANDLE;VkPipeline compute_=VK_NULL_HANDLE,display_=VK_NULL_HANDLE;
 std::vector<VkDescriptorSet> sets_;std::vector<Slot> slots_;
 rhi::GpuImage environment_;VkImageView environmentView_=VK_NULL_HANDLE;VkSampler environmentSampler_=VK_NULL_HANDLE;
 rhi::GpuBuffer materials_,pixels_,history_,guides_;
 RendererSceneState state_;std::vector<AssetPose> poses_;
 tracing::Meshes meshes_;tracing::Geometry geometry_;scene::SceneDescription scene_;
 RenderSettings settings_;Uniform uniform_{};std::uint64_t fingerprint_=0,generation_=1,resets_=0;
 unsigned samples_=0,current_=0,width_=0,height_=0;VkDeviceSize maxBuffer_=0;
 void allocateHistory(const RenderContext&);
 void upload(rhi::GpuBuffer&,const void*,std::size_t,VkBufferUsageFlags);
};
}
