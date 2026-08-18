#include "EditorSession.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace azurerender {

EditorSession::EditorSession(std::shared_ptr<EditorContext> context)
    : context_(std::move(context)) {
    if (context_ == nullptr) {
        throw std::invalid_argument("Editor session requires a context");
    }
}

bool EditorSession::execute(const EditorCommand command) noexcept {
    lastError_.clear();
    if (command == EditorCommand::ResetLayout) {
        layoutResetRequested_ = true;
        context_->log("Default editor layout requested");
        return true;
    }
    if (command == EditorCommand::Undo) {
        return context_->undo();
    }
    if (command == EditorCommand::Redo) {
        return context_->redo();
    }
    if (command == EditorCommand::ReloadAssets) {
        const std::size_t changed = context_->reloadChangedAssets();
        assetReloadRequested_ = true;
        context_->log("Asset reload requested; changed files: "
            + std::to_string(changed));
        return true;
    }
    if (command == EditorCommand::Capture) {
        captureRequested_ = true;
        context_->log("Viewport capture requested: " + captureLabel_);
        return true;
    }
    try {
        if (command == EditorCommand::Reload) {
            context_->reload();
            return true;
        }
        context_->save();
        return true;
    } catch (const std::exception& exception) {
        lastError_ = exception.what();
    } catch (...) {
        lastError_ = "Unknown editor command failure";
    }
    context_->log("ERROR: " + lastError_);
    return false;
}

bool EditorSession::saveOnClose() noexcept {
    return !context_->dirty() || execute(EditorCommand::Save);
}

bool EditorSession::consumeLayoutResetRequest() noexcept {
    const bool requested = layoutResetRequested_;
    layoutResetRequested_ = false;
    return requested;
}

bool EditorSession::consumeAssetReloadRequest() noexcept {
    const bool requested = assetReloadRequested_;
    assetReloadRequested_ = false;
    return requested;
}

bool EditorSession::consumeCaptureRequest(std::string& label) noexcept {
    if (!captureRequested_) {
        return false;
    }
    captureRequested_ = false;
    label = captureLabel_;
    return true;
}

void EditorSession::setCaptureLabel(std::string label) {
    if (label.empty()) {
        label = "editor_capture";
    }
    for (char& character : label) {
        const bool valid = (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9')
            || character == '-' || character == '_';
        if (!valid) {
            character = '_';
        }
    }
    captureLabel_ = std::move(label);
}

}  // namespace azurerender
