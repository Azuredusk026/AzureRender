#pragma once
#include "render/ReferenceClipProjection.hpp"
#include "rhi/Rhi.hpp"
#include <memory>
#include <string>
namespace azurerender {
enum class ReferenceDepthConvention {Forward,Reversed};
struct ReferenceDepthConfig {
    unsigned schemaVersion=1;
    ReferenceDepthConvention convention=ReferenceDepthConvention::Reversed;
    bool flipNdcY=true;
};
struct ReferenceDepthGeometry {
    // Borrowed VERTEX_BUFFER vec4 positions (xyz camera-relative, w ignored),
    // and INDEX_BUFFER uint32 triangles. All indices address vertexCount.
    VkBuffer positions{},indices{};
    std::uint32_t vertexCount=0,indexCount=0;
};
struct ReferenceDepthShaders {
    std::vector<char> vertex,fragment;
    static ReferenceDepthShaders load(const std::string& directory);
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
// Output is a genuine rasterized opaque surface; material coverage is supplied
// by the owner of the geometry workflow. Each record clears the selected slot.
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
