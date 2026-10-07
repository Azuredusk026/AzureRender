#include "runtime/SceneDocument.hpp"
#include "scene/SceneDescription.hpp"

#include <stdexcept>

namespace azurerender {
scene::SceneDescription SceneDocument::renderDescription() const {
    scene::SceneDescription result;
    result.resources.reserve(this->resources.size());
    for (const azurerender::SceneResource& resource :
         this->resources) {
        result.resources.push_back(
            {resource.id, resource.path.string()});
    }
    result.nodes.reserve(this->nodes.size());
    for (const azurerender::SceneNode& node : this->nodes) {
        azurerender::scene::SceneNodeDesc desc{};
        desc.id = node.id;
        desc.resourceId = node.resourceId;
        desc.parentId = node.parentId;
        desc.translation = node.translation;
        desc.rotation = node.rotation;
        desc.scale = node.scale;
        desc.visible = node.visible;
        result.nodes.push_back(std::move(desc));
    }
    std::unordered_map<std::string, std::size_t> nodeIndices;
    nodeIndices.reserve(this->nodes.size());
    for (std::size_t index = 0; index < this->nodes.size(); ++index) {
        nodeIndices.emplace(this->nodes[index].id, index);
    }
    std::vector<azurerender::internal::Matrix4> nodeWorld(
        this->nodes.size());
    std::vector<std::uint8_t> nodeState(this->nodes.size(), 0);
    const auto resolveNodeWorld = [&](const std::size_t index,
        auto&& self) -> const azurerender::internal::Matrix4& {
        if (nodeState[index] == 2) {
            return nodeWorld[index];
        }
        if (nodeState[index] == 1) {
            throw std::runtime_error(
                "Scene transform hierarchy contains a cycle");
        }
        nodeState[index] = 1;
        const azurerender::SceneNode& node = this->nodes[index];
        const azurerender::internal::Matrix4 local =
            azurerender::scene::composeTrs(
                node.translation, node.rotation, node.scale);
        const auto parent = nodeIndices.find(node.parentId);
        if (!node.parentId.empty() && parent != nodeIndices.end()) {
            nodeWorld[index] = azurerender::internal::multiply(
                self(parent->second, self), local);
        } else {
            nodeWorld[index] = local;
        }
        nodeState[index] = 2;
        return nodeWorld[index];
    };
    result.lights.reserve(this->lights.size());
    for (const azurerender::SceneLight& light : this->lights) {
        const auto node = nodeIndices.find(light.nodeId);
        if (node == nodeIndices.end()) {
            throw std::runtime_error(
                "Scene light references missing node: " + light.nodeId);
        }
        const auto& world = resolveNodeWorld(node->second, resolveNodeWorld);
        const auto position = azurerender::internal::transformPosition(
            world, {0.0F, 0.0F, 0.0F});
        result.lights.push_back({
            light.id,
            light.nodeId,
            position,
            light.color,
            light.intensity,
            light.radius,
            light.enabled,
        });
    }
    return result;
}
}  // namespace azurerender
