#include "EditorContext.hpp"

#include "diagnostics/RuntimeDiagnostics.hpp"

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

void EditorContext::log(std::string message) {
    constexpr std::size_t kConsoleCapacity = 256;
    if (consoleMessages_.size() == kConsoleCapacity) {
        consoleMessages_.erase(consoleMessages_.begin());
    }
    RuntimeDiagnostics::instance().info("editor", message);
    consoleMessages_.push_back(std::move(message));
}

}  // namespace azurerender
