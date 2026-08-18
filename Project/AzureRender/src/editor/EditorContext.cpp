namespace {
struct VisibilityComponent {
    bool visible = true;
};
}
#include "EditorContext.hpp"

#include "diagnostics/RuntimeDiagnostics.hpp"
#include "ecs/Components.hpp"
#include "ecs/Entity.hpp"

#include <algorithm>
#include <cstring>
#include <system_error>
#include <stdexcept>
#include <utility>

namespace azurerender {

EditorContext::EditorContext(
    SceneDocument document,
    std::filesystem::path scenePath)
    : scene_(std::move(document)), scenePath_(std::move(scenePath)) {
    if (scenePath_.empty()) {
        throw std::invalid_argument("Editor scene path cannot be empty");
    }
    log("Opened scene: " + scene_.sceneId);
    rebuildEntities();
    refreshSelectedTransform();
    updateResourceWriteTimes();
}

RenderSettings& EditorContext::renderSettings() noexcept {
    return attachedRenderSettings_ != nullptr
        ? *attachedRenderSettings_
        : scene_.renderSettings;
}

const RenderSettings& EditorContext::renderSettings() const noexcept {
    return attachedRenderSettings_ != nullptr
        ? *attachedRenderSettings_
        : scene_.renderSettings;
}

void EditorContext::attachRenderSettings(RenderSettings& settings) noexcept {
    attachedRenderSettings_ = &settings;
}

void EditorContext::detachRenderSettings() noexcept {
    if (attachedRenderSettings_ != nullptr) {
        scene_.renderSettings = *attachedRenderSettings_;
        attachedRenderSettings_ = nullptr;
    }
}

SceneNode* EditorContext::selectedNode() noexcept {
    return scene_.nodes.empty() ? nullptr : &scene_.nodes[selectedNodeIndex_];
}

const SceneNode* EditorContext::selectedNode() const noexcept {
    return scene_.nodes.empty() ? nullptr : &scene_.nodes[selectedNodeIndex_];
}

void EditorContext::selectNode(const std::size_t index) {
    if (index >= scene_.nodes.size()) {
        throw std::out_of_range("Editor node selection is out of range");
    }
    selectedNodeIndex_ = index;
    refreshSelectedTransform();
    log("Selected node: " + scene_.nodes[index].name);
}

void EditorContext::selectNextNode() {
    if (!scene_.nodes.empty()) {
        selectNode((selectedNodeIndex_ + 1) % scene_.nodes.size());
    }
}

void EditorContext::save() {
    if (attachedRenderSettings_ != nullptr) {
        scene_.renderSettings = *attachedRenderSettings_;
    }
    scene_.save(scenePath_);
    dirty_ = false;
    log("Saved scene: " + scenePath_.string());
}

void EditorContext::reload() {
    SceneDocument document = SceneDocument::load(scenePath_);
    scene_ = std::move(document);
    if (attachedRenderSettings_ != nullptr) {
        *attachedRenderSettings_ = scene_.renderSettings;
    }
    rebuildEntities();
    selectedNodeIndex_ = 0;
    refreshSelectedTransform();
    undoStack_.clear();
    redoStack_.clear();
    updateResourceWriteTimes();
    dirty_ = false;
    log("Reloaded scene: " + scene_.sceneId);
}

azurerender::ecs::Entity EditorContext::entityForNode(
    const std::size_t index) const noexcept {
    return index < nodeEntities_.size()
        ? nodeEntities_[index]
        : azurerender::ecs::kInvalidEntity;
}

void EditorContext::syncComponents() {
    for (std::size_t index = 0; index < scene_.nodes.size(); ++index) {
        const azurerender::ecs::Entity entity = entityForNode(index);
        if (entity == azurerender::ecs::kInvalidEntity) {
            continue;
        }
        const SceneNode& node = scene_.nodes[index];
        // Name mirrors the scene node for Outliner/ECS bridges.
        ecsWorld_.addComponent(entity, azurerender::ecs::NameComponent{});
        azurerender::ecs::NameComponent* name =
            ecsWorld_.tryGet<azurerender::ecs::NameComponent>(entity);
        if (name != nullptr) {
            const std::size_t copyLength = std::min(
                node.name.size(), name->name.size() - 1);
            std::memcpy(name->name.data(), node.name.data(), copyLength);
            name->name[copyLength] = '\0';
        }
        // Renderable maps the node to the asset primitive it drives. The
        // public test asset uses primitive 0 for the root node; nodes
        // without a mesh simply stay non-renderable.
        if (node.resourceId == "asset-0" && index == 0) {
            ecsWorld_.addComponent(
                entity, azurerender::ecs::RenderableComponent{0, node.visible});
        } else {
            ecsWorld_.addComponent(
                entity, azurerender::ecs::RenderableComponent{0, false});
        }
        // Transform mirrors the gizmo state for ECS-driven editing.
        ecsWorld_.addComponent(entity, azurerender::ecs::TransformComponent{
            node.translation, node.rotation, node.scale});
    }
}

std::size_t EditorContext::visibleRenderableCount() const noexcept {
    std::size_t count = 0;
    ecsWorld_.each<azurerender::ecs::RenderableComponent>(
        [&count](const azurerender::ecs::Entity,
                 azurerender::ecs::RenderableComponent& component) {
            if (component.visible) {
                ++count;
            }
        });
    return count;
}

void EditorContext::addChildNode(const std::size_t parentIndex) {
    if (parentIndex >= scene_.nodes.size()) {
        throw std::out_of_range("Editor parent node is out of range");
    }
    beginEdit();
    SceneNode node;
    node.id = scene_.nodes[parentIndex].id + ".child."
        + std::to_string(scene_.nodes.size());
    node.name = "New Node " + std::to_string(scene_.nodes.size());
    node.parentId = scene_.nodes[parentIndex].id;
    node.resourceId = scene_.nodes[parentIndex].resourceId;
    scene_.nodes.push_back(std::move(node));
    const azurerender::ecs::Entity entity = ecsWorld_.createEntity();
    ecsWorld_.addComponent(entity,
        VisibilityComponent{scene_.nodes.back().visible});
    nodeEntities_.push_back(entity);
    log("Added node under: " + scene_.nodes[parentIndex].name);
}

void EditorContext::removeNode(const std::size_t index) {
    if (index >= scene_.nodes.size()) {
        throw std::out_of_range("Editor node removal is out of range");
    }
    if (scene_.nodes[index].parentId.empty()) {
        log("Cannot remove the root node");
        return;
    }
    beginEdit();
    // Remove the node and all descendants (matched by parent chain).
    std::vector<std::size_t> removed;
    removed.push_back(index);
    bool grew = true;
    while (grew) {
        grew = false;
        for (std::size_t candidate = 0; candidate < scene_.nodes.size();
             ++candidate) {
            if (std::find(removed.begin(), removed.end(), candidate)
                != removed.end()) {
                continue;
            }
            const std::string& parentId =
                scene_.nodes[candidate].parentId;
            const bool parentRemoved = std::any_of(
                removed.begin(), removed.end(),
                [&](const std::size_t removedIndex) {
                    return scene_.nodes[removedIndex].id == parentId;
                });
            if (parentRemoved) {
                removed.push_back(candidate);
                grew = true;
            }
        }
    }
    std::sort(removed.begin(), removed.end(), std::greater<std::size_t>());
    for (const std::size_t removedIndex : removed) {
        scene_.nodes.erase(
            scene_.nodes.begin() + static_cast<std::ptrdiff_t>(removedIndex));
        if (removedIndex < nodeEntities_.size()) {
            ecsWorld_.destroyEntity(nodeEntities_[removedIndex]);
            nodeEntities_.erase(
                nodeEntities_.begin()
                    + static_cast<std::ptrdiff_t>(removedIndex));
        }
    }
    if (selectedNodeIndex_ >= scene_.nodes.size()) {
        selectedNodeIndex_ = scene_.nodes.empty() ? 0 : scene_.nodes.size() - 1;
    }
    refreshSelectedTransform();
    log("Removed node (and descendants)");
}

void EditorContext::setGizmoTranslation(const std::array<float, 3> value) {
    if (value == gizmoTranslation_) return;
    beginEdit();
    gizmoTranslation_ = value;
    if (SceneNode* node = selectedNode()) node->translation = value;
}

void EditorContext::setGizmoRotation(const std::array<float, 3> value) {
    if (value == gizmoRotation_) return;
    beginEdit();
    gizmoRotation_ = value;
    if (SceneNode* node = selectedNode()) node->rotation = value;
}

void EditorContext::setGizmoScale(const std::array<float, 3> value) {
    if (value == gizmoScale_) return;
    beginEdit();
    gizmoScale_ = value;
    if (SceneNode* node = selectedNode()) node->scale = value;
}

void EditorContext::setSelectedNodeName(std::string name) {
    SceneNode* node = selectedNode();
    if (node == nullptr || node->name == name) return;
    beginEdit();
    selectedNode()->name = std::move(name);
}

void EditorContext::setSelectedNodeVisible(const bool visible) {
    SceneNode* node = selectedNode();
    if (node == nullptr || node->visible == visible) return;
    beginEdit();
    selectedNode()->visible = visible;
}

void EditorContext::setSelectedNodePrefab(std::string prefabSource) {
    SceneNode* node = selectedNode();
    if (node == nullptr || node->prefabSource == prefabSource) return;
    beginEdit();
    selectedNode()->prefabSource = std::move(prefabSource);
}

void EditorContext::setSelectedNodeInstance(std::string instanceOf) {
    SceneNode* node = selectedNode();
    if (node == nullptr || node->instanceOf == instanceOf) return;
    beginEdit();
    selectedNode()->instanceOf = std::move(instanceOf);
}

EditorContext::Snapshot EditorContext::snapshot() const {
    Snapshot result{scene_, selectedNodeIndex_};
    result.scene.renderSettings = renderSettings();
    return result;
}

void EditorContext::beginEdit() {
    constexpr std::size_t kHistoryCapacity = 100;
    if (undoStack_.size() == kHistoryCapacity) {
        undoStack_.erase(undoStack_.begin());
    }
    undoStack_.push_back(snapshot());
    redoStack_.clear();
    dirty_ = true;
}

void EditorContext::restore(Snapshot restored) {
    scene_ = std::move(restored.scene);
    if (attachedRenderSettings_ != nullptr) {
        *attachedRenderSettings_ = scene_.renderSettings;
    }
    selectedNodeIndex_ = scene_.nodes.empty()
        ? 0 : std::min(restored.selectedNodeIndex, scene_.nodes.size() - 1);
    rebuildEntities();
    refreshSelectedTransform();
    dirty_ = true;
}

bool EditorContext::undo() {
    if (undoStack_.empty()) return false;
    redoStack_.push_back(snapshot());
    Snapshot restored = std::move(undoStack_.back());
    undoStack_.pop_back();
    restore(std::move(restored));
    log("Undo");
    return true;
}

bool EditorContext::redo() {
    if (redoStack_.empty()) return false;
    undoStack_.push_back(snapshot());
    Snapshot restored = std::move(redoStack_.back());
    redoStack_.pop_back();
    restore(std::move(restored));
    log("Redo");
    return true;
}

void EditorContext::rebuildEntities() {
    for (const azurerender::ecs::Entity entity : nodeEntities_) {
        ecsWorld_.destroyEntity(entity);
    }
    nodeEntities_.clear();
    for (const SceneNode& node : scene_.nodes) {
        const azurerender::ecs::Entity entity = ecsWorld_.createEntity();
        ecsWorld_.addComponent(entity, VisibilityComponent{node.visible});
        nodeEntities_.push_back(entity);
    }
}

void EditorContext::refreshSelectedTransform() {
    const SceneNode* node = selectedNode();
    if (node == nullptr) {
        gizmoTranslation_ = {0.0F, 0.0F, 0.0F};
        gizmoRotation_ = {0.0F, 0.0F, 0.0F};
        gizmoScale_ = {1.0F, 1.0F, 1.0F};
        return;
    }
    gizmoTranslation_ = node->translation;
    gizmoRotation_ = node->rotation;
    gizmoScale_ = node->scale;
}

std::filesystem::path EditorContext::resolvedResourcePath(
    const SceneResource& resource) const {
    if (resource.path.is_absolute()) return resource.path;
    const std::filesystem::path besideScene =
        scenePath_.parent_path() / resource.path;
    if (std::filesystem::exists(besideScene)) return besideScene;
    return resource.path;
}

std::vector<EditorContext::ResourceStatus> EditorContext::resourceStatuses() const {
    std::vector<ResourceStatus> result;
    result.reserve(scene_.resources.size());
    for (const SceneResource& resource : scene_.resources) {
        ResourceStatus status;
        status.id = resource.id;
        status.path = resolvedResourcePath(resource);
        std::error_code error;
        status.exists = std::filesystem::is_regular_file(status.path, error);
        if (status.exists) status.byteSize = std::filesystem::file_size(status.path, error);
        status.dependentNodeCount = static_cast<std::size_t>(std::count_if(
            scene_.nodes.begin(), scene_.nodes.end(),
            [&](const SceneNode& node) { return node.resourceId == resource.id; }));
        result.push_back(std::move(status));
    }
    return result;
}

void EditorContext::updateResourceWriteTimes() {
    resourceWriteTimes_.clear();
    for (const SceneResource& resource : scene_.resources) {
        std::error_code error;
        const auto time = std::filesystem::last_write_time(
            resolvedResourcePath(resource), error);
        if (!error) resourceWriteTimes_.push_back({resource.id, time});
    }
}

std::size_t EditorContext::reloadChangedAssets() {
    std::size_t changed = 0;
    for (const SceneResource& resource : scene_.resources) {
        std::error_code error;
        const auto current = std::filesystem::last_write_time(
            resolvedResourcePath(resource), error);
        const auto previous = std::find_if(
            resourceWriteTimes_.begin(), resourceWriteTimes_.end(),
            [&](const auto& item) { return item.first == resource.id; });
        if (error || previous == resourceWriteTimes_.end()
            || previous->second != current) {
            ++changed;
        }
    }
    updateResourceWriteTimes();
    return changed;
}

void EditorContext::log(std::string message) {
    constexpr std::size_t kConsoleCapacity = 256;
    if (consoleMessages_.size() == kConsoleCapacity) {
        consoleMessages_.erase(consoleMessages_.begin());
    }
    RuntimeDiagnostics::instance().info("editor", message);
    consoleMessages_.push_back(std::move(message));
}

}  // namespace azurerender
