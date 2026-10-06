#pragma once

#include "SceneModel.hpp"
#include "ecs/Components.hpp"
#include "ecs/World.hpp"
#include "runtime/AssetDatabase.hpp"
#include "runtime/Level.hpp"
#include "runtime/PresentationRuntime.hpp"
#include "editor/commands/DocumentVersion.hpp"
#include <memory>
#include <vector>

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace azurerender {
class AssetImportJob;

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
    static std::shared_ptr<EditorContext> openProject(const std::filesystem::path& path);
    bool isProject() const noexcept { return assets_ != nullptr; }
    const Project& project() const { return *project_; }
    AssetDatabase& assets() { if(!assets_)throw std::logic_error("Asset access requires a project");return *assets_; }
    const AssetDatabase& assets() const { if(!assets_)throw std::logic_error("Asset access requires a project");return *assets_; }
    std::string importAsset(const std::filesystem::path& path);
    const nlohmann::json& importSummary() const noexcept { return importSummary_; }
    void startImport(const std::filesystem::path& path);
    void cancelImport();
    bool importing() const noexcept { return importJob_!=nullptr; }
    float importProgress() const;
    std::optional<std::string> pollImport();
    void placeResource(const std::string& resource, const std::string& nodeId = {});
    void createNode(const std::string& nodeId);
    void placePrefab(const std::string& asset, const std::string& instance);
    void previewAnimation(const std::string& state, double time, const std::string& previous = {}, double crossfade = 0);
    void clearAnimationPreview() noexcept { animationPreview_.reset(); }
    const std::optional<NodeAnimationFrame>& animationPreview() const noexcept { return animationPreview_; }
    void selectNodes(std::vector<std::size_t> indices);
    const std::vector<std::size_t>& selectedNodes() const noexcept { return selectedNodes_; }
    void duplicateSelection();
    void deleteSelection();
    nlohmann::json componentData(const std::string& node, const std::string& type) const;
    void setComponentField(const std::string& type, const std::string& field, const nlohmann::json& value);
    void addGameplayComponent(const std::string& type);
    std::map<std::string,nlohmann::json> runtimeComponents() const;
    nlohmann::json levelDocument() const;

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

    void markDirty() noexcept { dirty_ = true; ++revision_; closeEditMerge(); }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
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
    nlohmann::json documentContent() const;
    DocumentVersion documentVersion() const;
    std::size_t undoCount() const noexcept { return undoStack_.size(); }
    std::size_t redoCount() const noexcept { return redoStack_.size(); }
    void closeEditMerge() noexcept { mergeKey_.clear(); }
    void notifyAssetVersionChanged() { ++revision_;closeEditMerge();clearAnimationPreview(); }
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
    void setDebugProjection(std::array<float,16> value) { debugProjection_=value; }
    void setPickTargets(std::map<std::string,std::array<float,3>> targets) { pickTargets_=std::move(targets); }
    const std::map<std::string,std::array<float,3>>& pickTargets() const { return pickTargets_; }
    std::optional<std::array<float,2>> projectDebugPoint(const std::array<float,3>& point) const {
        const auto& m=debugProjection_;const float w=m[3]*point[0]+m[7]*point[1]+m[11]*point[2]+m[15];
        if(w<=.001F)return {};
        return std::array<float,2>{(.5F*(m[0]*point[0]+m[4]*point[1]+m[8]*point[2]+m[12])/w)+.5F,
            (.5F*(m[1]*point[0]+m[5]*point[1]+m[9]*point[2]+m[13])/w)+.5F};
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
    friend class EditTransaction;
    friend class EditService;
    std::string documentId_=newDocumentIdentity();
    std::uint64_t revision_=1,mergeRevision_=0;
    std::string mergeKey_;
    std::vector<std::size_t> mergeSelection_;
    struct Snapshot {
        SceneDocument scene;
        std::size_t selectedNodeIndex = 0;
        std::map<std::string,nlohmann::json> components;
        std::vector<std::size_t> selectedNodes;
        nlohmann::json sourceLevel;
        std::map<std::filesystem::path,std::string> resourceReferences;
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
    std::array<float,16> debugProjection_{};
    std::map<std::string,std::array<float,3>> pickTargets_;
    bool dirty_ = false;
    std::vector<std::string> consoleMessages_;
    std::vector<Snapshot> undoStack_;
    std::vector<Snapshot> redoStack_;
    std::vector<std::size_t> selectedNodes_{0};
    std::unique_ptr<Project> project_;
    std::unique_ptr<AssetDatabase> assets_;
    nlohmann::json sourceLevel_;
    std::map<std::filesystem::path,std::string> resourceReferences_;
    std::shared_ptr<AssetImportJob> importJob_;
    std::string commitImport(AssetImportJob& job);
    std::map<std::string,nlohmann::json> components_;
    std::optional<NodeAnimationFrame> animationPreview_;
    nlohmann::json importSummary_=nlohmann::json::object();
    void validateComponentReferences(const SceneNode& node, const nlohmann::json& data,
        const SceneDocument& scene) const;
    std::vector<std::pair<std::string, std::filesystem::file_time_type>>
        resourceWriteTimes_;
};

}  // namespace azurerender
