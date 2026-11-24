#pragma once
#include "ThemeTokens.hpp"
#include "UiMetrics.hpp"
namespace azurerender::ui {
// Fonts are borrowed from the active host's ImGui atlas.
struct UiContext {
    ImFont* body=nullptr;
    ImFont* title=nullptr;
    UiMetrics metrics=UiMetrics::fromScale(1);
    ThemeTokens theme=ThemeTokens::dark();
    ImFont* titleFont() const { return title?title:body?body:ImGui::GetFont(); }
};
}
