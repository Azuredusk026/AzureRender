#pragma once

#include "SceneModel.hpp"
#include "ecs/Components.hpp"
#include "ecs/World.hpp"
#include <vector>

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace azurerender {

class EditorContext final {
public:
    struct ResourceStatus {
        std::string id;
        std::filesystem::path path;
        bool exists = false;
        std::uintmax_t byteSize = 0;
        std::size_t dependentNodeCount = 0;
    };
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
    void setGizmoTranslation(std::array<float, 3> value);
    void setGizmoRotation(std::array<float, 3> value);
    void setGizmoScale(std::array<float, 3> value);

    void markDirty() noexcept { dirty_ = true; }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    void save();
    void reload();
    void addChildNode(std::size_t parentIndex);
    void removeNode(std::size_t index);
    void setSelectedNodeName(std::string name);
    void setSelectedNodeVisible(bool visible);
    void setSelectedNodePrefab(std::string prefabSource);
    void setSelectedNodeInstance(std::string instanceOf);

    void beginEdit();
    [[nodiscard]] bool canUndo() const noexcept { return !undoStack_.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !redoStack_.empty(); }
    bool undo();
    bool redo();
    [[nodiscard]] std::vector<ResourceStatus> resourceStatuses() const;
    std::size_t reloadChangedAssets();

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
    [[nodiscard]] azurerender::ecs::World& ecs() noexcept { return ecsWorld_; }
    [[nodiscard]] const azurerender::ecs::World& ecs() const noexcept { return ecsWorld_; }
    [[nodiscard]] azurerender::ecs::Entity entityForNode(std::size_t index) const noexcept;
    void syncComponents();
    [[nodiscard]] std::size_t visibleRenderableCount() const noexcept;
    [[nodiscard]] GizmoMode gizmoMode() const noexcept { return gizmoMode_; }
    void setGizmoMode(const GizmoMode value) noexcept {
        gizmoMode_ = value;
    }

    void log(std::string message);
    [[nodiscard]] const std::vector<std::string>& consoleMessages() const noexcept {
        return consoleMessages_;
    }

private:
    struct Snapshot {
        SceneDocument scene;
        std::size_t selectedNodeIndex = 0;
    };
    [[nodiscard]] Snapshot snapshot() const;
    void restore(Snapshot snapshot);
    void rebuildEntities();
    void refreshSelectedTransform();
    [[nodiscard]] std::filesystem::path resolvedResourcePath(
        const SceneResource& resource) const;
    void updateResourceWriteTimes();
    mutable azurerender::ecs::World ecsWorld_;
    std::vector<azurerender::ecs::Entity> nodeEntities_;
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
    std::vector<Snapshot> undoStack_;
    std::vector<Snapshot> redoStack_;
    std::vector<std::pair<std::string, std::filesystem::file_time_type>>
        resourceWriteTimes_;
};

}  // namespace azurerender
