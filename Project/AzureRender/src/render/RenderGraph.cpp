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
