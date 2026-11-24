#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "rhi/Rhi.hpp"

namespace azurerender {

struct RenderGraphResource {
    std::string name;
    rhi::ImageBarrierDesc initial{};
    rhi::BufferBarrierDesc initialBuffer{};
};

enum class RenderGraphUsage { Sampled, ComputeSampled, ColorAttachment, DepthAttachment, Storage, VertexStorage, FragmentStorage, TransferSrc, TransferDst, Present, VertexBuffer, IndexBuffer, IndirectBuffer };

struct RenderGraphUse {
    std::uint32_t resource = 0;
    RenderGraphUsage usage = RenderGraphUsage::Sampled;
    bool write = false;
    VkImageLayout attachmentFinalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct RenderGraphPass {
    std::string name;
    std::vector<std::uint32_t> reads;
    std::vector<std::uint32_t> writes;
    std::vector<RenderGraphUse> uses;
    std::vector<std::uint32_t> dependencies;
    std::function<void()> record;
    std::function<void(rhi::ICommandRecorder&)> recordCommands;
    bool parallelRecording = false;
    rhi::RenderPassBeginDesc renderPass;
    bool graphicsPass = false;
    std::vector<std::function<void(rhi::ICommandRecorder&)>> recordingChunks;
    std::function<void(rhi::ICommandRecorder&)> afterRenderPass;
};

struct RenderGraphBarrier {
    std::uint32_t pass = 0;
    std::uint32_t resource = 0;
    rhi::ImageBarrierDesc image{};
};

struct RenderGraphBufferBarrier {
    std::uint32_t pass = 0;
    std::uint32_t resource = 0;
    rhi::BufferBarrierDesc buffer{};
};

class RenderGraph final {
public:
    using ResourceId = std::uint32_t;
    using PassId = std::uint32_t;

    ResourceId addResource(std::string name);
    ResourceId importBuffer(std::string name, const rhi::BufferBarrierDesc& initial);
    ResourceId importImage(std::string name, const rhi::ImageBarrierDesc& initial);
    PassId addPass(std::string name, std::function<void()> record = {});
    PassId addCommandPass(std::string name,
        std::function<void(rhi::ICommandRecorder&)> record,
        bool parallelRecording = false);
    PassId addGraphicsPass(std::string name, rhi::RenderPassBeginDesc renderPass,
        std::function<void(rhi::ICommandRecorder&)> draws,
        std::function<void(rhi::ICommandRecorder&)> after = {});
    void setRecordingChunks(PassId pass,
        std::vector<std::function<void(rhi::ICommandRecorder&)>> chunks);
    void dependsOn(PassId pass, PassId prerequisite);
    void execute(rhi::ICommandRecorder* recorder = nullptr,
        const std::function<bool(PassId)>& executeRecorded = {},
        bool useFullInlineCallback = false) const;
    void read(PassId pass, ResourceId resource);
    void write(PassId pass, ResourceId resource);
    void attachment(PassId pass, ResourceId resource, RenderGraphUsage usage, VkImageLayout finalLayout);
    void use(PassId pass, ResourceId resource, RenderGraphUsage usage, bool write);

    [[nodiscard]] bool compile(std::string& error);
    [[nodiscard]] const std::vector<PassId>& executionOrder() const noexcept {
        return executionOrder_;
    }
    [[nodiscard]] const std::vector<RenderGraphResource>& resources() const noexcept {
        return resources_;
    }
    [[nodiscard]] const std::vector<RenderGraphPass>& passes() const noexcept {
        return passes_;
    }
    [[nodiscard]] const std::vector<RenderGraphBarrier>& barriers() const noexcept {
        return barriers_;
    }

    [[nodiscard]] const std::vector<RenderGraphBufferBarrier>& bufferBarriers() const noexcept {
        return bufferBarriers_;
    }
private:
    bool compiled_ = false;
    bool valid(PassId pass, ResourceId resource, std::string& error) const;
    std::vector<RenderGraphResource> resources_;
    std::vector<RenderGraphPass> passes_;
    std::vector<PassId> executionOrder_;
    std::vector<RenderGraphBarrier> barriers_;
    std::vector<RenderGraphBufferBarrier> bufferBarriers_;
    std::vector<std::vector<std::size_t>> imageBarrierIndices_;
    std::vector<std::vector<std::size_t>> bufferBarrierIndices_;
};

}  // namespace azurerender
