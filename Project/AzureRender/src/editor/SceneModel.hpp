#pragma once

#include "render/RenderSettings.hpp"

#include <cstdint>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace azurerender {

struct SceneResource {
    std::string id;
    std::string type;
    std::filesystem::path path;
};

struct SceneNode {
    std::string id;
    std::string name;
    std::string parentId;
    std::string resourceId;
    bool visible = true;
    std::array<float, 3> translation{0.0F, 0.0F, 0.0F};
    std::array<float, 3> rotation{0.0F, 0.0F, 0.0F};
    std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
    std::string prefabSource;
    std::string instanceOf;
};

// Point light attached to a scene node. The node supplies world placement;
// these fields describe the emitter's appearance and range.
struct SceneLight {
    std::string id;
    std::string nodeId;
    std::array<float, 3> color{1.0F, 1.0F, 1.0F};
    float intensity = 1.0F;
    float radius = 5.0F;
    bool enabled = true;
};

struct SceneDocument {
    static constexpr std::uint32_t kSchemaVersion = 3;

    std::string sceneId = "untitled";
    std::vector<SceneResource> resources;
    std::vector<SceneNode> nodes;
    std::vector<SceneLight> lights;
    // Scene renderer selector persisted through renderSettings.sceneType.
    RenderSettings renderSettings;

    static SceneDocument fromAsset(
        const std::filesystem::path& assetPath);
    static SceneDocument load(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;
};

}  // namespace azurerender
