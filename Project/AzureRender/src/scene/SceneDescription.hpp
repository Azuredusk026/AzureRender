#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "scene/TransformMath.hpp"

namespace azurerender::scene {

// One gltf asset a scene references. The first resource is the hero asset:
// its skinning and animation features stay fully active. Later resources
// render at bind pose in the current stage.
struct SceneResourceDesc {
    std::string id;
    std::string path;
};

// One placed occurrence. Nodes sharing a resourceId instance the same mesh
// with different transforms.
struct SceneNodeDesc {
    std::string id;
    std::string resourceId;
    std::string parentId;
    std::array<float, 3> translation{0.0F, 0.0F, 0.0F};
    std::array<float, 3> rotation{0.0F, 0.0F, 0.0F};
    std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
    bool visible = true;
};

struct SceneLightDesc {
    std::string id;
    std::string nodeId;
    std::array<float, 3> position{0.0F, 0.0F, 0.0F};
    std::array<float, 3> color{1.0F, 1.0F, 1.0F};
    float intensity = 1.0F;
    float radius = 5.0F;
    bool enabled = true;
};

// Engine-provided scene content for renderers. Replaces the single-asset
// assumption: a scene is a set of assets and placed nodes, and renderers
// build instances from it.
struct SceneDescription {
    std::vector<SceneResourceDesc> resources;
    std::vector<SceneNodeDesc> nodes;
    std::vector<SceneLightDesc> lights;
};

// Resolves editor node links into deterministic world transforms. The
// serializer owns node ids; the renderer consumes this compact array and does
// not need to know how the document is stored. A missing parent is treated as
// a root. Cycles keep the local transform and are reported by the caller via
// the optional cycle count.
[[nodiscard]] inline std::vector<azurerender::internal::Matrix4>
resolveNodeWorldTransforms(
    const SceneDescription& scene,
    std::size_t* cycleCount = nullptr) {
    std::unordered_map<std::string, std::size_t> indices;
    indices.reserve(scene.nodes.size());
    for (std::size_t index = 0; index < scene.nodes.size(); ++index) {
        if (!scene.nodes[index].id.empty()) {
            indices.emplace(scene.nodes[index].id, index);
        }
    }
    std::vector<azurerender::internal::Matrix4> result(scene.nodes.size());
    std::vector<std::uint8_t> state(scene.nodes.size(), 0);
    if (cycleCount != nullptr) {
        *cycleCount = 0;
    }
    const auto visit = [&](const std::size_t index, auto&& self)
        -> azurerender::internal::Matrix4 {
        if (state[index] == 2) {
            return result[index];
        }
        const SceneNodeDesc& node = scene.nodes[index];
        const azurerender::internal::Matrix4 local = composeTrs(
            node.translation, node.rotation, node.scale);
        if (state[index] == 1) {
            if (cycleCount != nullptr) {
                ++*cycleCount;
            }
            state[index] = 2;
            result[index] = local;
            return local;
        }
        state[index] = 1;
        azurerender::internal::Matrix4 world = local;
        const auto parent = indices.find(node.parentId);
        if (!node.parentId.empty() && parent != indices.end()
            && parent->second != index) {
            world = azurerender::internal::multiply(
                self(parent->second, self), local);
        }
        state[index] = 2;
        result[index] = world;
        return world;
    };
    for (std::size_t index = 0; index < scene.nodes.size(); ++index) {
        static_cast<void>(visit(index, visit));
    }
    return result;
}

}  // namespace azurerender::scene
