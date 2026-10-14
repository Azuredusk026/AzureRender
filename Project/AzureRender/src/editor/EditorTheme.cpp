#include "EditorTheme.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
#include <algorithm>
#include "ui/ThemeTokens.hpp"
#include "ui/UiMetrics.hpp"
namespace azurerender {
void EditorTheme::apply(float dpiScale) {
    auto& style=ImGui::GetStyle();style=ImGuiStyle{};
    ImGui::StyleColorsDark(&style);
    const auto tokens=ui::ThemeTokens::dark();const auto metrics=ui::UiMetrics::fromScale(std::clamp(dpiScale,.75F,3.F));
    auto& c=style.Colors;
    c[ImGuiCol_Text]=tokens.text;c[ImGuiCol_TextDisabled]=tokens.muted;
    c[ImGuiCol_WindowBg]=tokens.surface;c[ImGuiCol_ChildBg]=tokens.recessed;
    c[ImGuiCol_PopupBg]=tokens.surface;c[ImGuiCol_Border]=tokens.border;
    c[ImGuiCol_FrameBg]=tokens.control;c[ImGuiCol_FrameBgHovered]=tokens.hover;
    c[ImGuiCol_FrameBgActive]=tokens.selected;c[ImGuiCol_CheckMark]=tokens.accent;
    c[ImGuiCol_Button]=tokens.control;c[ImGuiCol_ButtonHovered]=tokens.hover;
    c[ImGuiCol_ButtonActive]=tokens.selected;c[ImGuiCol_Header]=tokens.selected;
    c[ImGuiCol_HeaderHovered]=tokens.hover;c[ImGuiCol_HeaderActive]=tokens.selected;
    c[ImGuiCol_TitleBg]=tokens.recessed;c[ImGuiCol_TitleBgActive]=tokens.control;
    c[ImGuiCol_MenuBarBg]=tokens.recessed;c[ImGuiCol_Tab]=tokens.surface;
    c[ImGuiCol_TabSelected]=tokens.selected;c[ImGuiCol_TabHovered]=tokens.hover;
    c[ImGuiCol_DockingEmptyBg]=tokens.recessed;
    style.WindowPadding={8,8};style.FramePadding={8,4};style.ItemSpacing={8,4};
    style.WindowRounding=6;style.FrameRounding=4;style.PopupRounding=6;
    style.WindowBorderSize=1;style.FrameBorderSize=1;style.TabBorderSize=0;
    style.FontSizeBase=14;style.FontScaleDpi=metrics.scale;
    style.ScaleAllSizes(style.FontScaleDpi);
}
}
#else
namespace azurerender { void EditorTheme::apply(float){} }
#endif
