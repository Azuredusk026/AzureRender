#pragma once

#include "SceneModel.hpp"

#include <array>
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

    [[nodiscard]] const std::array<float, 3>& gizmoTranslation() const noexcept {
        return gizmoTranslation_;
    }
    [[nodiscard]] const std::array<float, 3>& gizmoRotation() const noexcept {
        return gizmoRotation_;
    }
    [[nodiscard]] const std::array<float, 3>& gizmoScale() const noexcept {
        return gizmoScale_;
    }
    void setGizmoTranslation(std::array<float, 3> value) {
        gizmoTranslation_ = value;
        markDirty();
    }
    void setGizmoRotation(std::array<float, 3> value) {
        gizmoRotation_ = value;
        markDirty();
    }
    void setGizmoScale(std::array<float, 3> value) {
        gizmoScale_ = value;
        markDirty();
    }

    void markDirty() noexcept { dirty_ = true; }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    void save();
    void reload();
    void addChildNode(std::size_t parentIndex);
    void removeNode(std::size_t index);

    enum class GizmoMode { Translate, Rotate, Scale };

    struct GizmoScreenData {
        bool valid = false;
        float centerX = 0.0F;
        float centerY = 0.0F;
        float axisXScreenX = 1.0F;
        float axisXScreenY = 0.0F;
        float axisYScreenX = 0.0F;
        float axisYScreenY = -1.0F;
        float axisZScreenX = 0.7F;
        float axisZScreenY = 0.7F;
        float pixelToWorld = 0.005F;
    };
    [[nodiscard]] const GizmoScreenData& gizmoScreen() const noexcept {
        return gizmoScreen_;
    }
    void setGizmoScreen(const GizmoScreenData& value) {
        gizmoScreen_ = value;
    }
    [[nodiscard]] GizmoMode gizmoMode() const noexcept { return gizmoMode_; }
    void setGizmoMode(const GizmoMode value) noexcept {
        gizmoMode_ = value;
    }

    void log(std::string message);
    [[nodiscard]] const std::vector<std::string>& consoleMessages() const noexcept {
        return consoleMessages_;
    }

private:
    SceneDocument scene_;
    std::filesystem::path scenePath_;
    RenderSettings* attachedRenderSettings_ = nullptr;
    std::size_t selectedNodeIndex_ = 0;
    std::array<float, 3> gizmoTranslation_{0.0F, 0.0F, 0.0F};
    std::array<float, 3> gizmoRotation_{0.0F, 0.0F, 0.0F};
    std::array<float, 3> gizmoScale_{1.0F, 1.0F, 1.0F};
    GizmoMode gizmoMode_ = GizmoMode::Translate;
    GizmoScreenData gizmoScreen_;
    bool dirty_ = false;
    std::vector<std::string> consoleMessages_;
};

}  // namespace azurerender
