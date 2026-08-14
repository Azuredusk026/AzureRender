#pragma once

#include <string_view>

namespace azurerender {

class EditorContext;

class IEditorPanel {
public:
    virtual ~IEditorPanel() = default;

    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
    [[nodiscard]] virtual std::string_view title() const noexcept = 0;
    virtual void draw(EditorContext& context) = 0;
};

}  // namespace azurerender
