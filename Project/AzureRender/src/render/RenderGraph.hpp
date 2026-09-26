#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace azurerender {

struct RenderGraphResource {
    std::string name;
};

struct RenderGraphPass {
    std::string name;
    std::vector<std::uint32_t> reads;
    std::vector<std::uint32_t> writes;
};

class RenderGraph final {
public:
    using ResourceId = std::uint32_t;
    using PassId = std::uint32_t;

    ResourceId addResource(std::string name);
    PassId addPass(std::string name);
    void read(PassId pass, ResourceId resource);
    void write(PassId pass, ResourceId resource);

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

private:
    bool valid(PassId pass, ResourceId resource, std::string& error) const;
    std::vector<RenderGraphResource> resources_;
    std::vector<RenderGraphPass> passes_;
    std::vector<PassId> executionOrder_;
};

}  // namespace azurerender
