#include "EditorContext.hpp"

#include "diagnostics/RuntimeDiagnostics.hpp"

#include <algorithm>
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
    log("Selected node: " + scene_.nodes[index].name);
}

void EditorContext::selectNextNode() {
    if (!scene_.nodes.empty()) {
        selectNode((selectedNodeIndex_ + 1) % scene_.nodes.size());
    }
}

void EditorContext::save() {
    detachRenderSettings();
    scene_.save(scenePath_);
    dirty_ = false;
    log("Saved scene: " + scenePath_.string());
}

void EditorContext::reload() {
    detachRenderSettings();
    SceneDocument document = SceneDocument::load(scenePath_);
    scene_ = std::move(document);
    selectedNodeIndex_ = 0;
    dirty_ = false;
    log("Reloaded scene: " + scene_.sceneId);
}

void EditorContext::addChildNode(const std::size_t parentIndex) {
    if (parentIndex >= scene_.nodes.size()) {
        throw std::out_of_range("Editor parent node is out of range");
    }
    SceneNode node;
    node.id = scene_.nodes[parentIndex].id + ".child."
        + std::to_string(scene_.nodes.size());
    node.name = "New Node " + std::to_string(scene_.nodes.size());
    node.parentId = scene_.nodes[parentIndex].id;
    node.resourceId = scene_.nodes[parentIndex].resourceId;
    scene_.nodes.push_back(std::move(node));
    markDirty();
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
    }
    if (selectedNodeIndex_ >= scene_.nodes.size()) {
        selectedNodeIndex_ = scene_.nodes.empty() ? 0 : scene_.nodes.size() - 1;
    }
    markDirty();
    log("Removed node (and descendants)");
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
