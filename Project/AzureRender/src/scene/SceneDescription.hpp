#pragma once

#include <array>
#include <string>
#include <vector>

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
    std::string resourceId;
    std::string parentId;
    std::array<float, 3> translation{0.0F, 0.0F, 0.0F};
    std::array<float, 3> rotation{0.0F, 0.0F, 0.0F};
    std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
    bool visible = true;
};

// Engine-provided scene content for renderers. Replaces the single-asset
// assumption: a scene is a set of assets and placed nodes, and renderers
// build instances from it.
struct SceneDescription {
    std::vector<SceneResourceDesc> resources;
    std::vector<SceneNodeDesc> nodes;
};

}  // namespace azurerender::scene
