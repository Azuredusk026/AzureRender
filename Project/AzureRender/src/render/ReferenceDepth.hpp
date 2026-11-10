#pragma once
#include "render/ReferenceClipProjection.hpp"
#include "render/MaterialUvGradients.hpp"
#include "rhi/Rhi.hpp"
#include <memory>
#include <optional>
#include <string>
namespace azurerender {
enum class ReferenceDepthConvention {Forward,Reversed};
struct ReferenceDepthConfig {
    unsigned schemaVersion=1;
    ReferenceDepthConvention convention=ReferenceDepthConvention::Reversed;
    bool flipNdcY=true;
};
enum class ReferenceDepthAlphaLodPolicy {Device,EllipseMajorAxisMirroredCoarseY};
struct ReferenceDepthAlpha {
    VkBuffer uv{};
    VkImageView image{};
    VkSampler sampler{};
    VkImageLayout layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    unsigned channel=3;
    float cutoff=.5F,lodBias=0;
    ReferenceDepthAlphaLodPolicy lodPolicy=ReferenceDepthAlphaLodPolicy::Device;
};
struct ReferenceDepthSurface {
    unsigned schemaVersion=1;
    VkCullModeFlags cullMode=VK_CULL_MODE_NONE;
    VkFrontFace frontFace=VK_FRONT_FACE_CLOCKWISE;
    std::optional<ReferenceDepthAlpha> alpha;
};
struct ReferenceDepthGeometry {
    // Borrowed VERTEX_BUFFER vec4 positions (xyz camera-relative, w ignored),
    // and INDEX_BUFFER uint32 triangles. All indices address vertexCount.
    VkBuffer positions{},indices{};
    std::uint32_t vertexCount=0,indexCount=0;
    ReferenceDepthSurface surface;
};
struct ReferenceDepthShaders {
    std::vector<char> vertex,fragment;
    std::vector<char> maskedVertex,maskedFragment;
    std::vector<char> ellipseFragment;
    MaterialUvGradientSupport ellipseSupport;
    static ReferenceDepthShaders load(const std::string& directory);
    static ReferenceDepthShaders loadMasked(const std::string& directory);
    static ReferenceDepthShaders loadEllipseMasked(const std::string& directory,MaterialUvGradientSupport);
};
struct ReferenceDepthOutput {
    VkImage image{},depthImage{};
    VkImageView view{},depthView{};
    VkExtent2D extent{};
    VkImageLayout layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkImageLayout depthLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
};
// The caller publishes input VERTEX_INPUT dependencies before record, retires
// all slot consumers before reuse, and waits for GPU completion before destroy.
// Alpha inputs borrow packed vec2 VERTEX_BUFFER UVs and sampled resources.
// Surface version2 admits explicit ellipse LOD with an isotropic sampler.
// Its caller supplies finite texel derivatives and keeps sampler LOD bias zero.
// A slot supports 128 masked draws. Each record clears the selected slot.
class ReferenceDepthGpu final {
public:
    ReferenceDepthGpu(rhi::IRhi&,VkExtent2D,unsigned frames,const ReferenceDepthConfig&,const ReferenceDepthShaders&);
    ~ReferenceDepthGpu();
    ReferenceDepthGpu(const ReferenceDepthGpu&)=delete;
    ReferenceDepthGpu& operator=(const ReferenceDepthGpu&)=delete;
    static bool supported(rhi::IRhi&,VkExtent2D,unsigned frames);
    std::uint64_t byteBudget() const noexcept;
    ReferenceDepthOutput output(unsigned slot) const;
    void record(rhi::ICommandRecorder&,unsigned slot,const ReferenceClipProjectionProfile&,const std::vector<ReferenceDepthGeometry>&);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
