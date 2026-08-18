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

struct SceneDocument {
    static constexpr std::uint32_t kSchemaVersion = 2;

    std::string sceneId = "untitled";
    std::vector<SceneResource> resources;
    std::vector<SceneNode> nodes;
    // Scene renderer selector persisted through renderSettings.sceneType.
    RenderSettings renderSettings;

    static SceneDocument fromAsset(
        const std::filesystem::path& assetPath);
    static SceneDocument load(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;
};

}  // namespace azurerender
