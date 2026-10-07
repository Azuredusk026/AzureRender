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
#include <chrono>
#include <cstring>
#include <fstream>
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
    if(scene_.nodes.empty())selectedNodes_.clear();
    rebuildEntities();
    refreshSelectedTransform();
    updateResourceWriteTimes();
    checkpointSaved();
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
    return selectedNodes_.empty()||selectedNodeIndex_>=scene_.nodes.size() ? nullptr : &scene_.nodes[selectedNodeIndex_];
}

const SceneNode* EditorContext::selectedNode() const noexcept {
    return selectedNodes_.empty()||selectedNodeIndex_>=scene_.nodes.size() ? nullptr : &scene_.nodes[selectedNodeIndex_];
}

void EditorContext::selectNode(const std::size_t index) {
    if (index >= scene_.nodes.size()) {
        throw std::out_of_range("Editor node selection is out of range");
    }
    if(selectedNodes_!=std::vector<std::size_t>{index}) { ++revision_;closeEditMerge(); }
    if(index!=selectedNodeIndex_)clearAnimationPreview();
    selectedNodeIndex_ = index;
    selectedNodes_ = {index};
    refreshSelectedTransform();
    log("Selected node: " + scene_.nodes[index].name);
}

void EditorContext::selectNextNode() {
    if (!scene_.nodes.empty()) {
        selectNode((selectedNodeIndex_ + 1) % scene_.nodes.size());
    }
}

std::filesystem::path EditorContext::recoveryPath() const {
    const auto directory = project_ ? project_->file.parent_path() : scenePath_.parent_path();
    std::uint64_t identity = 14695981039346656037ULL;
    for (const unsigned char byte : std::filesystem::absolute(scenePath_).generic_string()) {
        identity = (identity ^ byte) * 1099511628211ULL;
    }
    return directory / ".azure" / "recovery"
        / (std::to_string(identity) + "-" + scenePath_.filename().string());
}

void EditorContext::save() {
    if (std::filesystem::is_regular_file(scenePath_)) {
        std::ifstream input(scenePath_, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot read recovery source");
        const std::string bytes{std::istreambuf_iterator<char>(input), {}};
        if (input.bad()) throw std::runtime_error("Cannot read complete recovery source");
        if (assets_ && scenePath_.extension() == ".azurelevel")
            (void)Level::parse(nlohmann::json::parse(bytes), *assets_);
        else (void)SceneDocument::load(scenePath_);
        const auto destination = recoveryPath();
        std::filesystem::create_directories(destination.parent_path());
        const auto writeVerified = [](const std::filesystem::path& target, const std::string& content) {
            const auto temporary = target.string() + ".tmp."
                + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            try {
                {
                    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
                    output.write(content.data(), static_cast<std::streamsize>(content.size()));
                    output.flush();
                    if (!output) throw std::runtime_error("Cannot write complete recovery document");
                }
                std::ifstream verification(temporary, std::ios::binary);
                const std::string actual{std::istreambuf_iterator<char>(verification), {}};
                if (verification.bad() || actual != content)
                    throw std::runtime_error("Recovery document verification failed");
                verification.close();
                std::filesystem::rename(temporary, target);
            } catch (...) {
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
                throw;
            }
        };
        writeVerified(destination, bytes);
        writeVerified(destination.string() + ".json", nlohmann::json{
            {"source", std::filesystem::absolute(scenePath_).generic_string()},
            {"size", bytes.size()}, {"schemaVersion", 1}}.dump(2));
    }
    if (attachedRenderSettings_ != nullptr) {
        scene_.renderSettings = *attachedRenderSettings_;
    }
    if (assets_ && scenePath_.extension()==".azurelevel") {
        auto document = levelDocument();
        auto level = Level::parse(document, *assets_);level.save(scenePath_);
        sourceLevel_ = std::move(document);assets_->refresh();
    } else if(assets_){auto portable=scene_;for(auto& resource:portable.resources){
        auto reference=resourceReferences_.at(resource.path);
        if(reference.find(":/")==std::string::npos){const auto& record=assets_->records().at(reference);reference=record.virtualPath;}
        resource.path=std::filesystem::u8path(reference);
    }portable.save(scenePath_);}else scene_.save(scenePath_);
    checkpointSaved();
    ++revision_;closeEditMerge();
    log("Saved scene: " + scenePath_.string());
}

void EditorContext::reload() {
    SceneDocument document;
    if (assets_ && scenePath_.extension()==".azurelevel") {
        assets_->refresh();auto level=Level::load(scenePath_,*assets_);document=level.scene;components_=level.components;
        std::ifstream file(scenePath_);file>>sourceLevel_;
    } else document = assets_?project_->loadStartupScene():SceneDocument::load(scenePath_);
    scene_ = std::move(document);
    clearAnimationPreview();
    if (attachedRenderSettings_ != nullptr) {
        *attachedRenderSettings_ = scene_.renderSettings;
    }
    rebuildEntities();
    selectedNodeIndex_ = 0;
    selectedNodes_ = scene_.nodes.empty()?std::vector<std::size_t>{}:std::vector<std::size_t>{0};
    refreshSelectedTransform();
    undoStack_.clear();
    redoStack_.clear();
    updateResourceWriteTimes();
    checkpointSaved();
    ++revision_;closeEditMerge();
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
        if ((assets_ && !node.resourceId.empty()) || (node.resourceId == "asset-0" && index == 0)) {
            ecsWorld_.addComponent(
                entity, azurerender::ecs::RenderableComponent{0, node.visible});
        } else {
            ecsWorld_.addComponent(
                entity, azurerender::ecs::RenderableComponent{0, false});
        }
        // Transform mirrors the gizmo state for ECS-driven editing.
        ecsWorld_.addComponent(entity, azurerender::ecs::TransformComponent{
            node.translation, node.rotation, node.scale});
        std::vector<azurerender::ecs::LightEmitterComponent> emitters;
        for (const SceneLight& light : scene_.lights) {
            if (light.nodeId == node.id) {
                emitters.push_back({
                    light.id,
                    light.color,
                    light.intensity,
                    light.radius,
                    light.enabled});
            }
        }
        if (!emitters.empty()) {
            ecsWorld_.addComponent(
                entity,
                azurerender::ecs::LightComponent{std::move(emitters)});
        } else {
            ecsWorld_.removeComponent<azurerender::ecs::LightComponent>(entity);
        }
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
    std::vector<std::string> selectionIds;
    for(const auto selected:selectedNodes_)selectionIds.push_back(scene_.nodes.at(selected).id);
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
        const auto id=scene_.nodes[removedIndex].id;
        components_.erase(id);
        scene_.lights.erase(std::remove_if(scene_.lights.begin(),scene_.lights.end(),
            [&](const SceneLight& light) { return light.nodeId==id; }),scene_.lights.end());
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
    selectedNodes_.clear();
    for(std::size_t candidate=0;candidate<scene_.nodes.size();++candidate)
        if(std::find(selectionIds.begin(),selectionIds.end(),scene_.nodes[candidate].id)!=selectionIds.end())selectedNodes_.push_back(candidate);
    if(selectedNodes_.empty()&&!selectionIds.empty()&&!scene_.nodes.empty())selectedNodes_.push_back(selectedNodeIndex_);
    if(!selectedNodes_.empty())selectedNodeIndex_=selectedNodes_.back();
    clearAnimationPreview();
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
    Snapshot result{scene_, selectedNodeIndex_, components_, selectedNodes_, sourceLevel_, resourceReferences_};
    result.scene.renderSettings = renderSettings();
    return result;
}

void EditorContext::beginEdit() {
    auto checkpoint=snapshot();
    constexpr std::size_t kHistoryCapacity = 100;
    if (undoStack_.size() == kHistoryCapacity) {
        undoStack_.erase(undoStack_.begin());
    }
    undoStack_.push_back(std::move(checkpoint));
    redoStack_.clear();
    dirty_ = true;
    ++revision_;closeEditMerge();
}

void EditorContext::restore(Snapshot restored) {
    scene_ = std::move(restored.scene);
    components_ = std::move(restored.components);
    sourceLevel_ = std::move(restored.sourceLevel);
    resourceReferences_ = std::move(restored.resourceReferences);
    clearAnimationPreview();
    selectedNodes_ = std::move(restored.selectedNodes);
    if (attachedRenderSettings_ != nullptr) {
        *attachedRenderSettings_ = scene_.renderSettings;
    }
    selectedNodeIndex_ = scene_.nodes.empty()
        ? 0 : std::min(restored.selectedNodeIndex, scene_.nodes.size() - 1);
    rebuildEntities();
    refreshSelectedTransform();
    refreshDirty();
}

bool EditorContext::undo() {
    if (undoStack_.empty()) return false;
    ++revision_;closeEditMerge();
    redoStack_.push_back(snapshot());
    Snapshot restored = std::move(undoStack_.back());
    undoStack_.pop_back();
    restore(std::move(restored));
    log("Undo");
    return true;
}

bool EditorContext::redo() {
    if (redoStack_.empty()) return false;
    ++revision_;closeEditMerge();
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
    syncComponents();
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
    if(assets_ && resource.path.generic_u8string().find(":/")!=std::string::npos)return assets_->resolveReference(resource.path.generic_u8string());
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
    if(changed&&assets_)try { assets_->refresh(); }
        catch(const std::exception& error) { updateResourceWriteTimes();log("Asset reload rejected: "+std::string(error.what()));return 0; }
    updateResourceWriteTimes();
    if(changed)notifyAssetVersionChanged();
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
