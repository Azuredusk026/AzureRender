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

}  // namespace azurerender
