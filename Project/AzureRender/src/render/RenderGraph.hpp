#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "rhi/Rhi.hpp"

namespace azurerender {

struct RenderGraphResource {
    std::string name;
};

struct RenderGraphPass {
    std::string name;
    std::vector<std::uint32_t> reads;
    std::vector<std::uint32_t> writes;
};

enum class RenderGraphUsage { Sampled, ColorAttachment, DepthAttachment, Storage, TransferSrc, TransferDst, VertexBuffer, IndexBuffer };

struct RenderGraphUse {
    std::uint32_t resource = 0;
    RenderGraphUsage usage = RenderGraphUsage::Sampled;
    bool write = false;
};

struct RenderGraphBarrier {
    std::uint32_t pass = 0;
    std::uint32_t resource = 0;
    rhi::ImageBarrierDesc image{};
};

class RenderGraph final {
public:
    using ResourceId = std::uint32_t;
    using PassId = std::uint32_t;

    ResourceId addResource(std::string name);
    PassId addPass(std::string name);
    void read(PassId pass, ResourceId resource);
    void write(PassId pass, ResourceId resource);
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

private:
    bool valid(PassId pass, ResourceId resource, std::string& error) const;
    std::vector<RenderGraphResource> resources_;
    std::vector<RenderGraphPass> passes_;
    std::vector<PassId> executionOrder_;
    std::vector<RenderGraphBarrier> barriers_;
};

}  // namespace azurerender
