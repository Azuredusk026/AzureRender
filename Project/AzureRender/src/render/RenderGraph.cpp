#include "render/RenderGraph.hpp"

#include <algorithm>
#include <stdexcept>
#include <queue>

namespace azurerender {

RenderGraph::ResourceId RenderGraph::addResource(std::string name) {
    compiled_ = false;
    resources_.push_back({std::move(name), {}, {}});
    return static_cast<ResourceId>(resources_.size() - 1);
}

RenderGraph::ResourceId RenderGraph::importBuffer(
    std::string name, const rhi::BufferBarrierDesc& initial) {
    if (initial.buffer == VK_NULL_HANDLE || initial.size == 0)
        throw std::invalid_argument("Imported buffer requires handle and nonempty range");
    const auto id = addResource(std::move(name));
    resources_[id].initialBuffer = initial;
    return id;
}

RenderGraph::ResourceId RenderGraph::importImage(
    std::string name, const rhi::ImageBarrierDesc& initial) {
    if (initial.image == VK_NULL_HANDLE) throw std::invalid_argument("Imported image is null");
    const auto id = addResource(std::move(name));
    resources_[id].initial = initial;
    return id;
}

RenderGraph::PassId RenderGraph::addPass(std::string name, std::function<void()> record) {
    compiled_ = false;
    passes_.push_back({std::move(name), {}, {}, {}, {}, std::move(record)});
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
    compiled_ = false;
    passes_[pass].reads.push_back(resource);
}

void RenderGraph::write(const PassId pass, const ResourceId resource) {
    std::string error;
    if (!valid(pass, resource, error)) throw std::out_of_range(error);
    compiled_ = false;
    passes_[pass].writes.push_back(resource);
}

void RenderGraph::use(
    const PassId pass,
    const ResourceId resource,
    const RenderGraphUsage usage,
    const bool isWrite) {
    std::string error;
    if (!valid(pass, resource, error)) throw std::out_of_range(error);
    compiled_ = false;
    if (isWrite) passes_[pass].writes.push_back(resource);
    else passes_[pass].reads.push_back(resource);
    passes_[pass].uses.push_back({resource, usage, isWrite});
}

void RenderGraph::attachment(PassId pass, ResourceId resource,
                             RenderGraphUsage usage, VkImageLayout finalLayout) {
    if ((usage != RenderGraphUsage::ColorAttachment && usage != RenderGraphUsage::DepthAttachment)
        || finalLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        throw std::invalid_argument("Attachment requires valid usage and final layout");
    use(pass, resource, usage, true);
    passes_[pass].uses.back().attachmentFinalLayout = finalLayout;
}

void RenderGraph::dependsOn(PassId pass, PassId prerequisite) {
    if (pass >= passes_.size() || prerequisite >= passes_.size())
        throw std::out_of_range("Invalid pass dependency");
    compiled_ = false;
    passes_[pass].dependencies.push_back(prerequisite);
}

void RenderGraph::execute(rhi::ICommandRecorder* recorder) const {
    if (!compiled_) throw std::logic_error("Render graph must compile before execution");
    for (const auto& barrier : barriers_)
        if (barrier.image.image != VK_NULL_HANDLE && recorder == nullptr)
            throw std::logic_error("Bound image requires command recorder");
    if (!bufferBarriers_.empty() && recorder == nullptr)
        throw std::logic_error("Bound buffer requires command recorder");
    for (const auto pass : executionOrder_) {
        for (const auto& barrier : bufferBarriers_)
            if (barrier.pass == pass) recorder->bufferBarrier(barrier.buffer);
        for (const auto& barrier : barriers_)
            if (barrier.pass == pass && barrier.image.image != VK_NULL_HANDLE)
                recorder->imageBarrier(barrier.image);
        if (passes_[pass].record) passes_[pass].record();
    }
}

bool RenderGraph::compile(std::string& error) {
    compiled_ = false;
    executionOrder_.clear();
    barriers_.clear();
    bufferBarriers_.clear();
    error.clear();
    const std::size_t count = passes_.size();
    std::vector<std::vector<PassId>> outgoing(count);
    std::vector<std::size_t> indegree(count, 0);
    const auto edge = [&](PassId before, PassId after) {
        if (before == after) return;
        auto& edges = outgoing[before];
        if (std::find(edges.begin(), edges.end(), after) == edges.end()) {
            edges.push_back(after);
            ++indegree[after];
        }
    };
    std::vector<PassId> writers(resources_.size(), static_cast<PassId>(count));
    std::vector<std::vector<PassId>> readers(resources_.size());
    for (PassId pass = 0; pass < count; ++pass) {
        for (const auto prerequisite : passes_[pass].dependencies) {
            if (prerequisite == pass) { error = "Self dependency: " + passes_[pass].name; return false; }
            edge(prerequisite, pass);
        }
        std::vector<bool> declared(resources_.size(), false);
        for (const auto& use : passes_[pass].uses) {
            if (declared[use.resource]) {
                error = "duplicate state declaration in pass " + passes_[pass].name
                    + ": " + resources_[use.resource].name;
                return false;
            }
            declared[use.resource] = true;
        }
        for (const ResourceId resource : passes_[pass].reads) {
            if (writers[resource] != count) edge(writers[resource], pass);
            readers[resource].push_back(pass);
        }
        for (const ResourceId resource : passes_[pass].writes) {
            if (writers[resource] != count) edge(writers[resource], pass);
            for (const PassId reader : readers[resource]) edge(reader, pass);
            readers[resource].clear();
            writers[resource] = pass;
        }
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

    struct ResourceState {
        VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkAccessFlags access = 0;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };
    std::vector<ResourceState> states(resources_.size());
    for (std::size_t i = 0; i < resources_.size(); ++i) {
        const auto& initial = resources_[i].initial;
        const auto& buffer = resources_[i].initialBuffer;
        if (buffer.buffer != VK_NULL_HANDLE) {
            states[i] = {buffer.dstStageMask ? buffer.dstStageMask : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                buffer.dstAccessMask, VK_IMAGE_LAYOUT_UNDEFINED};
            continue;
        }
        states[i] = {initial.dstStageMask ? initial.dstStageMask : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            initial.dstAccessMask, initial.newLayout};
    }
    const auto stateFor = [](const RenderGraphUsage usage, const bool write) {
        ResourceState state{};
        switch (usage) {
        case RenderGraphUsage::Present:
            state.stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            state.access = 0;
            state.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            break;
        case RenderGraphUsage::TransferSrc:
            state.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            state.access = VK_ACCESS_TRANSFER_READ_BIT;
            state.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            break;
        case RenderGraphUsage::TransferDst:
            state.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            state.access = VK_ACCESS_TRANSFER_WRITE_BIT;
            state.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            break;
        case RenderGraphUsage::ColorAttachment:
            state.stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            state.access = write ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            state.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case RenderGraphUsage::DepthAttachment:
            state.stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            state.access = write ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            state.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        case RenderGraphUsage::VertexBuffer:
            state.stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            state.access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            state.layout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        case RenderGraphUsage::IndexBuffer:
            state.stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            state.access = VK_ACCESS_INDEX_READ_BIT;
            state.layout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        case RenderGraphUsage::Sampled:
            state.stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            state.access = VK_ACCESS_SHADER_READ_BIT;
            state.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case RenderGraphUsage::ComputeSampled:
            state.stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            state.access = VK_ACCESS_SHADER_READ_BIT;
            state.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case RenderGraphUsage::Storage:
            state.stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            state.access = write ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT;
            state.layout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        case RenderGraphUsage::VertexStorage:
            state.stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            state.access = write ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT;
            state.layout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        case RenderGraphUsage::FragmentStorage:
            state.stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            state.access = write ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT;
            state.layout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        }
        return state;
    };
    for (const PassId pass : executionOrder_) {
        for (const RenderGraphUse& use : passes_[pass].uses) {
            ResourceState next = stateFor(use.usage, use.write);
            if (use.attachmentFinalLayout != VK_IMAGE_LAYOUT_UNDEFINED) {
                next.layout = use.attachmentFinalLayout;
                states[use.resource] = next;
                continue;
            }
            if ((use.usage == RenderGraphUsage::Sampled
                    || use.usage == RenderGraphUsage::ComputeSampled)
                && (resources_[use.resource].initial.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT))
                next.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            const ResourceState previous = states[use.resource];
            if (resources_[use.resource].initialBuffer.buffer != VK_NULL_HANDLE) {
                RenderGraphBufferBarrier barrier{};
                barrier.pass = pass;
                barrier.resource = use.resource;
                barrier.buffer = resources_[use.resource].initialBuffer;
                barrier.buffer.srcStageMask = previous.stage;
                barrier.buffer.srcAccessMask = previous.access;
                barrier.buffer.dstStageMask = next.stage;
                barrier.buffer.dstAccessMask = next.access;
                bufferBarriers_.push_back(barrier);
                states[use.resource] = next;
                continue;
            }
            RenderGraphBarrier barrier{};
            barrier.pass = pass;
            barrier.resource = use.resource;
            barrier.image = resources_[use.resource].initial;
            barrier.image.oldLayout = previous.layout;
            barrier.image.newLayout = next.layout;
            barrier.image.srcStageMask = previous.stage;
            barrier.image.srcAccessMask = previous.access;
            barrier.image.dstStageMask = next.stage;
            barrier.image.dstAccessMask = next.access;
            barriers_.push_back(barrier);
            states[use.resource] = next;
        }
    }
    compiled_ = true;
    return true;
}

}  // namespace azurerender
