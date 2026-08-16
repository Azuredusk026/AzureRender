#pragma once

#include "render/RenderSettings.hpp"

#include <cstdint>
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
};

struct SceneDocument {
    static constexpr std::uint32_t kSchemaVersion = 1;

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
