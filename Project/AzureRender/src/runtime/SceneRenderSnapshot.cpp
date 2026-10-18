#include "runtime/SceneDocument.hpp"
#include "scene/SceneDescription.hpp"

#include <stdexcept>

namespace azurerender {
scene::SceneDescription SceneDocument::renderDescription() const {
    scene::SceneDescription result;
    reflectionProbe.validate();
    result.reflectionProbe = reflectionProbe;
    irradianceVolume.validate();
    result.irradianceVolume = irradianceVolume;
    result.resources.reserve(this->resources.size());
    for (const azurerender::SceneResource& resource :
         this->resources) {
        result.resources.push_back(
            {resource.id, resource.path.u8string()});
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
        if(!node.materialSlots.empty()){
            if(node.resolvedMaterials.size()!=node.materialSlots.size())throw std::invalid_argument("Material bindings must be resolved before rendering");
            const auto resource=std::find_if(resources.begin(),resources.end(),[&](const auto& item){return item.id==node.resourceId;});
            if(resource==resources.end())throw std::invalid_argument("Material override requires a model resource");
            desc.resourceId=node.resourceId+"::materials::"+node.id;
            result.resources.push_back({desc.resourceId,resource->path.u8string(),node.resolvedMaterials});
        }
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
            light.shape,
        });
        auto& shape = result.lights.back().shape;
        shape.validate();
        const auto local = shape.direction;
        float lengthSquared = 0;
        for (unsigned axis = 0; axis < 3; ++axis) {
            shape.direction[axis] = world[axis]*local[0] + world[axis+4]*local[1] + world[axis+8]*local[2];
            lengthSquared += shape.direction[axis]*shape.direction[axis];
        }
        if (!(lengthSquared > 1e-12F)) throw std::runtime_error("Degenerate light node transform");
        for (auto& value : shape.direction) value /= std::sqrt(lengthSquared);
    }
    return result;
}
}  // namespace azurerender
