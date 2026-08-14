#pragma once

#include "EditorContext.hpp"

#include <memory>
#include <string>

namespace azurerender {

enum class EditorCommand { Save, ResetLayout };

class EditorSession final {
public:
    explicit EditorSession(std::shared_ptr<EditorContext> context);

    [[nodiscard]] EditorContext& context() noexcept { return *context_; }
    [[nodiscard]] const EditorContext& context() const noexcept {
        return *context_;
    }
    [[nodiscard]] bool execute(EditorCommand command) noexcept;
    [[nodiscard]] bool saveOnClose() noexcept;
    [[nodiscard]] bool consumeLayoutResetRequest() noexcept;
    [[nodiscard]] const std::string& lastError() const noexcept {
        return lastError_;
    }

private:
    std::shared_ptr<EditorContext> context_;
    std::string lastError_;
    bool layoutResetRequested_ = false;
};

}  // namespace azurerender
