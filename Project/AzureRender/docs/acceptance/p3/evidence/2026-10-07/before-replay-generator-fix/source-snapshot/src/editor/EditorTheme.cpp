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
    auto rgb=[](int r,int g,int b){return ImVec4(r/255.F,g/255.F,b/255.F,1);};
    const auto tokens=ui::ThemeTokens::dark();const auto metrics=ui::UiMetrics::fromScale(std::clamp(dpiScale,.75F,3.F));
    auto& c=style.Colors;
    c[ImGuiCol_Text]=tokens.text;c[ImGuiCol_TextDisabled]=tokens.muted;
    c[ImGuiCol_WindowBg]=tokens.surface;c[ImGuiCol_ChildBg]=tokens.recessed;
    c[ImGuiCol_PopupBg]=rgb(32,35,41);c[ImGuiCol_Border]=tokens.border;
    c[ImGuiCol_FrameBg]=rgb(20,22,26);c[ImGuiCol_FrameBgHovered]=rgb(48,59,74);
    c[ImGuiCol_FrameBgActive]=rgb(49,80,113);c[ImGuiCol_CheckMark]=tokens.accent;
    c[ImGuiCol_Button]=tokens.control;c[ImGuiCol_ButtonHovered]=tokens.hover;
    c[ImGuiCol_ButtonActive]=rgb(46,107,173);c[ImGuiCol_Header]=rgb(46,66,90);
    c[ImGuiCol_HeaderHovered]=rgb(57,86,119);c[ImGuiCol_HeaderActive]=rgb(46,107,173);
    c[ImGuiCol_TitleBg]=rgb(21,23,27);c[ImGuiCol_TitleBgActive]=rgb(36,43,53);
    c[ImGuiCol_MenuBarBg]=rgb(21,23,27);c[ImGuiCol_Tab]=rgb(24,27,33);
    c[ImGuiCol_TabSelected]=rgb(46,66,90);c[ImGuiCol_TabHovered]=rgb(57,86,119);
    c[ImGuiCol_DockingEmptyBg]=rgb(21,23,27);
    style.WindowPadding={8,8};style.FramePadding={8,4};style.ItemSpacing={8,4};
    style.WindowRounding=3;style.FrameRounding=3;style.PopupRounding=3;
    style.WindowBorderSize=1;style.FrameBorderSize=1;style.TabBorderSize=0;
    style.FontSizeBase=14;style.FontScaleDpi=metrics.scale;
    style.ScaleAllSizes(style.FontScaleDpi);
}
}
#else
namespace azurerender { void EditorTheme::apply(float){} }
#endif
