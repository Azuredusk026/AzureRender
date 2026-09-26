#include "render/RenderGraph.hpp"

#include <algorithm>
#include <stdexcept>
#include <queue>

namespace azurerender {

RenderGraph::ResourceId RenderGraph::addResource(std::string name) {
    resources_.push_back({std::move(name)});
    return static_cast<ResourceId>(resources_.size() - 1);
}

RenderGraph::PassId RenderGraph::addPass(std::string name) {
    passes_.push_back({std::move(name), {}, {}});
    return static_cast<PassId>(passes_.size() - 1);
}

bool RenderGraph::valid(
    const PassId pass,
    const ResourceId resource,
    std::string& error) const {
    if (pass >= passes_.size()) {
        error = "render graph pass id is out of range";
        return false;
    }
    if (resource >= resources_.size()) {
        error = "render graph resource id is out of range";
        return false;
    }
    return true;
}

void RenderGraph::read(const PassId pass, const ResourceId resource) {
    std::string error;
    if (!valid(pass, resource, error)) throw std::out_of_range(error);
    passes_[pass].reads.push_back(resource);
}

void RenderGraph::write(const PassId pass, const ResourceId resource) {
    std::string error;
    if (!valid(pass, resource, error)) throw std::out_of_range(error);
    passes_[pass].writes.push_back(resource);
}

void RenderGraph::use(
    const PassId pass,
    const ResourceId resource,
    const RenderGraphUsage usage,
    const bool isWrite) {
    std::string error;
    if (!valid(pass, resource, error)) throw std::out_of_range(error);
    if (isWrite) passes_[pass].writes.push_back(resource);
    else passes_[pass].reads.push_back(resource);
    const auto stage = [&]() {
        switch (usage) {
        case RenderGraphUsage::TransferSrc:
        case RenderGraphUsage::TransferDst: return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case RenderGraphUsage::ColorAttachment: return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case RenderGraphUsage::DepthAttachment: return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        case RenderGraphUsage::VertexBuffer:
        case RenderGraphUsage::IndexBuffer: return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        case RenderGraphUsage::Sampled:
        case RenderGraphUsage::Storage: return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }();
    const auto access = [&]() {
        if (usage == RenderGraphUsage::TransferSrc) return VK_ACCESS_TRANSFER_READ_BIT;
        if (usage == RenderGraphUsage::TransferDst) return VK_ACCESS_TRANSFER_WRITE_BIT;
        if (usage == RenderGraphUsage::ColorAttachment) return isWrite ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        if (usage == RenderGraphUsage::DepthAttachment) return isWrite ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        if (usage == RenderGraphUsage::VertexBuffer) return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        if (usage == RenderGraphUsage::IndexBuffer) return VK_ACCESS_INDEX_READ_BIT;
        return isWrite ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT;
    }();
    RenderGraphBarrier barrier{};
    barrier.pass = pass;
    barrier.resource = resource;
    barrier.image.srcStageMask = 0;
    barrier.image.dstStageMask = stage;
    barrier.image.dstAccessMask = access;
    barrier.image.newLayout = usage == RenderGraphUsage::ColorAttachment
        ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        : usage == RenderGraphUsage::DepthAttachment
            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            : usage == RenderGraphUsage::TransferDst
                ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                : usage == RenderGraphUsage::TransferSrc
                    ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers_.push_back(barrier);
}

bool RenderGraph::compile(std::string& error) {
    executionOrder_.clear();
    const std::size_t count = passes_.size();
    std::vector<std::vector<PassId>> outgoing(count);
    std::vector<std::size_t> indegree(count, 0);
    for (PassId consumer = 0; consumer < count; ++consumer) {
        const auto dependsOn = [&](const ResourceId resource) {
            for (PassId producer = 0; producer < consumer; ++producer) {
                if (std::find(
                        passes_[producer].writes.begin(),
                        passes_[producer].writes.end(),
                        resource) != passes_[producer].writes.end()) {
                    outgoing[producer].push_back(consumer);
                    ++indegree[consumer];
                    break;
                }
            }
        };
        for (const ResourceId resource : passes_[consumer].reads) dependsOn(resource);
        for (const ResourceId resource : passes_[consumer].writes) dependsOn(resource);
    }
    std::queue<PassId> ready;
    for (PassId pass = 0; pass < count; ++pass)
        if (indegree[pass] == 0) ready.push(pass);
    while (!ready.empty()) {
        const PassId pass = ready.front();
        ready.pop();
        executionOrder_.push_back(pass);
        for (const PassId next : outgoing[pass])
            if (--indegree[next] == 0) ready.push(next);
    }
    if (executionOrder_.size() != count) {
        executionOrder_.clear();
        error = "render graph contains a dependency cycle";
        return false;
    }
    return true;
}

}  // namespace azurerender
