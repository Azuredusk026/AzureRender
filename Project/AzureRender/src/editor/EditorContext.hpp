#pragma once

#include "SceneModel.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace azurerender {

class EditorContext final {
public:
    EditorContext(SceneDocument document, std::filesystem::path scenePath);

    [[nodiscard]] SceneDocument& scene() noexcept { return scene_; }
    [[nodiscard]] const SceneDocument& scene() const noexcept { return scene_; }
    [[nodiscard]] const std::filesystem::path& scenePath() const noexcept {
        return scenePath_;
    }

    [[nodiscard]] RenderSettings& renderSettings() noexcept;
    [[nodiscard]] const RenderSettings& renderSettings() const noexcept;
    void attachRenderSettings(RenderSettings& settings) noexcept;
    void detachRenderSettings() noexcept;

    [[nodiscard]] std::size_t selectedNodeIndex() const noexcept {
        return selectedNodeIndex_;
    }
    [[nodiscard]] SceneNode* selectedNode() noexcept;
    [[nodiscard]] const SceneNode* selectedNode() const noexcept;
    void selectNode(std::size_t index);
    void selectNextNode();

    void markDirty() noexcept { dirty_ = true; }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    void save();

    void log(std::string message);
    [[nodiscard]] const std::vector<std::string>& consoleMessages() const noexcept {
        return consoleMessages_;
    }

private:
    SceneDocument scene_;
    std::filesystem::path scenePath_;
    RenderSettings* attachedRenderSettings_ = nullptr;
    std::size_t selectedNodeIndex_ = 0;
    bool dirty_ = false;
    std::vector<std::string> consoleMessages_;
};

}  // namespace azurerender
